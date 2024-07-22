
#include "led_control_sim.h"

static QueueHandle_t led_state_queue;
static TaskHandle_t status_blink_task_handle = NULL;
static TaskHandle_t connection_type_blink_task_handle = NULL;
static TaskHandle_t connection_blink_task_handle = NULL;
enum LedCommand last_command = ALL_OFF_LED;
const char *TAG_LED = "LED Handler";

void blink_led_task(void *pvParameters)
{
    BlinkParams *params = (BlinkParams *)pvParameters;
    int pin = params->pin;
    int cycle_time = params->cycle_time;
    int blink_interval = (cycle_time / portTICK_PERIOD_MS) / 2;

    while (1)
    {
        gpio_set_level(pin, 1);
        vTaskDelay(blink_interval);
        gpio_set_level(pin, 0);
        vTaskDelay(blink_interval);
    }
}

void activate_led_blink(int pin, int cycle_time, TaskHandle_t *task_handle)
{
    // if there is an existing blink task -> delete it
    if (*task_handle != NULL)
    {
        ESP_LOGI(TAG_LED, "Deleting existing blink task for pin %d", pin);
        vTaskDelete(*task_handle);
        *task_handle = NULL;
    }

    static BlinkParams params;
    params.pin = pin;
    params.cycle_time = cycle_time;
    ESP_LOGI(TAG_LED, "Creating new blink task for pin %d with cycle time %d", pin, cycle_time);
    xTaskCreate(blink_led_task, "Blink LED Task", 2048, (void *)&params, 10, task_handle);
}

void remove_previous_led_task(int pin)
{
    if (pin == STATUS && status_blink_task_handle != NULL)
    {
        vTaskDelete(status_blink_task_handle);
        status_blink_task_handle = NULL;
        return;
    }
    if (pin == CONNECTION_TYPE && connection_type_blink_task_handle != NULL)
    {
        vTaskDelete(connection_type_blink_task_handle);
        connection_type_blink_task_handle = NULL;
        return;
    }
    if (pin == CONNECTION && connection_blink_task_handle != NULL)
    {
        vTaskDelete(connection_blink_task_handle);
        connection_blink_task_handle = NULL;
        return;
    }
}

void ping_led(int pin, int delay)
{
    remove_previous_led_task(pin);
    gpio_set_level(pin, 0);
    vTaskDelay(delay / portTICK_PERIOD_MS);
    gpio_set_level(pin, 1);
}

void change_led_to_static_mode(int pin, int static_mode)
{
    remove_previous_led_task(pin);
    gpio_set_level(pin, static_mode);
}

void activate_static_led(int pin)
{
    ESP_LOGI(TAG_LED, "Activating static on LED for pin %d", pin);
    change_led_to_static_mode(pin, 1);
}

void deactivate_led(int pin)
{
    ESP_LOGI(TAG_LED, "Deactivating LED for pin %d", pin);
    change_led_to_static_mode(pin, 0);
}

void set_led_state(enum LedCommand new_state)
{
    // last_command = new_state;
    xQueueSend(led_state_queue, &new_state, portMAX_DELAY);
}

//TODO: Check if the 100ms is indeed needed
void led_manager_task(void *)
{
    enum LedCommand current_state;
    while (1)
    {
        if (xQueueReceive(led_state_queue, &current_state, portMAX_DELAY))
        {
            ESP_LOGI(TAG_LED, "Received new LED state: %d", current_state);
            switch (current_state)
            {
            case ALL_OFF_LED:
                deactivate_led(STATUS);
                deactivate_led(CONNECTION);
                deactivate_led(CONNECTION_TYPE);
                break;
            // blink fast status
            case INIT_LED:
                activate_led_blink(STATUS, 1000, &status_blink_task_handle);
                break;
            // static status
            case ACTIVE_LED:
                activate_static_led(STATUS);
                break;
            // static connection type
            case WIFI_LED:
                activate_static_led(CONNECTION_TYPE);
                break;
            // blink connection type slow
            case SIM_LED:
                activate_led_blink(CONNECTION_TYPE, 2000, &connection_type_blink_task_handle);
                break;
            // blink connection type and connection very fast
            case BLE_LED:
                activate_led_blink(CONNECTION, 500, &connection_blink_task_handle);
                vTaskDelay(100);
                activate_led_blink(CONNECTION_TYPE, 500, &connection_type_blink_task_handle);
                break;
            // blink connection slow
            case ESTABILISHING_CONNECTION_LED:
                activate_led_blink(CONNECTION, 2000, &connection_blink_task_handle);
                deactivate_led(CONNECTION_TYPE);
                break;
            // static connection
            case CONNECTED_LED:
                activate_static_led(CONNECTION);
                break;
            // blink all leds very slow
            case ERROR_LED:
                activate_led_blink(STATUS, 4000, &status_blink_task_handle);
                vTaskDelay(100);
                activate_led_blink(CONNECTION_TYPE, 4000, &connection_type_blink_task_handle);
                vTaskDelay(100);
                activate_led_blink(CONNECTION, 4000, &connection_blink_task_handle);
                break;
            case COMMAND_RECEIVED:
                ping_led(STATUS, 1000);
                break;
            default:
                break;
            }
        }
    }
}

void led_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STATUS) | (1ULL << CONNECTION_TYPE) | (1ULL << CONNECTION),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    // create the queue
    led_state_queue = xQueueCreate(10, sizeof(enum LedCommand));

    // create the LED control queue consumer task
    xTaskCreate(led_manager_task, "LED Handler Task", 4096, NULL, 10, NULL);
    set_led_state(ALL_OFF_LED);
}