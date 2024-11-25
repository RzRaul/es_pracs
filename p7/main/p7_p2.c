#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LCD_20X04 1

#define I2C_ACK_CHECK_EN 1
#define I2C_ADDRESS_LCD 0x27
#define I2C_SCL_LCD 21
#define I2C_SDA_LCD 22

/* Comandos de la LCD */
#define CLEAR_DISPLAY 0x01
#define RETURN_HOME_UNSHIFT 0x02
#define CURSOR_RIGHT_NO_SHIFT 0x04
#define CURSOR_RIGHT_SHIFT 0x05
#define CURSOR_RIGHT_NO_SHIFT_LEFT 0x06
#define CURSOR_RIGHT_SHIFT_LEFT 0x07
#define DISPLAY_OFF 0x08
#define DISPLAY_ON_CURSOR_OFF 0x0C
#define DISPLAY_ON_CURSOR_ON_STEADY 0x0E
#define DISPLAY_ON_CURSOR_ON_BLINK 0x0F
#define SHIFT_CURSOR_LEFT 0x10
#define SHIFT_CURSOR_RIGHT 0x14
#define SHIFT_DISPLAY_LEFT 0x18
#define SHIFT_DISPLAY_RIGHT 0x1C
#define SET_4BIT_MODE 0x28
#define RETURN_HOME 0x80

/* PCF8574 */
#define PCF8574_RS 0
#define PCF8574_RW 1
#define PCF8574_EN 2
#define PCF8574_BL 3

#define LCD_RS_CMD 0
#define LCD_RS_DATA 1
#define LCD_LEN 20
#define LCD_SHOW_ROW 1
#define LCD_REFRESH_TIME_MS (60 / portTICK_PERIOD_MS)

typedef struct {
    uint8_t i2c_address;
    uint8_t i2c_port;
    uint8_t screen_size;
    uint8_t screen_backlight;
} lcd_i2c_device_t;

void i2c_init(void);
void lcd_init(lcd_i2c_device_t *lcd);
void lcd_i2c_write_byte(lcd_i2c_device_t *lcd, uint8_t data);
void lcd_i2c_write_command(lcd_i2c_device_t *lcd, uint8_t register_select,
                           uint8_t cmd);
void lcd_set_cursor(lcd_i2c_device_t *lcd, uint8_t column, uint8_t row);
void lcd_i2c_write_custom_char(lcd_i2c_device_t *lcd, uint8_t char_address,
                               const uint8_t *pixels);

void i2c_init(void) {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_LCD,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_LCD,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    esp_err_t error = i2c_param_config(I2C_NUM_1, &i2c_config);
    if (error != ESP_OK) {
        while (1)
            ;
    }
    i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);
}

void lcd_init(lcd_i2c_device_t *lcd) {
    lcd_i2c_write_command(lcd, LCD_RS_CMD, RETURN_HOME_UNSHIFT);
    lcd_i2c_write_command(lcd, LCD_RS_CMD, SET_4BIT_MODE);
    lcd_i2c_write_command(lcd, LCD_RS_CMD, CLEAR_DISPLAY);
    lcd_i2c_write_command(lcd, LCD_RS_CMD, DISPLAY_ON_CURSOR_OFF);
    lcd_i2c_write_command(lcd, LCD_RS_CMD, CURSOR_RIGHT_NO_SHIFT_LEFT);
    vTaskDelay(20 / portTICK_PERIOD_MS);
}

void lcd_i2c_write_byte(lcd_i2c_device_t *lcd, uint8_t data) {
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle,
                          (lcd->i2c_address << 1) | I2C_MASTER_WRITE,
                          I2C_ACK_CHECK_EN);
    i2c_master_write_byte(cmd_handle, data, 1);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(lcd->i2c_port, cmd_handle, 100 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
}

void lcd_i2c_write_command(lcd_i2c_device_t *lcd, uint8_t register_select,
                           uint8_t cmd) {
    uint8_t config = (register_select) ? (1 << PCF8574_RS) : 0;
    config |= (lcd->screen_backlight) ? (1 << PCF8574_BL) : 0;

    config |= (config & 0x0F) | (0xF0 & cmd);
    config |= (1 << PCF8574_EN);
    lcd_i2c_write_byte(lcd, config);
    ets_delay_us(10);
    config &= ~(1 << PCF8574_EN);
    lcd_i2c_write_byte(lcd, config);
    ets_delay_us(50);

    config = (config & 0x0F) | (cmd << 4);
    config |= (1 << PCF8574_EN);
    lcd_i2c_write_byte(lcd, config);
    ets_delay_us(10);
    config &= ~(1 << PCF8574_EN);
    lcd_i2c_write_byte(lcd, config);
    ets_delay_us(50);

    if (cmd == CLEAR_DISPLAY) {
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void lcd_set_cursor(lcd_i2c_device_t *lcd, uint8_t column, uint8_t row) {
    switch (row) {
    case 0:
        lcd_i2c_write_command(lcd, LCD_RS_CMD, 0x80 + column);
        break;
    case 1:
        lcd_i2c_write_command(lcd, LCD_RS_CMD, 0xC0 + column);
        break;
    case 2:
        lcd_i2c_write_command(lcd, LCD_RS_CMD, 0x94 + column);
        break;
    case 3:
        lcd_i2c_write_command(lcd, LCD_RS_CMD, 0xD4 + column);
        break;
    default:
        break;
    }
}

void lcd_i2c_write_custom_char(lcd_i2c_device_t *lcd, uint8_t address,
                               const uint8_t *pixels) {
    lcd_i2c_write_command(lcd, LCD_RS_CMD, 0x40 | (address << 3));

    for (uint8_t i = 0; i < 8; i++) {
        lcd_i2c_write_command(lcd, LCD_RS_DATA, pixels[i]);
    }

    lcd_i2c_write_command(lcd, LCD_RS_CMD, RETURN_HOME);
}
void lcd_i2c_print_msg_circular(lcd_i2c_device_t *lcd, char *msg, int len) {
    int pos_aux, full_len = len + LCD_LEN - 1;
    char *buff = (char *)malloc(full_len);
    memset(buff, ' ', LCD_LEN - 1);
    memcpy(buff + LCD_LEN - 1, msg, len);
    for (int i = 0; i < full_len; i++) {
        for (uint8_t j = 0; j < LCD_LEN; j++) {
            pos_aux = (i + j) % full_len;
            lcd_i2c_write_command(lcd, LCD_RS_DATA, buff[pos_aux]);
        }

        vTaskDelay(LCD_REFRESH_TIME_MS);
        lcd_set_cursor(lcd, 0, LCD_SHOW_ROW);
    }
    free(buff);
}

void lcd_i2c_print_msg(lcd_i2c_device_t *lcd, char *msg) {
    uint8_t i = 0;

    while (msg[i] != '\0') {
        lcd_i2c_write_command(lcd, LCD_RS_DATA, msg[i++]);
    }
}

#define SMILE 5
#define DROP 2

void app_main() {
    i2c_init();

    lcd_i2c_device_t my_lcd = {
        .i2c_port = I2C_NUM_1,
        .i2c_address = I2C_ADDRESS_LCD,
        .screen_size = LCD_20X04,
        .screen_backlight = 1,
    };

    vTaskDelay(20 / portTICK_PERIOD_MS);
    lcd_init(&my_lcd);

    uint8_t smile[8] = {0x00, 0x0A, 0x0A, 0x00, 0x11, 0x0E, 0x00, 0x00};
    uint8_t drop[8] = {0x04, 0x04, 0x0E, 0x0E, 0x1F, 0x1F, 0x1F, 0x0E};

    lcd_i2c_write_custom_char(&my_lcd, SMILE, smile);
    lcd_i2c_write_custom_char(&my_lcd, DROP, drop);

    bool print_msg = true;
    uint8_t conteo_msg = 0;

    char message[] = {'H', 'o', 'l', 'a',  ' ',   'M', 'u',
                      'n', 'd', 'o', DROP, SMILE, 0};
    char spaces_char[] = "            ";
    char msgg[] = "Este es un mensaje de mas de 20 chars";

    while (1) {
        lcd_set_cursor(&my_lcd, 0, LCD_SHOW_ROW);
        lcd_i2c_print_msg_circular(&my_lcd, message, strlen(message));
        vTaskDelay(250 / portTICK_PERIOD_MS);
        lcd_i2c_print_msg_circular(&my_lcd, message, strlen(message));
        vTaskDelay(250 / portTICK_PERIOD_MS);
        lcd_i2c_print_msg_circular(&my_lcd, msgg, strlen(msgg));
        vTaskDelay(250 / portTICK_PERIOD_MS);
        lcd_i2c_print_msg_circular(&my_lcd, msgg, strlen(msgg));
    }
}
