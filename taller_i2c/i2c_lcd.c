
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include <stdio.h>

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

typedef struct {
  uint8_t i2c_address;
  uint8_t i2c_port;
  uint8_t screen_size;
  uint8_t screen_backlight;
} lcd_i2c_device_t; // Estructura para la información del dispositivo i2C

void i2c_init(void);
void lcd_init(lcd_i2c_device_t *lcd);
void lcd_i2c_write_byte(lcd_i2c_device_t *lcd, uint8_t data);
void lcd_i2c_write_command(lcd_i2c_device_t *lcd, uint8_t register_select,
                           uint8_t cmd);
void lcd_set_cursor(lcd_i2c_device_t *lcd, uint8_t column, uint8_t row);
void lcd_i2c_write_custom_char(lcd_i2c_device_t *lcd, uint8_t char_address,
                               const uint8_t *pixels);

/* Función que inicializa el dispositivo I2C de acuerdo a los Symbols definidos
en la biblioteca Argumentos: Ninguno Retornos: Ninguno
*/
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

/* Función que utiliza el protocolo I2C para escribir un Byte en el Bus
con destino el dispositivo que se le indique
Argumentos: lcd, struct tipo lcd_i2c_device_t
            data, uint8_t, byte a enviar al dispositivo
Retorno: Ninguno
*/
void lcd_i2c_write_byte(lcd_i2c_device_t *lcd, uint8_t data) {
  i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
  i2c_master_start(cmd_handle);
  i2c_master_write_byte(cmd_handle, (lcd->i2c_address << 1) | I2C_MASTER_WRITE,
                        I2C_ACK_CHECK_EN);
  i2c_master_write_byte(cmd_handle, data, 1);
  i2c_master_stop(cmd_handle);
  i2c_master_cmd_begin(lcd->i2c_port, cmd_handle, 100 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd_handle);
}

/* Función que le manda un comando al dispositivo LCD por medio de I2C
Apoyándose en la función de lcd_i2c_write_byte
Argumentos: lcd, struct lcd_i2c_device_t, destino
            register_select, uint8_t, especifica si lo enviado
            es Comando o Dato, ya que se utiliza el mismo bus para ambos, pero
diferente registro cmd, uint8_t, especifica el comando que se va a enviar por el
bus

*/
void lcd_i2c_write_command(lcd_i2c_device_t *lcd, uint8_t register_select,
                           uint8_t cmd) {
  // Coloca los bits para el Registro de instrucciones (0) o registro de
  // datos(1)
  uint8_t config = (register_select) ? (1 << PCF8574_RS) : 0;

  // Coloca los bits en config para el encendido o apagado del LCD
  config |= (lcd->screen_backlight) ? (1 << PCF8574_BL) : 0;

  // Prepara el byte para ambas partes de los comandos de 8 bits
  // Parte alta y baja (Ya que se inicia en modo 4 bits)
  config |= (config & 0x0F) | (0xF0 & cmd);

  // Coloca el bit necesario para habilitar el LCD y que comience a trabajar
  config |= (1 << PCF8574_EN);

  // Escribe el byte generado con las instrucciones anteriores al bus de I2C con
  // el LCD de destino
  lcd_i2c_write_byte(lcd, config);
  ets_delay_us(10);

  // Baja la linea E para que el LCD cargue los valores
  config &= ~(1 << PCF8574_EN);
  lcd_i2c_write_byte(lcd, config);
  ets_delay_us(50);

  // Prepara los bits de la parte baja del comando para enviarlos
  // En la parte alta del byte, conservando las configuraciones
  // Como el register_select
  config = (config & 0x0F) | (cmd << 4);
  // Habilita E para comenzar a leer del bus
  config |= (1 << PCF8574_EN);
  lcd_i2c_write_byte(lcd, config);
  ets_delay_us(10);
  // Baja E para completar la escritura al Instruction register
  config &= ~(1 << PCF8574_EN);
  lcd_i2c_write_byte(lcd, config);
  ets_delay_us(50);

  // Si el comando era limiar pantalla, le da tiempo al LCD de ejecutarlo
  if (cmd == CLEAR_DISPLAY) {
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

/* Inicializa el dispositivo I2C
 * Para iniciarlizarlo, manda comandos para regresar el cursos a su posición de
 inicio
 * Luego indica al LCd que utilice el modo de 4 bits
 * Limpia la pantalla, desactiva el cursor y mueve el cursor sin cambiar valores
 de las
 * DDRAM
 * Argumentos: lcd, El dispositivo I2C al cual se le envían los comandos
 * Retorna: Nada
 *

*/
void lcd_init(lcd_i2c_device_t *lcd) {
  lcd_i2c_write_command(lcd, LCD_RS_CMD, RETURN_HOME_UNSHIFT);
  lcd_i2c_write_command(lcd, LCD_RS_CMD, SET_4BIT_MODE);
  lcd_i2c_write_command(lcd, LCD_RS_CMD, CLEAR_DISPLAY);
  lcd_i2c_write_command(lcd, LCD_RS_CMD, DISPLAY_ON_CURSOR_OFF);
  lcd_i2c_write_command(lcd, LCD_RS_CMD, CURSOR_RIGHT_NO_SHIFT_LEFT);
  vTaskDelay(20 / portTICK_PERIOD_MS);
}
/*
 * Mueve el cursor a una posición dada por el reglón y columna
 * Argumentos: column, row, posiciones X,Y del LCD
 *            lcd, dispositivo LCD  destino
 */
void lcd_set_cursor(lcd_i2c_device_t *lcd, uint8_t column, uint8_t row) {
  switch (row) {
  case 0: // Si el renglón es el primero, la dirección empieza en 0x80
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
/* Los caracteres custom, se almacenan en direcciones específicas
 * las cuales serían los bytes 0-7 de la DDRAM donde se encuentran los chars
 * 0x40
 */
void lcd_i2c_write_custom_char(lcd_i2c_device_t *lcd, uint8_t address,
                               const uint8_t *pixels) {
  lcd_i2c_write_command(lcd, LCD_RS_CMD, 0x40 | (address << 3));
  // Carga los chars linea por linea (empezando por arriba ) donde
  // los 5 bits representan los pixeles activados(1) y desactivados(0)
  for (uint8_t i = 0; i < 8; i++) {
    lcd_i2c_write_command(lcd, LCD_RS_DATA, pixels[i]);
  }
  // Después de cargar, regresa el cursor a la posición inicial
  lcd_i2c_write_command(lcd, LCD_RS_CMD, RETURN_HOME);
}

void lcd_i2c_print_msg(lcd_i2c_device_t *lcd, char *msg) {
  uint8_t i = 0;

  while (msg[i] != '\0') {
    lcd_i2c_write_command(lcd, LCD_RS_DATA, msg[i++]);
  }
}

#define SMILE 5 // Define el valor binario de dónde se encuentra el custom char
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
  // Valores de los pixeles, yendo de arriba hacia abajo de los custom chars
  uint8_t smile[8] = {0x00, 0x0A, 0x0A, 0x00, 0x11, 0x0E, 0x00, 0x00};
  uint8_t drop[8] = {0x04, 0x04, 0x0E, 0x0E, 0x1F, 0x1F, 0x1F, 0x0E};

  lcd_i2c_write_custom_char(&my_lcd, SMILE, smile);
  lcd_i2c_write_custom_char(&my_lcd, DROP, drop);

  bool print_msg = true;
  uint8_t conteo_msg = 0;

  char message[] = {'H', 'o', 'l', 'a',  ' ',   'M', 'u',
                    'n', 'd', 'o', DROP, SMILE, 0};
  char spaces_char[] = "            ";

  while (1) {
    // Inicia el cursor en la 2,0
    lcd_set_cursor(&my_lcd, 2, conteo_msg);
    // Imprime el mensaje si está permitido
    if (print_msg) {
      lcd_i2c_print_msg(&my_lcd, message);
    } else {
      lcd_i2c_print_msg(&my_lcd, spaces_char);
    }
    // Aumenta el conteo de la linea (renglón)
    conteo_msg++;
    // Si llegó al último renglón, imprime espacios (borra los mensajes)
    // desde el renglón 0, y repite
    if (conteo_msg > 3) {
      conteo_msg = 0;
      print_msg = !print_msg;
    }

    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}
