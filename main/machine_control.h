#ifndef MACHINE_CONTROL_H
#define MACHINE_CONTROL_H

#include "driver/gpio.h"
#include "heat_pump_state.h"

#define SMART_GRID_CONTROLLER_PIN_1 GPIO_NUM_25
#define SMART_GRID_CONTROLLER_PIN_2 GPIO_NUM_26

void machine_control_init(void);
enum EnergyConsumptionState get_current_machine_state(StateData *);
void send_control_signal(enum EnergyConsumptionState);
void change_to_next_state(enum EnergyConsumptionState);

#endif