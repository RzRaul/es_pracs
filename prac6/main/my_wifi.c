#include "my_wifi.h"

const char *TAG_AP = "WiFi SoftAP";
const char *TAG_STA = "WiFi Sta";
int s_retry_num = 5;
char ssid[WiFi_MAX_CHARS] = {0};
char password[WiFi_MAX_CHARS] = {0};
char deviceName[WiFi_MAX_CHARS] = {0};
EventGroupHandle_t s_wifi_event_group = NULL;

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG_STA, "Station started");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* Initialize soft AP */
esp_netif_t *wifi_init_softap(void){
    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = AP_DEFAULT_SSID,
            .ssid_len = strlen(AP_DEFAULT_SSID),
            .password = AP_DEFAULT_PASS,
            .channel = DEFAULT_WIFI_CHANNEL,
            .max_connection = DEFAULT_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    if (strlen(AP_DEFAULT_PASS) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             AP_DEFAULT_SSID, AP_DEFAULT_PASS, DEFAULT_WIFI_CHANNEL);

    return esp_netif_ap;
}

/* Initialize wifi station */
esp_netif_t *wifi_init_sta(void){
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_sta_config = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = DEFAULT_WIFI_RETRY,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
            * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_LOGI(TAG_STA, "Setting SSID: %s, Password: %s", ssid, password);
    strncpy((char*)wifi_sta_config.sta.ssid, ssid, sizeof(ssid));
    strncpy((char*)wifi_sta_config.sta.password, password, sizeof(password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_STA, "wifi_init_sta finished.");

    return esp_netif_sta;
}

uint8_t replace_char(char* str, char find, char replace){
    char *current_pos = strchr(str, find);
    if (current_pos) {
        *current_pos = replace;
        return 1;
    }
    return 0;
}

esp_err_t set_nvs_creds_and_name(char* new_ssid, char* new_pass, char* deviceName){
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_AP, "Error (%s) opening NVS handle", esp_err_to_name(err));
        return err;
    } else {
        err = nvs_set_str(nvs_handle, "ssid", new_ssid);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_AP, "Error (%s) setting ssid in NVS", esp_err_to_name(err));
            return err;
        }
        err = nvs_set_str(nvs_handle, "password", new_pass);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_AP, "Error (%s) setting password in NVS", esp_err_to_name(err));
            return err;
        }
        err = nvs_set_str(nvs_handle, "deviceName", deviceName);
    
        if (err != ESP_OK) {
            ESP_LOGE(TAG_AP, "Error (%s) setting password in NVS", esp_err_to_name(err));
            return err;
        }
        nvs_close(nvs_handle);
    }
    return ESP_OK;
}

//Check for credentials in NVS and if not found, set default values
uint8_t check_credentials(){
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_AP, "Error (%s) opening NVS handle", esp_err_to_name(err));
        return 0;
    } else {
        size_t ssid_len = sizeof(ssid);
        size_t password_len = sizeof(password);
        size_t deviceName_len = sizeof(deviceName);
        err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
        if (err != ESP_OK) {
            ESP_LOGI(TAG_AP, "SSID not found initializing as AP");
           return 0;
        }
        err = nvs_get_str(nvs_handle, "password", password, &password_len);
        if (err != ESP_OK) {
            ESP_LOGI(TAG_AP, "Password not found, setting default");
           return 0;
        }
        err = nvs_get_str(nvs_handle, "deviceName", deviceName, &deviceName_len);
        if (err != ESP_OK) {
            ESP_LOGI(TAG_AP, "Device name not found, setting default");
           return 0;
        }
        nvs_close(nvs_handle);
        return 1;
    }
}

void init_creds_strings(){
    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));
    memset(deviceName, 0, sizeof(deviceName));
}