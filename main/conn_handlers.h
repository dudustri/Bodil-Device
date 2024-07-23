#ifndef CONN_HANDLERS_H
#define CONN_HANDLERS_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

enum NetworkModuleUsed{
    WIFI_MODULE,
    SIM_NETWORK_MODULE,
    DEACTIVATED
};

enum ConnectionPreference{
    WIFI,
    SIM,
    ANY
};

void set_network_disconnected(bool);
enum NetworkModuleUsed * start_conn_handlers(void);
enum NetworkModuleUsed get_current_network_module(void);

    #ifdef CONN_HANDLER_INTERNAL_ACCESS

        #include "esp_err.h"
        #include "esp_log.h"
        #include "bluetooth.h"
        #include "customer_info.h"
        #include "sim_network.h"
        #include "wifi_connection.h"
        #include "mqtt_service.h"
        #include "led_control.h"

        esp_err_t handle_netif_mode(const BodilCustomer *, enum NetworkModuleUsed *, enum ConnectionPreference *conn_settings);
        void connection_status_handler(char *, bool *, TaskHandle_t *, esp_mqtt_client_handle_t *mqtt_client);
        bool is_connection_estabilished(enum NetworkModuleUsed *);
        enum ConnectionPreference get_connection_preference(const char *);

    #endif

#endif