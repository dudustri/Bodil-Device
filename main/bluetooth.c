#include "bluetooth.h"
#include "data.h"

#define GATT_DEVICE_INFO_UUID                   0x0B0D
#define GATT_READ_SERVICE_UUID                  0x00BB
#define GATT_WRITE_SERVICE_UUID                 0x01BB

char *cached_device_info_buffer;
uint8_t ble_address_type;

/* ----------------------------------------------------------------
------------------ Device Info Buffer Cache -----------------------*/

void update_buffer(void){
        snprintf(cached_device_info_buffer, 512, "Device Information:\n"
                                      "Name: %s\n"
                                      "DeviceID: %d\n"
                                      "SSID: %s\n"
                                      "Password: %s\n"
                                      "API Key: %s\n",
             customer_info.name, customer_info.deviceid, customer_info.ssid, customer_info.pass, customer_info.api_key);
}

void initialize_buffer_cache(void){

    cached_device_info_buffer = NULL;
    cached_device_info_buffer = malloc(512);

    if (cached_device_info_buffer == NULL) {
        fprintf(stderr, "Customer info's cached buffer memory allocation failed\n");
        return;
    }

    memset(cached_device_info_buffer, 0, 512);
    update_buffer();
}

/* ----------------------------------------------------------------
--------------------- Customer Info Update ------------------------*/

int match_key(const char *key){
    
    enum ConfigKey config_key = UNKNOWN;

    if (strcmp(key, "Password") == 0) {
        config_key = PASSWORD;
    } else if (strcmp(key, "Name") == 0) {
        config_key = NAME;
    } else if (strcmp(key, "SSID") == 0) {
        config_key = SSID;
    }
    return config_key;
}

int set_customer_info(const char *key, const char *value){
 
    int config_key = match_key(key);

    // Function to set values in the customer_info structure based on key-value pairs
    switch(config_key) {

        case PASSWORD:
            strncpy(customer_info.pass, value, sizeof(customer_info.pass) - 1);
            customer_info.pass[sizeof(customer_info.pass) - 1] = '\0';
            return 1;
        case NAME:
            strncpy(customer_info.name, value, sizeof(customer_info.name) - 1);
            customer_info.name[sizeof(customer_info.name) - 1] = '\0';
            return 1;
        case SSID:
            strncpy(customer_info.ssid, value, sizeof(customer_info.ssid) - 1);
            customer_info.ssid[sizeof(customer_info.ssid) - 1] = '\0';
            return 1;
        default: //the key does not match with configuration
            return 0;
    }
}

/* ----------------------------------------------------------------
-------------------- Bluetooth Low Energy --------------------------*/

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char * data = (char *)ctxt->om->om_data;

    // Find the colon in the received data
    char *pair = strtok(data, ",");
    while (pair != NULL) {
        // Split key and value
        char *key = strtok(pair, ":");
        char *value = strtok(NULL, ":");

        if (key != NULL && value != NULL) {
            // Trim leading and trailing whitespaces from key and value
            while (*key == ' ') key++;
            char *end = key + strlen(key) - 1;
            while (end > key && *end == ' ') end--;
            *(end + 1) = '\0';

            while (*value == ' ') value++;
            end = value + strlen(value) - 1;
            while (end > value && *end == ' ') end--;
            *(end + 1) = '\0';

            // Set the values in customer_info
            if (set_customer_info(key, value)) {
                printf("New %s set: %s\n", key, value);
            } else {
                printf("Unknown key: %s\n", key);
            }
        }

        // Get the next key-value pair
        pair = strtok(NULL, ",");
    }
    return 0;
}

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    os_mbuf_append(ctxt->om, cached_device_info_buffer, strlen(cached_device_info_buffer));
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
         {.uuid = BLE_UUID16_DECLARE(GATT_WRITE_SERVICE_UUID),
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write},
         {0}}},
    {0}};


// Bluetooth event handler
static int ble_gap_event(struct ble_gap_event *event, void *arg){
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_advertisement();
        }
        else{
            if(cached_device_info_buffer == NULL) initialize_buffer_cache();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_DISCONNECT:
        free(cached_device_info_buffer);
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECTED");
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

//TODO: Implement an bluetooth advertisement / module activation with http request and change the advertisement interval 
void ble_advertisement(void){
    // GAP service name configuration
    int rc;
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0 ){
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
    //advertising interval in units of 0.625 ms 
    adv_params.itvl_min = 160; // 100 ms
    adv_params.itvl_max = 800; // 500 ms
    // high duty cycle deactivated to save energy - no high connection performance is needed
    adv_params.high_duty_cycle = 0;

    rc = ble_gap_adv_start(ble_address_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);

    if (rc != 0) {
        ESP_LOGE("BLE Advertisement", "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

void ble_start_on_sync(void){
    ble_hs_id_infer_auto(0, &ble_address_type); // determines the best address type automatically
    ble_advertisement();
}


// main ble process
void ble_task(void *param){
    nimble_port_run(); // returns only when nimble_port_stop() is executed
}

void initialize_bluetooth_service(char *BLE_device_name){
    initialize_buffer_cache();
    nimble_port_init(); 
    ble_svc_gap_device_name_set(BLE_device_name); // set the service name to be read inside the advertisement function 
    ble_svc_gap_init();                           // initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                          // ~~ gatt service
    ble_gatts_count_cfg(gatt_svcs);               // ~~ config gatt services
    ble_gatts_add_svcs(gatt_svcs);                // ~~ queues gatt services.
    ble_hs_cfg.sync_cb = ble_start_on_sync;       // ble application initialization
    nimble_port_freertos_init(ble_task);          // initialize the ble task as a process in a separated thread in the OS
}