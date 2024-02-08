#ifndef BODIL_IOT_H
#define BODIL_IOT_H

#define MAX_NAME_LENGTH 32
#define MAX_SSID_LENGTH 32
#define MAX_PASS_LENGTH 32
#define MAX_API_KEY_LENGTH 32

typedef struct BodilCustomer
{
    char name[MAX_NAME_LENGTH];
    int deviceid;
    char ssid[MAX_SSID_LENGTH];
    char pass[MAX_PASS_LENGTH];
    char api_key[MAX_API_KEY_LENGTH];
} BodilCustomer;

enum NetworkModuleUsed{
    WIFI,
    GSM,
    DEACTIVATED
};

enum ConfigKey
{
    PASSWORD,
    NAME,
    SSID,
    UNKNOWN
};

extern struct BodilCustomer customer_info;

void init_customer_info(BodilCustomer *);
void clear_blob_nvs(const char *, const char *);
void save_to_nvs(const char *, const char *, const void *, size_t);
void load_from_nvs(const char *, const char *, void *, size_t);
void print_customer_info(const BodilCustomer *);
void set_customer_info(BodilCustomer *, const char *, int, const char *, const char *, const char *);
void periodic_heatpump_state_check_task(void *);
bool is_credentials_set(const BodilCustomer *);

#endif