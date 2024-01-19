#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"

extern int retry_conn_num; 

void wifi_connection_init();
void wifi_connection_start(const char *, const char *);
int wifi_connection_get_status();

#endif