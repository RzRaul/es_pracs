#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "bme280.h"
#include "bme280_defs.h"


#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 1000000
#define BME280_ADDR 0x77     
#define BME280_ADDR_ID 0xD0               
#define SAMPLE_COUNT  UINT8_C(5)

static esp_err_t i2c_master_init();
static esp_err_t i2c_master_read_register(uint8_t reg, uint8_t *data, size_t len);
static esp_err_t i2c_master_write_register(uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr);
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
}bme280_data;

bme280_data data;

static const char *TAG = "BME280";

static void bme280_task(void *pvParameters) {
    float temperature, pressure, humidity;
    uint8_t who_am_i=0;
    rslt = i2c_master_read_register(BME280_ADDR_ID, &who_am_i, 1);
    ESP_LOGI(TAG, "BME280 WHO AM I?: 0x%x", who_am_i);
    //Reads TEMP_LOW, TEMP_HIGH, and TEMP_XLSB registers
    printRegs();
    rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
    rslt = bme280_get_sensor_data(BME280_ALL, &data, &dev);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printRegs();

    vTaskDelay(4000 / portTICK_PERIOD_MS);

    while (1) {

        rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
        rslt = bme280_get_sensor_data(BME280_ALL, &data, &dev);
        temperature = data.temperature;
        pressure = data.pressure;
        humidity = data.humidity;

        ESP_LOGI(TAG, "Temp: %.2f C, Presión: %.2f hPa, Humedad: %.2f %%", temperature, pressure, humidity);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void app_main() {
    ESP_ERROR_CHECK(i2c_master_init());
    bme280_initialize();
    xTaskCreate(bme280_task, "bme280_task", 4096, NULL, 5, NULL);
}

static esp_err_t i2c_master_init() {
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static esp_err_t i2c_master_read_register(uint8_t reg, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BME280_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BME280_ADDR << 1 | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK); // Read the last byte with NACK
    // Stop
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_master_write_register(uint8_t reg_addr, const uint8_t *data, uint32_t len,
                                                    void *intf_ptr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BME280_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, data,len, true);


    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}
//create a delay_us function using vTaskDelay
static void bme280_delay_us(uint32_t period, void *intf_ptr)
{
    vTaskDelay(period / portTICK_PERIOD_MS);
}

void bme280_initialize(){
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
void printRegs(){
    uint8_t readings[3];
    rslt = i2c_master_read_register(0xFA, readings, 3);
    ESP_LOGI(TAG, "\nTEMP_LOW: 0x%x\nTEMP_HIGH: 0x%x\nTEMP_XLSB: 0x%x", readings[0], readings[1], readings[2]);
    //Reads PRESSURE_LOW, PRESSURE_HIGH, and PRESSURE_XLSB registers
    rslt = i2c_master_read_register(0xF7, readings, 3);
    ESP_LOGI(TAG, "\n\nPRESSURE_LOW: 0x%x\nPRESSURE_HIGH: 0x%x\nPRESSURE_XLSB: 0x%x", readings[0], readings[1], readings[2]);
    //Reads HUMIDITY_LOW and HUMIDITY_HIGH registers
    rslt = i2c_master_read_register(0xFD, readings, 2);
    ESP_LOGI(TAG, "\n\nHUMIDITY_LOW: 0x%x\nHUMIDITY_HIGH: 0x%x\n", readings[0], readings[1]);
}