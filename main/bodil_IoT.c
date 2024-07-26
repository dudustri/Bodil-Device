#define INTERNAL_BODIL_MAIN_MODULE
#include "bodil_IoT.h"

#define BLE_DEVICE_NAME "BodilBox"

bool bluetooth_active = false;
extern int retry_conn_num;
TaskHandle_t requestHandler = NULL;
esp_mqtt_client_handle_t *mqtt_client = NULL;

void periodic_heatpump_state_check_task(void *args)
{
    const char *TAG_PHSC = "Periodic Heat Pump State Check";
    // variable to store the max memory used by the function and calibrate the xTaskCreate stack size in the main thread (~ 3200 now)
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // retrieve the arguments and cast it to the right structure
    PeriodicRequestArgs *request_info = (PeriodicRequestArgs *)args;

    while (1)
    {
        vTaskDelay(120000 / portTICK_PERIOD_MS);
        // check if connected before making the HTTP request
        if (is_connection_estabilished(request_info->module))
        {
            get_heatpump_set_state(request_info->service_url, request_info->api_header, request_info->api_key);
            vTaskDelay(30000 / portTICK_PERIOD_MS);
        }
        else
        {
            ESP_LOGW(TAG_PHSC, "Module not connected to network or with a weak signal. Waiting 30 seconds to execute a new request...\n");
            vTaskDelay(30000 / portTICK_PERIOD_MS);
            continue;
        }

        // exposes the maximum memory usage by the function tracked and stored in the water mark variable.
        ESP_LOGD(TAG_PHSC, "High water mark Periodic HP State Req: %d", (int)uxHighWaterMark);
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

/* --------------------------------------------------------------------------
   -------------------------------------------------------------------------- */

void app_main(void)
{
    const char *TAG_MAIN = "Main Thread";
    const uint8_t check_conn_minutes_cycle = 1;

    machine_control_init();
    led_init();
    set_led_state(INIT_LED);

    /*  _____________________________________________________
        --------------Initialization procedure:--------------
        ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾ */
    ESP_LOGI(TAG_MAIN, "Startup...");
    ESP_LOGI(TAG_MAIN, "Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG_MAIN, "IDF version: %s", esp_get_idf_version());
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("uart_terminal", ESP_LOG_ERROR); //to supress RX Break warning!

    esp_err_t ret;

    ESP_ERROR_CHECK(nvs_dotenv_load());

    enum ConnectionPreference conn_preference = get_connection_preference(getenv("CONNECTION_PREFERENCE"));
    const char *api_header_name = getenv("API_HEADER_NAME");
    const char *api_key = getenv("API_KEY");
    const char *service_url = getenv("SERVICE_URL");
    const char *default_ssid = getenv("SSID_DEFAULT");
    const char *default_pass = getenv("PASS_DEFAULT");
    const char *default_broker_mqtt_url = getenv("BROKER_MQTT_URL");
    const char *broker_username = getenv("BROKER_MQTT_USER");
    const char *broker_pass = getenv("BROKER_PASS");

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
        ESP_LOGI(TAG_MAIN, "Customer initialized with the default data.\n");
        set_customer_info(&customer_info, "bodil_dev", device_id, default_ssid, default_pass, api_key);
        print_customer_info(&customer_info);
        save_to_nvs("storage", "customer", &customer_info, sizeof(BodilCustomer));
    }

    enum NetworkModuleUsed *netif_connected_module = start_conn_handlers();

    heat_pump_state_init();

    handle_netif_mode(&customer_info, netif_connected_module, &conn_preference);

    mqtt_client = mqtt_service_start(default_broker_mqtt_url, broker_username, broker_pass);

    // TODO: create a function to retrieve the api key if is empty! -> Create a endpoint in the server side first or using the mqtt register return

    // TODO: move this to a new function
    // Creates a periodic task that calls get_heatpump_set_state
    PeriodicRequestArgs *args = prepare_task_args(service_url, api_header_name, customer_info.api_key, netif_connected_module);
    if (args != NULL)
    {
        xTaskCreate(&periodic_heatpump_state_check_task, "periodic_heatpump_state_check", 3200, args, 3, &requestHandler);
    }
    else
    {
        ESP_LOGE(TAG_MAIN, "Periodic Requests Task - failed to initialize the arguments struct");
    }

    connection_status_handler(BLE_DEVICE_NAME, &bluetooth_active, &requestHandler, mqtt_client);
    set_led_state(READY_LED);

    /*  _____________________________________________________
        ----- core loop keeping the main thread running -----
        ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾ */
    while (1)
    {
        vTaskDelay((60000 * check_conn_minutes_cycle) / portTICK_PERIOD_MS);
        ESP_LOGI("", "----------------------------------------------------------------------");
        ESP_LOGI(TAG_MAIN, "Checking system health. Cycle set -> %d minute(s)", check_conn_minutes_cycle);
        send_healthcheck();
        if (!is_connection_estabilished(netif_connected_module))
        {
            handle_netif_mode(&customer_info, netif_connected_module, &conn_preference);
        }
        connection_status_handler(BLE_DEVICE_NAME, &bluetooth_active, &requestHandler, mqtt_client);
        ESP_LOGI("", "----------------------------------------------------------------------");
    }
}