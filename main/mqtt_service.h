#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include "mqtt_client.h"

esp_mqtt_client_handle_t * mqtt_service_start(const char *, const char *, const char *);
esp_err_t suspend_mqtt_service(esp_mqtt_client_handle_t *);
esp_err_t resume_mqtt_service(esp_mqtt_client_handle_t *);
esp_err_t send_healthcheck();

    #ifdef MQTT_PROTOCOL

        // Enables heat pump state extra methods
        #define HP_STATE_MANAGER
        #define MACHINE_INTERFACE
        #define MAX_TOPIC_LENGTH 32
        #define MAX_PAYLOAD_LENGTH 64
        
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

        #include "customer_info.h"
        #include "heat_pump_state.h"
        #include "led_control.h"
        #include "machine_control.h"
        #include "utils.h"

        char topic_unique[MAX_TOPIC_LENGTH];
        char topic_confirmation[MAX_TOPIC_LENGTH];
        char topic_healthcheck[MAX_TOPIC_LENGTH];
        char payload_confirmation[MAX_PAYLOAD_LENGTH];
        char payload_registration[MAX_PAYLOAD_LENGTH];
        char payload_healthcheck[MAX_PAYLOAD_LENGTH];

        static void mqtt_event_handler(void *, esp_event_base_t, int32_t, void *);
        static void mqtt_client(const char *, const char *, const char *);
        void refresh_healthcheck_payload(char *, int, unsigned long long);

#endif

#endif