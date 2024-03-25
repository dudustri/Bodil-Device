#include "bodil_IoT.h"

#define BLE_DEVICE_NAME "BodilBox"

enum NetworkModuleUsed netif_connected_module = DEACTIVATED;

extern int retry_conn_num;

void periodic_heatpump_state_check_task(void *pvParameter)
{
    while (1)
    {
        // Get the wifi status
        int wifi_status = wifi_connection_get_status();
        // Check if connected before making the HTTP request
        if (wifi_status == ESP_OK)
        {
            get_heatpump_set_state();
            // Wait for 15 seconds before making the next request
            vTaskDelay(15000 / portTICK_PERIOD_MS);
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

//TODO: maybe add a BLUETOOTH option to the module type to identify it and destroy the BLE object when a connection is established
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
    }
    return;
}

void app_main(void)
{
    esp_err_t ret;

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
        set_customer_info(&customer_info, "bodil_dev", 1, "Bodil_Fiber", "99741075", "api_key");
        print_customer_info(&customer_info);
        save_to_nvs("storage", "customer", &customer_info, sizeof(BodilCustomer));
    }

    led_init();
    heat_pump_state_init();
    machine_control_init();

    handle_netif_mode(&customer_info, &netif_connected_module);

    if(netif_connected_module == DEACTIVATED){
        initialize_bluetooth_service(BLE_DEVICE_NAME);
    }

    // Create a periodic task that calls get_heatpump_set_state every 15 seconds
    xTaskCreate(&periodic_heatpump_state_check_task, "periodic_heatpump_state_check", 4096, NULL /*arguments*/, 3, NULL /*handlers*/);

    while (1)
    {
        // just do whatever in the main thread loop for now
        ESP_LOGI("MAIN THREAD", "5 minutes passed in the main thread \n");
        vTaskDelay(300000 / portTICK_PERIOD_MS);
        //TODO: put here the connection check
        connection_status_handler(BLE_DEVICE_NAME);
        // ------------------------------------------------------------------------------------------------
        // energy saving mode to use in the future
        // esp_deep_sleep(5 * 60 * 1000000);  // Sleep for 5 minutes in microseconds
    }
}