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

// TODO: use this when implemented the certificates in the parition table
extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t broker_cert_pem_start[] asm("_binary_ca_crt_start");
extern const uint8_t broker_cert_pem_end[] asm("_binary_ca_crt_end");

// // Example! 
static const char *MQTT_TAG = "MQTT_SERVICE";

// /*
//  * @brief Event handler registered to receive MQTT events
//  *
//  *  This function is called by the MQTT client event loop.
//  *
//  * @param handler_args user data registered to the event.
//  * @param base Event base for the handler(always MQTT Base in this example).
//  * @param event_id The id for the received event.
//  * @param event_data The data for the event, esp_mqtt_event_handle_t.
//  */

// TODO: refactor this error function
static void log_error_if_nonzero(const char *message, int error_code){
    if (error_code != 0) {
        ESP_LOGE(MQTT_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
    ESP_LOGI(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int message_status;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("MQTT HANDLER", "MQTT_EVENT_CONNECTED");
        //This one should send the device ID to this topic and the server will receive and create a MQTT_TAG in the list of monitored devices.
        message_status = esp_mqtt_client_publish(client, "bodil/connect", "{\"deviceID\": \"2000\"}", 0, 1, 0);
        ESP_LOGI("MQTT HANDLER", "sent publish successful, message_status=%d", message_status);

        //Subscribe to the all topic.
        message_status = esp_mqtt_client_subscribe(client, "bodil/all", 0);
        ESP_LOGI("MQTT HANDLER", "sent subscribe successful, message_status=%d", message_status);

        //Subscribe to its own topic to receive messages from the server individually.
        message_status = esp_mqtt_client_subscribe(client, "bodil/device/2000", 1);
        ESP_LOGI("MQTT HANDLER", "sent subscribe successful, message_status=%d", message_status);

        // Example of unsubscribe - it should trigger a function in the server to remove this device from the list
        // message_status = esp_mqtt_client_unsubscribe(client, "bodil/disconnect");
        // ESP_LOGI("MQTT HANDLER", "sent unsubscribe successful, message_status=%d", message_status);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI("MQTT HANDLER", "MQTT_EVENT_DISCONNECTED");
        /* 
        TODO:
        trigger a task here to check if it is a failure from the netif / network connection otherwise try to reconnect. 
        If it is not and it keep disconnected send an http to remove the activated status of this device in the server.
        */
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
        // TODO: dispatch a new task to change the machine state based on the message status.
        // Send back to the server a confirmation server
        message_status = esp_mqtt_client_publish(client, "bodil/device/2000/confirmation", "command received", 0, 1, 0);
        ESP_LOGI(MQTT_TAG, "sent publish successful, message_status=%d", message_status);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGE(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGW(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
}


// TODO: call the mqtt_service_start in the main thread
static void mqtt_client(const char * broker_url){

    esp_err_t err;

/* TODO: 
    review the .broker.verification.skip_cert_common_name_check (probably set to false) for production
    it can introduce vulnerabilities such as man in the middle MITM attacks
*/
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_url,
        .broker.verification.certificate = (const char *)broker_cert_pem_start,
        .broker.verification.skip_cert_common_name_check = false,
        .credentials = {
            .authentication = {
                .certificate = (const char *)client_cert_pem_start,
                .key = (const char *)client_key_pem_start,
            },
        }
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if(!client) {
        ESP_LOGE(MQTT_TAG, "Failed to initialize client");
        return;
    }
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if(err != ESP_OK) {
        ESP_LOGE(MQTT_TAG, "Failed to register the event handlers to the initialized client");
        return;
    }
    esp_mqtt_client_start(client);
    if(err != ESP_OK) {
        ESP_LOGE(MQTT_TAG, "Failed to start the mqtt service!");
        return;
    }
}


void mqtt_service_start(const char * broker_url){
    mqtt_client(broker_url);
    //TODO: print here the certificates for inspection
    return;
}
