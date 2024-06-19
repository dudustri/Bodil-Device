#define INTERNAL_BLUETOOTH
#include "bluetooth.h"

static char cached_device_info_buffer[INFO_BUFFER_SIZE];
uint8_t ble_address_type;

// BLE Read Control Variables
int info_size = 0;
int ble_read_package_numbers = 0;
int current_package_sent = 0;

static bool device_read_execution = false;

// (static) Mutex for synchronizing access to the cached_device_info_buffer
SemaphoreHandle_t cache_mutex = NULL;
StaticSemaphore_t mutex_buffer_cache;

/* ----------------------------------------------------------------
------------------ Device Info Buffer Cache -----------------------*/

// TODO: Change the buffer info to json format!
void update_buffer(void)
{
    if (strlen(customer_info.name) == 0 || strlen(customer_info.ssid) == 0 || strlen(customer_info.pass) == 0 || strlen(customer_info.api_key) == 0)
    {
        return;
    }
    snprintf(cached_device_info_buffer, INFO_BUFFER_SIZE, "Device Information:\n Name: %s\n DeviceID: %d\n SSID: %s\n Password: %s\n API Key: %s\n",
             customer_info.name, customer_info.device_id, customer_info.ssid, customer_info.pass, customer_info.api_key);
 
    info_size = strlen(cached_device_info_buffer);
    ble_read_package_numbers = (int)(ceil((float)info_size/BYTES_PER_BLE_PACKET));
}

void initialize_buffer_cache(void)
{
    memset(cached_device_info_buffer, 0, INFO_BUFFER_SIZE);
    update_buffer();

    cache_mutex = xSemaphoreCreateMutexStatic(&mutex_buffer_cache);
    if (cache_mutex == NULL)
    {
        ESP_LOGE("Mutex", "Mutex creation failed...\n");
    }
}

/* ----------------------------------------------------------------
--------------------- Customer Info Update ------------------------*/

int ble_set_customer_info(const int key, const char *value, BodilCustomer *customer)
{
    // Function to set values in the customer_info structure based on key-value pairs
    switch (key)
    {
    case PASSWORD:
        snprintf(customer->pass, sizeof(customer->pass), "%s", value);
        break;
    case NAME:
        snprintf(customer->name, sizeof(customer->name), "%s", value);
        break;
    case SSID:
        snprintf(customer->ssid, sizeof(customer->ssid), "%s", value);
        break;
    default: // the key does not match with configuration
        return 0;
    }

    clear_blob_nvs("storage", "customer");
    save_to_nvs("storage", "customer", customer, sizeof(BodilCustomer));
    print_customer_info(customer);

    return 1;
}

/* ----------------------------------------------------------------
--------------------- Bluetooth Low Energy -------------------------*/

static int uuid_check(uint16_t uuid)
{
    ESP_LOGI("UUID CHECK FUNCTION", "%d", uuid);
    switch (uuid)
    {
    case GATT_WRITE_NAME_SERVICE_UUID:
        return NAME;
    case GATT_WRITE_SSID_SERVICE_UUID:
        return SSID;
    case GATT_WRITE_PASS_SERVICE_UUID:
        return PASSWORD;
    default:
        return UNKNOWN;
    }
}

void allow_info_ble_read_delay()
{
    vTaskDelay(1500 / portTICK_PERIOD_MS);
    device_read_execution = false;
    current_package_sent = 0;
    vTaskDelete(NULL);
}

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (!device_read_execution)
    {
        ESP_LOGI("READ GATT SERVICE", "Read requested by a bluetooth connection!");
        ESP_LOGI("DEVICE READ SERVICE", "Total info package: %d, Number of packets to be sent: %d", info_size, ble_read_package_numbers);
        xTaskCreate(&allow_info_ble_read_delay, "ble read info delay allowance", 700, NULL, 3, NULL);
    }

    if (current_package_sent <= ble_read_package_numbers && xSemaphoreTake(cache_mutex, (TickType_t)portMAX_DELAY) == pdTRUE)
    {
        ESP_LOGI("DEVICE READ SERVICE", "Sending the package (%d/%d)...", current_package_sent++, ble_read_package_numbers);
        os_mbuf_append(ctxt->om, cached_device_info_buffer, INFO_BUFFER_SIZE);
        xSemaphoreGive(cache_mutex);
    }
    device_read_execution = true;
    return 0;
}

// Write service callback to update a parameter in the customer info object
static int device_write_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t ble_message_buffer_size = ctxt->om->om_len;
    char data [ble_message_buffer_size + 1];
    memset(data, 0, INFO_BUFFER_SIZE);
    if (arg == NULL)
    {
        ESP_LOGE("WRITE GATT SERVICE", "NULL pointer argument passed unabling to recognize the service UUID\n");
        return 1;
    }
    ESP_LOGI("DEVICE WRITE HANDLER", "%d", (int)arg);
    uint16_t uuid = *((uint16_t *)arg);

    if (xSemaphoreTake(cache_mutex, (TickType_t)portMAX_DELAY) == pdTRUE)
    {

        // Copy data from om_data to the stack memory buffer
        memcpy(data, ctxt->om->om_data, ble_message_buffer_size);

        int user_set_type = uuid_check(uuid);

        if (user_set_type == UNKNOWN)
        {
            ESP_LOGI("WRITE GATT SERVICE", "UUID not recognized to set available parameters.");
            return 0;
        }

        ble_set_customer_info(user_set_type, data, &customer_info);
        ESP_LOGI("WRITE GATT SERVICE", "New %d set: %s\n", user_set_type, data);

        update_buffer();
        xSemaphoreGive(cache_mutex);
    }
    return 0;
}

// Service configs
uint16_t name_callback_argument = GATT_WRITE_NAME_SERVICE_UUID;
uint16_t ssid_callback_argument = GATT_WRITE_SSID_SERVICE_UUID;
uint16_t pass_callback_argument = GATT_WRITE_PASS_SERVICE_UUID;
// UUID - Universal Unique Identifier
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(GATT_READ_SERVICE_UUID),
          .flags = BLE_GATT_CHR_F_READ,
          .access_cb = device_read},
         {.uuid = BLE_UUID16_DECLARE(GATT_WRITE_NAME_SERVICE_UUID),
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write_handler,
          .arg = &name_callback_argument},
         {.uuid = BLE_UUID16_DECLARE(GATT_WRITE_SSID_SERVICE_UUID),
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write_handler,
          .arg = &ssid_callback_argument},
         {.uuid = BLE_UUID16_DECLARE(GATT_WRITE_PASS_SERVICE_UUID),
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write_handler,
          .arg = &pass_callback_argument},
         {0}}},
    {0}};

// Bluetooth event handler
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_advertisement();
        }
        else
        {
            initialize_buffer_cache();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECTED");
        ble_advertisement();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        ble_advertisement();
        break;
    default:
        break;
    }
    return 0;
}

void ble_advertisement(void)
{
    // GAP service name configuration
    int rc;
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    // Set the device type, flags
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    //  Set the manufacturer data
    static const uint8_t manufacturer_data[MANUFACTURER_DATA_SIZE] = {
        'B', 'o', 'd', 'i', 'l', ' ', 'E', 'n', 'e', 'r', 'g', 'y', ' ', 'A', 'p', 'S'};
    fields.mfg_data = manufacturer_data;
    fields.mfg_data_len = MANUFACTURER_DATA_SIZE;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE("BLE Advertisement", "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    // GAP device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    // undirected-connectable - allow other devices to connect to it
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    // general-discoverable - make sure it will be found by any BLE scanning device
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    // no white list for connecting devices
    adv_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
    // advertising interval in units of 0.625 ms
    adv_params.itvl_min = 160; // 100 ms
    adv_params.itvl_max = 800; // 500 ms
    // high duty cycle deactivated to save energy - no high connection performance is needed
    adv_params.high_duty_cycle = 0;

    rc = ble_gap_adv_start(ble_address_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);

    if (rc != 0)
    {
        ESP_LOGE("BLE Advertisement", "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

void ble_start_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_address_type); // determines the best address type automatically
    ESP_LOGI("BLE ADDRESS TYPE", "%d /n", ble_address_type);
    ble_advertisement();
}

// main ble process
void ble_task(void *param)
{
    nimble_port_run();             // go to the next line only when nimble_port_stop() is executed
    nimble_port_freertos_deinit(); // release the task resources
}

int initialize_bluetooth_service(char *BLE_device_name)
{
    ESP_LOGI("BLE Service Init", "Initializing bluetooth advertisement service!");
    int ret;
    ret = nimble_port_init();
    if (ret != 0)
    {
        ESP_LOGE("BLE Service Init", "Error initializing nimble port!");
        return ret;
    }
    // set the service name to be read inside the advertisement function
    ret = ble_svc_gap_device_name_set(BLE_device_name);
    if (ret != 0)
    {
        ESP_LOGE("BLE Service Init", "Error setting the service name...");
        return ret;
    }
    // initialize NimBLE configuration - gap service
    ble_svc_gap_init();
    // ~~ gatt service
    ble_svc_gatt_init();
    // ~~ config gatt services
    ret = ble_gatts_count_cfg(gatt_svcs);
    if (ret != 0)
    {
        ESP_LOGE("BLE Service Init", "Error adjusting the GATT configuration object to accommodate the service definition...");
        return ret;
    }
    // ~~ queues gatt services.
    ret = ble_gatts_add_svcs(gatt_svcs);
    if (ret != 0)
    {
        ESP_LOGE("BLE Service Init", "Error queueing the service definitions for registration...");
        return ret;
    }
    // ble application initialization
    ble_hs_cfg.sync_cb = ble_start_on_sync;
    // initialize the ble task as a process in a separated thread in the OS
    nimble_port_freertos_init(ble_task);

    return ret;
}

int stop_bluetooth_service()
{
    int ret = nimble_port_stop();
    if (ret == 0)
    {
        ret = nimble_port_deinit();
    }
    if (ret == 0)
    {
        ESP_LOGI("BLE Service Deinit", "Bluetooth service stopped!");
    }
    return ret;
}