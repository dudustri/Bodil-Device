#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include "esp_wifi.h"

extern int retry_conn_num;

esp_err_t wifi_connection_start(const char *, const char *);
esp_err_t wifi_connection_get_status();
esp_err_t destroy_wifi_module(esp_netif_t *);

    #ifdef INTERNAL_WIFI_MODULE

    // WiFi module internal dependencies
    #include "esp_log.h"
    #include "esp_event.h"
    #include "lwip/err.h"
    #include "lwip/sys.h"
    #include "lwip/ip4_addr.h"
    #include "bodil_IoT.h"

    // Private functions
    esp_err_t wifi_connection_init();

    #endif

#endif