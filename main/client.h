#ifndef CLIENT_H
#define CLIENT_H

// Public Function Declarations
void get_heatpump_set_state();

    #ifdef INTERNAL_CLIENT
    // Enables heat pump state extra methods
    #define HP_STATE_MANAGER
    #define MACHINE_INTERFACE

    //Client internal dependencies
    #include "esp_http_client.h"
    #include "esp_log.h"
    #include "heat_pump_state.h"
    #include "led_control.h"
    #include "machine_control.h"
    #include "utils.h"

    // Private Functions Declarations
    esp_err_t client_handler(esp_http_client_event_handle_t);

    #endif

#endif