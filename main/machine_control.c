#include "machine_control.h"

enum EnergyConsumptionState machine_state = NORMAL;

void machine_control_init(void){
        gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SMART_GRID_CONTROLLER_PIN_1) | (1ULL << SMART_GRID_CONTROLLER_PIN_2),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
}

enum EnergyConsumptionState get_current_machine_state(){
    return machine_state;
}

void send_control_signal(enum EnergyConsumptionState){
    //TODO: send it for how long, is this just a square signal, sine? for now is set as a continuous on/off
    gpio_set_level(SMART_GRID_CONTROLLER_PIN_1, machine_state == MEDIUM || machine_state == MAX);
    gpio_set_level(SMART_GRID_CONTROLLER_PIN_2, machine_state == OFF || machine_state == MAX);
}