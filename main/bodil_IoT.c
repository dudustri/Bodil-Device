#include <stdio.h> 
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"

const char *ssid = "ssid"; //change it before flashing
const char *pass = "pass"; //change it before flashing
int retry_conn_num = 0;

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data){
    switch (event_id){
        case WIFI_EVENT_STA_START:
            printf("Starting the WiFi connection, wait...\n");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            printf("Connected!\n");
            retry_conn_num = 0;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            printf("WiFi connection lost\n");
            if(retry_conn_num<5){esp_wifi_connect();retry_conn_num++;printf("Retrying to connect... (%d)\n", retry_conn_num); vTaskDelay(2000 / portTICK_PERIOD_MS);}
            break;
        case IP_EVENT_STA_GOT_IP:
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            uint32_t ip_addr = ip4_addr_get_u32(&event->ip_info.ip);
            char ip_str[16]; // Buffer to store IP address string (xxx.xxx.xxx.xxx\0)
            ip4addr_ntoa_r((const ip4_addr_t *)&ip_addr, ip_str, sizeof(ip_str));
            printf("IP address of the connected device: %s\n", ip_str);
            break;
    }
}

void wifi_connection(){
    esp_netif_init(); // initiates network interface
    esp_event_loop_create_default(); // dispatch events loop callback
    esp_netif_create_default_wifi_sta(); // create default wifi station
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config); // initialize wifi
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL); // register event handler for events
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL); // register event handler for network
    wifi_config_t wifi_configuration = {
        .sta= {
            .ssid = "",
            .password= "", 
        },
    };
    strcpy((char*)wifi_configuration.sta.ssid, ssid);
    strcpy((char*)wifi_configuration.sta.password, pass);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_start(); // start wifi
    esp_wifi_set_mode(WIFI_MODE_STA); // set wifi mode to connect to an existing Wi-Fi network as a client (station)
    esp_wifi_connect();
    printf( "wifi_init_softap finished. SSID:%s  password:%s \n",ssid,pass);
}

void app_main(void){
    nvs_flash_init(); // store the wifi configs (non volatile) [ssid, password]
    wifi_connection();
}