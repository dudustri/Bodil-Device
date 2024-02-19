#include "bluetooth.h"

#define GATT_DEVICE_INFO_UUID 0x0B0D
#define GATT_READ_SERVICE_UUID 0x00BB
#define GATT_WRITE_NAME_SERVICE_UUID 0x0BB1
#define GATT_WRITE_SSID_SERVICE_UUID 0x0BB2
#define GATT_WRITE_PASS_SERVICE_UUID 0x0BB3

#define INFO_BUFFER_SIZE 200
#define MANUFACTURER_DATA_SIZE 16
#define QUEUE_TOKEN_SIZE 5

char *cached_device_info_buffer = NULL;
uint8_t ble_address_type;

// Mutex for synchronizing access to the cached_device_info_buffer
SemaphoreHandle_t buffer_mutex;

/* ----------------------------------------------------------------
------------------ Device Info Buffer Cache -----------------------*/

void update_buffer(void)
{
    if (strlen(customer_info.name) == 0 || strlen(customer_info.ssid) == 0 || strlen(customer_info.pass) == 0 || strlen(customer_info.api_key) == 0 || cached_device_info_buffer == NULL)
    {
        return;
    }
    snprintf(cached_device_info_buffer, INFO_BUFFER_SIZE, "Device Information:\n Name: %s\n DeviceID: %d\n SSID: %s\n Password: %s\n API Key: %s\n",
             customer_info.name, customer_info.deviceid, customer_info.ssid, customer_info.pass, customer_info.api_key);
}

void initialize_buffer_cache(void)
{

    cached_device_info_buffer = NULL;
    cached_device_info_buffer = (char *)calloc(INFO_BUFFER_SIZE, sizeof(char));

    if (cached_device_info_buffer == NULL)
    {
        ESP_LOGE("Customer Info Cache", "Customer info's cached buffer memory allocation failed...\n");
        return;
    }

    update_buffer();

    buffer_mutex = xSemaphoreCreateMutex();
    if (buffer_mutex == NULL)
    {
        ESP_LOGE("Mutex", "Mutex creation failed...\n");
        free(cached_device_info_buffer);
        cached_device_info_buffer = NULL;
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

static int uuid_check(uint16_t uuid) {
    switch (uuid) {
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

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (xSemaphoreTake(buffer_mutex, (TickType_t)portMAX_DELAY) == pdTRUE)
    {
        ESP_LOGI("READ GATT SERVICE", "Read requested by a bluetooth connection!");
        os_mbuf_append(ctxt->om, cached_device_info_buffer, INFO_BUFFER_SIZE);
        xSemaphoreGive(buffer_mutex);
    }
    return 0;
}

// Write service callback to update a parameter in the customer info object
static int device_write_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char *data = NULL;
    uint16_t uuid = *((uint16_t *)arg);

    if (xSemaphoreTake(buffer_mutex, (TickType_t)portMAX_DELAY) == pdTRUE)
    {

        data = (char *)malloc(ctxt->om->om_len + 1);
        if (data == NULL)
        {
            ESP_LOGE("WRITE GATT SERVICE", "data buffer memory allocation failed...\n");
            free(data);
            return 1;
        }

        // Copy data from om_data to the dynamically allocated buffer
        memcpy(data, ctxt->om->om_data, ctxt->om->om_len);
        data[ctxt->om->om_len] = '\0';

        int user_set_type = uuid_check(uuid);

        if (user_set_type == UNKNOWN)
        {
            ESP_LOGI("WRITE GATT SERVICE", "UUID not recognized to set available parameters.");
            return 0;
        }

        ble_set_customer_info(user_set_type, data, &customer_info);
        ESP_LOGI("WRITE GATT SERVICE", "New %d set: %s\n", user_set_type, data);

        update_buffer();
        free(data);
        xSemaphoreGive(buffer_mutex);
    }
    return 0;
}

// Service configs
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
          .arg = (void *)GATT_WRITE_NAME_SERVICE_UUID},
         {.uuid = BLE_UUID16_DECLARE(GATT_WRITE_SSID_SERVICE_UUID),
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write_handler,
          .arg = (void *)GATT_WRITE_SSID_SERVICE_UUID},
         {.uuid = BLE_UUID16_DECLARE(GATT_WRITE_PASS_SERVICE_UUID),
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write_handler,
          .arg = (void *)GATT_WRITE_PASS_SERVICE_UUID},
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
            if (cached_device_info_buffer == NULL)
                initialize_buffer_cache();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_DISCONNECT:
        free(cached_device_info_buffer);
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

// TODO: 
// Implement ble module activation when there is no netif network connection and review the advertisement interval
// Add also the deactivation when the device is already connected to the network by wifi or gsm.
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
    'B', 'o', 'd', 'i', 'l', ' ', 'E', 'n', 'e', 'r', 'g', 'y', ' ', 'A', 'p', 'S'
    };
    fields.mfg_data =  manufacturer_data;
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
    nimble_port_run(); // returns only when nimble_port_stop() is executed
}

int initialize_bluetooth_service(char *BLE_device_name)
{
    nimble_port_init();
    ble_svc_gap_device_name_set(BLE_device_name); // set the service name to be read inside the advertisement function
    ble_svc_gap_init();                           // initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                          // ~~ gatt service
    ble_gatts_count_cfg(gatt_svcs);               // ~~ config gatt services
    ble_gatts_add_svcs(gatt_svcs);                // ~~ queues gatt services.
    ble_hs_cfg.sync_cb = ble_start_on_sync;       // ble application initialization
    nimble_port_freertos_init(ble_task);          // initialize the ble task as a process in a separated thread in the OS

    return 0;
}