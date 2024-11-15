#include "web_server.h"
#include "my_wifi.h"
#include <stdlib.h>
#include "esp_timer.h"
#include "driver/gpio.h"


#define SHIPS 20
#define MAX_TRIES 30
#define BOARD_SIZE 10
#define CHAR_MISSED '_'
#define CHAR_HIT 'X'
#define CHAR_REMAIN 'O'
#define BTN_END GPIO_NUM_19
#define RST_BTN "<a href=\"/battleship\"><button style=\"background-color: #4CAF50; color: white; border: none; padding: 10px 20px; font-size: 16px; border-radius: 8px; cursor: pointer; transition: background-color 0.3s ease;\">Reset Game</button></a>"

const char *TAG_WEBS = "Web Server";

extern unsigned char top_view_start[] asm("_binary_top_view_html_start");
extern unsigned char top_view_end[] asm("_binary_top_view_html_end");
extern unsigned char bot_view_start[] asm("_binary_bot_view_html_start");
extern unsigned char bot_view_end[] asm("_binary_bot_view_html_end");

static const httpd_uri_t configSite = {
    .uri       = "/battleship",
    .method    = HTTP_GET,
    .handler   = config_get_handler,
};

enum{
    SAFE,
    HAS_SHIP,
    HIT_SHIP,
    MISSED
};

typedef enum{
    PLAYING,
    WINNER,
    LOSER,
    ENDED
}game_state_t;

typedef struct{
    uint8_t x;
    uint8_t y;
}coord_t;

typedef struct{
    uint8_t board[BOARD_SIZE][BOARD_SIZE];
    uint8_t total_ships;
    uint8_t turn_number;
    uint8_t sank_ships;
    game_state_t gameState;
}game_controller_t;


game_controller_t game_controller= {
    .total_ships = SHIPS,
    .turn_number = 10,
    .sank_ships = 0,
    .gameState = PLAYING,
};

static volatile uint32_t last_isr_time = 0;
#define DEBOUNCE_TIME 100
enum btn_state{
    PRESSED,
    IDLE,
    RELEASED
};
int state = IDLE;

clock_t last_interrupt_time = 0;

// The interrupt handler for the button press
static void IRAM_ATTR gpio_interrupt_handler(void *arg){
    uint32_t gpio_num = (uint32_t) arg;
    if (state == IDLE && gpio_get_level(BTN_END) == 0){
        last_interrupt_time = clock();
        state = PRESSED;
        return;
    }
    if((clock() - last_interrupt_time) < DEBOUNCE_TIME){
        return;
    }
    if(gpio_get_level(gpio_num) == IDLE && state == PRESSED){
        state = IDLE;
        game_controller.gameState = ENDED;
    }
    return;
}

void check_hit(int x, int y){
    switch (game_controller.board[x][y])
    {
    case SAFE:
        //miss
        game_controller.turn_number--;
        game_controller.board[x][y] = MISSED;
        break;
    case HAS_SHIP:
        //Hit
        game_controller.sank_ships++;
        game_controller.board[x][y] = HIT_SHIP;
        break;
    case HIT_SHIP:
        //Already hit
    case MISSED:
        //Already missed
    default:
        break;
    }
    if(game_controller.gameState == PLAYING){
        if(game_controller.sank_ships == SHIPS){
            game_controller.gameState = WINNER;
        }else if(game_controller.turn_number == 0){
            game_controller.gameState = LOSER;
        }
    }
}

void init_ships(){
    
    memset(game_controller.board, 0, sizeof(game_controller.board));
    int x,y;
    for (int i = 0; i < SHIPS; i++){
        x = rand() % BOARD_SIZE;
        y = rand() % BOARD_SIZE;
        if (game_controller.board[x][y] == SAFE){
            game_controller.board[x][y] = HAS_SHIP;
        }else if(game_controller.board[x][y] == HAS_SHIP){
            i--;
        }
    }
}
void reset_game(){
    game_controller.total_ships = SHIPS;
    game_controller.turn_number = MAX_TRIES;
    game_controller.sank_ships = 0;
    game_controller.gameState = PLAYING;
    init_ships();
}
/* An HTTP GET handler */
esp_err_t config_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    size_t topLen = top_view_end - top_view_start;
    size_t botLen = bot_view_end - bot_view_start;
    char tviewHtml[topLen > botLen ? topLen : botLen];
    ESP_LOGI("HTTP", "LEN-> %d", sizeof(tviewHtml));
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

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        ESP_LOGI(TAG_WEBS, "Found URL query len: %d", buf_len);
        buf = malloc(buf_len);
        char x_aux[5] = {0};
        char y_aux[5] = {0};
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG_WEBS, "Found URL query => %s", buf);
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "x", x_aux, sizeof(x_aux)) == ESP_OK) {
                ESP_LOGI(TAG_WEBS, "Found URL query parameter => x=%s", x_aux);
            }
            if (httpd_query_key_value(buf, "y", y_aux, sizeof(y_aux)) == ESP_OK) {
                ESP_LOGI(TAG_WEBS, "Found URL query parameter => y=%s", y_aux);
            }

            //Response hit, miss, invalid, end game
            ESP_LOGI("server", "User sent-> %s, %s", x_aux, y_aux);
            check_hit(atoi(x_aux), atoi(y_aux));

        }
        free(buf);
    }

    memcpy(tviewHtml, top_view_start, topLen);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, tviewHtml, topLen);
    //Dynamically created scoreboard
    char scoreboardHtml[128];  // Buffer for the scoreboard's HTML
    // Send the opening scoreboard div
    httpd_resp_send_chunk(req, "<div class=\"scoreboard\">", strlen("<div class=\"scoreboard\">"));
    switch(game_controller.gameState){
        case WINNER:
            snprintf(scoreboardHtml, sizeof(scoreboardHtml), "<h2>HAS GANADO!</h2>");
            httpd_resp_send_chunk(req, scoreboardHtml, strlen(scoreboardHtml));
        break;
        case LOSER:
            snprintf(scoreboardHtml, sizeof(scoreboardHtml), "<h2>HAS PERDIDO!</h2>");
            httpd_resp_send_chunk(req, scoreboardHtml, strlen(scoreboardHtml));
        break;
        case ENDED:
            snprintf(scoreboardHtml, sizeof(scoreboardHtml), "<h2>HAS TERMINADO EL JUEGO!</h2>");
            httpd_resp_send_chunk(req, scoreboardHtml, strlen(scoreboardHtml));
        break;
        case PLAYING:
            snprintf(scoreboardHtml, sizeof(scoreboardHtml), "<h2>Naves totales: %d</h2>", game_controller.total_ships);
            httpd_resp_send_chunk(req, scoreboardHtml, strlen(scoreboardHtml));
            // Send the ships sunk text
            snprintf(scoreboardHtml, sizeof(scoreboardHtml), "<h2>Naves hundidas: %d</h2>", game_controller.sank_ships);
            httpd_resp_send_chunk(req, scoreboardHtml, strlen(scoreboardHtml));
            // Send the number of turns text
            snprintf(scoreboardHtml, sizeof(scoreboardHtml), "<h2>Turnos restantes: %d</h2>", game_controller.turn_number);
            httpd_resp_send_chunk(req, scoreboardHtml, strlen(scoreboardHtml));
            break;
        default:
            break;
        
    }
    // Close the scoreboard div
    httpd_resp_send_chunk(req, "</div>", strlen("</div>"));

    char boardHtml[128];  // Buffer for the board's HTML
     httpd_resp_send_chunk(req, "<div class=\"board\">", strlen("<div class=\"board\">"));
    
    for (int i=0;i<10;i++){
        httpd_resp_send_chunk(req, "<div class=\"row\">", strlen("<div class=\"row\">"));
        for (int j=0;j<10;j++){
            memset(boardHtml,0,sizeof(boardHtml));
            char c= ' ';
            if(game_controller.board[i][j] == MISSED) c = CHAR_MISSED;
            else if(game_controller.board[i][j] == HIT_SHIP) c = CHAR_HIT;
            else if((game_controller.gameState == LOSER || game_controller.gameState == ENDED) && game_controller.board[i][j] == HAS_SHIP) c = CHAR_REMAIN;
            snprintf(boardHtml,sizeof(boardHtml),"<div class=\"cell\" onclick=\"handleCellClick(event, %d, %d)\">%c</div>",i,j,c );
            // ESP_LOGI("server", "Sent->%s",boardHtml);
            httpd_resp_send_chunk(req, boardHtml, strlen(boardHtml));
        }
        httpd_resp_send_chunk(req, "</div>", strlen("</div>"));
    }
    httpd_resp_send_chunk(req, "</div>", strlen("</div>"));
    switch (game_controller.gameState)
    {
    case LOSER:
    case WINNER:
    case ENDED:
        httpd_resp_send_chunk(req, RST_BTN, strlen(RST_BTN));
        reset_game();
        break;
    default:
        break;
    }

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    memcpy(tviewHtml, bot_view_start, botLen);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, tviewHtml, botLen);
    
    httpd_resp_send_chunk(req, NULL, 0);
    
    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG_WEBS, "Request headers lost");
    }
    
    return ESP_OK;
}

void init_input_btn(){
// Configure the button GPIO as input with pull-up resistor (assuming button connects to GND)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_END),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE, 
    };
    gpio_config(&io_conf);

    // Install GPIO interrupt handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_END, gpio_interrupt_handler, NULL);  // Add the ISR handler for the button
}



httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    srand(1);
    init_ships();
    init_input_btn();

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