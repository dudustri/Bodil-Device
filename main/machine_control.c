#include "machine_control.h"

void machine_control_init(void){
        gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SMART_GRID_CONTROLLER_PIN_1) | (1ULL << SMART_GRID_CONTROLLER_PIN_2),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
}

void send_control_signal(enum EnergyConsumptionState machine_state){
    //TODO: send it for how long, is this just a square signal, sine? for now is set as a continuous on/off
    gpio_set_level(SMART_GRID_CONTROLLER_PIN_1, machine_state == MEDIUM || machine_state == MAX);
    gpio_set_level(SMART_GRID_CONTROLLER_PIN_2, machine_state == OFF || machine_state == MAX);
}

enum EnergyConsumptionState get_current_machine_state(StateData * current_status){
    return current_status->state;
}

// This method is only for testing purposes!!! Do not use it in production/field applications
void change_to_next_state(enum EnergyConsumptionState machine_state)
{
    switch (machine_state)
    {
    case NORMAL:
        machine_state = MEDIUM;
        break;
    case MEDIUM:
        machine_state = OFF;
        break;
    case OFF:
        machine_state = MAX;
        break;
    case MAX:
        machine_state = NORMAL;
        break;
    default:
        machine_state = NORMAL;
    }
    send_control_signal(machine_state);
}