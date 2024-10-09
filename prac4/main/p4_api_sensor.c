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

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 1000000
#define BME280_ADDR 0x77
#define BME280_ADDR_ID 0xD0
#define SAMPLE_COUNT UINT8_C(5)
#define DATA_LENGHT sizeof(int) * 3

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

bme280_data data;

static const char *TAG = "BME280";
i2c_slave_dev_handle_t slave_handler;
i2c_master_dev_handle_t master_handler;
i2c_master_bus_handle_t master_bus_handler;

i2c_device_config_t dev_cfg = {
    .dev_addr_length = 7,
    .device_address = BME280_ADDR,
    .scl_speed_hz = I2C_MASTER_FREQ_HZ,
};

static void bme280_task(void *pvParameters) {
  float temperature, pressure, humidity;
  int aux_data[3] = {0};
  rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
  rslt = bme280_get_sensor_data(BME280_ALL, &data, &dev);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  printRegs();

  while (1) {

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
    rslt = bme280_get_sensor_data(BME280_ALL, &data, &dev);
    temperature = data.temperature;
    pressure = data.pressure;
    humidity = data.humidity;
    aux_data[0] = temperature;
    aux_data[1] = pressure;
    aux_data[2] = humidity;
    i2c_slave_transmit(slave_handler, aux_data, DATA_LENGHT, 1000);
    ESP_LOGI(TAG, "Temp: %d C, Presión: %d hPa, Humedad: %d %%", aux_data[0],
             aux_data[1], aux_data[2]);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

esp_err_t init_slave_mode() {
  esp_err_t err;
  i2c_slave_config_t i2c_slave_conf = {
      .i2c_port = I2C_NUM_0, // Autoselect
      .slave_addr = 0x26,
      .scl_io_num = GPIO_NUM_12,
      .sda_io_num = GPIO_NUM_14,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .send_buf_depth = 256,
  };
  err = (i2c_new_slave_device(&i2c_slave_conf, &slave_handler));
  return err;
}

void app_main() {
  ESP_ERROR_CHECK(i2c_master_init());
  ESP_ERROR_CHECK(init_slave_mode());
  bme280_initialize();
  xTaskCreate(bme280_task, "bme280_task", 4096, NULL, 5, NULL);
  while (1) {
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    ESP_LOGI("Main", "Working...");
  }
}

static esp_err_t i2c_master_init() {
  i2c_master_bus_config_t conf = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_NUM_1,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .flags.enable_internal_pullup = true,
  };
  esp_err_t err = i2c_new_master_bus(&conf, &master_bus_handler);
  if (err != ESP_OK)
    return err;
  err =
      i2c_master_bus_add_device(master_bus_handler, &dev_cfg, &master_handler);
  return err;
}

static esp_err_t i2c_master_read_register(uint8_t reg, uint8_t *data,
                                          size_t len) {
  // ESP_ERROR_CHECK(
  //     i2c_master_bus_add_device(master_bus_handler, &dev_cfg,
  //     &master_handler));
  esp_err_t err = i2c_master_transmit_receive(master_handler, &reg, sizeof(reg),
                                              data, len, -1);
  if (err != ESP_OK)
    return err;
  // err = i2c_master_bus_rm_device(master_handler);
  return err;
}

static esp_err_t i2c_master_write_register(uint8_t reg_addr,
                                           const uint8_t *data, uint32_t len,
                                           void *intf_ptr) {
  esp_err_t err;
  uint8_t *i2c_data = malloc(len * sizeof(uint8_t) + 1);
  memcpy(i2c_data, data, len);
  i2c_data[len] = reg_addr;
  //  ESP_ERROR_CHECK(
  //     i2c_master_bus_add_device(master_bus_handler, &dev_cfg,
  //     &master_handler));
  err = i2c_master_transmit(master_handler, i2c_data, len + 2, -1);
  // i2c_master_bus_rm_device(master_handler);
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
  settings.osr_t = BME280_OVERSAMPLING_2X;
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
