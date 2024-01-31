#include "wifi_connection.h"

// connection retry state
int retry_conn_num = 0;
esp_netif_t *netif_pointer = NULL;

// handler for the wifi events loop
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI("WIFI EVENT", "Starting the WiFi connection, wait...\n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI("WIFI EVENT", "Connected!\n");
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

esp_err_t destroy_wifi_module(esp_netif_t *esp_netif)
{
    esp_err_t deinit_wifi_status = esp_wifi_deinit();
    esp_netif_destroy_default_wifi(esp_netif);
    return deinit_wifi_status;
}

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
        ESP_LOGE("WIFI Start", "Error initializiing module WiFi... \n");
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

    do
    {
        wifi_check = wifi_connection_get_status();
        vTaskDelay(1200 * retry_conn_num / portTICK_PERIOD_MS);
    } while (wifi_check != ESP_OK && retry_conn_num < 5);

    return wifi_check == ESP_OK ? ESP_OK : destroy_wifi_module(netif_pointer) == ESP_OK ? ESP_FAIL
                                                                                        : ESP_ERR_WIFI_NOT_INIT;
}

esp_err_t wifi_connection_get_status()
{
    wifi_ap_record_t ap_info;
    int wifi_status = esp_wifi_sta_get_ap_info(&ap_info);

    if (wifi_status == ESP_OK)
    {
        if (ap_info.rssi >= -80)
        {
            return ESP_OK; // Connected and good signal strength -> bigger than -80 dBm
        }
        else
        {
            return ESP_ERR_WIFI_NOT_CONNECT; // Signal strength below threshold
        }
    }
    else
    {
        return ESP_ERR_WIFI_NOT_CONNECT; // Not connected to WiFi
    }
}