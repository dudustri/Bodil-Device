#ifndef BLUETOOTH_H
#define BLUETOOTH_H

enum ConfigKey
{
    PASSWORD,
    NAME,
    SSID,
    UNKNOWN
};

// Public Function Declarations
int initialize_bluetooth_service(char *);
int stop_bluetooth_service(void);

    #ifdef INTERNAL_BLUETOOTH

    #define CUSTOMER_MANAGER // allows customer object modification

    //Bluetooth internal dependencies
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdbool.h>
    #include <string.h>
    #include <math.h>
    #include "nimble/nimble_port.h"
    #include "nimble/nimble_port_freertos.h"
    #include "host/ble_hs.h"
    #include "services/gap/ble_svc_gap.h"
    #include "services/gatt/ble_svc_gatt.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_system.h"
    #include "esp_log.h"
    #include "esp_bt.h"
    #include "customer_info.h"
    #include "non_volatile_memory.h"

    #define GATT_DEVICE_INFO_UUID 0x0B0D
    #define GATT_READ_SERVICE_UUID 0x00BB
    #define GATT_WRITE_NAME_SERVICE_UUID 0x0BB1
    #define GATT_WRITE_SSID_SERVICE_UUID 0x0BB2
    #define GATT_WRITE_PASS_SERVICE_UUID 0x0BB3

    #define INFO_BUFFER_SIZE 210
    #define MANUFACTURER_DATA_SIZE 16
    #define QUEUE_TOKEN_SIZE 5
    #define BYTES_PER_BLE_PACKET 16 // Data bytes sent per BLE packet

    // Private Functions Declarations
    void update_buffer(void);
    void initialize_buffer_cache(void);
    int ble_set_customer_info(const int, const char *, BodilCustomer *);
    void ble_advertisement(void);
    void ble_start_on_sync(void);
    void ble_task(void *);

    #endif

#endif