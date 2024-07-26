#define CONN_HANDLER_INTERNAL_ACCESS
#include "conn_handlers.h"

enum NetworkModuleUsed current_network_module;

void set_network_disconnected(bool conn)
{
    if (!conn)
    {
        current_network_module = DEACTIVATED;
        set_led_state(DISCONNECTED_LED);
    }
    return;
}

bool is_connection_estabilished(enum NetworkModuleUsed *module)
{
    const char *TAG_ICS = "ESTABILISHED CONNECTION";
    ESP_LOGD(TAG_ICS, "module: %d", *module);
    if (*module == DEACTIVATED)
        return false;
    else
    {
        switch (*module)
        {
        case WIFI_MODULE:
            return wifi_connection_get_status() == ESP_OK ? true : false;
        case SIM_NETWORK_MODULE:
            return (pppos_is_connected() || pppos_is_retrying_to_connect());
        default:
            ESP_LOGW(TAG_ICS, "Unexpected error when trying to identify the network module.");
            return false;
        }
    }
}

esp_err_t handle_netif_mode(const BodilCustomer *customer, enum NetworkModuleUsed *module_type, enum ConnectionPreference *conn_settings)
{
    const char *TAG_NMH = "Netif Mode Handler";
    // TODO develop this functionality
    const bool gps_info_required = false; //(latitude == 0 && longitude == 0) || coordinates_refresh ? true : false;
    set_led_state(ESTABILISHING_CONNECTION_LED);
    if (*module_type == DEACTIVATED)
    {
        ESP_LOGD(TAG_NMH, "Checking customer information ~ ssid: %s, pass: %s", customer_info.ssid, customer_info.pass);
        // WIFI interface initialization
        if (is_credentials_set(customer) && *conn_settings != SIM && wifi_connection_start(customer_info.ssid, customer_info.pass) == ESP_OK)
        {
            ESP_LOGI(TAG_NMH, "Automatic connection established with the Wi-Fi module in station mode!");
            *module_type = WIFI_MODULE;
            set_led_state(WIFI_LED);
            set_led_state(CONNECTED_LED);
            return ESP_OK;
        }
        // SIM_NETWORK interface initialization
        if (*conn_settings != WIFI && start_sim_network_module(gps_info_required) == ESP_OK)
        {
            ESP_LOGI(TAG_NMH, "Automatic connection established with the SIM_NETWORK module in data mode!");
            *module_type = SIM_NETWORK_MODULE;
            set_led_state(SIM_LED);
            set_led_state(CONNECTED_LED);
            return ESP_OK;
        }
        ESP_LOGW(TAG_NMH, "Entering in standby mode since neither module could stabilish a network connection...");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void connection_status_handler(char *ble_name, bool *ble_active, TaskHandle_t *requestHandler, esp_mqtt_client_handle_t *mqtt_client)
{
    const char *TAG_CSH = "Connection Status Handler";
    if (current_network_module != DEACTIVATED && !*ble_active)
    {
        ESP_LOGI(TAG_CSH, "The connection is stable!");
        return;
    }
    else if (current_network_module == DEACTIVATED && !*ble_active)
    {
        ESP_LOGW(TAG_CSH, "request handler: %p mqtt client: %p", requestHandler, mqtt_client);
        if (initialize_bluetooth_service(ble_name) == 0)
        {
            *ble_active = true;
            set_led_state(BLE_LED);
            if (*requestHandler != NULL)
            {
                ESP_LOGW(TAG_CSH, "Server request thread suspended.");
                vTaskSuspend(*requestHandler);
            }
            if (*mqtt_client != NULL)
            {
                if (suspend_mqtt_service(mqtt_client) == ESP_OK)
                {
                    ESP_LOGW(TAG_CSH, "MQTT client suspended.");
                }
                else
                {
                    ESP_LOGE(TAG_CSH, "Error when trying to suspend MQTT service.");
                }
            }
            return;
        }
        else
        {
            ESP_LOGE(TAG_CSH, "Error initializing Bluetooth service.");
        }
    }
    else if (current_network_module != DEACTIVATED && *ble_active)
    {
        ESP_LOGW(TAG_CSH, "request handler: %p mqtt client: %p", requestHandler, mqtt_client);
        if (stop_bluetooth_service() == 0)
        {
            *ble_active = false;
            if (*requestHandler != NULL)
            {
                ESP_LOGI(TAG_CSH, "Server request thread resumed!");
                vTaskResume(*requestHandler);
            }
            else
            {
                // TODO: add a routine here to try to create a new task to make the requests.
                ESP_LOGW(TAG_CSH, "Unexpected behaviour! No request task was created in the background!");
            }

            if (*mqtt_client != NULL)
            {
                if (resume_mqtt_service(mqtt_client) == ESP_OK)
                {
                    ESP_LOGI(TAG_CSH, "MQTT client resumed.");
                }
                else
                {
                    ESP_LOGE(TAG_CSH, "Error resuming the MQTT service.");
                }
            }
            return;
        }
        else
        {
            ESP_LOGE(TAG_CSH, "An error happened when stopping the BLE service...");
            return;
        }
    }
    set_led_state(BLE_LED);
    ESP_LOGI(TAG_CSH, "The connection problem wasn't solved and the status did not change. Keep running the BLE service...");
    return;
}

enum ConnectionPreference get_connection_preference(const char *preference)
{
    const char *TAG_GCP = "Get Connection Preference";

    if (preference == NULL)
    {
        ESP_LOGW(TAG_GCP, "no preference specified - setting ANY connection as default.\n");
        return ANY;
    }

    if (strcmp(preference, "WIFI") == 0)
    {
        return WIFI;
    }
    else if (strcmp(preference, "SIM") == 0)
    {
        return SIM;
    }
    else if (strcmp(preference, "ANY") == 0)
    {
        return ANY;
    }
    else
    {
        ESP_LOGW(TAG_GCP, "Unknown connection preference: %s - setting ANY connection as default.\n", preference);
        return ANY;
    }
}

enum NetworkModuleUsed *start_conn_handlers(void)
{
    current_network_module = DEACTIVATED;
    return &current_network_module;
}

enum NetworkModuleUsed get_current_network_module(void)
{
    return current_network_module;
}