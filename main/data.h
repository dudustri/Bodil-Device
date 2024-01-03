#ifndef DATA_H
#define DATA_H

typedef struct BodilCustomer{
    char *name;
    int deviceid;
    char *ssid;
    char *pass;
    char *api_key;
} BodilCustomer;

enum ConfigKey {
    PASSWORD,
    NAME,
    SSID,
    UNKNOWN
};

extern struct BodilCustomer customer_info;

#endif