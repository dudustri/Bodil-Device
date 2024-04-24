#ifndef BODIL_IOT_H
#define BODIL_IOT_H

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
#include "gsm.h"
#include "led_control_sim.h"
#include "heat_pump_state.h"
#include "machine_control.h"
#include "customer_info.h"
#include "non_volatile_memory.h"
#include "nvs_dotenv.h"

enum NetworkModuleUsed{
    WIFI,
    GSM,
    DEACTIVATED
};

typedef struct PeriodicRequestArgs {
    const char *service_url;
    const char *api_header;
    const char *api_key;
} PeriodicRequestArgs;

void periodic_heatpump_state_check_task(void *);

#endif