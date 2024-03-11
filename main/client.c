
#include "client.h"
#include "led_control_sim.h"

// get http request event handler
esp_err_t client_handler(esp_http_client_event_handle_t event)
{
    switch (event->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", event->data_len, (char *)event->data);
        change_to_next_color(led_state);
        break;

    default:
        break;
    }
    return ESP_OK;
}

void get_heatpump_set_state()
{
    esp_http_client_config_t config_get = {
        .url = "http://192.168.0.25:6000/device/1000",
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .event_handler = client_handler};

    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

/*-------------------------------------------------------------------------------
------------------------------- Request Templates -------------------------------
-------------------------------------------------------------------------------*/

void patch_template()
{
    esp_http_client_config_t config_put = {
        .url = "URL_TO_PATCH",
        .method = HTTP_METHOD_PATCH,
        .cert_pem = NULL,
        .event_handler = client_handler, // change if it needs a different event handler
    };

    esp_http_client_handle_t client = esp_http_client_init(&config_put);

    const char *put_data = "updated_value";
    esp_http_client_set_post_field(client, put_data, strlen(put_data));

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void put_template()
{
    esp_http_client_config_t config_put = {
        .url = "URL_TO_PUT",
        .method = HTTP_METHOD_PATCH,
        .cert_pem = NULL,
        .event_handler = client_handler, // change if it needs a different event handler
    };

    esp_http_client_handle_t client = esp_http_client_init(&config_put);

    const char *put_data = "updated_value";
    esp_http_client_set_post_field(client, put_data, strlen(put_data));

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void delete_example()
{
    esp_http_client_config_t config_delete = {
        .url = "URL_TO_DELETE",
        .method = HTTP_METHOD_DELETE,
        .cert_pem = NULL,
        .event_handler = client_handler, // change if it needs a different event handler
    };

    esp_http_client_handle_t client = esp_http_client_init(&config_delete);

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}
