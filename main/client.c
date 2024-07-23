#define INTERNAL_CLIENT
#include "client.h"

// get http request event handler
esp_err_t client_handler(esp_http_client_event_handle_t event)
{
    switch (event->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI("CLIENT HANDLER - ", "HTTP_EVENT_ON_DATA: %.*s\n", event->data_len, (char *)event->data);
        if (process_heat_pump_energy_state_response(truncate_event_data((char *)event->data)))
            send_control_signal(get_current_energy_consumption_state()->state);
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE("CLIENT HANDLER - ", "HTTP_EVENT_ERROR: %.*s\n", event->data_len, (char *)event->data);
        break;
    default:
        break;
    }
    // set_led_state(GREEN);
    return ESP_OK;
}

// TODO: find a way to pass the customer object to compose the request and the api key
void get_heatpump_set_state(const char *service_url, const char *api_header, const char *api_key) // const BodilCustomer *customer)
{
    esp_http_client_config_t config_get = {
        .url = service_url,
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .event_handler = client_handler};

    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_http_client_set_header(client, api_header, api_key);
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGE("HTTP CLIENT PERFORM", "HTTP request failed: %d\n", err);
        // set_led_state(RED);
    }
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
