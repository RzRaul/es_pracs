#include "web_server.h"
#include "my_wifi.h"
#include <stdlib.h>


#define SHIPS 20
#define MAX_TRIES 30
#define BOARD_SIZE 10

const char *TAG_WEBS = "Web Server";

static const httpd_uri_t configSite = {
    .uri       = "/battleship",
    .method    = HTTP_GET,
    .handler   = config_get_handler,
};

typedef struct{
    uint8_t x;
    uint8_t y;
}coord_t;

typedef struct{
    uint8_t board[BOARD_SIZE][BOARD_SIZE];
    coord_t hits[MAX_TRIES];
    coord_t ships[SHIPS];
    uint8_t total_ships;
    uint8_t turn_number;
    uint8_t sank_ships;
}game_controller_t;


game_controller_t game_controller= {
    .total_ships = SHIPS,
    .turn_number = 0,
    .sank_ships = 0,
    .board = {{0}},
    .hits = {{0}},
};


void init_ships(){
    memset(game_controller.board, 0, sizeof(game_controller.board));
    for (int i = 0; i < SHIPS; i++){
        game_controller.ships[i].x = rand() % BOARD_SIZE;
        game_controller.ships[i].y = rand() % BOARD_SIZE;
    }
}

/* An HTTP GET handler */
esp_err_t config_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    extern unsigned char view_start[] asm("_binary_view_html_start");
    extern unsigned char view_end[] asm("_binary_view_html_end");
    size_t view_len = view_end - view_start;
    char viewHtml[view_len];
    memcpy(viewHtml, view_start, view_len);
    ESP_LOGI(TAG_WEBS, "URI: %s\n", req->uri);

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG_WEBS, "Found header => Host: %s\n", buf);
        }
        free(buf);
    }
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, viewHtml, view_len);

    // buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    // if (buf_len > 1) {
    //     buf = malloc(buf_len);
    //     if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
    //         ESP_LOGI(TAG_WEBS, "Found header => Test-Header-2: %s", buf);
    //     }
    //     free(buf);
    // }


    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        ESP_LOGI(TAG_WEBS, "Found URL query len: %d", buf_len);
        buf = malloc(buf_len);
        coord_t coord_received = {0,0};
        char x_aux[3] = {0};
        char y_aux[3] = {0};
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG_WEBS, "Found URL query => %s", buf);
            char param[WiFi_MAX_CHARS];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "x", x_aux, sizeof(x_aux)) == ESP_OK) {
                ESP_LOGI(TAG_WEBS, "Found URL query parameter => x=%s", x_aux);
            }
            if (httpd_query_key_value(buf, "fpass", y_aux, sizeof(y_aux)) == ESP_OK) {
                ESP_LOGI(TAG_WEBS, "Found URL query parameter => fpass=%s", y_aux);
            }
            //Response hit, miss, invalid, end game
        }
        free(buf);
    }

    // /* Set some custom headers */
    // httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    // httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, viewHtml, view_len);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG_WEBS, "Request headers lost");
    }
    return ESP_OK;
}



httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    srand(time(NULL));
    init_ships();

    // Iniciar el servidor httpd 
    ESP_LOGI(TAG_WEBS, "Iniciando el servidor en el puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Manejadores de URI
        ESP_LOGI(TAG_WEBS, "Registrando manejadores de URI");
        httpd_register_uri_handler(server, &configSite);
        return server;
    }

    ESP_LOGI(TAG_WEBS, "Error starting server!");
    return NULL;
}