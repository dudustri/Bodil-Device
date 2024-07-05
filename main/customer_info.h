#ifndef CUSTOMER_INFO_H
#define CUSTOMER_INFO_H

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "esp_log.h"

#define MAX_NAME_LENGTH 34
#define MAX_SSID_LENGTH 34
#define MAX_PASS_LENGTH 34
#define MAX_API_KEY_LENGTH 34

typedef struct DeviceLocation{
    float latitude;
    float longitude;
    float altitude;
    unsigned long long last_check_timestamp;
}DeviceLocation;
typedef struct BodilCustomer
{
    char name[MAX_NAME_LENGTH];
    int device_id;
    char ssid[MAX_SSID_LENGTH];
    char pass[MAX_PASS_LENGTH];
    char api_key[MAX_API_KEY_LENGTH];
    DeviceLocation location_info;
} BodilCustomer;

extern struct BodilCustomer customer_info;

    #ifdef INTERNAL_CUSTOMER
        #define CUSTOMER_MANAGER
        #define LOCATION_MANAGER
    #endif

    #ifdef CUSTOMER_MANAGER
    void print_customer_info(const BodilCustomer *);
    void set_customer_info(BodilCustomer *, const char *, int, const char *, const char *, const char *);
    void init_customer_info(BodilCustomer *);
    bool is_credentials_set(const BodilCustomer *);
    #endif

    #ifdef LOCATION_MANAGER
    void set_device_location_info(BodilCustomer *, float, float, float, unsigned long long);
    #endif

#endif
