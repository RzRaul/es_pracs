#include "bme280.h"
#include "bme280_defs.h"
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/i2c_types.h"
#include "portmacro.h"
#include "soc/clk_tree_defs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define I2C_MASTER_SCL_IO GPIO_NUM_22
#define I2C_MASTER_SDA_IO GPIO_NUM_21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_SLAVE_NUM I2C_NUM_1
#define I2C_MASTER_FREQ_HZ 200000
#define I2C_SALVE_SDA_IO GPIO_NUM_33
#define I2C_SALVE_SCL_IO GPIO_NUM_32
#define BME280_ADDR 0x77
#define BME280_ADDR_ID 0xD0
#define MY_SLAVE_ADDR 0x28
#define SAMPLE_COUNT UINT8_C(5)
#define DATA_LENGTH 10

static esp_err_t i2c_master_init();
static esp_err_t i2c_master_read_register(uint8_t reg, uint8_t *data,
                                          size_t len);
static esp_err_t i2c_master_write_register(uint8_t reg_addr,
                                           const uint8_t *data, uint32_t len,
                                           void *intf_ptr);
static void bme280_delay_us(uint32_t period, void *intf_ptr);
void bme280_initialize();
static void bme280_task(void *pvParameters);
void printRegs();

int8_t rslt;
uint32_t period;
struct bme280_dev dev;
struct bme280_settings settings;
typedef struct {
  /*! Compensated pressure */
  double pressure;

  /*! Compensated temperature */
  double temperature;

  /*! Compensated humidity */
  double humidity;
} bme280_data;

typedef struct {
  uint8_t *data;
  uint8_t reg;
  uint8_t dev_addr;
} i2c_package_t;
uint8_t done_counter = 0;
bme280_data bme_data;

static const char *TAG = "BME280";
QueueHandle_t s_receive_queue;
i2c_slave_dev_handle_t slave_handler;
i2c_master_dev_handle_t bme280_dev;
i2c_master_bus_handle_t master_bus_handler;

i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = BME280_ADDR,
    .scl_speed_hz = I2C_MASTER_FREQ_HZ,
};
static IRAM_ATTR bool i2c_slave_rx_done_callback(i2c_slave_dev_handle_t channel, const i2c_slave_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
      xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
      done_counter++;
    
    return high_task_wakeup == pdTRUE;
}

static void listen_bus_and_transmit(void * pvParameters){
    i2c_slave_rx_done_event_data_t rx_data;
    uint8_t data_rd[2] ={0};
    uint8_t aux_data[5] = {0x27, 0x9F, 0, 0, 0};
    rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
    if (rslt != BME280_OK) {
      ESP_LOGE(TAG, "Error setting sensor mode: %d", rslt);
    }
    rslt = bme280_get_sensor_data(BME280_ALL, &bme_data, &dev);
    if (rslt != BME280_OK) {
      ESP_LOGE(TAG, "Error setting sensor mode: %d", rslt);
    }
    aux_data[2] =bme_data.temperature;
    aux_data[3] =bme_data.pressure / 1000;
    aux_data[4] =bme_data.humidity;
    i2c_slave_transmit(slave_handler, aux_data, sizeof(aux_data), 500);
    ESP_LOGI(TAG, "Temp %.2f C, Presión %.2f hPa, Humedad %.2f %%", bme_data.temperature,
          bme_data.pressure, bme_data.humidity);
    while(1){
      
      
      ESP_ERROR_CHECK(i2c_slave_receive(slave_handler, data_rd, 2));
      // ESP_LOGI(TAG, "Received data: %x %x done_counter 1 %d", data_rd[0],data_rd[1], done_counter);
      xQueueReceive(s_receive_queue, &rx_data, portMAX_DELAY);
        // ESP_LOGI(TAG, "Received data: %x %x done_counter 2 %d", rx_data.buffer[0],rx_data.buffer[1], done_counter);
        // ESP_LOGI(TAG, "Received data: %x %x done_counter %d", data_rd[0],data_rd[1], done_counter);
        //Logs the data in hex
        if (rx_data.buffer[0] ==0x27 && rx_data.buffer[1] == 0x9F){
          rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
          if (rslt != BME280_OK) {
            ESP_LOGE(TAG, "Error setting sensor mode: %d", rslt);
          }
          rslt = bme280_get_sensor_data(BME280_ALL, &bme_data, &dev);
          if (rslt != BME280_OK) {
            ESP_LOGE(TAG, "Error setting sensor mode: %d", rslt);
          }
          aux_data[2] =bme_data.temperature;
          aux_data[3] =bme_data.pressure - 100000;
          aux_data[4] =bme_data.humidity;
          i2c_slave_transmit(slave_handler, aux_data, 5, 500);
          ESP_LOGI(TAG, "Temp %.2f C, Presión %.2f hPa, Humedad %.2f %%", bme_data.temperature,
                bme_data.pressure, bme_data.humidity);
        }
      
    }
}

static void bme280_task(void *pvParameters) {
  float temperature, pressure, humidity;
  int aux_data[3] = {0};
  printRegs();
  rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
  rslt = bme280_get_sensor_data(BME280_ALL, &bme_data, &dev);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  // printRegs();

  while (1) {

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
    if (rslt != BME280_OK) {
      ESP_LOGE(TAG, "Error setting sensor mode: %d", rslt);
    }
    rslt = bme280_get_sensor_data(BME280_ALL, &bme_data, &dev);
    if (rslt != BME280_OK) {
      ESP_LOGE(TAG, "Error setting sensor mode: %d", rslt);
    }
    aux_data[0] =bme_data.temperature;
    aux_data[1] =bme_data.pressure / 1000;
    aux_data[2] =bme_data.humidity;
    // i2c_slave_transmit(slave_handler, aux_data, sizeof(aux_data), 100);
    ESP_LOGI(TAG, "Temp %.2f C, Presión %.2f hPa, Humedad %.2f %%", bme_data.temperature,
             bme_data.pressure, bme_data.humidity);
    ESP_LOGI(TAG, "Temp: %d C, Presión: %d hPa, Humedad: %d %%", aux_data[0],
             aux_data[1], aux_data[2]);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

esp_err_t init_slave_mode() {
  esp_err_t err;
  i2c_slave_config_t i2c_slave_conf = {
    .addr_bit_len = I2C_ADDR_BIT_LEN_7,
      .i2c_port = I2C_SLAVE_NUM, // Autoselect
      .slave_addr = MY_SLAVE_ADDR,
      .scl_io_num = I2C_SALVE_SCL_IO,
      .sda_io_num = I2C_SALVE_SDA_IO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .send_buf_depth = 256,
  };
  err = (i2c_new_slave_device(&i2c_slave_conf, &slave_handler));
  s_receive_queue = xQueueCreate(1, sizeof(i2c_slave_rx_done_event_data_t));
  i2c_slave_event_callbacks_t cbs = {
      .on_recv_done = i2c_slave_rx_done_callback,
  };
  ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(slave_handler, &cbs, s_receive_queue));  
  return err;
}

void app_main() {
  ESP_ERROR_CHECK(i2c_master_init());
  ESP_ERROR_CHECK(init_slave_mode());
  bme280_initialize();
  // xTaskCreate(bme280_task, "bme280_task", 4096, NULL, 5, NULL);
  xTaskCreate(listen_bus_and_transmit, "listen_bus_and_transmit", 4096, NULL, NULL, NULL);
  
  while (1) {
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    // ESP_LOGI("Main", "Working...");
  }
}

static esp_err_t i2c_master_init() {
  i2c_master_bus_config_t conf = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_MASTER_NUM,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .flags.enable_internal_pullup = true,
  };
  esp_err_t err = i2c_new_master_bus(&conf, &master_bus_handler);
  if (err != ESP_OK)
    return err;
  err =
      i2c_master_bus_add_device(master_bus_handler, &dev_cfg, &bme280_dev);
  return err;
}

static esp_err_t IRAM_ATTR i2c_master_read_register(uint8_t reg, uint8_t *data, size_t len) {
  bzero(data, len);
  esp_err_t err = i2c_master_transmit_receive(bme280_dev, &reg, sizeof(reg), data, len, 100);
  if (err != ESP_OK)
    return err;
  return err;
}

static esp_err_t IRAM_ATTR i2c_master_write_register(uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr) {
  esp_err_t err;
  uint8_t *i2c_data = malloc(len * sizeof(uint8_t) + 1);
  memcpy(i2c_data+1, data, len);
  i2c_data[0] = reg_addr;
  //prints the hex of the data is sending, including the register
  
  err = i2c_master_transmit(bme280_dev, i2c_data, len + 1, portMAX_DELAY);
  free(i2c_data);
  return err;
}
// create a delay_us function using vTaskDelay
static void bme280_delay_us(uint32_t period, void *intf_ptr) {
  vTaskDelay(period / portTICK_PERIOD_MS);
}

void bme280_initialize() {
  rslt = BME280_OK;

  dev.chip_id = BME280_ADDR;
  dev.intf = BME280_I2C_INTF;
  dev.read = (bme280_read_fptr_t)i2c_master_read_register;
  dev.write = (bme280_write_fptr_t)i2c_master_write_register;
  dev.delay_us = (bme280_delay_us_fptr_t)bme280_delay_us;
  rslt = bme280_init(&dev);
  ESP_LOGI(TAG, "bme280_init() result: %d", rslt);

  /* Recommended mode of operation: Indoor navigation */
  settings.osr_h = BME280_OVERSAMPLING_1X;
  settings.osr_p = BME280_OVERSAMPLING_16X;
  settings.osr_t = BME280_OVERSAMPLING_1X;
  settings.filter = BME280_FILTER_COEFF_16;
  settings.standby_time = BME280_STANDBY_TIME_0_5_MS;
  rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &dev);
  ESP_LOGI(TAG, "bme280_set_sensor_settings() result: %d", rslt);
  ESP_LOGI(TAG, "bme280_set_sensor_mode() result: %d", rslt);
}
void printRegs() {
  uint8_t readings[3];
  rslt = i2c_master_read_register(0xFA, readings, 3);
  ESP_LOGI(TAG, "\nTEMP_LOW: 0x%x\nTEMP_HIGH: 0x%x\nTEMP_XLSB: 0x%x",
           readings[0], readings[1], readings[2]);
  // Reads PRESSURE_LOW, PRESSURE_HIGH, and PRESSURE_XLSB registers
  rslt = i2c_master_read_register(0xF7, readings, 3);
  ESP_LOGI(TAG,
           "\n\nPRESSURE_LOW: 0x%x\nPRESSURE_HIGH: 0x%x\nPRESSURE_XLSB: 0x%x",
           readings[0], readings[1], readings[2]);
  // Reads HUMIDITY_LOW and HUMIDITY_HIGH registers
  rslt = i2c_master_read_register(0xFD, readings, 2);
  ESP_LOGI(TAG, "\n\nHUMIDITY_LOW: 0x%x\nHUMIDITY_HIGH: 0x%x\n", readings[0],
           readings[1]);
}
