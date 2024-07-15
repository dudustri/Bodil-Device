#define INTERNAL_WIFI_MODULE
#include "wifi_connection.h"

// connection retry state
int retry_conn_num = 0;
esp_netif_t *netif_pointer = NULL;
bool status_connected = false;

// handler for the wifi events loop
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    const char *TAG_WEV = "WiFi Event";
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG_WEV, "Starting the WiFi connection, wait...\n");
        status_connected = false;
        retry_conn_num = 0;
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG_WEV, "Connected!\n");
        status_connected = true;
        retry_conn_num = 0;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        if(status_connected){
            status_connected = false;
            ESP_LOGI(TAG_WEV, "WiFi disconected\n");
            if (retry_conn_num < 5)
            {
                retry_conn_num++;
                ESP_LOGI(TAG_WEV, "Retrying to connect... (%d)\n", retry_conn_num);
                vTaskDelay(1000 * retry_conn_num / portTICK_PERIOD_MS);
                esp_wifi_connect();
                esp_err_t wifi_check = wifi_connection_get_status();
                if(wifi_check == ESP_OK) status_connected = true;
                break;
            }
            destroy_wifi_module(netif_pointer);
            set_network_disconnected(status_connected);
        }
        break;
    case IP_EVENT_STA_GOT_IP:
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        uint32_t ip_addr = ip4_addr_get_u32(&event->ip_info.ip);
        char ip_str[16]; // Buffer to store IP address string (xxx.xxx.xxx.xxx\0)
        ip4addr_ntoa_r((const ip4_addr_t *)&ip_addr, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG_WEV, "IP address of the connected device: %s\n", ip_str);
        break;
    }
}

esp_err_t destroy_wifi_module(esp_netif_t *esp_netif)
{
    const char *TAG_WDS = "Destroy Wi-Fi Module";
    esp_err_t stop_wifi_status = esp_wifi_disconnect();
    ESP_LOGW(TAG_WDS, "Destroying the Wi-Fi component >.<");
    if (stop_wifi_status != ESP_OK)
    {
        ESP_LOGE(TAG_WDS, "An error occured when disconnecting from Wi-Fi");
        return stop_wifi_status;
    }

    stop_wifi_status = esp_wifi_stop();
    if (stop_wifi_status != ESP_OK)
    {
        ESP_LOGE(TAG_WDS, "An error occured while trying to stop Wi-Fi module");
        return stop_wifi_status;
    }
    stop_wifi_status = esp_wifi_deinit();
    if (stop_wifi_status != ESP_OK)
    {
        ESP_LOGE(TAG_WDS, "An error occured while deinitializing the Wi-Fi module");
        return stop_wifi_status;
    }
    stop_wifi_status = esp_event_loop_delete_default();
    if (stop_wifi_status != ESP_OK)
    {
        ESP_LOGE(TAG_WDS, "An error occured while deleting the event_loop.");
        return stop_wifi_status;
    }
    esp_netif_destroy_default_wifi(esp_netif);
    return stop_wifi_status;
}

esp_err_t wifi_connection_init()
{
    const char *TAG_WIN = "WiFi Init";
    esp_err_t init_check = ESP_OK;

    init_check = esp_netif_init(); // initiates network interface
    if (init_check != ESP_OK)
    {
        ESP_LOGE(TAG_WIN, "Error initializing netif");
        return init_check;
    }

    init_check = esp_event_loop_create_default(); // dispatch events loop callback
    if (init_check != ESP_OK)
    {
        ESP_LOGE(TAG_WIN, "Error Creating Event Loop! Free Heap Size: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
        return init_check;
    }

    netif_pointer = esp_netif_create_default_wifi_sta(); // create default wifi station
    if (netif_pointer == NULL)
    {
        ESP_LOGE(TAG_WIN, "Error creating WiFi STA netif");
        return ESP_FAIL;
    }

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    init_check = esp_wifi_init(&wifi_init_config); // initialize wifi
    if (init_check != ESP_OK)
    {
        ESP_LOGE(TAG_WIN, "Error initializing WiFi!");
        return init_check;
    }

    init_check = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL); // register event handler for events
    if (init_check != ESP_OK)
    {
        ESP_LOGE(TAG_WIN, "Error Registering Wi-Fi event handler!");
        return init_check;
    }

    init_check = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL); // register event handler for network
    return init_check;
}

esp_err_t wifi_connection_start(const char *ssid, const char *pass)
{
    const char *TAG_WCS = "WiFi Start";
    esp_err_t wifi_check = ESP_OK;

    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };

    strcpy((char *)wifi_configuration.sta.ssid, ssid);
    strcpy((char *)wifi_configuration.sta.password, pass);

    ESP_LOGI(TAG_WCS, "Checking customer information... ssid: %s, pass: %s - wifi sta ssid: %s, pass: %s", ssid, pass, wifi_configuration.sta.ssid, wifi_configuration.sta.password);

    wifi_check = wifi_connection_init();
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE(TAG_WCS, "Error initializing module WiFi... \n");
        return wifi_check;
    }

    wifi_check = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE(TAG_WCS, "Set STA config Error \n");
        return wifi_check;
    }

    wifi_check = esp_wifi_start(); // start wifi
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE(TAG_WCS, "Error starting WiFi...\n");
        return wifi_check;
    }

    wifi_check = esp_wifi_set_mode(WIFI_MODE_STA); // set wifi mode to connect to an existing Wi-Fi network as a client (station)
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE(TAG_WCS, "STA set mode error.\n");
        return wifi_check;
    }

    wifi_check = esp_wifi_connect();
    if (wifi_check != ESP_OK)
    {
        ESP_LOGE(TAG_WCS, "Connection error!\n");
        return wifi_check;
    }

    int conn_stats_num = 0;
    do
    {
        ESP_LOGI(TAG_WCS, "WiFi Connection status ~ Inspecting the connection status -> Retries (%d)", conn_stats_num);
        vTaskDelay(1500 * conn_stats_num / portTICK_PERIOD_MS);
        wifi_check = wifi_connection_get_status();
        conn_stats_num++;
    } while (wifi_check != ESP_OK && conn_stats_num <= 3);

    return wifi_check == ESP_OK ? ESP_OK : destroy_wifi_module(netif_pointer) == ESP_OK ? ESP_FAIL
                                                                                        : ESP_ERR_WIFI_NOT_INIT;
}

esp_err_t wifi_connection_get_status()
{
    const char *TAG_WCGS = "WiFi connection Status:";
    wifi_ap_record_t ap_info;

    if (!status_connected)
    {
        ESP_LOGW(TAG_WCGS, "No STA Connect event reaches the handler so far...");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    int wifi_status = esp_wifi_sta_get_ap_info(&ap_info);

    if (wifi_status == ESP_OK)
    {
        if (ap_info.rssi >= -80)
        {
            ESP_LOGI(TAG_WCGS, "Good connection and strenght of the signal!");
            return ESP_OK; // Connected and good signal strength -> bigger than -80 dBm
        }
        else
        {
            ESP_LOGW(TAG_WCGS, "The strenght of the signal is too low!");
            return ESP_ERR_WIFI_NOT_CONNECT; // Signal strength below threshold
        }
    }
    else
    {
        ESP_LOGE(TAG_WCGS, "Error fetching the wifi sta ap info data!");
        return ESP_ERR_WIFI_NOT_CONNECT; // Not connected to WiFi
    }
}