#ifndef NON_VOLATILE_MEMORY_H
#define NON_VOLATILE_MEMORY_H

#include "nvs_flash.h"
#include "esp_log.h"

void clear_blob_nvs(const char *, const char *);
void save_to_nvs(const char *, const char *, const void *, size_t);
void load_from_nvs(const char *, const char *, void *, size_t);

#endif