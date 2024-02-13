#ifndef HEAT_PUMP_STATE_H
#define HEAT_PUMP_STATE_H

#include "jsmn.h"
#include "esp_log.h"
#include "esp_system.h"

#define TOKEN_SIZE 128;

enum EnergyConsumptionState{
    MAX,
    MEDIUM,
    LOW,
    OFF,
    UNKNOWN,
};

#endif