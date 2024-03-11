#ifndef HEAT_PUMP_STATE_H
#define HEAT_PUMP_STATE_H

#include <string.h>
#include "jsmn.h"
#include "esp_log.h"
#include "esp_system.h"

#define TOKEN_SIZE 128

enum EnergyConsumptionState{
    UNKNOWN,
    NORMAL,
    MEDIUM,
    OFF,
    MAX,
};

typedef struct StateData {
    int timestamp;
    enum EnergyConsumptionState state;
} StateData;

#endif