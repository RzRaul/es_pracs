#ifndef MY_WIFI_H
#define MY_WIFI_H

#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include <esp_http_server.h>
#include "esp_eth.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define DEFAULT_WIFI_CHANNEL 1
#define DEFAULT_MAX_STA_CONN 4
#define AP_DEFAULT_SSID "Battleship_DEMO"
#define AP_DEFAULT_PASS "12345678"
#define DEFAULT_WIFI_RETRY 5
#define WiFi_MAX_CHARS 32


extern EventGroupHandle_t s_wifi_event_group;
extern char ssid[WiFi_MAX_CHARS];
extern char password[WiFi_MAX_CHARS];
extern char deviceName[WiFi_MAX_CHARS];
extern int s_retry_num;

esp_netif_t * wifi_init_sta(void);
esp_netif_t * wifi_init_softap(void);
uint8_t replace_char(char* str, char find, char replace);
uint8_t check_credentials();
void wifi_event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data);
esp_err_t set_nvs_creds_and_name(char* new_ssid, char* new_pass, char* deviceName);
void init_creds_strings();


#endif