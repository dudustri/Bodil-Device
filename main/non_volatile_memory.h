#ifndef NON_VOLATILE_MEMORY_H
#define NON_VOLATILE_MEMORY_H

#include "nvs_flash.h"
#include "esp_log.h"

int clear_blob_nvs(const char *, const char *);
int save_to_nvs(const char *, const char *, const void *, size_t);
int load_from_nvs(const char *, const char *, void *, size_t);

#endif