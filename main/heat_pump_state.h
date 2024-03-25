#ifndef HEAT_PUMP_STATE_H
#define HEAT_PUMP_STATE_H

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include <time.h>

#define RESPONSE_DATA_SIZE 88

enum EnergyConsumptionState{
    UNKNOWN_ENERGY_STATE,
    NORMAL,
    MEDIUM,
    OFF,
    MAX,
};

typedef struct StateData {
    unsigned long long timestamp;
    enum EnergyConsumptionState state;
} StateData;

// export the state data as a general variable
extern StateData * current_state;

StateData *heat_pump_state_init(void);
StateData *get_current_energy_consumptionState(void);
void heat_pump_state_destroy(StateData*);
void set_energy_consumption_state(StateData*, int, enum EnergyConsumptionState);
char * match_state_from_tokens_object(int);
bool process_heat_pump_energy_state_response(const char*);

 // Include jsmn only if INCLUDE_JSMN is defined
#ifdef INCLUDE_JSMN
    #include "jsmn.h"

    int json_key_is_equal(const char *, jsmntok_t *, const char *);
    bool identify_and_set_state(jsmntok_t *, int, const char *, StateData*);

#endif

#endif