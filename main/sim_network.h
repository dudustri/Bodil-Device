#ifndef SIM_NETWORK_H
#define SIM_NETWORK_H

#include "esp_modem_api.h"

// Public Function Declarations
bool pppos_is_connected(void);
bool pppos_is_retrying_to_connect(void);
esp_err_t start_sim_network_module(bool);

    #ifdef INTERNAL_SIM_NETWORK

        #define LOCATION_MANAGER // allows location changes

        //module internal dependencies
        #include <stdio.h>
        #include <string.h>
        #include "freertos/FreeRTOS.h"
        #include "freertos/task.h"
        #include "freertos/event_groups.h"
        #include "esp_log.h"
        #include "esp_event.h"
        #include "esp_system.h"
        #include "esp_netif.h"
        #include "esp_netif_ppp.h"
        #include "esp_http_client.h"
        #include "driver/uart.h"
        #include "driver/gpio.h"
        #include "esp_modem_config.h"
        #include "esp_modem_dce_config.h"
        #include "lwip/err.h"
        #include "lwip/sys.h"
        #include "lwip/ip4_addr.h"
        #include "lwip/netif.h"
        #include "bodil_IoT.h"
        #include "utils.h"
        #include "conn_handlers.h"

        // Private Functions Declarations
        void on_ip_lost(void);
        static void on_ip_event(void *, esp_event_base_t, int32_t , void *);
        static void on_ppp_changed(void *, esp_event_base_t, int32_t, void *);
        esp_err_t set_pin(esp_modem_dce_t *, const char *);
        esp_err_t dce_init(esp_modem_dce_t **, esp_netif_t **);
        void get_module_connection_info(esp_modem_dce_t *);
        esp_err_t turn_off_gnss(esp_modem_dce_t *, int *);
        void get_gnss_initial_data(esp_modem_dce_t *, int *);
        void get_basic_module_info(esp_modem_dce_t *, int *);
        bool modem_check_sync(esp_modem_dce_t *);
        bool synchronize_module(esp_modem_dce_t *, uint8_t);
        void network_module_power(void);
        bool sim_module_start_network(esp_modem_dce_t *);
        bool sim_module_stop_network(esp_modem_dce_t *);
        void sim_module_reset(esp_modem_dce_t *);
        esp_err_t sim_module_power_up(esp_modem_dce_t *);
        esp_err_t destroy_sim_network_module(esp_modem_dce_t *, esp_netif_t *);
        esp_err_t check_signal_quality(esp_modem_dce_t *);
        esp_err_t sim_network_connection_get_status(void);
        bool start_network(esp_modem_dce_t *, int *);

    #endif

#endif