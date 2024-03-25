#ifndef MACHINE_CONTROL_H
#define MACHINE_CONTROL_H

#define SMART_GRID_CONTROLLER_PIN_1 GPIO_NUM_26
#define SMART_GRID_CONTROLLER_PIN_2 GPIO_NUM_27

void machine_control_init(void);

    #ifdef MACHINE_INTERFACE
    
    #include "driver/gpio.h"
    #include "heat_pump_state.h"

    void send_control_signal(enum EnergyConsumptionState);
    
    #endif

#endif