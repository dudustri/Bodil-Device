
#include "led_control_sim.h"

enum LedState led_state = DARK;

void led_init(void)
{
    // Configure GPIO pins as outputs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RED_PIN) | (1ULL << BLUE_PIN) | (1ULL << GREEN_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
}

void set_led_state(enum LedState led_state)
{
    gpio_set_level(RED_PIN, led_state == RED || led_state == REDBLUE || led_state == REDGREEN || led_state == ALL);
    gpio_set_level(BLUE_PIN, led_state == BLUE || led_state == REDBLUE || led_state == BLUEGREEN || led_state == ALL);
    gpio_set_level(GREEN_PIN, led_state == GREEN || led_state == REDGREEN || led_state == BLUEGREEN || led_state == ALL);
}

void change_to_next_color(enum LedState current_state)
{
    switch (current_state)
    {
    case RED:
        led_state = BLUE;
        break;
    case BLUE:
        led_state = GREEN;
        break;
    case GREEN:
        led_state = RED;
        break;
    case ALL:
        led_state = GREEN;
        break;
    default:
        led_state = ALL;
    }
    set_led_state(led_state);
}

void change_led_to_red_color(){
    set_led_state(RED);
}

void change_led_to_green_color(){
    set_led_state(GREEN);
}

void change_led_to_blue_color(){
    set_led_state(BLUE);
}

void set_darkness(){
    set_led_state(DARK);
}