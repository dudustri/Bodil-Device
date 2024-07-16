#define AT_COMMANDS_HANDLER
#include "utils.h"

char *truncate_event_data(char *data)
{
    if (strlen(data) > RESPONSE_DATA_SIZE)
    {
        // Truncate to the pre defined size of the tokens
        data[RESPONSE_DATA_SIZE] = '\0';
    }
    return data;
}

/*----------------------------------------------------------------
---------------------- AT COMMANDS HANDLER -----------------------
----------------------------------------------------------------*/

const char *TAG_AT_COM = "AT Command Handler";

void handle_at_command_log(esp_err_t cmd_result, success_callback_t success_action, void *data)
{
    if (cmd_result == ESP_OK)
    {
        success_action(data);
    }
    else
    {
        ESP_LOGE(TAG_AT_COM, "Failed with %s", cmd_result == ESP_ERR_TIMEOUT ? "TIMEOUT" : "ERROR");
    }
}

void success_action_int(void *data)
{
    void **data_array = (void **)data;
    int count = *(int *)data_array[0];
    for (int i = 0; i < count; i++)
    {
        const char *name = (const char *)data_array[2 * i + 1];
        int value = *(int *)data_array[2 * i + 2];
        ESP_LOGI(TAG_AT_COM, "OK. %s: %d", name, value);
    }
}

void success_action_raw_char(void *data)
{
    void **data_array = (void **)data;
    int count = *(int *)data_array[0];
    for (int i = 0; i < count; i++)
    {
        const char *name = (const char *)data_array[2 * i + 1];
        const char *value = (const char *)data_array[2 * i + 2];
        ESP_LOGI(TAG_AT_COM, "OK. %s: %s", name, value);
    }
}

// To use this function it ise necessary to define a byte mask setting the order of the elements -> 1 for integers 0 for char[]
void success_action_mix_by_mask(void *data)
{
    void **data_array = (void **)data;
    int count = *(int *)data_array[0];
    uint8_t mask = *(uint8_t *)data_array[1];
    for (int i = 0; i < count; i++)
    {
        const char *name = (const char *)data_array[2 * i + SA_MIX_NAME_GAP];
        const void *value_adress = data_array[2 * i + SA_MIX_VALUE_GAP];
        if (mask & (1 << i))
        {
            int value = *(int *)value_adress;
            ESP_LOGI(TAG_AT_COM, "OK. %s: %d", name, value);
        }
        else
        {
            const char *value = (const char *)value_adress;
            ESP_LOGI(TAG_AT_COM, "OK. %s: %s", name, value);
        }
    }
}

void success_action_data_buffer(void *data)
{
    char *at_response_buffer = (char *)data;
    ESP_LOGI(TAG_AT_COM, "OK. %s", at_response_buffer);
}