/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#include "softap.h"

extern const uint8_t web_server_html_start[]    asm("_binary_index_pink_html_start");
extern const uint8_t web_server_html_end[]      asm("_binary_index_pink_html_end");
extern esp_err_t web_server_display_html(httpd_req_t *req);
extern esp_err_t web_server_handle(httpd_req_t *req);

#define WEB_SERVER_BUFF_LEN     (512)

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "example";

static httpd_handle_t server = NULL;
static const httpd_uri_t httpd_url_handle[] = {
    {"/", HTTP_GET, web_server_display_html, NULL},
    {"/setinfo", HTTP_POST, web_server_handle, NULL},
};

static uint8_t *web_server_buf = NULL;

esp_err_t web_server_handle(httpd_req_t *req)
{
    int body_len = req->content_len;
    int total_received_len = 0;
    int single_received_len = 0;

    while (total_received_len < body_len) {
        single_received_len = httpd_req_recv(req, (char *)web_server_buf, WEB_SERVER_BUFF_LEN);
        if (single_received_len <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_500(req);
            ESP_LOGE(TAG, "Failed to post control value");
            return ESP_FAIL;
        }
        total_received_len += single_received_len;
    }
    web_server_buf[total_received_len] = '\0'; // now ,the post is str format, like ssid=yuxin&pwd=TestPWD&chl=1&ecn=0&maxconn=1&ssidhidden=0
    ESP_LOGI(TAG, "Post data is : %s\n", web_server_buf);

    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    if (web_server_buf == NULL) {
        web_server_buf = (uint8_t *)calloc(1, WEB_SERVER_BUFF_LEN);
    }

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");

        for (int i = 0; i < sizeof(httpd_url_handle) / sizeof(httpd_url_handle[0]); i++) {
            httpd_register_uri_handler(server, &httpd_url_handle[i]);
        }

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    const size_t html_size = (web_server_html_end - web_server_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, (const char*) web_server_html_start, html_size);
    return httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t web_server_display_html(httpd_req_t *req)
{
    index_html_get_handler(req);

    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_softap();

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, &server));

    /* Start the server for the first time */
    server = start_webserver();
}
