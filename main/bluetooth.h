#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "sdkconfig.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "bodil_IoT.h"

// Function Declarations
void update_buffer(void);
void initialize_buffer_cache(void);
int match_key(const char *);
int ble_set_customer_info(const int, const char *, BodilCustomer *);
void ble_task(void *);
void ble_advertisement(void);
void ble_start_on_sync(void);
int initialize_bluetooth_service(char *);

#endif