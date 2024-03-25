#ifndef HEAT_PUMP_STATE_H
#define HEAT_PUMP_STATE_H

#include <stdbool.h>

// HP state data structure
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

// Public functions
StateData *heat_pump_state_init(void);
void heat_pump_state_destroy(StateData*);

    // Include jsmn only if INCLUDE_JSMN is defined - parser definition
    #ifdef INCLUDE_JSMN

        #include "jsmn.h"

        // Parser helper functions
        int json_key_is_equal(const char *, jsmntok_t *, const char *);
        bool identify_and_set_state(jsmntok_t *, int, const char *, StateData*);

    #endif

    #ifdef INTERNAL_HP_STATE

    //HP State internal dependencies
    #include <string.h>
    #include "esp_log.h"
    #include "esp_system.h"
    #include <time.h>

    // Private functions
    void set_energy_consumption_state(StateData*, int, enum EnergyConsumptionState);
    char * match_state_from_tokens_object(int);

    #endif

    // Heat pump state manager
    #ifdef HP_STATE_MANAGER

    // Token Size!
    #define RESPONSE_DATA_SIZE 88

    // State management functions
    StateData *get_current_energy_consumptionState(void);
    bool process_heat_pump_energy_state_response(const char*);

    #endif

#endif