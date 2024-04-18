#include "bodil_IoT.h"

#define BLE_DEVICE_NAME "BodilBox"

enum NetworkModuleUsed netif_connected_module = DEACTIVATED;

extern int retry_conn_num;

//TODO: use this variable to control the process and suspend when we are disconnected from the network and resume when reconnected
TaskHandle_t requestHandle = NULL;

void periodic_heatpump_state_check_task(void *args)
{
    //retrieve the arguments and cast it to the right structure
    PeriodicRequestArgs *request_info = (PeriodicRequestArgs *)args;

    while (1)
    {
        // Get the wifi status
        int wifi_status = wifi_connection_get_status();
        // Check if connected before making the HTTP request
        if (wifi_status == ESP_OK)
        {
            get_heatpump_set_state(request_info->service_url, request_info->api_header, request_info->api_key);
            change_led_to_blue_color();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            // Wait for 15 seconds before making the next request
            set_darkness();
            vTaskDelay(14000 / portTICK_PERIOD_MS);
        }
        else
        {
            ESP_LOGI("WIFI CONNECTION", "Not connected to WiFi. Waiting 5 seconds to execute a new request...\n");
            change_led_to_red_color();
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            set_darkness();
            continue;
        }
    }
}

PeriodicRequestArgs *prepare_task_args(const char *service_url, const char *api_header, char *api_key) {
    PeriodicRequestArgs *args = (PeriodicRequestArgs *)pvPortMalloc(sizeof(PeriodicRequestArgs));
    if (args != NULL) {
        args->service_url = service_url;
        args->api_header = api_header;
        args->api_key = api_key;
    }
    return args;
}

//TODO: add a BLUETOOTH option to the module type to identify it and destroy the BLE object when a connection is established
void handle_netif_mode(const BodilCustomer *customer, enum NetworkModuleUsed *module_type)
{
    // WIFI interface initialization

    if ( is_credentials_set(customer) && wifi_connection_start(customer_info.ssid, customer_info.pass) == ESP_OK)
    {
        ESP_LOGI("Netif Mode Handler", "Automatic connection established with the Wi-Fi module in station mode!");
        *module_type = WIFI;
        return;
    }
    // GSM interface initialization
    if (start_gsm_module() == ESP_OK)
    {
        ESP_LOGI("Netif Mode Handler", "Automatic connection established with the GSM module in data mode!");
        *module_type = GSM;
        return;
    }
    ESP_LOGE("Netif Mode Handler", "Entering in standby mode since neither module could stabilish a network connection...");
    *module_type = DEACTIVATED;
}

//TODO: do a logic handler to switch on and off the bluetooth based on the connection type!
// change the connection type to DEACTIVATED if one of the modules initially connected doesn't work for certain time
// similar to an timeout. Then it turns on the  bluetooth stack and log the problem.
void connection_status_handler(char * ble_name){
    if(netif_connected_module == DEACTIVATED){
        initialize_bluetooth_service(ble_name);
        if (requestHandle != NULL){
            vTaskSuspend(requestHandle);
        }
    }
    return;
}

void app_main(void)
{
    esp_err_t ret;

    ESP_ERROR_CHECK(nvs_dotenv_load());

    const char* api_header_name = getenv("API_HEADER_NAME");
    const char* api_key = getenv("API_KEY");
    const char* service_url = getenv("SERVICE_URL");
    const char* default_ssid = getenv("SSID_DEFAULT");
    const char* default_pass = getenv("PASS_DEFAULT");

    ret = nvs_flash_init(); // store the customer configs (non volatile) [struct config]

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    init_customer_info(&customer_info);
    clear_blob_nvs("storage", "customer");
    load_from_nvs("storage", "customer", &customer_info, sizeof(BodilCustomer));
    print_customer_info(&customer_info);

    // If deviceid is zero then the device shoud be reseted to the default values
    if (customer_info.deviceid == 0)
    {
        set_customer_info(&customer_info, "bodil_dev", 1, default_ssid, default_pass, api_key);
        print_customer_info(&customer_info);
        save_to_nvs("storage", "customer", &customer_info, sizeof(BodilCustomer));
    }

    led_init();
    heat_pump_state_init();
    machine_control_init();

    handle_netif_mode(&customer_info, &netif_connected_module);

    connection_status_handler(BLE_DEVICE_NAME);

    //TODO: create a function to retrieve the api key if is empty! -> Create a endpoint in the server side first

    // Create a periodic task that calls get_heatpump_set_state every 15 seconds
    PeriodicRequestArgs *args = prepare_task_args(service_url, api_header_name, customer_info.api_key);
    if (args != NULL) {
        xTaskCreate(&periodic_heatpump_state_check_task, "periodic_heatpump_state_check", 4096, args, 3, &requestHandle);
    } else{
        ESP_LOGE("MAIN THREAD - Periodic Requests Task", "failed to initialize the arguments struct\n");
    }

    while (1)
    {
        // just do whatever in the main thread loop for now
        vTaskDelay(300000 / portTICK_PERIOD_MS);
        ESP_LOGI("MAIN THREAD", "5 minutes passed in the main thread \n");
        //TODO: put here the connection check
        connection_status_handler(BLE_DEVICE_NAME);
        // ------------------------------------------------------------------------------------------------
        // energy saving mode to use in the future
        // esp_deep_sleep(5 * 60 * 1000000);  // Sleep for 5 minutes in microseconds
    }
}