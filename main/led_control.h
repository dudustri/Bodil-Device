#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "driver/gpio.h"

#define STATUS GPIO_NUM_25
#define CONNECTION_TYPE GPIO_NUM_32
#define CONNECTION GPIO_NUM_33

enum LedCommand
{
    ALL_OFF_LED,
    INIT_LED,
    READY_LED,
    WIFI_LED,
    SIM_LED,
    BLE_LED,
    ESTABILISHING_CONNECTION_LED,
    CONNECTED_LED,
    DISCONNECTED_LED,
    ERROR_LED,
    COMMAND_RECEIVED,
    BODIL_ON_CONTROL,
};

void led_init(void);
void set_led_state(enum LedCommand);

// extern enum LedCommand last_command;

    #ifdef LED_MANAGER

        #include "esp_log.h"
        #include "freertos/FreeRTOS.h"
        #include "freertos/task.h"
        #include "freertos/queue.h"
        #include "conn_handlers.h"
        #include "heat_pump_state.h"

        typedef struct
        {
            int pin;
            int cycle_time;
        } BlinkParams;

        void blink_led_task(void *);
        void activate_led_blink(int, int, TaskHandle_t *);
        void remove_previous_led_task(int);
        void ping_led(int, int);
        void change_led_to_static_mode(int, int);
        void activate_static_led(int);
        void deactivate_led(int);
        void led_manager_task();
        
    #endif

#endif