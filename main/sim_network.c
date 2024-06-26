#define INTERNAL_SIM_NETWORK
#include "sim_network.h"

// PPPoS Configuration
#define PPP_UART_NUM UART_NUM_2
#define PPP_TX_PIN GPIO_NUM_17
#define PPP_RX_PIN GPIO_NUM_16
#define SIM_POWER_PIN GPIO_NUM_15

// Set user APN details
#define APN "onomondo"
#define APN_PIN ""
#define SIM_NETWORK_RETRIES_NUM 5
#define BUF_SIZE 1024

const char *TAG = "Modem SIM Network";
const char *CONFIG_APN = APN;
const char *CONFIG_APN_PIN = APN_PIN;
static esp_modem_dce_t *sim_mod_dce = NULL;
static esp_netif_t *ppp_netif = NULL;
static volatile bool pppos_connected = false;
static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const int DISCONNECT_BIT = BIT1;

/*----------------------------------------------------------------
------------------------- EVENT HANDLERS -------------------------
----------------------------------------------------------------*/

static void on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    ESP_LOGD(TAG, "IP event! %" PRId32, event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);
        pppos_connected = true;
        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        xEventGroupSetBits(event_group, DISCONNECT_BIT);
        pppos_connected = false;
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        pppos_connected = true;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));        
    }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI("SIM_NETWORK - PPP - ESP MODEM", "PPP state changed event %" PRIu32, event_id);
    if (event_id == NETIF_PPP_ERRORUSER)
    {
        /* User interrupted event from esp-netif */
        esp_netif_t *p_netif = *(esp_netif_t**)event_data;
        ESP_LOGI("SIM_NETWORK - PPP - ESP MODEM", "User interrupted event from netif:%p", p_netif);
    }
        else if(event_id == NETIF_PPP_ERRORCONNECT)
    {
        ESP_LOGI(TAG, "NETIF_PPP_ERRORCONNECT");
        xEventGroupSetBits(event_group, DISCONNECT_BIT);
    }
}

/*----------------------------------------------------------------
-------------------------- DCE - MODEM ---------------------------
----------------------------------------------------------------*/

esp_err_t set_pin(esp_modem_dce_t *dce, const char *pin)
{
    esp_err_t ret;
    bool status_pin = false;
    ret = esp_modem_read_pin(dce, &status_pin);
    if (ret == ESP_OK && status_pin == false)
    {
        ret = esp_modem_set_pin(dce, pin);
        if (ret == ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            ESP_LOGE("SET PIN SIM_NETWORK MODULE", "Failed to set pin in the sim_network module.");
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t dce_init(esp_modem_dce_t **dce, esp_netif_t **netif)
{
    /* Configure the PPP netif */
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    // dte configuration
    // dte_config = {
    //     .dte_buffer_size = 512,
    //     .task_stack_size = 4096,
    //     .task_priority = 5,
    //     .uart_config = {
    //         .port_num = PPP_UART_NUM,
    //         .data_bits = UART_DATA_8_BITS,
    //         .stop_bits = UART_STOP_BITS_1,
    //         .parity = UART_PARITY_DISABLE,
    //         .flow_control = ESP_MODEM_FLOW_CONTROL_NONE,
    //         .source_clk = UART_SCLK_APB,
    //         .baud_rate = 115200,
    //         .tx_io_num = PPP_TX_PIN,
    //         .rx_io_num = PPP_RX_PIN,
    //         //.rts_io_num = 27, // RTS (Request to Send) - Not used since it is for flow control
    //         //.cts_io_num = 23, // CTS (Clear to Send) - Not used since it is for flow control
    //         .rx_buffer_size = 4096,
    //         .tx_buffer_size = 512,
    //         .event_queue_size = 30,
    //     },
    // };

    // DCE configuration - The APN name depends of the company that provides the service.
    esp_modem_dce_config_t dce_config = {
        .apn = CONFIG_APN,
    };
    ESP_LOGI(TAG, "1 - APN set: %s", dce_config.apn);
    // should return a pointer of the modem module
    ESP_LOGI(TAG, "Initializing esp_modem for a generic module...");
    // dce = esp_modem_new(&dte_config, &dce_config, esp_netif);
    *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7070, &dte_config, &dce_config, *netif);
    
    // check pin configuration
    if (strlen(CONFIG_APN_PIN) != 0 && *dce)
    {
        esp_err_t err;
        ESP_LOGW(TAG, "SET PIN - CHECK: Entering in the set pin configuration...");
        err = set_pin(*dce, CONFIG_APN_PIN);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "SET PIN - CHECK: Not chipset or pin config was identified. Destroying SIM_NETWORK module...");
            if (destroy_sim_network_module(*dce, *netif) != ESP_OK)
            {
                ESP_LOGE(TAG, "SET PIN - CHECK: Error destroying the SIM_NETWORK module.");
            }
            return err;
        }
    }
    ESP_LOGE(TAG, "modem pointer: %p - %p",*dce, *netif);
    return *dce ? ESP_OK : ESP_FAIL;
}

/*----------------------------------------------------------------
------------------------- NETWORK MODEM --------------------------
----------------------------------------------------------------*/

// CHECK THESE FUNCTIONS!!
// ---------------------------------------------------------------
bool modem_check_sync(esp_modem_dce_t *dce)
{
    return esp_modem_sync(dce) == ESP_OK;
}

bool modem_start_network(esp_modem_dce_t *dce)
{
    return esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA) == ESP_OK;
}

bool modem_stop_network(esp_modem_dce_t *dce)
{
    return esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
}

void modem_reset(esp_modem_dce_t *dce)
{
    esp_modem_reset(dce);
}

void modem_deinit_network(esp_modem_dce_t *dce)
{
    if (dce) {
        esp_modem_destroy(dce);
        dce = NULL;
    }
}

bool modem_check_signal(esp_modem_dce_t *dce)
{
    int rssi, ber;
    if (esp_modem_get_signal_quality(dce, &rssi, &ber) == ESP_OK) {
        return rssi != 99 && rssi > 5;
    }
    return false;
}
// -------------------------------------------------------------------------------
// TODO: Adjust this function to start and restart the module pulling down the pin
void network_module_reset(void)
{
    ESP_LOGI(TAG, "Network module power pin - pullin down!");
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1 << SIM_POWER_PIN;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    gpio_set_level(SIM_POWER_PIN, 0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    gpio_set_level(SIM_POWER_PIN, 1);
}

esp_err_t destroy_sim_network_module(esp_modem_dce_t *dce, esp_netif_t *esp_netif)
{
    esp_err_t stop_sim_network_status;

    ESP_LOGW("Destroy SIM_NETWORK Module", "Destroying the SIM_NETWORK component u.u");
    esp_modem_destroy(dce);
    stop_sim_network_status = esp_event_loop_delete_default();
    if (stop_sim_network_status != ESP_OK)
    {
        ESP_LOGE("Destroy SIM_NETWORK Module", "An error occured while deleting the event_loop.");
        return stop_sim_network_status;
    }
    esp_netif_destroy(esp_netif);
    return stop_sim_network_status;
}

esp_err_t check_signal_quality(esp_modem_dce_t *dce)
{
    int rssi, ber;
    esp_err_t ret = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (ret != ESP_OK)
    {
        ESP_LOGE("SIM_NETWORK - SIGNAL QUALITY - ESP MODEM", "esp_modem_get_signal_quality failed with %d %s", ret, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI("SIM_NETWORK - SIGNAL QUALITY - ESP MODEM", "Signal quality: rssi=%d, ber=%d", rssi, ber);
    return rssi != 99 && rssi > 5 ? ESP_OK : ESP_FAIL;
}

// TODO: this method will keep being called since the set_network_disconnected(false) is not implemented. Define a threshold to detroy the SIM_NETWORK module.
esp_err_t sim_network_connection_get_status(void)
{
    int retries = 0;
    int signal_ok = false;
    const char *TAG_SIG_CHECK = "Modem Network Signal Check";
    esp_err_t ret_check;
    do
    {
        ret_check = check_signal_quality(sim_mod_dce); // direct access to sim_mod_dce in this file
        if (ret_check != ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(2000 * retries));
            retries++;
            if (retries >= SIM_NETWORK_RETRIES_NUM)
            {
                ESP_LOGE(TAG_SIG_CHECK, "SIGNAL QUALITY CHECK: No signal was identified.");
                return ret_check;
            }
            ESP_LOGW(TAG_SIG_CHECK, "SIGNAL QUALITY CHECK:The signal quality check failed... Retrying (%d/%d)", retries, SIM_NETWORK_RETRIES_NUM);
        }
        else
        {
            signal_ok = true;
        }
    } while (!signal_ok);

    return ret_check;
}

void start_network(esp_modem_dce_t *dce)
{
    EventBits_t bits = 0;
    while ((bits & CONNECT_BIT) == 0) {
        if (!modem_check_sync(dce)) {
            ESP_LOGE(TAG, "Modem does not respond, maybe in DATA mode? ...exiting network mode");
            modem_stop_network(dce);
            if (!modem_check_sync(dce)) {
                ESP_LOGE(TAG, "Modem does not respond to AT ...restarting");
                modem_reset(dce);
                ESP_LOGI(TAG, "Restarted");
            }
            continue;
        }
        if (!modem_check_signal(dce)) {
            ESP_LOGI(TAG, "Poor signal ...will check after 5s");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        if (!modem_start_network(dce)) {
            ESP_LOGE(TAG, "Modem could not enter network mode ...will retry after 10s");
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }
        bits = xEventGroupWaitBits(event_group, (DISCONNECT_BIT | CONNECT_BIT), pdTRUE, pdFALSE, pdMS_TO_TICKS(30000));
        if (bits & DISCONNECT_BIT) {
            ESP_LOGE(TAG, "Modem got disconnected ...retrying");
            modem_stop_network(dce);
        }
    }
}

esp_err_t start_sim_network_module(void)
{
    esp_err_t ret_check;
    const char *TAG_START = "Modem SIM Network";

    ESP_LOGI(TAG_START, "Initializing the SIM_NETWORK module ...");
    network_module_reset();

    //Wait for network module to initialize (it takes a while)
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Initialize esp_netif and default event loop
    ret_check = esp_netif_init(); // initiates network interface
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_START, "NETIF - Initialization: netif init failed with %d", ret_check);
        return ret_check;
    }
    ret_check = esp_event_loop_create_default(); // dispatch events loop callback
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_START, "EVENT LOOP - Creation: Event Loop creation failed with %d", ret_check);
        return ret_check;
    }
    event_group = xEventGroupCreate();

    // Initialize lwip network interface in PPP mode
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    ppp_netif = esp_netif_new(&netif_ppp_config);

    // Initialize the PPP network and register for IP event
    ret_check = dce_init(&sim_mod_dce, &ppp_netif);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_START, "NETWORK INITIALIZATION - Register: IP event handler registering failed with %d", ret_check);
        return ret_check;
    }

    ret_check = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event, NULL);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_START, "IP EVENT HANDLER - Register: IP event handler registering failed with %d", ret_check);
        return ret_check;
    }

    ret_check = esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_START, "PPP EVENT HANDLER - Register: Set Pin procedure failed with %d... Stopping SIM_NETWORK Module", ret_check);
        return ret_check;
    }

    // ESP_LOGI(TAG, "dce type protocol: %d", (int)sim_mod_dce->dte_type);
    esp_modem_set_apn(sim_mod_dce, CONFIG_APN);
    // ESP_LOGI(TAG, "DCE set: %p", sim_mod_dce->DCE);

    start_network(sim_mod_dce);

    // Try to sync and communicate with the SIM Network Module
    // run_at(sim_mod_dce, 3);
    // run_at_ping(sim_mod_dce);

    // check signal quality
    // ret_check = sim_network_connection_get_status();
    // if (ret_check != ESP_OK)
    // {
    //     ESP_LOGW(TAG_START, "SIGNAL QUALITY CHECK: Destroying the SIM_NETWORK module and set modem state to DEACTIVATED.");
    //     if (destroy_sim_network_module(sim_mod_dce, ppp_netif) != ESP_OK)
    //     {
    //         ESP_LOGE(TAG_START, "SIGNAL QUALITY CHECK: Error destroying the SIM_NETWORK module.");
    //     }
    //     return ret_check;
    // }

    // set the SIM_NETWORK module to data mode
    // ret_check = esp_modem_set_mode(sim_mod_dce, ESP_MODEM_MODE_DATA);
    // if (ret_check != ESP_OK)
    // {
    //     ESP_LOGE(TAG_START, "SET DATA MODE - ESP MODEM: failed with %d", ret_check);
    //     if (destroy_sim_network_module(sim_mod_dce, ppp_netif) != ESP_OK)
    //     {
    //         ESP_LOGE(TAG_START, "SET DATA MODE: Error destroying the SIM_NETWORK module.");
    //     }
    //     return ret_check;
    // }

    return ESP_OK;
}

/*----------------------------------------------------------------
------------------------- PPOS/SIM INFO --------------------------
----------------------------------------------------------------*/

bool pppos_is_connected(void)
{
    return pppos_connected;
}


/*----------------------------------------------------------------
----------------------- OTHER STUFF LOW LVL ----------------------
----------------------------------------------------------------*/

// BARE AT COMMANDS

/* #define CHECK_ERR(cmd, success_action)                                                     \
     do                                                                                     \
     {                                                                                      \
         esp_err_t ret = cmd;                                                               \
         if (ret == ESP_OK)                                                                 \
         {                                                                                  \
             success_action;                                                                \
         }                                                                                  \
         else                                                                               \
         {                                                                                  \
             ESP_LOGE(TAG, "Failed with %s", ret == ESP_ERR_TIMEOUT ? "TIMEOUT" : "ERROR"); \
         }                                                                                  \
     } while (0)
*/

// bool check_registration_status(const char *data)
// {
//     const char *found = strstr(data, ",5");
//     return found != NULL;
// }

// void run_at(esp_modem_dce_t *dce, uint8_t count)
// {
//     for (int i = 0; i < count; i++)
//     {
//         CHECK_ERR(esp_modem_sync(dce), ESP_LOGI(TAG, "OK"));
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

// void run_at_ping(esp_modem_dce_t *dce)
// {
//     char data[BUF_SIZE];
//     int rssi, ber;
//     CHECK_ERR(esp_modem_get_signal_quality(dce, &rssi, &ber), ESP_LOGI(TAG, "OK. rssi=%d, ber=%d", rssi, ber));
//     vTaskDelay(pdMS_TO_TICKS(1000));

//     CHECK_ERR(esp_modem_at(dce, "AT+CPSMS?", data, 500), ESP_LOGI(TAG, "OK. %s", data));
//     vTaskDelay(pdMS_TO_TICKS(1000));
//     CHECK_ERR(esp_modem_at(dce, "AT+CNMP?", data, 500), ESP_LOGI(TAG, "OK. %s", data));                         // Check preference mode
//     CHECK_ERR(esp_modem_at(dce, "AT+CNMP=2", data, 500), ESP_LOGI(TAG, "OK. %s", data));                        // Set preference to automatic mode
//     CHECK_ERR(esp_modem_at(dce, "AT+CPSI?", data, 500), ESP_LOGI(TAG, "OK. %s", data));                         // Inquiring UE system information
//     CHECK_ERR(esp_modem_at(dce, "AT+CGDCONT?", data, 500), ESP_LOGI(TAG, "OK. %s", data));                      // Check APN set
//     CHECK_ERR(esp_modem_at(dce, "AT+CGDCONT=1,\"IP\",\"onomondo\",\"0.0.0.0\",0,0", data, 500), ESP_LOGI(TAG, "OK. %s", data)); // Set the APN to onomondo
//     vTaskDelay(pdMS_TO_TICKS(1000));
//     CHECK_ERR(esp_modem_at(dce, "AT+COPS?", data, 500), ESP_LOGI(TAG, "OK. %s", data));                         // Check the connections available
//     CHECK_ERR(esp_modem_at(dce, "AT+COPS=0", data, 500), ESP_LOGI(TAG, "OK. %s", data));                        // Set the modem to automatically chose the network

//     //TODO Close the network set the sock to 1 and open again
//     CHECK_ERR(esp_modem_at(dce, "AT+CSOCKSETPN=1", data, 500), ESP_LOGI(TAG, "OK. %s", data));                  // Set PDP 1 
//     CHECK_ERR(esp_modem_at(dce, "AT+NETOPEN?", data, 500), ESP_LOGI(TAG, "OK. %s", data));                      // Check open network
//     do
//     {
//         vTaskDelay(pdMS_TO_TICKS(1000));
//         CHECK_ERR(esp_modem_at(dce, "AT+COPS?", data, 500), ESP_LOGI(TAG, "OK. %s", data));                     // Check the connections available
//         CHECK_ERR(esp_modem_at(dce, "AT+CGDCONT?", data, 500), ESP_LOGI(TAG, "OK. %s", data));
//         CHECK_ERR(esp_modem_at(dce, "AT+CEREG?", data, 500), ESP_LOGI(TAG, "OK. %s", data));                    // Check if it is registered
//         CHECK_ERR(esp_modem_at(dce, "AT+CGREG?", data, 500), ESP_LOGI(TAG, "OK. %s", data));                       // Check APN set
//     } while (!check_registration_status(data));
//     CHECK_ERR(esp_modem_at(dce, "AT+CNACT=0,1", data, 500), ESP_LOGI(TAG, "OK. %s", data));                     // Activate the APP network
//     CHECK_ERR(esp_modem_at(dce, "AT+SNPDPID0", data, 500), ESP_LOGI(TAG, "OK. %s", data));                      // Select PDP index for PING
//     CHECK_ERR(esp_modem_at(dce, "AT+SNPING4=\"8.8.8.8\",5,16,5000", data, 500), ESP_LOGI(TAG, "OK. %s", data)); // Send IPv4 PING
//     // CHECK_ERR(esp_modem_at(dce, "AT+CNACT=0,0", data, 500), ESP_LOGI(TAG, "OK. %s", data));                  // Deactivate the APP network
// }