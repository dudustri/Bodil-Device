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
#define START_NETWORK_RETRIES_NUM 5
#define SIM_POWER_UP_TRIES 3
#define BUF_SIZE 512
#define MAX_ESP_INFO_SIZE 32

const char *TAG = "Modem SIM Network";
const char *CONFIG_APN = APN;
const char *CONFIG_APN_PIN = APN_PIN;
static esp_modem_dce_t *sim_mod_dce = NULL;
static esp_netif_t *ppp_netif = NULL;
static volatile bool pppos_connected = false;
static bool pppos_retrying_connection = false;
static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const int DISCONNECT_BIT = BIT1;
//TODO: add this to customer information
float latitude, longitude;

/*----------------------------------------------------------------
------------------------- EVENT HANDLERS -------------------------
----------------------------------------------------------------*/
/* TODO: add a function that will handle the disconnect setting the modem to command mode and check the signal strength.
Try it for a few minutes otherwise destroy and wait for the main thread call the start sim module function*/

void on_ip_lost(void)
{
    uint8_t retries = 0;
    pppos_retrying_connection = true;
    do
    {
        retries++;
        vTaskDelay(pdMS_TO_TICKS(1000 * retries));
        if (pppos_is_connected())
        {
            ESP_LOGI(TAG, "reconnected!");
            pppos_retrying_connection = false;
            return;
        }
        ESP_LOGI(TAG, "retrying to reconnect after lost the ip %d / %d", retries, SIM_NETWORK_RETRIES_NUM);
    } while (retries < SIM_NETWORK_RETRIES_NUM);

    ESP_LOGW(TAG, "setting the state DEACTIVATED in the main thread");
    pppos_retrying_connection = false;
    pppos_connected = false;
    set_network_disconnected(false);
    ESP_LOGI(TAG, "sim modem destroyed successfully and the main thread now holds the DEACTIVATED status flag");
    destroy_sim_network_module(sim_mod_dce, ppp_netif);
};

static void on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %" PRId32, event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP)
    {
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
    }
    else if (event_id == IP_EVENT_PPP_LOST_IP)
    {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        xEventGroupSetBits(event_group, DISCONNECT_BIT);
        if (!pppos_is_retrying_to_connect())
        {
            pppos_connected = false;
            on_ip_lost();
        }
    }
    else if (event_id == IP_EVENT_GOT_IP6)
    {
        ESP_LOGI(TAG, "GOT IPv6 event!");
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        xEventGroupSetBits(event_group, CONNECT_BIT);
        pppos_connected = true;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // To more information about the PPP failure events check: https://github.com/espressif/esp-idf/blob/master/components/esp_netif/include/esp_netif_ppp.h
    ESP_LOGI(TAG, "PPP state changed event %"PRIu32, event_id);
    if (event_id != 0)
    {
        if (event_id == NETIF_PPP_ERRORUSER)
        {
            esp_netif_t *p_netif = *(esp_netif_t **)event_data;
            ESP_LOGW(TAG, "User interrupted event from netif: %p", p_netif);
            return;
        }
        else if (event_id == NETIF_PPP_ERRORCONNECT)
        {
            ESP_LOGE(TAG, "Error ~ connection lost: NETIF_PPP_ERRORCONNECT");
        }
        else
        {
            ESP_LOGE(TAG, "A specific NETIF_PPP error happened, please check (esp_netif_ppp_status_event_t) -> ID: %"PRIu32, event_id);
        }
        xEventGroupSetBits(event_group, DISCONNECT_BIT);
        return;
    }
    ESP_LOGI(TAG, "NETIF_PPP sucessfully connected!");
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

/* Configure the PPP netif */
esp_err_t dce_init(esp_modem_dce_t **dce, esp_netif_t **netif)
{
    // DTE configuration
    esp_modem_dte_config_t dte_config = {
        .dte_buffer_size = 512,
        .task_stack_size = 4096,
        .task_priority = 5,
        .uart_config = {
            .port_num = PPP_UART_NUM,
            .data_bits = UART_DATA_8_BITS,
            .stop_bits = UART_STOP_BITS_1,
            .parity = UART_PARITY_DISABLE,
            .flow_control = ESP_MODEM_FLOW_CONTROL_NONE,
            .source_clk = UART_SCLK_APB,
            .baud_rate = 115200,
            .tx_io_num = PPP_TX_PIN,
            .rx_io_num = PPP_RX_PIN,
            //.rts_io_num = 27, // RTS (Request to Send) - Not used since it is for flow control
            //.cts_io_num = 23, // CTS (Clear to Send) - Not used since it is for flow control
            .rx_buffer_size = 4096,
            .tx_buffer_size = 512,
            .event_queue_size = 30,
        },
    };

    // DCE configuration - The APN name depends of the company that provides the service.
    esp_modem_dce_config_t dce_config = {
        .apn = CONFIG_APN,
    };
    ESP_LOGI(TAG, "APN set: %s", dce_config.apn);
    ESP_LOGI(TAG, "Initializing esp_modem for a generic module...");
    *dce = esp_modem_new(&dte_config, &dce_config, *netif);
    // *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, *netif);

    // TODO: check this if statement if is needed due to the implementation of set_pin
    //  check pin configuration
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
    ESP_LOGI(TAG, "modem pointer: %p - %p", *dce, *netif);
    return *dce ? ESP_OK : ESP_FAIL;
}

/*----------------------------------------------------------------
----------------------- OTHER STUFF LOW LVL ----------------------
----------------------------------------------------------------*/

// BARE AT COMMANDS
// TODO: remove this define and implement a global error check function in utils instead.
#define CHECK_ERR(cmd, success_action)                                                     \
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

bool check_registration_status(const char *data)
{
    const char *found = strstr(data, ",5");
    return found != NULL;
}

void run_at(esp_modem_dce_t *dce, uint8_t count)
{
    for (int i = 0; i < count; i++)
    {
        CHECK_ERR(esp_modem_sync(dce), ESP_LOGI(TAG, "OK"));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void run_at_get_info(esp_modem_dce_t *dce)
{
    char data[BUF_SIZE];
    int rssi, ber;
    CHECK_ERR(esp_modem_get_signal_quality(dce, &rssi, &ber), ESP_LOGI(TAG, "OK. rssi=%d, ber=%d", rssi, ber));
    CHECK_ERR(esp_modem_at(dce, "AT+CPSMS?", data, 500), ESP_LOGI(TAG, "OK. %s", data));    // Check Power Saving Modeching
    CHECK_ERR(esp_modem_at(dce, "AT+CNMP?", data, 500), ESP_LOGI(TAG, "OK. %s", data));     // Check preference mode
    CHECK_ERR(esp_modem_at(dce, "AT+CPSI?", data, 500), ESP_LOGI(TAG, "OK. %s", data));     // Inquiring UE system information
    CHECK_ERR(esp_modem_at(dce, "AT+CGDCONT?", data, 500), ESP_LOGI(TAG, "OK. %s", data));  // Check APN set
    CHECK_ERR(esp_modem_at(dce, "AT+CPIN?", data, 500), ESP_LOGI(TAG, "OK. %s", data));     // Check SIM card ready
    CHECK_ERR(esp_modem_at(dce, "AT+CFUN?", data, 500), ESP_LOGI(TAG, "OK. %s", data));     // Check functionality
    CHECK_ERR(esp_modem_at(dce, "AT+COPS?", data, 500), ESP_LOGI(TAG, "OK. %s", data));     // Check the connections available
    CHECK_ERR(esp_modem_at(dce, "AT+CEREG?", data, 500), ESP_LOGI(TAG, "OK. %s", data));    // Check if it is registered 1
    CHECK_ERR(esp_modem_at(dce, "AT+CGREG?", data, 500), ESP_LOGI(TAG, "OK. %s", data));    // Check if it is registered 2
}

void run_at_start_gnss(esp_modem_dce_t *dce, int current_power_mode){
    char data[BUF_SIZE];

    if (current_power_mode == 0) esp_modem_set_gnss_power_mode(sim_mod_dce, 1);

    /* TODO: Wrap this part in a function with a config parameter of the GNSS starting it cold, warm or hot. */

    // ---------------------------------------------------------------------------------------------------------------
    vTaskDelay(pdMS_TO_TICKS(5000));
    CHECK_ERR(esp_modem_at(dce, "AT+CGNSCOLD", data, 2000), ESP_LOGI(TAG, "OK. %s", data)); // Cold Start GNSS module
    vTaskDelay(pdMS_TO_TICKS(10000));
    // ---------------------------------------------------------------------------------------------------------------

    CHECK_ERR(esp_modem_at(dce, "AT+CGNSMOD?", data, 500), ESP_LOGI(TAG, "OK. %s", data));  // Check CGNS work mode set
    // TODO: Improve this loop and the waiting time and number of loops depends of the starting mode configured
    for (int i = 0; i<6; i ++) {
        CHECK_ERR(esp_modem_at(dce, "AT+CGNSINF", data, 500), ESP_LOGI(TAG, "OK. %s", data));   // Check if GPS info is ready for fetching
        //TODO: Create function that parses the GPS info set latitude and longitude and breaks the loop (stop criteria (lat and long !=0))
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    esp_modem_set_gnss_power_mode(sim_mod_dce, 0);
    CHECK_ERR(esp_modem_at(dce, "AT+CFUN?", data, 500), ESP_LOGI(TAG, "OK. %s", data)); 
    CHECK_ERR(esp_modem_at(dce, "AT+CFUN=1,1", data, 500), ESP_LOGI(TAG, "OK. %s", data)); 
    vTaskDelay(pdMS_TO_TICKS(15000));
}

/*----------------------------------------------------------------
------------------------- NETWORK MODEM --------------------------
----------------------------------------------------------------*/

void network_module_power(void)
{
    ESP_LOGI(TAG, "Network module power pin - pulling down!");
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

bool modem_check_sync(esp_modem_dce_t *dce)
{
    return esp_modem_sync(dce) == ESP_OK;
}

// Set the modem to data mode and no command can be passed due this state.
bool sim_module_start_network(esp_modem_dce_t *dce)
{
    return esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA) == ESP_OK;
}

// Set the modem to command mode allowing AT commands and the use of the esp modem api.
bool sim_module_stop_network(esp_modem_dce_t *dce)
{
    return esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND) == ESP_OK;
}

void sim_module_reset(esp_modem_dce_t *dce)
{   
    esp_err_t err = esp_modem_reset(dce);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "modem reset AT command error: %d", err);
        ESP_LOGD(TAG, "switching the power off/on to hard reset!");
        network_module_power();
        vTaskDelay(pdMS_TO_TICKS(3000));
        network_module_power();
    }
}

void sim_module_power_up(esp_modem_dce_t *dce)
{
    uint8_t tries = 0;
    while (tries++ < SIM_POWER_UP_TRIES)
    {
        if (modem_check_sync(dce))
        {
            return;
        }
        ESP_LOGI(TAG, "Error to sync with network module. Retries: %d / %d", tries, SIM_POWER_UP_TRIES);
        
        // Set sim to command mode since the problem can be that it is in data mode.
        if (tries == 1) sim_module_stop_network(dce); else vTaskDelay(pdMS_TO_TICKS(2000));
    }
    network_module_power();

    // Wait for network module to initialize (it takes a while)
    vTaskDelay(pdMS_TO_TICKS(5000));
}

esp_err_t destroy_sim_network_module(esp_modem_dce_t *dce, esp_netif_t *esp_netif)
{
    esp_err_t stop_sim_network_status;

    ESP_LOGW("Destroy SIM_NETWORK Module", "Destroying the SIM_NETWORK component u.u");
    esp_modem_destroy(dce);
    dce = NULL;
    stop_sim_network_status = esp_event_loop_delete_default();
    if (stop_sim_network_status != ESP_OK)
    {
        ESP_LOGE("Destroy SIM_NETWORK Module", "An error occured while deleting the event_loop.");
        return stop_sim_network_status;
    }
    esp_netif_destroy(esp_netif);
    esp_netif = NULL;
    return stop_sim_network_status;
}

esp_err_t check_signal_quality(esp_modem_dce_t *dce)
{
    const char *TAG_SIG_CHECK = "Modem Network Signal Quality Check";
    int rssi, ber = 0;
    esp_err_t ret = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_SIG_CHECK, "esp_modem_get_signal_quality failed with %d %s", ret, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG_SIG_CHECK, "Signal quality ~ rssi=%d, ber=%d", rssi, ber);
    return rssi != 99 && rssi > 5 ? ESP_OK : ESP_FAIL;
}

esp_err_t sim_network_connection_get_status(void)
{
    int retries = 0;
    int signal_ok = false;
    const char *TAG_CONN_STATUS = "Modem Network Connection Status";
    esp_err_t ret_check;
    do
    {
        ret_check = check_signal_quality(sim_mod_dce); // direct access to sim_mod_dce in this file
        if (ret_check != ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(1500 * retries));
            retries++;
            if (retries >= SIM_NETWORK_RETRIES_NUM)
            {
                ESP_LOGE(TAG_CONN_STATUS, "SIGNAL QUALITY CHECK: No signal was identified.");
                return ret_check;
            }
            ESP_LOGW(TAG_CONN_STATUS, "SIGNAL QUALITY CHECK: The signal quality check failed... Retrying (%d/%d)", retries, SIM_NETWORK_RETRIES_NUM);
        }
        else
        {
            signal_ok = true;
        }
    } while (!signal_ok);

    return ret_check;
}

bool start_network(esp_modem_dce_t *dce)
{
    uint8_t retries = 0;
    EventBits_t bits = 0;
    while ((bits & CONNECT_BIT) == 0 && retries < START_NETWORK_RETRIES_NUM)
    {
        retries++;
        if (!modem_check_sync(dce))
        {
            ESP_LOGE(TAG, "Modem does not respond... trying to exit from network data mode");
            if (!sim_module_stop_network(dce) || !modem_check_sync(dce))
            {
                ESP_LOGE(TAG, "Modem does not respond to AT anyway... restarting");
                sim_module_reset(dce);
                ESP_LOGI(TAG, "Restarted");
                continue;
            }
        }
        if (sim_network_connection_get_status() != ESP_OK)
        {
            ESP_LOGI(TAG, "The signal is too weak to continue with the network connection...");
            continue;
        }
        if (!sim_module_start_network(dce))
        {
            ESP_LOGE(TAG, "Modem could not enter in the network mode ...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        bits = xEventGroupWaitBits(event_group, (DISCONNECT_BIT | CONNECT_BIT), pdTRUE, pdFALSE, pdMS_TO_TICKS(30000));
        if (bits & DISCONNECT_BIT)
        {
            ESP_LOGE(TAG, "Modem got disconnected ...retrying");
            sim_module_stop_network(dce);
            continue;
        }
        return true;
    }
    return false;
}

esp_err_t start_sim_network_module(bool gnss_enabled)
{
    esp_err_t ret_check;
    char imsi[MAX_ESP_INFO_SIZE] = {0};
    char imei[MAX_ESP_INFO_SIZE] = {0};
    char operator_name[MAX_ESP_INFO_SIZE] = {0};
    char module_name[MAX_ESP_INFO_SIZE] = {0};
    int operator_act = 0;
    int gnss_power_mode = 0;

    // Initialize esp_netif and default event loop
    ret_check = esp_netif_init(); // initiates network interface
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG, "NETIF - Initialization: netif init failed with %d", ret_check);
        return ret_check;
    }
    ret_check = esp_event_loop_create_default(); // dispatch events loop callback
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG, "EVENT LOOP - Creation: Event Loop creation failed with %d", ret_check);
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
        ESP_LOGE(TAG, "NETWORK INITIALIZATION - Register: IP event handler registering failed with %d", ret_check);
        return ret_check;
    }

    ret_check = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event, NULL);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG, "IP EVENT HANDLER - Register: IP event handler registering failed with %d", ret_check);
        return ret_check;
    }

    ret_check = esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG, "PPP EVENT HANDLER - Register: Set Pin procedure failed with %d... Stopping SIM_NETWORK Module", ret_check);
        return ret_check;
    }

    ESP_LOGI(TAG, "Powering up and starting the sync with the SIM network module ...");
    sim_module_power_up(sim_mod_dce);

    esp_modem_get_imsi(sim_mod_dce, imsi);
    esp_modem_get_imei(sim_mod_dce, imei);
    esp_modem_get_operator_name(sim_mod_dce, operator_name, &operator_act);
    esp_modem_get_module_name(sim_mod_dce, module_name);
    esp_modem_get_gnss_power_mode(sim_mod_dce, &gnss_power_mode);
    ESP_LOGI(TAG, "Module: %s", module_name);
    ESP_LOGI(TAG, "Operator: %s %d", operator_name, operator_act);
    ESP_LOGI(TAG, "IMEI: %s", imei);
    ESP_LOGI(TAG, "IMSI: %s", imsi);
    ESP_LOGI(TAG, "GNSS Power Mode: %d", gnss_power_mode);

    esp_modem_set_apn(sim_mod_dce, CONFIG_APN);

    // Try to sync (extra checks) and get info from the SIM Network Module
    run_at(sim_mod_dce, 3);
    run_at_get_info(sim_mod_dce);

    if (gnss_enabled) run_at_start_gnss(sim_mod_dce, gnss_power_mode);
    
    if (!start_network(sim_mod_dce))
    {
        ESP_LOGW(TAG, "Data mode start failed. Destroying the SIM_NETWORK module.");
        if (destroy_sim_network_module(sim_mod_dce, ppp_netif) != ESP_OK)
        {
            ESP_LOGE(TAG, "Error destroying the SIM_NETWORK module.");
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

/*----------------------------------------------------------------
------------------------- PPOS/SIM INFO --------------------------
----------------------------------------------------------------*/

bool pppos_is_connected(void)
{
    return pppos_connected;
}

bool pppos_is_retrying_to_connect(void)
{
    return pppos_retrying_connection;
}