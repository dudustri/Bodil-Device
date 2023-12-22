#include <stdio.h> 
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "data.h"
#include "client.h"
#include "wifi_connection.h"
// #include "esp_sleep.h"

extern int retry_conn_num;
// Initializing the customer info. It should be updated in the main by bluetooth settings (ssid, pass) and by the api response (api_key).
static struct BodilCustomer customer_info = {.name= "bodil_dev", .deviceid = 1, .ssid="Bodil_Fiber", .pass="99741075", .api_key="api_key"}; //change it before flashing

void periodic_heatpump_state_check_task(void *pvParameter) {
    while (1) {
        // Get the wifi status
        int wifi_status = wifi_connection_get_status();
        // Check if connected before making the HTTP request
        if (wifi_status == ESP_OK) {
            get_heatpump_set_state();
        }
        else{
            printf("Not connected to WiFi. Waiting 2 seconds to execute a new request...\n");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        // Wait for 15 seconds before making the next request
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}


void app_main(void){
    nvs_flash_init(); // store the customer configs (non volatile) [struct config]
    wifi_connection_init();
    wifi_connection_start(customer_info.ssid, customer_info.pass);
    
    //TODO: create routine to update wifi credentials through bluetooth (update ssid and pass)

    /* Misterious and suspectfull bluettoth implementation here*/

    // Create a periodic task that calls get_heatpump_set_state every 15 seconds
    xTaskCreate(&periodic_heatpump_state_check_task, "periodic_heatpump_state_check", 4096, NULL /*arguments*/, 3, NULL /*handlers*/); 
    
    while (1) {
        // just do whatever in the main thread loop for now
        printf("1 minute passed in the main thread \n");
        vTaskDelay(60000 / portTICK_PERIOD_MS);

        //energy saving mode to use in the future
        //esp_deep_sleep(5 * 60 * 1000000);  // Sleep for 5 minutes in microseconds
    }
}