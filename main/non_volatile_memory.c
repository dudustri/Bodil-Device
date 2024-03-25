#include "non_volatile_memory.h"

void clear_blob_nvs(const char *namespace, const char *key)
{

    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &nvs);
    if (ret != ESP_OK)
    {
        ESP_LOGE("Non Volatile Storage Erase", "Error opening NVS namespace: %d\n", ret);
        return;
    }

    // Erase the specific key storing a blob
    ret = nvs_erase_key(nvs, key);
    if (ret != ESP_OK)
    {
        ESP_LOGE("Non Volatile Storage Erase", "Error erasing NVS key: %d\n", ret);
    }

    nvs_commit(nvs);
    nvs_close(nvs);
}

void save_to_nvs(const char *namespace, const char *key, const void *data, size_t size)
{
    nvs_handle_t nvs;
    esp_err_t err;

    err = nvs_open(namespace, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE("Non Volatile Storage Save", "Error opening NVS namespace: %d\n", err);
        return;
    }

    err = nvs_set_blob(nvs, key, data, size);
    if (err != ESP_OK)
    {
        ESP_LOGE("Non Volatile Storage Save", "Error setting NVS key: %d\n", err);
    }

    nvs_commit(nvs);
    nvs_close(nvs);
}

void load_from_nvs(const char *namespace, const char *key, void *data, size_t size)
{
    nvs_handle_t nvs;
    esp_err_t err;

    err = nvs_open(namespace, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE("Non Volatile Storage Load", "Error opening NVS namespace: %d\n", err);
        return;
    }

    err = nvs_get_blob(nvs, key, data, &size);
    if (err != ESP_OK)
    {
        ESP_LOGE("Non Volatile Storage Load", "Error getting NVS key: %d\n", err);
    }

    nvs_close(nvs);
}
