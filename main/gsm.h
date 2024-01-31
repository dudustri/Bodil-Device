#ifndef GSM_H
#define GSM_H

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
#include "esp_modem_api.h"
#include "esp_modem_config.h"
#include "esp_modem_dce_config.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"

esp_err_t start_gsm_module(void);
int set_pin(esp_modem_dce_t *, const char *);
void destroy_gsm_module(esp_modem_dce_t *, esp_netif_t *);
void check_signal_quality(esp_modem_dce_t *);

#endif