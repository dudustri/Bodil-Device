#define INTERNAL_GSM
#include "gsm.h"

// PPPoS Configuration
#define PPP_UART_NUM UART_NUM_2
#define PPP_TX_PIN GPIO_NUM_17
#define PPP_RX_PIN GPIO_NUM_16

// Set user APN details
#define CONFIG_APN "user_apn"
#define CONFIG_APN_PIN "12345678"

esp_modem_dce_t *gsm_modem = NULL;
esp_netif_t *esp_netif = NULL;

const esp_modem_dte_config_t dte_config = {
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

// The APN name depends of the company that provides the service.
const esp_modem_dce_config_t dce_config = {
    .apn = CONFIG_APN,
};

const esp_netif_t *netif = NULL;

int set_pin(esp_modem_dce_t *dce, const char *pin)
{

    bool status_pin = false;
    if (esp_modem_read_pin(dce, &status_pin) == ESP_OK && status_pin == false)
    {
        if (esp_modem_set_pin(dce, pin) == ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            ESP_LOGE("SET PIN GSM MODULE", "Failed to set pin in the gsm module.");
            return 1;
        }
    }
    return 0;
}

void destroy_gsm_module(esp_modem_dce_t *dce, esp_netif_t *esp_netif)
{
    esp_modem_destroy(dce);
    esp_netif_destroy(esp_netif);
}

void check_signal_quality(esp_modem_dce_t *dce)
{
    int rssi, ber;
    esp_err_t err = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (err != ESP_OK)
    {
        ESP_LOGE("SIGNAL QUALITY - ESP MODEM", "esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
        return;
    }
    ESP_LOGI("SIGNAL QUALITY - ESP MODEM", "Signal quality: rssi=%d, ber=%d", rssi, ber);
}

static void on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGD("IP EVENT - ESP MODEM", "IP event! %" PRIu32, event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP)
    {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI("IP EVENT - ESP MODEM", "Modem Connect to PPP Server");
        ESP_LOGI("IP EVENT - ESP MODEM", "~~~~~~~~~~~~~~");
        ESP_LOGI("IP EVENT - ESP MODEM", "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI("IP EVENT - ESP MODEM", "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI("IP EVENT - ESP MODEM", "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI("IP EVENT - ESP MODEM", "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI("IP EVENT - ESP MODEM", "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI("IP EVENT - ESP MODEM", "~~~~~~~~~~~~~~");
        // xEventGroupSetBits(event_group, CONNECT_BIT);

        ESP_LOGI("IP EVENT - ESP MODEM", "GOT ip event!!!");
    }
    else if (event_id == IP_EVENT_PPP_LOST_IP)
    {
        ESP_LOGI("IP EVENT - ESP MODEM", "Modem Disconnect from PPP Server");
    }
    else if (event_id == IP_EVENT_GOT_IP6)
    {
        ESP_LOGI("IP EVENT - ESP MODEM", "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI("IP EVENT - ESP MODEM", "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI("PPP - ESP MODEM", "PPP state changed event %" PRIu32, event_id);
    if (event_id == NETIF_PPP_ERRORUSER)
    {
        /* User interrupted event from esp-netif */
        esp_netif_t **p_netif = event_data;
        ESP_LOGI("PPP - ESP MODEM", "User interrupted event from netif:%p", *p_netif);
    }
}

esp_err_t start_gsm_module(void)
{

    esp_err_t ret_check;

    ret_check = esp_netif_init(); // initiates network interface
    if (ret_check != ESP_OK)
    {
        ESP_LOGE("NETIF - Initialization", "netif init failed with %d", ret_check);
        return ret_check;
    }
    ret_check = esp_event_loop_create_default(); // dispatch events loop callback
    if (ret_check != ESP_OK)
    {
        ESP_LOGE("EVENT LOOP - Creation", "Event Loop creation failed with %d", ret_check);
        return ret_check;
    }
    ret_check = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE("IP EVENT HANDLER - Register", "IP event handler registering failed with %d", ret_check);
        return ret_check;
    }
    ret_check = esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE("PPP EVENT HANDLER - Register", "PPP event handler registering failed with %d", ret_check);
        return ret_check;
    }

    /* Configure the PPP netif */
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    // should return a pointer of the modem module
    gsm_modem = esp_modem_new_dev(ESP_MODEM_DCE_SIM800, &dte_config, &dce_config, esp_netif);

    // check pin configuration
    if (!set_pin(gsm_modem, CONFIG_APN_PIN))
    {
        destroy_gsm_module(gsm_modem, esp_netif);
        return ret_check;
    }

    // check signal quality
    check_signal_quality(gsm_modem);

    // set the GSM module to data mode
    ret_check = esp_modem_set_mode(gsm_modem, ESP_MODEM_MODE_DATA);
    if (ret_check != ESP_OK)
    {
        ESP_LOGE("SET DATA MODE - ESP MODEM", "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", ret_check);
        destroy_gsm_module(gsm_modem, esp_netif);
        return ret_check;
    }

    return ESP_OK;
}