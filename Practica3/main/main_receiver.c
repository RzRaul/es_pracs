#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#define SENDER 1
#define PRINTER 0
#define ROLE PRINTER


#define TXD_PIN GPIO_NUM_17
#define RXD_PIN GPIO_NUM_16
#define UART_RECEIVER UART_NUM_2

#define TXD_PIN_SEND GPIO_NUM_1
#define RXD_PIN_SEND GPIO_NUM_3
#define UART_SENDER UART_NUM_0
#define VERBOSE 0


#define MAX_CHARS 25

static const int RX_BUFFER = 1024;

void uart_putchar(char c, uint8_t uartNum){
    uart_write_bytes(uartNum, &c, 1);
}

void uart_puts(const char *str, uint8_t uartNum){
    uint8_t len = strlen(str);
    while(len--){
        uart_putchar(*str++, uartNum);
    }
}

uint8_t uart_readChar(uint8_t uart_num){
    uint8_t data = 0;
    while (1) {
        uint16_t len = uart_read_bytes(uart_num, &data, 1, portMAX_DELAY);
        if (len == 1) return data;
    }
}

void uart_reads( char *msg, uint8_t uartNum, uint8_t len, int verboose){
    uint8_t data = 0;
    uint8_t i = 0;
    bzero(msg, len);
    while (1) {
        data = uart_readChar(uartNum);
        if (data == '\n' || data == '\r') break;
        else if (data == 127 && i > 0) {
            i--;
            if (verboose) uart_puts("\b \b", uartNum);
        }else if (i < len - 1) {
            msg[i++] = data;
            if (verboose) uart_putchar(data, uartNum);
        }
    }
    msg[i] = '\0';
    if (verboose) uart_puts("\n", uartNum);

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
    ESP_ERROR_CHECK(uart_driver_install(UART_RECEIVER, RX_BUFFER, RX_BUFFER, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_RECEIVER, &uart_config));
    // Set UART_SENDER
    // ESP_ERROR_CHECK(uart_driver_install(UART_SENDER, RX_BUFFER, RX_BUFFER, 0, NULL, 0));
    // ESP_ERROR_CHECK(uart_param_config(UART_SENDER, &uart_config));
    ESP_LOGI("UART", "UART setup complete");
}

void app_main(void){
    initUarts();
    char *data = (char *)malloc(RX_BUFFER+1);
    if (data == NULL){
        ESP_LOGE("UART", "Failed to allocate memory");
        return;
    }
    ESP_LOGI("UART", "UART setup complete");
    while (1){
        bzero(data, MAX_CHARS+1);
        uart_reads(data, UART_RECEIVER, MAX_CHARS, VERBOSE);
        ESP_LOGI("UART", "Data received: %s", data);
        // uart_puts(data, UART_SENDER);
    }
    free(data);
}