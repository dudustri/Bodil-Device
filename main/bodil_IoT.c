#define INTERNAL_BODIL_MAIN_MODULE
#include "bodil_IoT.h"

#define BLE_DEVICE_NAME "BodilBox"

enum NetworkModuleUsed netif_connected_module = DEACTIVATED;
bool bluetooth_active = false;

extern int retry_conn_num;

TaskHandle_t requestHandler = NULL;

// TODO: create a connection utils file and move the functions related to it
/* --------------------------------------------------------------------------
   -------------------------------------------------------------------------- */

void set_network_disconnected(bool conn)
{
    if (!conn)
    {
        netif_connected_module = DEACTIVATED;
    }
    return;
}

bool is_connection_stabilished(enum NetworkModuleUsed *module)
{
    ESP_LOGD("CONN_SERVICE", "module: %d", *module);
    if (*module == DEACTIVATED)
        return false;
    else
    {
        switch (*module)
        {
        case WIFI:
            return wifi_connection_get_status() == ESP_OK ? true : false;
        case SIM_NETWORK:
            return sim_network_connection_get_status() == ESP_OK ? true : false;
        default:
            ESP_LOGW("NETWORK CONNECTION CHECK", "Unexpected error when trying to identify the network module.");
            return false;
        }
    }
}

// TODO: change this method to also consider the connection via NB-IoT network with the sim_network module. (TEST the implementation)
void periodic_heatpump_state_check_task(void *args)
{
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // retrieve the arguments and cast it to the right structure
    PeriodicRequestArgs *request_info = (PeriodicRequestArgs *)args;

    while (1)
    {
        // Check if connected before making the HTTP request
        if (is_connection_stabilished(request_info->module))
        {
            get_heatpump_set_state(request_info->service_url, request_info->api_header, request_info->api_key);
            set_led_state(BLUE);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            // Wait for 30 seconds before making the next request
            set_led_state(DARK);
            vTaskDelay(30000 / portTICK_PERIOD_MS);
        }
        else
        {
            ESP_LOGW("NETWORK CONNECTION", "Module not connected to network or with a weak signal. Waiting 30 seconds to execute a new request...\n");
            set_led_state(RED);
            vTaskDelay(30000 / portTICK_PERIOD_MS);
            set_led_state(DARK);
            continue;
        }

        ESP_LOGI("High water mark Periodic HP State Req", "%d", (int)uxHighWaterMark);
    }
}

PeriodicRequestArgs *prepare_task_args(const char *service_url, const char *api_header, char *api_key, enum NetworkModuleUsed *module)
{
    PeriodicRequestArgs *args = (PeriodicRequestArgs *)pvPortMalloc(sizeof(PeriodicRequestArgs));
    if (args != NULL)
    {
        args->service_url = service_url;
        args->api_header = api_header;
        args->api_key = api_key;
        args->module = module;
    }
    return args;
}

void handle_netif_mode(const BodilCustomer *customer, enum NetworkModuleUsed *module_type)
{
    if (*module_type == DEACTIVATED)
    {
        ESP_LOGI("Checking customer information", "ssid: %s, pass: %s", customer_info.ssid, customer_info.pass);
        // WIFI interface initialization
        if (is_credentials_set(customer) && wifi_connection_start(customer_info.ssid, customer_info.pass) == ESP_OK)
        {
            ESP_LOGI("Netif Mode Handler", "Automatic connection established with the Wi-Fi module in station mode!");
            *module_type = WIFI;
            return;
        }
        // SIM_NETWORK interface initialization
        if (start_sim_network_module() == ESP_OK)
        {
            ESP_LOGI("Netif Mode Handler", "Automatic connection established with the SIM_NETWORK module in data mode!");
            *module_type = SIM_NETWORK;
            return;
        }
        ESP_LOGW("Netif Mode Handler", "Entering in standby mode since neither module could stabilish a network connection...");
    }
}

void connection_status_handler(char *ble_name, bool *ble_active)
{
    if ( netif_connected_module != DEACTIVATED && !*ble_active){
        ESP_LOGI("Connection Status Handler", "The connection is stable!");
        return;
    }
    else if (netif_connected_module == DEACTIVATED && !*ble_active)
    {
        if (initialize_bluetooth_service(ble_name) == 0)
        {
            *ble_active = true;
            if (requestHandler != NULL)
            {
                ESP_LOGW("Connection Status Handler", "Server request thread suspended.");
                vTaskSuspend(requestHandler);
                return;
            }
        }
        else{
            ESP_LOGE("Connection Status Handler", "Error initializing Bluetooth service.");
        }
    }
    else if (netif_connected_module != DEACTIVATED && *ble_active)
    {
        if (stop_bluetooth_service() == 0)
        {
            *ble_active = false;
            if (requestHandler != NULL)
            {
                ESP_LOGI("Connection Status Handler", "Server request thread resumed!");
                vTaskResume(requestHandler);
                return;
            }
            // TODO: add a routine here to try to create a new task to make the requests.
            ESP_LOGW("Connection Status Handler", "Unexpected behaviour! No request task was created in the background!");
            return;
        }
        else
        {
            ESP_LOGE("Connection Status Handler", "An error happened when stopping the BLE service...");
            return;
        }
    }
    ESP_LOGI("Connection Status Handler", "The connection problem wasn't solved and the status did not change. Keep running the BLE service...");
    return;
}

/* --------------------------------------------------------------------------
   -------------------------------------------------------------------------- */

void app_main(void)
{
    /*  _____________________________________________________
        --------------Initialization procedure:--------------
        ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾ */
    /* TODO: set the TAG for all log messages for defined ones and set the log level here.
    Example:
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);
    */

    esp_err_t ret;

    ESP_ERROR_CHECK(nvs_dotenv_load());

    const char *api_header_name = getenv("API_HEADER_NAME");
    const char *api_key = getenv("API_KEY");
    const char *service_url = getenv("SERVICE_URL");
    const char *default_ssid = getenv("SSID_DEFAULT");
    const char *default_pass = getenv("PASS_DEFAULT");
    const char *default_broker_mqtt_url = getenv("BROKER_MQTT_URL");
    const char *broker_username = getenv("BROKER_MQTT_USER");
    const char *broker_pass = getenv("BROKER_PASS");

    ESP_LOGI("DEBUG DOTENV", "%s %s", broker_username, broker_pass);

    ret = nvs_flash_init(); // store the customer configs (non volatile) [struct config]

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    init_customer_info(&customer_info);
    load_from_nvs("storage", "customer", &customer_info, sizeof(BodilCustomer));

    // TODO: Remove this from the init routine in production code
    clear_blob_nvs("storage", "customer");

    // If device_id is zero then the device shoud be reseted to the default values
    if (customer_info.device_id == 0)
    {
        const int device_id = atoi(getenv("DEVICE_ID"));
        ESP_LOGI("MAIN THREAD", "Customer initialized with the default data.\n");
        set_customer_info(&customer_info, "bodil_dev", device_id, default_ssid, default_pass, api_key);
        print_customer_info(&customer_info);
        save_to_nvs("storage", "customer", &customer_info, sizeof(BodilCustomer));
    }

    led_init();
    heat_pump_state_init();
    machine_control_init();

    // TEST MQTT
    handle_netif_mode(&customer_info, &netif_connected_module);
    connection_status_handler(BLE_DEVICE_NAME, &bluetooth_active);
    mqtt_service_start(default_broker_mqtt_url, broker_username, broker_pass);

    // TODO: create a function to retrieve the api key if is empty! -> Create a endpoint in the server side first

    // TODO: move this to a new function
    // Creates a periodic task that calls get_heatpump_set_state every 15 seconds
    PeriodicRequestArgs *args = prepare_task_args(service_url, api_header_name, customer_info.api_key, &netif_connected_module);
    if (args != NULL)
    {
        xTaskCreate(&periodic_heatpump_state_check_task, "periodic_heatpump_state_check", 3200, args, 3, &requestHandler);
    }
    else
    {
        ESP_LOGE("MAIN THREAD - Periodic Requests Task", "failed to initialize the arguments struct\n");
    }
    /*  _____________________________________________________
        ----- core loop keeping the main thread running -----
        ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾ */
    while (1)
    {
        handle_netif_mode(&customer_info, &netif_connected_module);
        connection_status_handler(BLE_DEVICE_NAME, &bluetooth_active);

        vTaskDelay(30000 / portTICK_PERIOD_MS);
        ESP_LOGI("MAIN THREAD", "5 minutes passed in the main thread \n");
    }
}