#define INTERNAL_WIFI_MODULE
#include "wifi_connection.h"

// connection retry state
int retry_conn_num = 0;
esp_netif_t *netif_pointer = NULL;
bool status_connected = false;

// handler for the wifi events loop
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI("WIFI EVENT", "Starting the WiFi connection, wait...\n");
        status_connected = false;
        retry_conn_num = 0;
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI("WIFI EVENT", "Connected!\n");
        status_connected = true;
        retry_conn_num = 0;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI("WIFI EVENT", "WiFi disconected\n");
        if (retry_conn_num < 5)
        {
            retry_conn_num++;
            ESP_LOGI("WIFI EVENT", "Retrying to connect... (%d)\n", retry_conn_num);
            vTaskDelay(1000 * retry_conn_num / portTICK_PERIOD_MS);
            esp_wifi_connect();
        }
        break;
    case IP_EVENT_STA_GOT_IP:
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        uint32_t ip_addr = ip4_addr_get_u32(&event->ip_info.ip);
        char ip_str[16]; // Buffer to store IP address string (xxx.xxx.xxx.xxx\0)
        ip4addr_ntoa_r((const ip4_addr_t *)&ip_addr, ip_str, sizeof(ip_str));
        ESP_LOGI("WIFI EVENT", "IP address of the connected device: %s\n", ip_str);
        break;
    }
}

// TODO: test this guy if it is working properly since it is not stoping the netif wifi module in a normal procedure (check documentation)
esp_err_t destroy_wifi_module(esp_netif_t *esp_netif)
{
    esp_err_t stop_wifi_status = esp_wifi_disconnect();
    ESP_LOGW("Destroy Wi-Fi Module", "Destroying the Wi-Fi component >.<");
    if (stop_wifi_status != ESP_OK)
    {
        ESP_LOGE("Destroy Wi-Fi Module", "An error occured when disconnecting from Wi-Fi");
        return stop_wifi_status;
    }

    stop_wifi_status = esp_wifi_stop();
    if (stop_wifi_status != ESP_OK)
    {
        ESP_LOGE("Destroy Wi-Fi Module", "An error occured while trying to stop Wi-Fi module");
        return stop_wifi_status;
    }
    stop_wifi_status = esp_wifi_deinit();
    if (stop_wifi_status != ESP_OK)
    {
        ESP_LOGE("Destroy Wi-Fi Module", "An error occured while deinitializing the Wi-Fi module");
        return stop_wifi_status;
    }
    stop_wifi_status = esp_event_loop_delete_default();
    if (stop_wifi_status != ESP_OK)
    {
        ESP_LOGE("Destroy Wi-Fi Module", "An error occured while deleting the event_loop.");
        return stop_wifi_status;
    }
    esp_netif_destroy_default_wifi(esp_netif);
    return stop_wifi_status;
}

// TODO: Make this better for god sake
esp_err_t wifi_connection_init()
{
    esp_err_t init_check = ESP_OK;

    init_check = esp_netif_init(); // initiates network interface
    if (init_check != ESP_OK)
    {
        ESP_LOGE("WIFI INIT", "Error initializing netif");
        return init_check;
    }

    init_check = esp_event_loop_create_default(); // dispatch events loop callback
    if (init_check != ESP_OK)
    {
        ESP_LOGE("WIFI INIT", "Error Creating Event Loop! Free Heap Size: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
        return init_check;
    }

    netif_pointer = esp_netif_create_default_wifi_sta(); // create default wifi station
    if (netif_pointer == NULL)
    {
        ESP_LOGE("WIFI INIT", "Error creating WiFi STA netif");
        return ESP_FAIL;
    }

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    init_check = esp_wifi_init(&wifi_init_config); // initialize wifi
    if (init_check != ESP_OK)
    {
        ESP_LOGE("WIFI INIT", "Error initializing WiFi!");
        return init_check;
    }

    init_check = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL); // register event handler for events
    if (init_check != ESP_OK)
    {
        ESP_LOGE("WIFI INIT", "Error Registering Wi-Fi event handler!");
        return init_check;
    }

    init_check = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL); // register event handler for network
    return init_check;
}

esp_err_t wifi_connection_start(const char *ssid, const char *pass)
{
    esp_err_t wifi_check = ESP_OK;

    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };

    strcpy((char *)wifi_configuration.sta.ssid, ssid);
    strcpy((char *)wifi_configuration.sta.password, pass);

    wifi_check = wifi_connection_init();
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE("WIFI Start", "Error initializing module WiFi... \n");
        return wifi_check;
    }

    wifi_check = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE("WIFI Start", "Set STA config Error \n");
        return wifi_check;
    }

    wifi_check = esp_wifi_start(); // start wifi
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE("WIFI Start", "Error starting WiFi...\n");
        return wifi_check;
    }

    wifi_check = esp_wifi_set_mode(WIFI_MODE_STA); // set wifi mode to connect to an existing Wi-Fi network as a client (station)
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE("WIFI Start", "STA set mode error.\n");
        return wifi_check;
    }

    wifi_check = esp_wifi_connect();
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE("WIFI Start", "Connection error!\n");
        return wifi_check;
    }

    int conn_stats_num = 0;
    do
    {
        ESP_LOGI("WIFI Connection STATUS:", "Inspecting the connection status -> Retries (%d)", conn_stats_num);
        vTaskDelay(300 * conn_stats_num / portTICK_PERIOD_MS);
        wifi_check = wifi_connection_get_status();
        conn_stats_num++;
    } while (wifi_check != ESP_OK && conn_stats_num <= 3);

    /*TODO:
    - this line is messy - change this to make more clear and also test the destroy - it needs to work properly
    - maybe print the pointer to see if it is being allocated properly. Check if there is some deinit functions for the wifi module.
    */
    return wifi_check == ESP_OK ? ESP_OK : destroy_wifi_module(netif_pointer) == ESP_OK ? ESP_FAIL
                                                                                        : ESP_ERR_WIFI_NOT_INIT;
}

// TODO: check this function since it is the one calling esp_wifi_sta_get_ap_info that is spamming the log!
// It should be called only if a connection is stabilished with an AP.
esp_err_t wifi_connection_get_status()
{
    wifi_ap_record_t ap_info;

    if (!status_connected)
    {
        ESP_LOGW("Wifi connection Status:", "No STA Connect event reaches the handler so far...");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    int wifi_status = esp_wifi_sta_get_ap_info(&ap_info);

    if (wifi_status == ESP_OK)
    {
        if (ap_info.rssi >= -80)
        {
            ESP_LOGI("Wifi Connection Status:", "Good connection and strenght of the signal!");
            return ESP_OK; // Connected and good signal strength -> bigger than -80 dBm
        }
        else
        {
            ESP_LOGW("Wifi Connection Status:", "The strenght of the signal is too low!");
            return ESP_ERR_WIFI_NOT_CONNECT; // Signal strength below threshold
        }
    }
    else
    {
        ESP_LOGE("Wifi Connection Status:", "Error fetching the wifi sta ap info data!");
        return ESP_ERR_WIFI_NOT_CONNECT; // Not connected to WiFi
    }
}