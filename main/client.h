#ifndef CLIENT_H
#define CLIENT_H

#include "esp_http_client.h"
#include "esp_log.h"
#include "heat_pump_state.h"
#include "led_control_sim.h"
#include "machine_control.h"

esp_err_t client_handler(esp_http_client_event_handle_t);
void get_heatpump_set_state();

#endif