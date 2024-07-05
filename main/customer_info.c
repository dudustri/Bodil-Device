#define INTERNAL_CUSTOMER
#include "customer_info.h"

struct BodilCustomer customer_info;
const char *TAG_CUSTOMER = "Customer Info";

void init_customer_info(BodilCustomer *customer)
{
    memset(customer, 0, sizeof(BodilCustomer));
}

void print_customer_info(const BodilCustomer *customer)
{
    ESP_LOGD(TAG_CUSTOMER, "Name: %s", customer->name);
    ESP_LOGD(TAG_CUSTOMER, "Device ID: %d", customer->device_id);
    ESP_LOGD(TAG_CUSTOMER, "SSID: %s", customer->ssid);
    ESP_LOGD(TAG_CUSTOMER, "Password: %s", customer->pass);
    ESP_LOGD(TAG_CUSTOMER, "API Key: %s", customer->api_key);
    ESP_LOGD(TAG_CUSTOMER, "Device Location ~ Latitude: %f, Longitude: %f, Altitude:%f, Last Check: %llu", customer->location_info.latitude, customer->location_info.longitude, customer->location_info.altitude, customer->location_info.last_check_timestamp);
}

void set_customer_info(BodilCustomer *customer, const char *name, int device_id, const char *ssid, const char *pass, const char *api_key)
{
    snprintf(customer->name, sizeof(customer->name), "%s", name);
    customer->device_id = device_id;
    snprintf(customer->ssid, sizeof(customer->ssid), "%s", ssid);
    snprintf(customer->pass, sizeof(customer->pass), "%s", pass);
    snprintf(customer->api_key, sizeof(customer->api_key), "%s", api_key);
}

void set_device_location_info(BodilCustomer *customer, float latitude, float longitude, float altitude, unsigned long long timestamp)
{
    customer->location_info.latitude = latitude;
    customer->location_info.longitude = longitude;
    customer->location_info.altitude = altitude;
    customer->location_info.last_check_timestamp = timestamp;
}

bool is_credentials_set(const BodilCustomer *customer)
{
    return !(strcmp(customer->ssid, "") == 0 || strcmp(customer->pass, "") == 0 || strcmp(customer->name, "") == 0);
}