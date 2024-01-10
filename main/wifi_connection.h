#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

extern int retry_conn_num; 

void wifi_connection_init();
void wifi_connection_start(const char *, const char *);
int wifi_connection_get_status();

#endif