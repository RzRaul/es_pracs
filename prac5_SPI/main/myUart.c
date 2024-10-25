
#include "myUart.h"
#include "esp_log.h"


void init_uart(void){
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(CONSOLE_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(CONSOLE_UART, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, 2048, 2048, 0, NULL, 0));
    ESP_LOGI("UART", "UART inicializado");
}

void uart_putchar(uint8_t uartNum, char c){
    uart_write_bytes(CONSOLE_UART, &c, 1);
}

void uart_puts(uint8_t uartNum, char *str){
    while(*str){
        uart_putchar(uartNum, *str++);
    }
}

char uart_readchar(uint8_t uartNum){
    uint8_t data;
    uart_read_bytes(uartNum, &data, 1, portMAX_DELAY);
    return data;
}

void uart_reads(uint8_t uartNum, char *buf, int max_len, bool verbose){
    uint8_t aux_len = 0;
    char c;
    while(1){
        c = uart_readchar(uartNum);
        if(c == '\n' || c == '\r'){
            break;
        }
        if(c == 127 || c == 8){ //Bakspace or Delete
            if(aux_len > 0){
                aux_len--;
                uart_putchar(uartNum, 8);
                uart_putchar(uartNum, ' ');
                uart_putchar(uartNum, 8);
            }
            continue;
        }
        if(aux_len < max_len){
            buf[aux_len++] = c;
            if (verbose){
                uart_putchar(uartNum, c);
            }
        }
    }
    buf[aux_len] = '\0';
}

