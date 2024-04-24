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

    //Bluetooth internal dependencies
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include "nimble/nimble_port.h"
    #include "nimble/nimble_port_freertos.h"
    #include "host/ble_hs.h"
    #include "services/gap/ble_svc_gap.h"
    #include "services/gatt/ble_svc_gatt.h"
    #include "esp_system.h"
    #include "esp_log.h"
    #include "esp_bt.h"
    #include "customer_info.h"
    #include "non_volatile_memory.h"

    // Private Functions Declarations
    void update_buffer(void);
    void initialize_buffer_cache(void);
    int ble_set_customer_info(const int, const char *, BodilCustomer *);
    void ble_advertisement(void);
    void ble_start_on_sync(void);
    void ble_task(void *);

    #endif

#endif