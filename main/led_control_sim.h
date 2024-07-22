#ifndef LED_CONTROL_SIM_H
#define LED_CONTROL_SIM_H

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define STATUS GPIO_NUM_25
#define CONNECTION_TYPE GPIO_NUM_32
#define CONNECTION GPIO_NUM_33

enum LedCommand
{
    ALL_OFF_LED,
    INIT_LED,
    ACTIVE_LED,
    WIFI_LED,
    SIM_LED,
    BLE_LED,
    ESTABILISHING_CONNECTION_LED,
    CONNECTED_LED,
    ERROR_LED,
    COMMAND_RECEIVED,
};

typedef struct
{
    int pin;
    int cycle_time;
} BlinkParams;

// extern enum LedCommand last_command;

void blink_led_task(void *);
void activate_led_blink(int, int, TaskHandle_t *);
void change_led_to_static_mode(int, int);
void activate_static_led(int);
void deactivate_led(int);
void set_led_state(enum LedCommand);
void led_manager_task();
void led_init(void);

#endif