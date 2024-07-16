#define MACHINE_INTERFACE
#include "machine_control.h"

void machine_control_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SMART_GRID_CONTROLLER_PIN_1) | (1ULL << SMART_GRID_CONTROLLER_PIN_2),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
}

void send_control_signal(enum EnergyConsumptionState machine_state)
{
    gpio_set_level(SMART_GRID_CONTROLLER_PIN_1, machine_state == OFF || machine_state == NORMAL);
    gpio_set_level(SMART_GRID_CONTROLLER_PIN_2, machine_state == MEDIUM || machine_state == NORMAL);
}