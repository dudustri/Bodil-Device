#include "utils.h"

char * truncate_event_data(char *data) {
    if (strlen(data) > RESPONSE_DATA_SIZE) {
        // Truncate to the pre defined size of the tokens
        data[RESPONSE_DATA_SIZE] = '\0';
    }
    return data;
}