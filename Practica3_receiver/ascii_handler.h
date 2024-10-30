#ifndef ASCIIART
#define ASCIIART

#include "driver/uart.h"
#include "esp_system.h"
#include "esp_log.h"
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

#if ROLE == PRINTER
#define TXD_PIN GPIO_NUM_17
#define RXD_PIN GPIO_NUM_16
#define UART_RECEIVER UART_NUM_2

#define TXD_PIN_SEND GPIO_NUM_1
#define RXD_PIN_SEND GPIO_NUM_3
#define UART_SENDER UART_NUM_0
#define VERBOSE 0
#elif ROLE == PRINTER
#define TXD_PIN GPIO_NUM_1
#define RXD_PIN GPIO_NUM_3
#define UART_RECEIVER UART_NUM_0

#define TXD_PIN_SEND GPIO_NUM_17
#define RXD_PIN_SEND GPIO_NUM_16
#define UART_SENDER UART_NUM_2
#define VERBOSE 0
#endif

#define ASCII_FONT_HEIGHT 5
#define ASCII_FONT_WIDTH 7
#define DESIRED_LETTERS_PER_LINE 20
#define DESIRED_LINES 10
#define CONSOLE_WIDTH (DESIRED_LETTERS_PER_LINE * ASCII_FONT_WIDTH) //
#define CONSOLE_HEIGHT (DESIRED_LINES * (ASCII_FONT_HEIGHT * 2)) // Added extra height for the description
#define TWO_SPACES 2
#define ASCII_NUMBERS_START_INDEX 26
#define ASCII_NUMBERS_END_INDEX 35
#define ASCII_SPACE_INDEX 36
#define ASCII_EXCLAMATION_INDEX 37
#define ASCII_DOT_INDEX 38
#define ASCII_PLUS_INDEX 39
#define ASCII_MINUS_INDEX 40
#define ASCII_PRINTABLE_START 32
#define ASCII_PRINTABLE_END 126
#define ASCII_NUMBERS_START '0'
#define ASCII_NUMBERS_END '9'
#define NEWLINE '\n'
#define CARRIAGE_RETURN '\r'
#define NULL_TERMINATOR '\0'
#define SPACE ' '
#define EXCLAMATION '!'
#define DOT '.'
#define PLUS '+'
#define MINUS '-'
#define START_UPPERCASE 'A'
#define END_UPPERCASE 'Z'
#define START_LOWERCASE 'a'
#define END_LOWERCASE 'z'
#define BUFFER_SIZE 2048
#define GOTOXY_BUFFER_SIZE 10
#define MAX_CHARS 25


typedef enum {
   ASCII_LETTER_TYPE = 0,
   ASCII_LETTER_LOWERCASE_TYPE,
   ASCII_NUMBER_TYPE,
   ASCII_SPACE_TYPE,
   ASCII_NEWLINE_TYPE,
   ASCII_CARRIAGE_RETURN_TYPE,
   ASCII_EXCLAMATION_TYPE,
   ASCII_DOT_TYPE,
   ASCII_PLUS_TYPE,
   ASCII_MINUS_TYPE,
   ASCII_ANOTHER_TYPE,
} ascii_type_t;

extern const char *asciiFont[];


static const int RX_BUFFER;
static char buffer[MAX_CHARS+1];

void clear_screen();
void go_to_XY(uint8_t, uint8_t, uint8_t);
void hide_cursor();
void show_cursor();
void uart_putchar(char c, uint8_t uartNum);

void uart_puts(const char *str, uint8_t uartNum);

uint8_t uart_readChar(uint8_t uart_num);


void uart_reads( char *msg, uint8_t uartNum, uint8_t len, int verboose);

void initUarts(void);
ascii_type_t get_char_type(char c);
void print_single_ascii_art(uint8_t x_pos, uint8_t y_pos, uint8_t nextChar);

void print_ascii_art(uint8_t uartPort, char *str);

#endif