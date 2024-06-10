#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

    void mqtt_service_start(const char *, const char *, const char *);

    #ifdef MQTT_PROTOCOL

        // Enables heat pump state extra methods
        #define HP_STATE_MANAGER
        #define MACHINE_INTERFACE
        
        #include <stdio.h>
        #include <stdint.h>
        #include <stddef.h>
        #include <string.h>
        #include "esp_wifi.h"
        #include "esp_system.h"
        #include "nvs_flash.h"
        #include "esp_event.h"
        #include "esp_netif.h"

        #include "freertos/FreeRTOS.h"
        #include "freertos/task.h"
        #include "freertos/semphr.h"
        #include "freertos/queue.h"

        #include "lwip/sockets.h"
        #include "lwip/dns.h"
        #include "lwip/netdb.h"

        #include "esp_log.h"
        #include "mqtt_client.h"

        #include "customer_info.h"
        #include "heat_pump_state.h"
        #include "led_control_sim.h"
        #include "machine_control.h"
        #include "utils.h"

        static void mqtt_event_handler(void *, esp_event_base_t, int32_t, void *);
        static void mqtt_client(const char *, const char *, const char *);

    #endif

#endif