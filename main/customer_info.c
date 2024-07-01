#include "customer_info.h"

struct BodilCustomer customer_info;

void init_customer_info(BodilCustomer *customer)
{
    memset(customer, 0, sizeof(BodilCustomer));
}

void print_customer_info(const BodilCustomer *customer)
{
    ESP_LOGD("Customer Info ->", "Name: %s\n", customer->name);
    ESP_LOGD("Customer Info ->", "Device ID: %d\n", customer->device_id);
    ESP_LOGD("Customer Info ->", "SSID: %s\n", customer->ssid);
    ESP_LOGD("Customer Info ->", "Password: %s\n", customer->pass);
    ESP_LOGD("Customer Info ->", "API Key: %s\n", customer->api_key);
}

void set_customer_info(BodilCustomer *customer, const char *name, int device_id, const char *ssid, const char *pass, const char *api_key)
{
    snprintf(customer->name, sizeof(customer->name), "%s", name);
    customer->device_id = device_id;
    snprintf(customer->ssid, sizeof(customer->ssid), "%s", ssid);
    snprintf(customer->pass, sizeof(customer->pass), "%s", pass);
    snprintf(customer->api_key, sizeof(customer->api_key), "%s", api_key);
}

bool is_credentials_set(const BodilCustomer *customer)
{
    return !(strcmp(customer->ssid, "") == 0 || strcmp(customer->pass, "") == 0 || strcmp(customer->name, "") == 0);
}