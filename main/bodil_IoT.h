#ifndef BODIL_IOT_H
#define BODIL_IOT_H

    #ifdef INTERNAL_BODIL_MAIN_MODULE

    #define CUSTOMER_MANAGER // allows customer object modification
    #define CONN_HANDLER_INTERNAL_ACCESS // allows access to the connection handlers

    #include <stdio.h>
    #include <stdlib.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_system.h"
    #include "esp_wifi.h"
    #include "esp_log.h"
    #include "esp_event.h"
    #include "client.h"
    #include "wifi_connection.h"
    #include "bluetooth.h"
    #include "sim_network.h"
    #include "led_control_sim.h"
    #include "heat_pump_state.h"
    #include "machine_control.h"
    #include "customer_info.h"
    #include "non_volatile_memory.h"
    #include "nvs_dotenv.h"
    #include "mqtt_service.h"
    #include "conn_handlers.h"

    typedef struct PeriodicRequestArgs {
        const char *service_url;
        const char *api_header;
        const char *api_key;
        enum NetworkModuleUsed *module;
    } PeriodicRequestArgs;

    void periodic_heatpump_state_check_task(void *);
    PeriodicRequestArgs *prepare_task_args(const char *, const char *, char *, enum NetworkModuleUsed *);
    #endif

#endif