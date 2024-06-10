#define MQTT_PROTOCOL
#include "mqtt_service.h"

// TODO: REMOVE HARDCODED DEVICE ID AND USE THE ONE FROM CUSTOMER INFO HANDLING THE STRING FORMATION

/* TODO: feature! - INVESTIGATE MQTT 5!!!
    _____________________________________________________
    ---- study this implementation and create broken ----
    ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾ */
/*
QoS types:
    Send:
    0 -> (fire and forget) -> HealthCheck
    1 -> (message reach the receiver at least once - can send duplicated data [server side should handle it]) -> Connect / Disconnect
    2 -> (exactly once -> too much overhead)
    Receive:

*/
extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t broker_cert_pem_start[] asm("_binary_ca_crt_start");
extern const uint8_t broker_cert_pem_end[] asm("_binary_ca_crt_end");

static const char *MQTT_TAG = "MQTT_SERVICE";

// TODO: refactor this error function
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(MQTT_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGI(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int message_status;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("MQTT HANDLER", "MQTT_EVENT_CONNECTED");
        // This one should send the device ID to this topic and the server will receive and create a MQTT_TAG in the list of monitored devices.
        message_status = esp_mqtt_client_publish(client, "bodil/connect", "{\"deviceID\": \"1000\"}", 0, 1, 0);
        ESP_LOGI("MQTT HANDLER", "sent publish successful, message_status=%d", message_status);

        // Subscribe to the all topic.
        message_status = esp_mqtt_client_subscribe(client, "bodil/all", 0);
        ESP_LOGI("MQTT HANDLER", "sent subscribe successful, message_status=%d", message_status);

        // Subscribe to its own topic to receive messages from the server individually.
        message_status = esp_mqtt_client_subscribe(client, "bodil/device/1000", 1);
        ESP_LOGI("MQTT HANDLER", "sent subscribe successful, message_status=%d", message_status);

        // Example of unsubscribe - it should trigger a function in the server to remove this device from the list
        // message_status = esp_mqtt_client_unsubscribe(client, "bodil/disconnect");
        // ESP_LOGI("MQTT HANDLER", "sent unsubscribe successful, message_status=%d", message_status);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI("MQTT HANDLER", "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI("MQTT HANDLER", "MQTT_EVENT_SUBSCRIBED, message_status=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, message_status=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, message_status=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        // TODO: check the format of the data? Probably there is a handler already during the parsing process [inspect it]
        // Dispatch a new task to change the machine state based on the message status
        if (process_heat_pump_energy_state_response(truncate_event_data(event->data))){
            send_control_signal(get_current_energy_consumptionState()->state);
        }
        // Send back to the server a confirmation server
        message_status = esp_mqtt_client_publish(client, "bodil/device/1000/confirmation", "{\"device\": \"1000\" {\"status\": \"command received\"}", 0, 1, 0);
        ESP_LOGI(MQTT_TAG, "sent publish successful, message_status=%d", message_status);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGE(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGW(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

// TODO: call the mqtt_service_start in the main thread
static void mqtt_client(const char *broker_url, const char * broker_user, const char *broker_pass)
{

    esp_err_t err;

    /* TODO:
        review the .broker.verification.skip_cert_common_name_check (probably set to false) for production
        it can introduce vulnerabilities such as man in the middle MITM attacks
    */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_url,

        // TODO: MAKE IT WORKS CERTIFICATES (TRY TO AUTOGENERATE WITH THE CLIENT KEY AND CERTIFICATE INSIDE ESP???)

        .broker.verification.certificate = (const char *)broker_cert_pem_start,
        .broker.verification.skip_cert_common_name_check = true, // TODO: careful about this declaration <- set to false
        .credentials = {
            .username = broker_user,
            .client_id = broker_user, //TODO: testing the id passing the username
            .authentication = {
                .password = broker_pass,
                .certificate = (const char *)client_cert_pem_start,
                .key = (const char *)client_key_pem_start,
            },
        }
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client)
    {
        ESP_LOGE(MQTT_TAG, "Failed to initialize client");
        return;
    }
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(MQTT_TAG, "Failed to register the event handlers to the initialized client");
        return;
    }
    esp_mqtt_client_start(client);
    if (err != ESP_OK)
    {
        ESP_LOGE(MQTT_TAG, "Failed to start the mqtt service!");
        return;
    }
}

void mqtt_service_start(const char *broker_url, const char * broker_user, const char *broker_pass)
{
    mqtt_client(broker_url, broker_user, broker_pass);
    // TODO: print here the certificates for inspection
    return;
}
