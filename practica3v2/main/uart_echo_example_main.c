#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#define SENDER 1
#define PRINTER 0
#define ROLE SENDER


static const int RX_BUF_SIZE = 1024;

#if ROLE == SENDER
#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_3)
#define UART_RECEIVER UART_NUM_0

#define TXD_PIN_SEND (GPIO_NUM_17)
#define RXD_PIN_SEND (GPIO_NUM_16)
#define UART_SENDER UART_NUM_2
#define VERBOSE 1
#else
#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)
#define UART_RECEIVER UART_NUM_2

#define TXD_PIN_SEND (GPIO_NUM_1)
#define RXD_PIN_SEND (GPIO_NUM_3)
#define UART_SENDER UART_NUM_0
#define VERBOSE 0
#endif

#define MAX_CHARS 25


void uart_putchar(char c, uint8_t uartNum){
    uart_write_bytes(uartNum, &c, 1);
}

void uart_puts(const char *str, uint8_t uartNum){
    uint8_t len = strlen(str);
    while(len--){
        uart_putchar(*str++, uartNum);
    }
}

char uart_readChar(uint8_t uart_num){
    uint8_t data = 0;
    while (1) {
        uint16_t len = uart_read_bytes(uart_num, &data, 1, portMAX_DELAY);
        if (len == 1) return (char) data;
    }
}

uint8_t uart_reads( char *msg, uint8_t uartNum, uint8_t len, int verboose){
    uint8_t data = 0;
    uint8_t i = 0;
    while (1) {
        data = uart_readChar(uartNum);
        if (data == '\n' || data == '\r') break;
        else if (data == 127 && i > 0) {
            i--;
        }else if (i < len - 1) {
            msg[i++] = data;
        }
    }
    msg[i] = '\0';
    return i;
}

void initUarts(void){
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };
    // Set UART_RECEIVER
    ESP_ERROR_CHECK(uart_driver_install(UART_RECEIVER, RX_BUF_SIZE, RX_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_RECEIVER, &uart_config));
    
    ESP_ERROR_CHECK(uart_set_pin(UART_RECEIVER, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // Set UART_SENDER
    ESP_ERROR_CHECK(uart_driver_install(UART_SENDER, RX_BUF_SIZE, RX_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_SENDER, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_SENDER, TXD_PIN_SEND, RXD_PIN_SEND, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI("UART", "UART setup complete");
}

void app_main(void){
    initUarts();
    char *data = (char *)malloc(RX_BUF_SIZE +1);
    while (1){
        uint8_t l = uart_reads(data, UART_RECEIVER, MAX_CHARS, VERBOSE);
        ESP_LOGI("UART", "Received: %s", data);
        uart_puts(data, UART_SENDER);
    }
    free(data);
}