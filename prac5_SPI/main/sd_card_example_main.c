/* SD card and FAT filesystem example.
   This example uses SPI peripheral to communicate with SD card.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_vfs_fat.h"
#include "myUart.h"
#include "sdmmc_cmd.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#define MAX_CHAR_SIZE 64

#define EVENT_INPUT_BIT 0
#define EVENT_OUTPUT_BIT 1

static const char *TAG = "sd_card";

#define MOUNT_POINT "/sdcard"

#define PIN_NUM_MISO GPIO_NUM_19
#define PIN_NUM_MOSI GPIO_NUM_23
#define PIN_NUM_CLK GPIO_NUM_18
#define PIN_NUM_CS GPIO_NUM_5

#define is_digit(c) ((c) >= '0' && (c) <= '9')
#define is_alpha(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
char *decompress_text_recursive(uint8_t, char *, uint8_t);

#define CHAR_INIT_GROUP '['
#define CHAR_END_GROUP ']'

char *test[5] = {"ABCDE", "AB[12C]DE", "AB[3CD]", "AB[2C[2EF]G]",
                 "IF[2A]LG[5M]D"};

char filename[MAX_CHAR_SIZE] = {0};
const char mount_point[] = MOUNT_POINT;
sdmmc_card_t *card;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();

typedef enum {
  STATE_IDLE,
  STATE_IN_NUMBER,
  STATE_IN_TEXT,
} repeater_state_t;

typedef struct {
  uint16_t repetitions;
  char *text_to_repeat;
  uint8_t text_to_repeat_size;
  repeater_state_t state;
} repeater_handle_t;

repeater_handle_t decompresser = {.repetitions = 0,
                                  .text_to_repeat = NULL,
                                  .text_to_repeat_size = 0,
                                  .state = STATE_IDLE};

static esp_err_t s_example_write_file(const char *path, char *data) {
  ESP_LOGI(TAG, "Abriendo archivo %s", path);
  FILE *f = fopen(path, "w");
  if (f == NULL) {
    ESP_LOGE(TAG, "Fallo abrir el archivo para escritura");
    return ESP_FAIL;
  }
  fprintf(f, data);
  fclose(f);
  ESP_LOGI(TAG, "Archivo escrito");

  return ESP_OK;
}

static esp_err_t sd_read_file(const char *path) {
  ESP_LOGI(TAG, "Abriendo archivo %s", path);
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    ESP_LOGE(TAG, "Fallo abrir el archivo para lectura");
    return ESP_FAIL;
  }
  char line[MAX_CHAR_SIZE];
  fgets(line, sizeof(line), f);
  fclose(f);

  // strip newline
  char *pos = strchr(line, '\n');
  if (pos) {
    *pos = '\0';
  }
  ESP_LOGI(TAG, "Lectura de archivo: '%s'", line);

  return ESP_OK;
}

#define CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED

void app_main(void) {

  init_uart();
  ESP_LOGI("TEST", "Iniciando tarea de lectura de consola");
  for (int i = 0; i < 5; i++) {
    decompress_text_recursive(CONSOLE_UART, test[i], 1);
    uart_puts(CONSOLE_UART, "\n");
  }
  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  // init_spi_sd();
  // Options for mounting the filesystem.
  // If format_if_mount_failed is set to true, SD card will be partitioned and
  // formatted in case when mounting fails.
  // The partition will be FAT16 with 1 sector size
}

void task_reading_console(void *pvParameters) {
  char buffer[MAX_CHAR_SIZE];

  while (1) {
    uart_puts(CONSOLE_UART, "Ingrese el nombre del archivo a leer: ");
    uart_reads(CONSOLE_UART, buffer, MAX_CHAR_SIZE, true);
    if (strncmp(buffer, "exit", 4) == 0) {
      break;
    }
  }
  vTaskDelete(NULL);
}

esp_err_t init_spi_sd() {
  esp_err_t ret;
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = true,
      .max_files = 5,
      .allocation_unit_size = 8 * 1024};

  ESP_LOGI(TAG, "Inicializando SD card");

  // Use settings defined above to initialize SD card and mount FAT filesystem.
  // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
  // Please check its source code and implement error recovery when developing
  // production applications.
  ESP_LOGI(TAG, "Usando periférico SPI");

  // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
  // For setting a specific frequency, use host.max_freq_khz (range 400kHz -
  // 20MHz for SDSPI) Example: for fixed frequency of 10MHz, use
  // host.max_freq_khz = 10000;
  host.max_freq_khz = 5000;

  spi_bus_config_t bus_cfg = {
      .mosi_io_num = PIN_NUM_MOSI,
      .miso_io_num = PIN_NUM_MISO,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
  };
  ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Fallo inicializacion del bus.");
    return ret;
  }

  // This initializes the slot without card detect (CD) and write protect (WP)
  // signals. Modify slot_config.gpio_cd and slot_config.gpio_wp if your board
  // has these signals.
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = PIN_NUM_CS;
  slot_config.host_id = host.slot;

  ESP_LOGI(TAG, "Montando el filesystem");
  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config,
                                &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Fallo montar el filesystem. "
                    "Si desea formatear la tarjeta, configure la opcion "
                    "CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED.");
    } else {
      ESP_LOGE(TAG,
               "Fallo la inicializacion de la tarjeta (%s). "
               "Asegurese de que las lineas de la tarjeta SD tengan "
               "resistencias pull-up en su lugar.",
               esp_err_to_name(ret));
    }
    return ret;
  }
  ESP_LOGI(TAG, "Filesystem montado");

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, card);
  return ret;
}

// esp_err_t create_file(){
//     // First create a file.
//     esp_err_t ret;
//     const char *file_hello = MOUNT_POINT"/hello.txt";
//     char data[MAX_CHAR_SIZE];
//     snprintf(data, MAX_CHAR_SIZE, "%s %s!\n", "Hola", card->cid.name);
//     ret = s_example_write_file(file_hello, data);
//     if (ret != ESP_OK) {
//         return;
//     }

//     const char *file_foo = MOUNT_POINT"/foo.txt";

//     // Check if destination file exists before renaming
//     struct stat st;
//     if (stat(file_foo, &st) == 0) {
//         // Delete it if it exists
//         unlink(file_foo);
//     }

//     // Rename original file
//     ESP_LOGI(TAG, "Renombrando archivo %s a %s", file_hello, file_foo);
//     if (rename(file_hello, file_foo) != 0) {
//         ESP_LOGE(TAG, "Fallo renombrar archivo");
//         return ret;
//     }

//     ret = sd_read_file(file_foo);
//     if (ret != ESP_OK) {
//         return ret;
//     }

//     // All done, unmount partition and disable SPI peripheral
//     esp_vfs_fat_sdcard_unmount(mount_point, card);
//     ESP_LOGI(TAG, "Tarjeta desmontada");

//     //deinitialize the bus after all devices are removed
//     spi_bus_free(host.slot);
// }

char *decompress_text_recursive(uint8_t uartNum, char *text, uint8_t reps) {
  char *p = text;
  uint16_t next_reps = 0;
  repeater_state_t state = STATE_IDLE;
  for (int i = 0; i < reps; i++) {
    p = text;
    while (*p != EOF && *p != '\0' && *p != ']') {
      if (*p == CHAR_INIT_GROUP) {
        state = STATE_IN_NUMBER;
      } else if (is_alpha(*p)) {
        if (state == STATE_IDLE) {
          uart_putchar(uartNum, *p);
        } else if (state == STATE_IN_NUMBER) {
          p = decompress_text_recursive(uartNum, p, next_reps);
          next_reps = 0;
          state = STATE_IDLE;
        }
      } else if (is_digit(*p)) {
        if (state == STATE_IN_NUMBER) {
          next_reps = next_reps * 10 + (*p - '0');
        }
      }
      p++;
    }
  }
  return p;
}
