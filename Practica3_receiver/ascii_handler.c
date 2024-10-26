#include "ascii_handler.h"
#include "stdint.h"
static const int RX_BUFFER = 2048;
static char buffer[MAX_CHARS+1]={0};
const char *asciiFont[] = {
    // Letras A-Z (índices 0-25)
    "  ▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D",  // A
    " ▓▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓▓▓▓  \e[B\x1b[7D",  // B
    "  ▓▓▓▓ \e[B\x1b[7D ▓     \e[B\x1b[7D ▓     \e[B\x1b[7D ▓     \e[B\x1b[7D  ▓▓▓▓ \e[B\x1b[7D",  // C
    " ▓▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓▓▓▓  \e[B\x1b[7D",  // D
    " ▓▓▓▓▓ \e[B\x1b[7D ▓     \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D ▓     \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D",  // E
    " ▓▓▓▓▓ \e[B\x1b[7D ▓     \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D ▓     \e[B\x1b[7D ▓     \e[B\x1b[7D",  // F
    "  ▓▓▓▓ \e[B\x1b[7D ▓     \e[B\x1b[7D ▓  ▓▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D  ▓▓▓▓ \e[B\x1b[7D",  // G
    " ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D",  // H
    "  ▓▓▓  \e[B\x1b[7D   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // I
    "   ▓▓▓ \e[B\x1b[7D    ▓  \e[B\x1b[7D    ▓  \e[B\x1b[7D ▓  ▓  \e[B\x1b[7D  ▓▓   \e[B\x1b[7D",  // J
    " ▓   ▓ \e[B\x1b[7D ▓  ▓  \e[B\x1b[7D ▓▓▓   \e[B\x1b[7D ▓  ▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D",  // K
    " ▓     \e[B\x1b[7D ▓     \e[B\x1b[7D ▓     \e[B\x1b[7D ▓     \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D",  // L
    " ▓   ▓ \e[B\x1b[7D ▓▓ ▓▓ \e[B\x1b[7D ▓ ▓ ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D",  // M
    " ▓   ▓ \e[B\x1b[7D ▓▓  ▓ \e[B\x1b[7D ▓ ▓ ▓ \e[B\x1b[7D ▓  ▓▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D",  // N
    "  ▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // O
    " ▓▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓▓▓▓  \e[B\x1b[7D ▓     \e[B\x1b[7D ▓     \e[B\x1b[7D",  // P
    "  ▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓  ▓▓ \e[B\x1b[7D  ▓▓▓▓ \e[B\x1b[7D",  // Q
    " ▓▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓▓▓▓  \e[B\x1b[7D ▓  ▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D",  // R
    "  ▓▓▓▓ \e[B\x1b[7D ▓     \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D     ▓ \e[B\x1b[7D ▓▓▓▓  \e[B\x1b[7D",  // S
    " ▓▓▓▓▓ \e[B\x1b[7D   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D",  // T
    " ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // U
    " ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D  ▓ ▓  \e[B\x1b[7D   ▓   \e[B\x1b[7D",  // V
    " ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓ ▓ ▓ \e[B\x1b[7D ▓▓ ▓▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D",  // W
    " ▓   ▓ \e[B\x1b[7D  ▓ ▓  \e[B\x1b[7D   ▓   \e[B\x1b[7D  ▓ ▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D",  // X
    " ▓   ▓ \e[B\x1b[7D  ▓ ▓  \e[B\x1b[7D   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D",  // Y
    " ▓▓▓▓▓ \e[B\x1b[7D    ▓  \e[B\x1b[7D   ▓   \e[B\x1b[7D  ▓    \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D",  // Z
    // Números 0-9 (índices 26-35)
    "  ▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // 0
    "   ▓   \e[B\x1b[7D  ▓▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // 1
    "  ▓▓▓  \e[B\x1b[7D     ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D ▓     \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D",  // 2
    "  ▓▓▓  \e[B\x1b[7D     ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D     ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // 3
    " ▓   ▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D     ▓ \e[B\x1b[7D     ▓ \e[B\x1b[7D",  // 4
    " ▓▓▓▓▓ \e[B\x1b[7D ▓     \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D     ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // 5
    "  ▓▓▓  \e[B\x1b[7D ▓     \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // 6
    " ▓▓▓▓▓ \e[B\x1b[7D     ▓ \e[B\x1b[7D    ▓  \e[B\x1b[7D   ▓   \e[B\x1b[7D  ▓    \e[B\x1b[7D",  // 7
    "  ▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // 8
    "  ▓▓▓  \e[B\x1b[7D ▓   ▓ \e[B\x1b[7D  ▓▓▓▓ \e[B\x1b[7D     ▓ \e[B\x1b[7D  ▓▓▓  \e[B\x1b[7D",  // 9
    // Caracteres especiales (índices 36-40)
    "       \e[B\x1b[7D       \e[B\x1b[7D       \e[B\x1b[7D       \e[B\x1b[7D       \e[B\x1b[7D",  // Espacio
    "   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D   ▓   \e[B\x1b[7D       \e[B\x1b[7D   ▓   \e[B\x1b[7D",  // !
    "       \e[B\x1b[7D       \e[B\x1b[7D       \e[B\x1b[7D       \e[B\x1b[7D   ▓   \e[B\x1b[7D",  // .
    "       \e[B\x1b[7D   ▓   \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D   ▓   \e[B\x1b[7D       \e[B\x1b[7D",  // +
    "       \e[B\x1b[7D       \e[B\x1b[7D ▓▓▓▓▓ \e[B\x1b[7D       \e[B\x1b[7D       \e[B\x1b[7D",  // -
};
void clear_screen() {printf("\033[2J"); }

void hide_cursor() { printf("\033[?25l"); }

void show_cursor() { printf("\033[?25h"); }

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
        if (len == 1){
            return (char)data;
        }
    }
}


void uart_reads( char *msg, uint8_t uartNum, uint8_t len, int verboose){
    uint8_t data = 0;
    uint8_t i = 0;
    bzero(msg, len);
    while (1) {
        data = uart_readChar(uartNum);
        if (data == '\n' || data == '\r') break;
        else if (data == 127 && i > 0) { //Backspace
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
    ESP_ERROR_CHECK(uart_set_pin(UART_RECEIVER, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // Set UART_SENDER
   //  ESP_ERROR_CHECK(uart_driver_install(UART_SENDER, RX_BUFFER, RX_BUFFER, 0, NULL, 0));
   //  ESP_ERROR_CHECK(uart_param_config(UART_SENDER, &uart_config));
   //  ESP_ERROR_CHECK(uart_set_pin(UART_SENDER, TXD_PIN_SEND, RXD_PIN_SEND, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI("UART", "UART setup complete");
}
ascii_type_t get_char_type(char c) {
   switch (c) {
      case EXCLAMATION:
         return ASCII_EXCLAMATION_TYPE;
         break;
      case DOT:
         return ASCII_DOT_TYPE;
         break;
      case PLUS:
         return ASCII_PLUS_TYPE;
         break;
      case MINUS:
         return ASCII_MINUS_TYPE;
         break;
      case SPACE:
         return ASCII_SPACE_TYPE;
         break;
      case NEWLINE:
         return ASCII_NEWLINE_TYPE;
         break;
      case CARRIAGE_RETURN:
         return ASCII_CARRIAGE_RETURN_TYPE;
         break;
      default:
         if (c >= START_UPPERCASE && c <= END_UPPERCASE) return ASCII_LETTER_TYPE;
         if (c >= START_LOWERCASE && c <= END_LOWERCASE) return ASCII_LETTER_LOWERCASE_TYPE;
         if (c >= ASCII_NUMBERS_START && c <= ASCII_NUMBERS_END) return ASCII_NUMBER_TYPE;
   }
   return ASCII_ANOTHER_TYPE;
}


void print_single_ascii_art(uint8_t x_pos, uint8_t y_pos, uint8_t nextChar) {
   //printF to go to X, Y
   printf("\033[%d;%dH", y_pos, x_pos);
   printf("%s" ,asciiFont[nextChar]);
}

void print_ascii_art(uint8_t uartPort, char *str) {
   clear_screen();
   printf("\033[%d;%dH", 0, 0);
   char *aux = (char *)malloc(BUFFER_SIZE);
   snprintf(aux, BUFFER_SIZE,
            "Minimum console size as set in program: %dx%d"
            "\nEquivalent to:\t%d letters per line and %d lines"
            "\nWaiting for data...\n",
            CONSOLE_WIDTH, CONSOLE_HEIGHT, DESIRED_LETTERS_PER_LINE, DESIRED_LINES);
   // uart_putchar(uartPort, aux);
   uint8_t x_pos = 0, y_pos = ASCII_FONT_HEIGHT;
   while (*str) {
      char nextChar = *str++;
      switch (get_char_type(nextChar)) {
         case ASCII_LETTER_TYPE:
            nextChar -= 'A';
            break;
         case ASCII_LETTER_LOWERCASE_TYPE:
            nextChar -= 'a';
            break;
         case ASCII_NUMBER_TYPE:
            nextChar = (nextChar - '0') + ASCII_NUMBERS_START_INDEX;
            break;
         case ASCII_SPACE_TYPE:
            nextChar = ASCII_SPACE_INDEX;
            break;
         case ASCII_EXCLAMATION_TYPE:
            nextChar = ASCII_EXCLAMATION_INDEX;
            break;
         case ASCII_DOT_TYPE:
            nextChar = ASCII_DOT_INDEX;
            break;
         case ASCII_PLUS_TYPE:
            nextChar = ASCII_PLUS_INDEX;
            break;
         case ASCII_MINUS_TYPE:
            nextChar = ASCII_MINUS_INDEX;
            break;
         default:
            continue;
      }

      print_single_ascii_art(x_pos, y_pos, (uint8_t)nextChar);
      x_pos += ASCII_FONT_WIDTH;
      if (x_pos > CONSOLE_WIDTH - ASCII_FONT_WIDTH) {
         x_pos = 0;
         y_pos += ASCII_FONT_HEIGHT + TWO_SPACES;
      }
   }
   fflush(stdout);
}