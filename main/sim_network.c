#define INTERNAL_SIM_NETWORK
#define AT_COMMANDS_HANDLER
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
#define START_NETWORK_RETRIES_NUM 2
#define SIM_POWER_UP_TRIES 3
#define BUF_SIZE 128
#define MAX_ESP_INFO_SIZE 32

const char *TAG_SIM = "Modem SIM Network";
const char *CONFIG_APN = APN;
const char *CONFIG_APN_PIN = APN_PIN;
static esp_modem_dce_t *sim_mod_dce = NULL;
static esp_netif_t *ppp_netif = NULL;
static volatile bool pppos_connected = false;
static volatile bool pppos_trying_connection = false;
// static EventGroupHandle_t module_connection = NULL;
// static const int CONNECT_BIT = BIT0;
// static const int DISCONNECT_BIT = BIT1;

ESP_EVENT_DEFINE_BASE(SIM_NETWORK_FAILURE_EVENT_BASE);

/*----------------------------------------------------------------
------------------------- EVENT HANDLERS -------------------------
----------------------------------------------------------------*/
void deactivate_sim_module(void)
{
    ESP_LOGW(TAG_SIM, "setting the state DEACTIVATED in the main thread");
    pppos_trying_connection = false;
    pppos_connected = false;
    destroy_sim_network_module(sim_mod_dce, ppp_netif);
    set_network_disconnected(false);
    ESP_LOGI(TAG_SIM, "sim modem destroyed successfully and the main thread now holds the DEACTIVATED status flag");
}

static void on_network_failure(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGW(TAG_SIM, "On network failure handler triggered!");
    if (pppos_connected)
    {
        int retries = 0;
        pppos_connected = false;
        pppos_trying_connection = true;
        if (sim_module_stop_network(sim_mod_dce))
        {
            if (sim_network_connection_get_status() == ESP_OK)
            {
                while (!pppos_connected && retries < SIM_NETWORK_RETRIES_NUM)
                {
                    vTaskDelay(pdMS_TO_TICKS(500 * retries));
                    retries++;
                }
                if (sim_module_start_network(sim_mod_dce))
                {
                    return;
                }
            }
        }
        pppos_trying_connection = false;
        deactivate_sim_module();
        return;
    }
    return;
}

void on_ip_lost(void)
{
    uint8_t retries = 0;
    do
    {
        retries++;
        vTaskDelay(pdMS_TO_TICKS(500 * retries));
        if (pppos_is_connected())
        {
            ESP_LOGI(TAG_SIM, "reconnected!");
            return;
        }
        ESP_LOGI(TAG_SIM, "retrying to reconnect after lost the ip %d / %d", retries, SIM_NETWORK_RETRIES_NUM);
    } while (retries < SIM_NETWORK_RETRIES_NUM);

    deactivate_sim_module();
};

static void on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG_SIM, "IP event! %" PRId32, event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP)
    {
        esp_netif_dns_info_t dns_info;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG_SIM, "Modem Connect to PPP Server");
        ESP_LOGI(TAG_SIM, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG_SIM, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG_SIM, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG_SIM, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG_SIM, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG_SIM, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG_SIM, "~~~~~~~~~~~~~~");
        // xEventGroupSetBits(module_connection, CONNECT_BIT);
        // xEventGroupClearBits(module_connection, DISCONNECT_BIT);
        pppos_connected = true;
        pppos_trying_connection = false;
        ESP_LOGI(TAG_SIM, "GOT ip event!!!");
    }
    else if (event_id == IP_EVENT_PPP_LOST_IP)
    {
        ESP_LOGI(TAG_SIM, "Modem Disconnect from PPP Server");
        // xEventGroupSetBits(module_connection, DISCONNECT_BIT);
        // xEventGroupClearBits(module_connection, CONNECT_BIT);
        if (!pppos_trying_connection)
        {
            pppos_connected = false;
            pppos_trying_connection = true;
            on_ip_lost();
        }
    }
    else if (event_id == IP_EVENT_GOT_IP6)
    {
        ESP_LOGI(TAG_SIM, "GOT IPv6 event!");
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        // xEventGroupSetBits(module_connection, CONNECT_BIT);
        // xEventGroupClearBits(module_connection, DISCONNECT_BIT);
        pppos_connected = true;
        pppos_trying_connection = false;
        ESP_LOGI(TAG_SIM, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // To more information about the PPP failure events check: https://github.com/espressif/esp-idf/blob/master/components/esp_netif/include/esp_netif_ppp.h
    ESP_LOGI(TAG_SIM, "PPP state changed event %" PRIu32, event_id);
    if (event_id != 0)
    {
        if (event_id == NETIF_PPP_ERRORUSER)
        {
            esp_netif_t *p_netif = *(esp_netif_t **)event_data;
            ESP_LOGW(TAG_SIM, "User interrupted event from netif: %p", p_netif);
            return;
        }
        else if (event_id == NETIF_PPP_ERRORCONNECT)
        {
            ESP_LOGE(TAG_SIM, "Error ~ connection lost: NETIF_PPP_ERRORCONNECT");
        }
        else
        {
            ESP_LOGE(TAG_SIM, "A specific NETIF_PPP error happened, please check (esp_netif_ppp_status_event_t) -> ID: %" PRIu32, event_id);
        }
        // xEventGroupSetBits(module_connection, DISCONNECT_BIT);
        // xEventGroupClearBits(module_connection, CONNECT_BIT);
        pppos_connected = true;
        return;
    }
    ESP_LOGI(TAG_SIM, "NETIF_PPP sucessfully connected!");
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
    ESP_LOGI(TAG_SIM, "APN set: %s", dce_config.apn);
    ESP_LOGI(TAG_SIM, "Initializing esp_modem for a generic module...");
    *dce = esp_modem_new(&dte_config, &dce_config, *netif);
    // *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, *netif);

    ESP_LOGI(TAG_SIM, "modem pointer: %p - %p", *dce, *netif);
    return *dce ? ESP_OK : ESP_FAIL;
}

/*----------------------------------------------------------------
------------------ BARE AT & MODEM API COMMANDS ------------------
----------------------------------------------------------------*/

void get_module_connection_info(esp_modem_dce_t *dce)
{
    char data[BUF_SIZE];
    int rssi, ber;
    int signal_info_size = 2;
    const char *rssi_name = "rssi";
    const char *ber_name = "ber";
    void *signal_data[] = {&signal_info_size, (void *)rssi_name, &rssi, (void *)ber_name, &ber};

    handle_at_command_log(esp_modem_get_signal_quality(dce, &rssi, &ber), success_action_int, signal_data);
    handle_at_command_log(esp_modem_at(dce, "AT+CPSMS?", data, 500), success_action_data_buffer, data);   // Check Power Saving Modeching
    handle_at_command_log(esp_modem_at(dce, "AT+CNMP?", data, 500), success_action_data_buffer, data);    // Check preference mode
    handle_at_command_log(esp_modem_at(dce, "AT+CPSI?", data, 500), success_action_data_buffer, data);    // Inquiring UE system information
    handle_at_command_log(esp_modem_at(dce, "AT+CGDCONT?", data, 500), success_action_data_buffer, data); // Check APN set
    handle_at_command_log(esp_modem_at(dce, "AT+CPIN?", data, 500), success_action_data_buffer, data);    // Check SIM card ready
    handle_at_command_log(esp_modem_at(dce, "AT+CFUN?", data, 500), success_action_data_buffer, data);    // Check functionality
    handle_at_command_log(esp_modem_at(dce, "AT+COPS?", data, 500), success_action_data_buffer, data);    // Check the connections available
    handle_at_command_log(esp_modem_at(dce, "AT+CEREG?", data, 500), success_action_data_buffer, data);   // Check if it is registered 1
    handle_at_command_log(esp_modem_at(dce, "AT+CGREG?", data, 500), success_action_data_buffer, data);   // Check if it is registered 2
}

esp_err_t turn_off_gnss(esp_modem_dce_t *dce, int *gnss_power_mode)
{

    char data[BUF_SIZE];
    esp_err_t response = esp_modem_set_gnss_power_mode(sim_mod_dce, 0);

    if (response == ESP_OK && esp_modem_get_gnss_power_mode(dce, gnss_power_mode) == ESP_OK)
    {
        handle_at_command_log(esp_modem_at(dce, "AT+CFUN?", data, 500), success_action_data_buffer, data);
        handle_at_command_log(esp_modem_at(dce, "AT+CFUN=1,1", data, 500), success_action_data_buffer, data);
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
    else
    {
        ESP_LOGE(TAG_SIM, "Error powering off the GNSS module. Error: %d", response);
    }
    return response;
}

void get_gnss_initial_data(esp_modem_dce_t *dce, int *gnss_power_mode)
{
    char data[BUF_SIZE];

    if (*gnss_power_mode == 0)
        esp_modem_set_gnss_power_mode(sim_mod_dce, 1);

    /* TODO: Wrap this part in a function with a config parameter of the GNSS starting it cold, warm or hot. */
    // ---------------------------------------------------------------------------------------------------------------
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG_SIM, "Starting GNSS in Cold Mode.");
    handle_at_command_log(esp_modem_at(dce, "AT+CGNSCOLD", data, 2000), success_action_data_buffer, data); // Cold Start GNSS module
    vTaskDelay(pdMS_TO_TICKS(10000));
    // ---------------------------------------------------------------------------------------------------------------

    handle_at_command_log(esp_modem_at(dce, "AT+CGNSMOD?", data, 500), success_action_data_buffer, data); // Check CGNS work mode set
    // TODO: Improve this loop and the waiting time and number of loops depends of the starting mode configured
    for (int i = 0; i < 20; i++)
    {
        handle_at_command_log(esp_modem_at(dce, "AT+CGNSINF", data, 500), success_action_data_buffer, data); // Check if GPS info is ready for fetching
        // TODO: Create function that parses the GPS info set latitude and longitude and breaks the loop (stop criteria (lat and long !=0))
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    turn_off_gnss(dce, gnss_power_mode);
}

void get_basic_module_info(esp_modem_dce_t *dce, int *gnss_power_mode)
{
    char imsi[MAX_ESP_INFO_SIZE] = {0};
    char imei[MAX_ESP_INFO_SIZE] = {0};
    char operator_name[MAX_ESP_INFO_SIZE] = {0};
    char module_name[MAX_ESP_INFO_SIZE] = {0};
    int operator_act = 0;

    int info_size = 2;
    const char *gnss_pm_log = "GNSS Power Mode";
    const char *imsi_log = "IMSI";
    const char *imei_log = "IMEI";
    const char *module_name_log = "Module";
    const char *operator_name_log = "Operator";
    const char *operator_act_log = "Operator Act";
    uint8_t operator_bit_mask = 0x02; // binary -> 0b00000010
    void *log_args_data[] = {&info_size, &operator_bit_mask, (void *)operator_name_log, operator_name, (void *)operator_act_log, &operator_act};

    handle_at_command_log(esp_modem_get_operator_name(dce, operator_name, &operator_act), success_action_mix_by_mask, log_args_data);

    info_size = 1;
    log_args_data[1] = (void *)imsi_log;
    log_args_data[2] = (void *)imsi;

    handle_at_command_log(esp_modem_get_imsi(dce, imsi), success_action_raw_char, log_args_data);

    log_args_data[1] = (void *)imei_log;
    log_args_data[2] = (void *)imei;

    handle_at_command_log(esp_modem_get_imei(dce, imei), success_action_raw_char, log_args_data);

    log_args_data[1] = (void *)module_name_log;
    log_args_data[2] = (void *)module_name;

    handle_at_command_log(esp_modem_get_module_name(dce, module_name), success_action_raw_char, log_args_data);

    log_args_data[1] = (void *)gnss_pm_log;
    log_args_data[2] = gnss_power_mode;

    handle_at_command_log(esp_modem_get_gnss_power_mode(dce, gnss_power_mode), success_action_int, log_args_data);
}

/*----------------------------------------------------------------
------------------------- NETWORK MODEM --------------------------
----------------------------------------------------------------*/

bool modem_check_sync(esp_modem_dce_t *dce)
{
    return esp_modem_sync(dce) == ESP_OK;
}

bool synchronize_module(esp_modem_dce_t *dce, uint8_t count)
{
    for (int i = 0; i < count; i++)
    {
        if (modem_check_sync(dce))
        {
            ESP_LOGI(TAG_SIM, "OK - Module Synchronized!");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
}

void network_module_power(void)
{
    ESP_LOGI(TAG_SIM, "Network module power pin - pulling down!");
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
        ESP_LOGE(TAG_SIM, "modem reset AT command error: %d", err);
        ESP_LOGD(TAG_SIM, "switching the power off/on to hard reset!");
        network_module_power();
        vTaskDelay(pdMS_TO_TICKS(3000));
        network_module_power();
    }
}

esp_err_t sim_module_power_up(esp_modem_dce_t *dce)
{
    uint8_t tries = 0;

    while (tries++ < SIM_POWER_UP_TRIES)
    {
        if (synchronize_module(dce, 1))
        {
            return ESP_OK;
        }
        ESP_LOGI(TAG_SIM, "Error to sync with network module. Retries: %d / %d", tries, SIM_POWER_UP_TRIES);

        // Set sim to command mode since the problem can be that it is in data mode.
        if (tries == 1)
            sim_module_stop_network(dce);
        else
            vTaskDelay(pdMS_TO_TICKS(2000));
    }
    network_module_power();

    // Wait for network module to initialize (it takes a while)
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Try to sync and get info from the SIM Network Module
    return synchronize_module(sim_mod_dce, 5) ? ESP_OK : ESP_FAIL;
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
    const char *TAG_SIM_SIG_CHECK = "Modem Network Signal Quality Check";
    int rssi, ber = 0;
    esp_err_t ret = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_SIM_SIG_CHECK, "esp_modem_get_signal_quality failed with %d %s", ret, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG_SIM_SIG_CHECK, "Signal quality ~ rssi=%d, ber=%d", rssi, ber);
    return rssi != 99 && rssi > 5 ? ESP_OK : ESP_FAIL;
}

esp_err_t sim_network_connection_get_status(void)
{
    int retries = 0;
    int signal_ok = false;
    const char *TAG_SIM_CONN_STATUS = "Modem Network Connection Status";
    esp_err_t ret_check;
    do
    {
        ret_check = check_signal_quality(sim_mod_dce); // direct access to sim_mod_dce in this file
        if (ret_check != ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(1500 * retries));
            retries++;
            if (retries > SIM_NETWORK_RETRIES_NUM)
            {
                ESP_LOGE(TAG_SIM_CONN_STATUS, "SIGNAL QUALITY CHECK: No signal was identified.");
                return ret_check;
            }
            ESP_LOGW(TAG_SIM_CONN_STATUS, "SIGNAL QUALITY CHECK: The signal quality check failed... Retrying (%d/%d)", retries, SIM_NETWORK_RETRIES_NUM);
        }
        else
        {
            signal_ok = true;
        }
    } while (!signal_ok);

    return ret_check;
}

bool start_network(esp_modem_dce_t *dce, int *gnss_power_mode)
{
    uint8_t retries = 0;
    // EventBits_t bits = 0;
    pppos_trying_connection = true;
    while (/*(bits & CONNECT_BIT) == 0*/ !pppos_connected && retries < START_NETWORK_RETRIES_NUM)
    {
        retries++;
        if (!modem_check_sync(dce))
        {
            ESP_LOGE(TAG_SIM, "Modem does not respond... trying to exit from network data mode");
            if (!sim_module_stop_network(dce) || !modem_check_sync(dce))
            {
                ESP_LOGE(TAG_SIM, "Modem does not respond to AT anyway... restarting");
                sim_module_reset(dce);
                ESP_LOGI(TAG_SIM, "Restarted");
                continue;
            }
        }
        if (sim_network_connection_get_status() != ESP_OK)
        {
            ESP_LOGI(TAG_SIM, "The signal is too weak to continue with the network connection...");
            continue;
        }
        if (*gnss_power_mode != 0)
        {
            if (turn_off_gnss(dce, gnss_power_mode) != ESP_OK)
            {
                continue;
            }
        }
        if (!sim_module_start_network(dce))
        {
            ESP_LOGE(TAG_SIM, "Modem could not enter in the network mode ...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        // bits = xEventGroupWaitBits(module_connection, (DISCONNECT_BIT | CONNECT_BIT), pdTRUE, pdFALSE, pdMS_TO_TICKS(30000));
        if (/*bits & DISCONNECT_BIT ||*/ !pppos_connected)
        {
            ESP_LOGE(TAG_SIM, "Modem got disconnected ...");
            sim_module_stop_network(dce);
            continue;
        }
        pppos_trying_connection = false;
        return true;
    }
    pppos_trying_connection = false;
    return false;
}

esp_err_t start_sim_network_module(bool gnss_enabled)
{
    esp_err_t ret_check;
    int gnss_power_mode = 0;

    // Initialize esp_netif and default event loop
    ret_check = esp_netif_init(); // initiates network interface
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_SIM, "NETIF - Initialization: netif init failed with %d", ret_check);
        return ret_check;
    }
    ret_check = esp_event_loop_create_default(); // dispatch events loop callback
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_SIM, "EVENT LOOP - Creation: Event Loop creation failed with %d", ret_check);
        return ret_check;
    }
    // module_connection = xEventGroupCreate();

    // Initialize lwip network interface in PPP mode
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    ppp_netif = esp_netif_new(&netif_ppp_config);

    // Initialize the PPP network and register for IP event
    ret_check = dce_init(&sim_mod_dce, &ppp_netif);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_SIM, "DCE Initialization Failed... (%d)", ret_check);
        return ret_check;
    }

    ret_check = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event, NULL);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_SIM, "ip Event Handler - Register: event handler registering failed with %d", ret_check);
        return ret_check;
    }

    ret_check = esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_SIM, "PPP Event Handler - Register: event handler registering failed with %d", ret_check);
        return ret_check;
    }

    ret_check = esp_event_handler_register(SIM_NETWORK_FAILURE_EVENT_BASE, ESP_EVENT_ANY_ID, &on_network_failure, NULL);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE(TAG_SIM, "SIM Network Failure Event Handler - Register: event handler registering failed with %d", ret_check);
        return ret_check;
    }

    // Energize the module
    ESP_LOGI(TAG_SIM, "Powering up and starting the sync with the SIM network module ...");
    if (sim_module_power_up(sim_mod_dce) != ESP_OK)
    {
        ESP_LOGE(TAG_SIM, "Power up procedure failed.");
        if (destroy_sim_network_module(sim_mod_dce, ppp_netif) != ESP_OK)
        {
            ESP_LOGE(TAG_SIM, "Error destroying the SIM_NETWORK module.");
        }
        return ESP_FAIL;
    }
    ESP_LOGI(TAG_SIM, "Module energized and syncronized successfully.");

    // Retrieve and display module information
    get_basic_module_info(sim_mod_dce, &gnss_power_mode);
    get_module_connection_info(sim_mod_dce);

    // Check pin configuration
    if (strlen(CONFIG_APN_PIN) != 0 && sim_mod_dce)
    {
        esp_err_t err;
        ESP_LOGW(TAG_SIM, "SET PIN - CHECK: Entering in the set pin configuration...");
        err = set_pin(sim_mod_dce, CONFIG_APN_PIN);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG_SIM, "SET PIN - CHECK: Not chipset or pin config was identified. Destroying SIM_NETWORK module...");
            if (destroy_sim_network_module(sim_mod_dce, ppp_netif) != ESP_OK)
            {
                ESP_LOGE(TAG_SIM, "SET PIN - CHECK: Error destroying the SIM_NETWORK module.");
            }
            return err;
        }
    }

    // Start gnss data to retrieve location information
    if (gnss_enabled)
    {
        get_gnss_initial_data(sim_mod_dce, &gnss_power_mode);
    }

    // Start network inspecting operational data and setting the modem to data mode
    if (!start_network(sim_mod_dce, &gnss_power_mode))
    {
        ESP_LOGE(TAG_SIM, "Data mode start failed. Destroying the SIM_NETWORK module.");
        if (destroy_sim_network_module(sim_mod_dce, ppp_netif) != ESP_OK)
        {
            ESP_LOGE(TAG_SIM, "Error destroying the SIM_NETWORK module.");
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
    return pppos_trying_connection;
}