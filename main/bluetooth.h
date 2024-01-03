#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// Function Declarations
void update_buffer(void);
void initialize_buffer_cache(void);
int match_key(const char *key);
int set_customer_info(const char *key, const char *value);
void ble_task(void *param);
void ble_advertisement(void);
void ble_start_on_sync(void);
void initialize_bluetooth_service(char *BLE_device_name);

#endif