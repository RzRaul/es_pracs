#include "driver/i2c_master.h"
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
#define ESP32_SLAVE_ADDR 0x26
#define SAMPLE_COUNT UINT8_C(5)
#define DATA_LENGHT sizeof(int) * 3

static esp_err_t i2c_master_init();
static esp_err_t i2c_master_read_register(uint8_t reg, uint8_t *data,
                                          size_t len);
static esp_err_t i2c_master_write_register(uint8_t reg_addr,
                                           const uint8_t *data, uint32_t len,
                                           void *intf_ptr);
static void read_esp32_slave_task(void *pvParameters);
void printRegs();

int8_t rslt;
uint32_t period;

static const char *TAG = "ESP";
i2c_master_dev_handle_t master_handler;
i2c_master_bus_handle_t master_bus_handler;

i2c_device_config_t dev_cfg = {
    .dev_addr_length = 7,
    .device_address = ESP32_SLAVE_ADDR,
    .scl_speed_hz = I2C_MASTER_FREQ_HZ,
};

static void read_esp32_slave_task(void *pvParameters) {
  int aux_data[3] = {0};

  while (1) {
    esp_err_t err = i2c_master_receive(master_handler, (uint8_t *)aux_data,
                                       sizeof(int) * 3, -1);
    if (err != ESP_OK)
      ESP_LOGE("i2cD", "Trouble reading esp32");
    ESP_LOGI(TAG, "Temp: %d C, Presión: %d hPa, Humedad: %d %%", aux_data[0],
             aux_data[1], aux_data[2]);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void app_main() {
  ESP_ERROR_CHECK(i2c_master_init());
  xTaskCreate(read_esp32_slave_task, "esp32R", 4096, NULL, 5, NULL);
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
