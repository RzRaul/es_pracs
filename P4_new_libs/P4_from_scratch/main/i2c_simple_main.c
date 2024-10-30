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
#define I2C_MASTER_FREQ_HZ 100000
#define SAMPLE_COUNT UINT8_C(5)
#define DATA_LENGHT sizeof(int) * 3

#define BME280_ADDR 0x77
#define BME280_REG_CHIP_ID                        UINT8_C(0xD0)
#define BME280_REG_RESET                          UINT8_C(0xE0)
#define BME280_REG_TEMP_PRESS_CALIB_DATA          UINT8_C(0x88)
#define BME280_REG_HUMIDITY_CALIB_DATA            UINT8_C(0xE1)
#define BME280_REG_CTRL_HUM                       UINT8_C(0xF2)
#define BME280_REG_STATUS                         UINT8_C(0xF3)
#define BME280_REG_PWR_CTRL                       UINT8_C(0xF4)
#define BME280_REG_CTRL_MEAS                      UINT8_C(0xF4)
#define BME280_REG_CONFIG                         UINT8_C(0xF5)
#define BME280_REG_DATA                           UINT8_C(0xF7)
#define BME280_RESET_VALUE                        UINT8_C(0xB6) // Soft reset
#define BME280_NO_OVERSAMPLING                    UINT8_C(0x00)
#define BME280_OVERSAMPLING_1X                    UINT8_C(0x01)
#define BME280_OVERSAMPLING_2X                    UINT8_C(0x02)
#define BME280_OVERSAMPLING_4X                    UINT8_C(0x03)
#define BME280_OVERSAMPLING_8X                    UINT8_C(0x04)
#define BME280_OVERSAMPLING_16X                   UINT8_C(0x05)
#define BME280_STANDBY_TIME_0_5_MS                (0x00)
#define BME280_STANDBY_TIME_62_5_MS               (0x01)
#define BME280_STANDBY_TIME_125_MS                (0x02)
#define BME280_STANDBY_TIME_250_MS                (0x03)
#define BME280_STANDBY_TIME_500_MS                (0x04)
#define BME280_STANDBY_TIME_1000_MS               (0x05)
#define BME280_STANDBY_TIME_10_MS                 (0x06)
#define BME280_STANDBY_TIME_20_MS                 (0x07)
#define BME280_FILTER_COEFF_OFF                   (0x00)
#define BME280_FILTER_COEFF_2                     (0x01)
#define BME280_FILTER_COEFF_4                     (0x02)
#define BME280_FILTER_COEFF_8                     (0x03)
#define BME280_FILTER_COEFF_16                    (0x04)

static esp_err_t i2c_master_init();
static esp_err_t i2c_master_read_register(uint8_t reg, uint8_t *data, size_t len);
static esp_err_t i2c_master_write_register(i2c_master_dev_handle_t periph, uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr);
esp_err_t bme280_initialize();
esp_err_t bme280_soft_reset();
static void bme280_task(void *pvParameters);
void printRegs();

int8_t rslt;
typedef struct {
  /*! Compensated pressure */
  double pressure;

  /*! Compensated temperature */
  double temperature;

  /*! Compensated humidity */
  double humidity;
} bme280_data;

bme280_data bme_data;

static const char *TAG = "BME280";
i2c_slave_dev_handle_t slave_handler;
i2c_master_dev_handle_t bme280_dev;
i2c_master_bus_handle_t master_bus_handler;

i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = BME280_ADDR,
    .scl_speed_hz = I2C_MASTER_FREQ_HZ,
};

void app_main() {
  ESP_ERROR_CHECK(i2c_master_init());
  // ESP_ERROR_CHECK(init_slave_mode());
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
      i2c_master_bus_add_device(master_bus_handler, &dev_cfg, &bme280_dev);
  return err;
}

static esp_err_t i2c_master_read_register(uint8_t reg, uint8_t *data, size_t len) {
  bzero(data, len);
  esp_err_t err = i2c_master_transmit_receive(bme280_dev, &reg, sizeof(reg), data, len, 100);
  for(int i = len;i>=0; i--){
    ESP_LOGI("READ", "0x%x", data[i]);
  }
  ESP_LOGI("READ", "Rstl: %d", err);
  if (err != ESP_OK)
    return err;
  return err;
}

static esp_err_t i2c_master_write_register(i2c_master_dev_handle_t periph, uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr) {
  esp_err_t err;
  uint8_t *i2c_data = malloc(len * sizeof(uint8_t) + 1);
  memcpy(i2c_data+1, data, len);
  i2c_data[0] = reg_addr;
  //prints the hex of the data is sending, including the register
  
  err = i2c_master_transmit(periph, i2c_data, len + 1, portMAX_DELAY);
  for(int i = len;i>=0; i--){
    ESP_LOGI("WRITE", "0x%x", i2c_data[i]);
  }
  ESP_LOGI("WRITE", "Rstl: %d", err);
  free(i2c_data);
  return err;
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

esp_err_t bme280_initialize() {
    rslt = ESP_OK;
    bme280_soft_reset();
    //sends 0x01 to 0xF2 (CTRL_HUM) register
    uint8_t data = BME280_OVERSAMPLING_1X;
    rslt = i2c_master_write_register(bme280_dev, BME280_REG_CTRL_HUM, &data, 1, NULL);
    if(rslt != ESP_OK){
        ESP_LOGE(TAG, "Error writing to CTRL_HUM register");
        return rslt;
    }
    //sends 0x54 to 0xF4 (CTRL_MEAS) register
    data = BME280_OVERSAMPLING_16X << 2 | BME280_OVERSAMPLING_2X << 5 | 0x3;
    rslt = i2c_master_write_register(bme280_dev, BME280_REG_CTRL_MEAS, &data, 1, NULL);
    if(rslt != ESP_OK){
        ESP_LOGE(TAG, "Error writing to CTRL_MEAS register");
        return rslt;
    }
    //sends 0x10 to 0xF5 (CONFIG) register
    data = BME280_STANDBY_TIME_0_5_MS << 5 | BME280_FILTER_COEFF_16 << 2;
    rslt = i2c_master_write_register(bme280_dev, BME280_REG_CONFIG, &data, 1, NULL);
    if(rslt != ESP_OK){
        ESP_LOGE(TAG, "Error writing to CONFIG register");
        return rslt;
    }
    return ESP_OK;
}

esp_err_t bme280_soft_reset(){
    uint8_t data = BME280_RESET_VALUE;
    return i2c_master_write_register(bme280_dev, BME280_REG_RESET, &data, 1, NULL);
}

static void bme280_task(void *pvParameters) {
    float temperature, pressure, humidity;
    uint8_t aux_data[3] = {0};
    uint8_t data_to_reg = 0x55; // 0x55 is the value to set the sensor to forced mode
    printRegs();

    while (1) {
        //Writes to F4 register to set the sensor to forced mode
        i2c_master_write_register(bme280_dev, BME280_REG_CTRL_MEAS, &data_to_reg, 1, NULL);
        //Reads the temperature registers
        i2c_master_read_register(0xFA, &aux_data, 3);
        temperature = (aux_data[0] << 12 | aux_data[1] << 4 | aux_data[2] >> 4) / 16.0;
        //Reads the pressure registers
        i2c_master_read_register(0xF7, &aux_data, 3);
        pressure = (aux_data[0] << 12 | aux_data[1] << 4 | aux_data[2] >> 4) / 16.0;
        //Reads the humidity registers
        i2c_master_read_register(0xFD, &aux_data, 2);
        humidity = (aux_data[0] << 8 | aux_data[1]) / 1024.0;
        ESP_LOGI(TAG, "Temp %.2f C, Presión %.2f hPa, Humedad %.2f %%", temperature,
                pressure, humidity);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}