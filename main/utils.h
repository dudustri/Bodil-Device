#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include "heat_pump_state.h"

char * truncate_event_data(char *);

    #ifdef AT_COMMANDS_HANDLER

    #include "esp_err.h"
    #include "esp_log.h"

    #define SA_INT_HEADER_SIZE 1
    #define SA_INT_NAME_GAP SA_INT_HEADER_SIZE
    #define SA_INT_VALUE_GAP (SA_INT_NAME_GAP + 1)
    #define SA_MIX_HEADER_SIZE 2
    #define SA_MIX_NAME_GAP SA_MIX_HEADER_SIZE
    #define SA_MIX_VALUE_GAP (SA_MIX_NAME_GAP + 1)

    // define the function pointer type for the success action
    typedef void (*success_callback_t)(void *data);

    void handle_at_command_log(esp_err_t, success_callback_t, void *);
    void success_action_int(void *);
    void success_action_raw_char(void *);
    void success_action_data_buffer(void *);
    void success_action_mix_by_mask(void *);

    #endif

#endif