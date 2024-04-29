#ifndef LED_CONTROL_SIM_H
#define LED_CONTROL_SIM_H

#include "driver/gpio.h"

#define RED_PIN GPIO_NUM_25
#define BLUE_PIN GPIO_NUM_32
#define GREEN_PIN GPIO_NUM_33

enum LedState
{
    RED,
    BLUE,
    GREEN,
    REDBLUE,
    REDGREEN,
    BLUEGREEN,
    ALL,
    DARK
};

extern enum LedState led_state;

void led_init(void);
void set_led_state(enum LedState);
void change_to_next_color(enum LedState);

#endif