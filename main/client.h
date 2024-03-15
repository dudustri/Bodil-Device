#ifndef CLIENT_H
#define CLIENT_H

#include "esp_http_client.h"
#include "esp_log.h"

esp_err_t client_handler(esp_http_client_event_handle_t);
void get_heatpump_set_state();

#endif