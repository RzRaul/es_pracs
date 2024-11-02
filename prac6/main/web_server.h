#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
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

esp_err_t config_get_handler(httpd_req_t *req);
httpd_handle_t start_webserver(void);
void init_ships();

#endif