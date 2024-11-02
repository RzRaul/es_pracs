#include <stdio.h>
#include "my_wifi.h"
#include "web_server.h"

static const char *TAG = "main";

void app_main(void)
{
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize event group */
    s_wifi_event_group = xEventGroupCreate();

    /* Register Event handler */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &wifi_event_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler,
                    NULL,
                    NULL));

    /*Initialize WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_init_softap();
        ESP_LOGI(TAG, "Setup as AP initializing server...");
        server = start_webserver(); 
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    if (server) {
        httpd_stop(server);
    }
}
