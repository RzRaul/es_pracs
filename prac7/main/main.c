
/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "driver/gpio.h"
#include "driver/touch_pad.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_spp_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sys/time.h"
#include "time.h"

#define LED_PIN 2
#define BUFF_RECV_LEN 256

EventGroupHandle_t event_group;
const int bt_data_ready = BIT0;

#define TOUCH_PAD_CHANNEL TOUCH_PAD_NUM4
#define TOUCH_THRESHOLD 0x200 /* promedio obtenido de los experimentos */

enum {
    LED_ON = 97, // a
    LED_OFF,
    LED_STATE,
    CAP_SENSOR_STATE,
};

typedef struct {
    uint16_t filtered_value;
    uint16_t raw_value;
    uint8_t touch;
} TOUCH_SENSOR_READ_t;

uint32_t bt_connection_handle;
bool led_state = 0;

#define SPP_TAG "SPP_ACCEPTOR_DEMO"
#define SPP_SERVER_NAME "SPP_SERVER"
#define EXAMPLE_DEVICE_NAME "ESP_PALINDROMES"

static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const bool esp_spp_enable_l2cap_ertm = true;

static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

char data_recv[BUFF_RECV_LEN];

void split_string(char *str, int *len, int *left, int *right) {
    char *args[] = {0, 0};
    char *aux = str;
    int j = 0;
    while (*str != '\n' && *str != '\r' && *str != '\0') {
        if (*str == ',') {
            args[j++] = str + 1;
            *str = '\0';
        }
        str++;
    }
    *left = atoi(args[0]);
    *right = atoi(args[1]);
    *len = *right - *left;
}

int compare_func(const void *_a, const void *_b) {
    char *a, *b;
    a = (char *)_a;
    b = (char *)_b;
    return (*a - *b);
}
void copy_backwards(char *str_rev, char *str_og, int len) {
    for (int i = 0; i < len; i++) {
        str_rev[i] = str_og[len - i - 1];
    }
    str_rev[len] = '\n';
}
void print_backwards(char *str, int len) {
    for (int i = len - 1; i >= 0; i--)
        printf("%c", str[i]);
}
void print_straight(char *str, int len) {
    for (int i = 0; i < len; i++) {
        printf("%c", str[i]);
    }
}
void print_permutes(char *str, char suffix, int index, int len) {
    char aux;
    if (index == len - 1 && (suffix || len > 1)) {
        print_straight(str, len);
        printf("%c", suffix);
        print_backwards(str, len);
        printf("\n");

        char buff[128] = {0};
        char *aux_ptr = buff;
        memcpy(buff, str, len);
        aux_ptr += len;
        if (suffix)
            *(aux_ptr++) = suffix;
        copy_backwards(aux_ptr, str, len);
        aux_ptr += len;
        *(aux_ptr++) = '\n';

        esp_spp_write(bt_connection_handle,
                      suffix == 0 ? len * 2 + 1 : len * 2 + 2, (uint8_t *)buff);
        vTaskDelay(10);
        return;
    }

    for (int i = index; i < len; i++) {

        // Swapping
        aux = str[index];
        str[index] = str[i];
        str[i] = aux;

        print_permutes(str, suffix, index + 1, len);

        // Backtrack
        aux = str[index];
        str[index] = str[i];
        str[i] = aux;
    }
}

static void generate_palindromes(char *sub_str, int len) {
    printf("Los palíndromos de \"%s\" con %d chars son:\n", sub_str, len);
    qsort(sub_str, len, sizeof(char), &compare_func);
    sub_str[len] = 0;
    char repeated[64] = {0};
    int reps_len = 0;
    char uniques[64] = {0};
    int uniques_len = 1;
    char *rep_aux = repeated;
    char *unique_aux = uniques + 1;
    for (int i = 0; i < len; i++) {
        if (sub_str[i] == sub_str[i + 1]) {
            *(rep_aux++) = sub_str[i++];
            reps_len++;
        } else {
            *(unique_aux++) = sub_str[i];
            uniques_len++;
        }
    }
    if (!reps_len || !uniques_len)
        return;
    for (int i = 0; i < uniques_len; i++) {
        for (int j = 1; j <= reps_len; j++) {
            for (int k = 0; k < reps_len; k++) {
                print_permutes(repeated + k, uniques[i], 0, j - k);
            }
        }
    }
}
static void init_led(void) {
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    ESP_LOGI(SPP_TAG, "Init led completed");
}

static void init_touch_sensor(void) {
    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5,
                          TOUCH_HVOLT_ATTEN_1V);
    touch_pad_config(TOUCH_PAD_CHANNEL, -1);
    touch_pad_filter_start(10);
}

static void read_touch_sensor(TOUCH_SENSOR_READ_t *read) {
    touch_pad_read_raw_data(TOUCH_PAD_CHANNEL, &read->raw_value);
    touch_pad_read_filtered(TOUCH_PAD_CHANNEL, &read->filtered_value);

    read->touch = (read->filtered_value < TOUCH_THRESHOLD) ? 1 : 0;
}

#define TOUCH_SENSOR_BUF_LEN 18
#define TOUCH_SENSOR_RESP_LEN 16
void process_bt_data(void *params) {

    while (true) {
        xEventGroupWaitBits(event_group, bt_data_ready, true, true,
                            portMAX_DELAY);

        // Process the palindromes
        int left, right, len;
        char substr[100] = {0};
        char buff[128] = {0};
        split_string(data_recv, &len, &left, &right);
        // sscanf(data_recv, "%s,%d,%d", substr, &left, &right);
        len = right - left + 1;
        generate_palindromes(data_recv + left, len);
    }
}

static char *bda2str(uint8_t *bda, char *str, size_t size) {
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3], p[4],
            p[5]);
    return str;
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    char bda_str[18] = {0};

    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
            esp_spp_start_srv(sec_mask, role_slave, 0, SPP_SERVER_NAME);
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_INIT_EVT status:%d", param->init.status);
        }
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG,
                 "ESP_SPP_CLOSE_EVT status:%d handle:%" PRIu32
                 " close_by_remote:%d",
                 param->close.status, param->close.handle, param->close.async);
        break;
    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG,
                     "ESP_SPP_START_EVT handle:%" PRIu32 " sec_id:%d scn:%d",
                     param->start.handle, param->start.sec_id,
                     param->start.scn);
            esp_bt_dev_set_device_name(EXAMPLE_DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
                                     ESP_BT_GENERAL_DISCOVERABLE);
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_START_EVT status:%d",
                     param->start.status);
        }
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
    case ESP_SPP_DATA_IND_EVT:
        uint16_t data_len = param->data_ind.len;
        if (data_len > BUFF_RECV_LEN) {
            data_len = BUFF_RECV_LEN;
        }
        memcpy(data_recv, param->data_ind.data, data_len);
        bt_connection_handle = param->data_ind.handle;
        xEventGroupSetBits(event_group, bt_data_ready);
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG,
                 "ESP_SPP_SRV_OPEN_EVT status:%d handle:%" PRIu32
                 ", rem_bda:[%s]",
                 param->srv_open.status, param->srv_open.handle,
                 bda2str(param->srv_open.rem_bda, bda_str, sizeof(bda_str)));
        break;
    case ESP_SPP_SRV_STOP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_STOP_EVT");
        break;
    case ESP_SPP_UNINIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_UNINIT_EVT");
        break;
    default:
        break;
    }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    char bda_str[18] = {0};

    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(SPP_TAG, "authentication success: %s bda:[%s]",
                     param->auth_cmpl.device_name,
                     bda2str(param->auth_cmpl.bda, bda_str, sizeof(bda_str)));
        } else {
            ESP_LOGE(SPP_TAG, "authentication failed, status:%d",
                     param->auth_cmpl.stat);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT: {
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d",
                 param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit) {
            ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        } else {
            ESP_LOGI(SPP_TAG, "Input pin code: 1234");
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1';
            pin_code[1] = '2';
            pin_code[2] = '3';
            pin_code[3] = '4';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    }

    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d bda:[%s]",
                 param->mode_chg.mode,
                 bda2str(param->mode_chg.bda, bda_str, sizeof(bda_str)));
        break;

    default: {
        ESP_LOGI(SPP_TAG, "event: %d", event);
        break;
    }
    }
    return;
}

void app_main(void) {
    char bda_str[18] = {0};

    init_led();
    init_touch_sensor();
    event_group = xEventGroupCreate();
    xTaskCreate(process_bt_data, "process_bt_data", 4096, NULL, 10, NULL);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s\n", __func__,
                 esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable controller failed: %s\n", __func__,
                 esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s\n", __func__,
                 esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s\n", __func__,
                 esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s gap register failed: %s\n", __func__,
                 esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp register failed: %s\n", __func__,
                 esp_err_to_name(ret));
        return;
    }

    esp_spp_cfg_t bt_spp_cfg = {
        .mode = esp_spp_mode,
        .enable_l2cap_ertm = esp_spp_enable_l2cap_ertm,
        .tx_buffer_size = 0, /* Only used for ESP_SPP_MODE_VFS mode */
    };
    if ((ret = esp_spp_enhanced_init(&bt_spp_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp init failed: %s\n", __func__,
                 esp_err_to_name(ret));
        return;
    }

    /*
     * Set default parameters for Legacy Pairing
     * Use variable pin, input pin code when pairing
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    ESP_LOGI(
        SPP_TAG, "Own address:[%s]",
        bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));
}
