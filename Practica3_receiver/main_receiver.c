#include "ascii_handler.h"

void app_main(void){
    initUarts();
    clear_screen();
    while (1){
        clear_screen();
        bzero(buffer, MAX_CHARS+1);
        uart_reads(buffer, UART_RECEIVER, MAX_CHARS, VERBOSE);
        ESP_LOGI("UART_MAIN", "Data received: %s", buffer);
        print_ascii_art(UART_SENDER, buffer);
        // uart_puts(buffer, UART_SENDER);
    }
}