#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

    static void mqtt_app_start(char *);

    #ifdef MQTT_PROTOCOL
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

        static void mqtt_event_handler(void *, esp_event_base_t, int32_t, void *);

    #endif

#endif