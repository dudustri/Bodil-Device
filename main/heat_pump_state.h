#ifndef HEAT_PUMP_STATE_H
#define HEAT_PUMP_STATE_H

#include <string.h>
#include "jsmn.h"
#include "esp_log.h"
#include "esp_system.h"
#include <time.h>

#define TOKEN_SIZE 128

enum EnergyConsumptionState{
    UNKNOWN,
    NORMAL,
    MEDIUM,
    OFF,
    MAX,
} EnergyConsumptionState;

typedef struct StateData {
    unsigned long long timestamp;
    enum EnergyConsumptionState state;
} StateData;

StateData *heat_pump_state_init(void);
void heat_pump_state_destroy(StateData*);
void set_energy_consumption_state(StateData*, int, enum EnergyConsumptionState);
char * match_state_from_tokens_object(int);
bool json_key_is_equal(const char *, jsmntok_t *, const char *);
bool identify_and_set_state(jsmntok_t *, int, const char *, StateData*);
bool process_heap_pump_energy_state_response(const char*, jsmntok_t *, StateData*);

StateData *get_current_energy_consumptionState(void);

#endif