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

typedef struct BodilCustomer
{
    char name[MAX_NAME_LENGTH];
    int deviceid;
    char ssid[MAX_SSID_LENGTH];
    char pass[MAX_PASS_LENGTH];
    char api_key[MAX_API_KEY_LENGTH];
} BodilCustomer;

extern struct BodilCustomer customer_info;

void print_customer_info(const BodilCustomer *);
void set_customer_info(BodilCustomer *, const char *, int, const char *, const char *, const char *);
void init_customer_info(BodilCustomer *);
bool is_credentials_set(const BodilCustomer *);


#endif
