#include "gsm.h"

// PPPoS Configuration
#define PPP_UART_NUM    UART_NUM_2
#define PPP_TX_PIN      GPIO_NUM_17
#define PPP_RX_PIN      GPIO_NUM_16

// Set user APN details
#define CONFIG_APN      "user_apn"
#define CONFIG_APN_PIN  "12345678"

const esp_modem_dte_config_t dte_config = {                                  
        .dte_buffer_size = 512,        
        .task_stack_size = 4096,       
        .task_priority = 5,            
        .uart_config = {               
            .port_num = UART_NUM_1,                 
            .data_bits = UART_DATA_8_BITS,          
            .stop_bits = UART_STOP_BITS_1,          
            .parity = UART_PARITY_DISABLE,          
            .flow_control = ESP_MODEM_FLOW_CONTROL_NONE,
            .source_clk = UART_SCLK_APB,
            .baud_rate = 115200,                    
            .tx_io_num = 17,                        
            .rx_io_num = 16,                        
            .rts_io_num = 27, // RTS (Request to Send) - Not used since it is for flow control                       
            .cts_io_num = 23, // CTS (Clear to Send) - Not used since it is for flow control                       
            .rx_buffer_size = 4096,                 
            .tx_buffer_size = 512,                  
            .event_queue_size = 30,                 
       },                                           
    };

// The APN name depends of the company that provides the service.
const esp_modem_dce_config_t dce_config = { .apn = CONFIG_APN,};

const esp_netif_t *netif = NULL;


int set_pin(esp_modem_dce_t *dce, const char *pin){

    bool status_pin = false;
    if (esp_modem_read_pin(dce, &status_pin) == ESP_OK && status_pin == false){
        if (esp_modem_set_pin(dce, pin) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            ESP_LOGE("SET PIN GSM MODULE", "Failed to set pin in the gsm module.");
            return 1;
        }
    }
    return 0;

}

void start_gsm_module(void){
    
    // should return a pointer of the modem module
    esp_modem_dce_t *gsm_modem = esp_modem_new_dev( ESP_MODEM_DCE_SIM800, &dte_config, &dce_config, NULL);
    if (!set_pin(gsm_modem, CONFIG_APN_PIN)){
        return;
    }

}