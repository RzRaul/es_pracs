#ifndef MYUART_H
#define MYUART_H

#include "driver/gpio.h"
#include "driver/uart.h"

#define CONSOLE_UART UART_NUM_0

void init_uart(void);

void uart_putchar(uint8_t uartNum, char c);

void uart_puts(uint8_t uartNum, char *str);

char uart_readchar(uint8_t uartNum);

void uart_reads(uint8_t uartNum, char *buf, int max_len, bool verbose);

#endif