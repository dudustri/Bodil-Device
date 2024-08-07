#define MQTT_PROTOCOL
#include "mqtt_service.h"

/* TODO: feature! - INVESTIGATE MQTT 5!!!
    _____________________________________________________
    ---- study this implementation and create broker ----
    ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾ */
/*
QoS types:
    Send:
    0 -> (fire and forget) -> HealthCheck
    1 -> (message reach the receiver at least once - can send duplicated data [server side should handle it]) -> Connect / Disconnect
    2 -> (exactly once) -> Null (too much overhead)
    Receive:
    0 -> (fire and forget) -> Null
    1 -> (at least once) bodil/all -> bodil/all [global] and bodil/device/id (a future regional topic may be added later)
    2 -> (exactly once) -> Null (too much overhead)

TODO: since we are using QoS 1 for the most cases, add an spam filter when the first message hits the device!

*/

esp_mqtt_client_handle_t client;

extern const uint8_t client_cert_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_end[] asm("_binary_client_key_end");
extern const uint8_t broker_cert_start[] asm("_binary_ca_crt_start");
extern const uint8_t broker_cert_end[] asm("_binary_ca_crt_end");

static const char *TAG_MQTT = "MQTT Service";

static int populate_standard_topic_and_payload(void)
{
    // TODO: add error check
    snprintf(topic_unique, MAX_TOPIC_LENGTH, "bodil/device/%d", customer_info.device_id);
    snprintf(topic_confirmation, MAX_TOPIC_LENGTH, "bodil/device/%d/confirmation", customer_info.device_id);
    snprintf(topic_healthcheck, MAX_TOPIC_LENGTH, "bodil/device/%d/healthcheck", customer_info.device_id);
    snprintf(payload_confirmation, MAX_PAYLOAD_LENGTH, "{\"device\": \"%d\" {\"status\": \"command received\"}", customer_info.device_id);
    snprintf(payload_registration, MAX_PAYLOAD_LENGTH, "{\"deviceID\": \"%d\"}", customer_info.device_id);
    return 0;
}

// TODO: refactor this error function
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG_MQTT, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG_MQTT, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t event_client = event->client;
    int message_status;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("MQTT HANDLER", "MQTT_EVENT_CONNECTED");
        // This one should send the device ID to this topic and the server will receive and create a TAG_MQTT in the list of monitored devices.
        message_status = esp_mqtt_client_publish(event_client, "bodil/connect", payload_registration, 0, 1, 0);
        ESP_LOGD("MQTT HANDLER", "sent publish successful, message_status=%d", message_status);

        // Subscribe to the all devices (global) topic
        message_status = esp_mqtt_client_subscribe(event_client, "bodil/all", 0);
        ESP_LOGD("MQTT HANDLER", "sent subscribe successful, message_status=%d", message_status);

        // Subscribe to its own topic to receive messages from the server individually.
        message_status = esp_mqtt_client_subscribe(event_client, topic_unique, 1);
        ESP_LOGD("MQTT HANDLER", "sent subscribe successful, message_status=%d", message_status);

        /*
        Example of unsubscribe - it should trigger a function in the server to remove this device from the list
            message_status = esp_mqtt_client_unsubscribe(event_client, "bodil/disconnect");
            ESP_LOGI("MQTT HANDLER", "sent unsubscribe successful, message_status=%d", message_status);
        */

        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGD("MQTT HANDLER", "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD("MQTT HANDLER", "MQTT_EVENT_SUBSCRIBED, message_status=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGD(TAG_MQTT, "MQTT_EVENT_UNSUBSCRIBED, message_status=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG_MQTT, "MQTT_EVENT_PUBLISHED, message_status=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG_MQTT, "MQTT_EVENT_DATA [TOPIC=%.*s | DATA=%.*s]", event->topic_len, event->topic, event->data_len, event->data);
        set_led_state(COMMAND_RECEIVED);
        // TODO: remove these printf statements (switch for logging)

        // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        // printf("DATA=%.*s\r\n", event->data_len, event->data);

        // Dispatch a new task to change the machine state based on the message status
        if (process_heat_pump_energy_state_response(truncate_event_data(event->data)))
        {
            send_control_signal(get_current_energy_consumption_state()->state);
            // Send back to the server a confirmation server
            message_status = esp_mqtt_client_publish(event_client, topic_confirmation, payload_confirmation, 0, 1, 0);
        }
        else
        {
            ESP_LOGW(TAG_MQTT, "Data received is not valid. No confirmation will be sent.");
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGD(TAG_MQTT, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGE(TAG_MQTT, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            if (get_current_network_module() == SIM_NETWORK_MODULE && pppos_is_connected() && !pppos_is_retrying_to_connect())
            {
                ESP_LOGW(TAG_MQTT, "Triggering SIM network failure event!");
                int32_t event_id = 0;
                if (esp_event_post("SIM_NETWORK_FAILURE_EVENT_BASE", event_id, NULL, 0, portMAX_DELAY) != ESP_OK)
                {
                    ESP_LOGE(TAG_MQTT, "Failed to dispatch the network failure event...");
                };
            }
        }
        break;
    default:
        ESP_LOGW(TAG_MQTT, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_client(const char *broker_url, const char *broker_user, const char *broker_pass)
{
    esp_err_t err;

    /* TODO:
        review the .broker.verification.skip_cert_common_name_check (probably set to false) for production
        it can introduce vulnerabilities such as man in the middle MITM attacks
    */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_url,
        .broker.verification.certificate = (const char *)broker_cert_start,
        .broker.verification.certificate_len = broker_cert_end - broker_cert_start,
        .broker.verification.skip_cert_common_name_check = true, // TODO: careful about this declaration <- set to false
        .credentials = {
            .username = broker_user,
            .client_id = broker_user, // TODO: testing the id passing the username
            .authentication = {
                .password = broker_pass,
                .certificate = (const char *)client_cert_start,
                .certificate_len = client_cert_end - client_cert_start,
                .key = (const char *)client_key_start,
                .key_len = client_key_end - client_key_start,
            },
        }};

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client)
    {
        ESP_LOGE(TAG_MQTT, "Failed to initialize client");
        return;
    }
    err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_MQTT, "Failed to register the event handlers to the initialized client");
        return;
    }
    // TODO: start the client only if there is connection, otherwise skip and start later
    esp_mqtt_client_start(client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_MQTT, "Failed to start the mqtt service!");
        return;
    }
}

esp_mqtt_client_handle_t *mqtt_service_start(const char *broker_url, const char *broker_user, const char *broker_pass)
{
    populate_standard_topic_and_payload();
    mqtt_client(broker_url, broker_user, broker_pass);
    return &client;
}

esp_err_t suspend_mqtt_service(esp_mqtt_client_handle_t *client)
{
    return esp_mqtt_client_stop(*client);
}

esp_err_t resume_mqtt_service(esp_mqtt_client_handle_t *client)
{
    return esp_mqtt_client_start(*client);
}

void refresh_healthcheck_payload(char *message, int state, unsigned long long timestamp)
{
    memset(message, 0, MAX_PAYLOAD_LENGTH);
    snprintf(message, MAX_PAYLOAD_LENGTH, "{\"State\": %d, \"Timestamp\": %llu}", state, timestamp);
}

esp_err_t send_healthcheck(void)
{
    ESP_LOGI(TAG_MQTT, "Sending healthcheck with the current state!");
    StateData *current = get_current_energy_consumption_state();
    refresh_healthcheck_payload(payload_healthcheck, current->state, current->timestamp);
    int healthcheck_status = esp_mqtt_client_publish(client, topic_healthcheck, payload_healthcheck, 0, 1, 0);
    // trigger here
    return healthcheck_status >= 0 ? ESP_OK : ESP_FAIL;
}