#include "bluetooth.h"
#include "data.h"

#define GATT_DEVICE_INFO_UUID                   0x0B0D
#define GATT_READ_SERVICE_UUID                  0x00BB
#define GATT_WRITE_SERVICE_UUID                 0x01BB
#define INFO_BUFFER_SIZE                        180
#define MANUFACTURER_DATA_SIZE                  17

char *cached_device_info_buffer = NULL;
uint8_t ble_address_type;

// Mutex for synchronizing access to the cached_device_info_buffer
SemaphoreHandle_t buffer_mutex;

/* ----------------------------------------------------------------
------------------ Device Info Buffer Cache -----------------------*/

void update_buffer(void){
    if (customer_info.name == NULL || customer_info.ssid == NULL || customer_info.pass == NULL || customer_info.api_key == NULL || cached_device_info_buffer == NULL) {
        return;
    }
    snprintf(cached_device_info_buffer, INFO_BUFFER_SIZE, "Device Information:\n Name: %s\n DeviceID: %d\n SSID: %s\n Password: %s\n API Key: %s\n",
        customer_info.name, customer_info.deviceid, customer_info.ssid, customer_info.pass, customer_info.api_key);
}

void initialize_buffer_cache(void){

    cached_device_info_buffer = NULL;
    cached_device_info_buffer = (char *)calloc(INFO_BUFFER_SIZE, sizeof(char));

    if (cached_device_info_buffer == NULL) {
        ESP_LOGE("Customer Info Cache", "Customer info's cached buffer memory allocation failed...\n");
        return;
    }

    update_buffer();

    buffer_mutex = xSemaphoreCreateMutex();
        if (buffer_mutex == NULL) {
        ESP_LOGE("Mutex", "Mutex creation failed...\n");
        free(cached_device_info_buffer);
        cached_device_info_buffer = NULL;
    }
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
            // strncpy(customer_info.pass, value, sizeof(customer_info.pass) - 1);
            // customer_info.pass[sizeof(customer_info.pass) - 1] = '\0';
            return 1;
        case NAME:
            // strncpy(customer_info.name, value, sizeof(customer_info.name) - 1);
            // customer_info.name[sizeof(customer_info.name) - 1] = '\0';
            return 1;
        case SSID:
            // strncpy(customer_info.ssid, value, sizeof(customer_info.ssid) - 1);
            // customer_info.ssid[sizeof(customer_info.ssid) - 1] = '\0';
            return 1;
        default: //the key does not match with configuration
            return 0;
    }
}

/* ----------------------------------------------------------------
-------------------- Bluetooth Low Energy --------------------------*/

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){

    char *data = NULL;

    if (xSemaphoreTake(buffer_mutex, (TickType_t)portMAX_DELAY) == pdTRUE) {
        
        //char * data = (char *)ctxt->om->om_data;

        data = (char *)malloc(ctxt->om->om_len + 1);  // +1 for null terminator

        if (data == NULL) {
            ESP_LOGE("WRITE GATT SERVICE", "data buffer memory allocation failed...\n");
            free(data);
            return 1;
        }

        // Copy data from om_data to the dynamically allocated buffer
        memcpy(data, ctxt->om->om_data, ctxt->om->om_len);

        // Null-terminate the string
        data[ctxt->om->om_len] = '\0';  

        //debug

        ESP_LOGI("WRITE GATT SERVICE", "Received data: %s / %s", ctxt->om->om_data, data);
        // ESP_LOGI("WRITE GATT SERVICE", "Cached buffer content: %s", cached_device_info_buffer);

        // ESP_LOGI("WRITE GATT SERVICE", "Address of data: %p", (void *)data); //1073534643
        // ESP_LOGI("WRITE GATT SERVICE", "Address of cached_device_info_buffer: %p", (void *)cached_device_info_buffer); //1073597892

        // size_t size_between_addresses = (size_t)(cached_device_info_buffer - data);

        // ESP_LOGI("WRITE GATT SERVICE", "Size between addresses: %zu bytes", size_between_addresses);


        //TODO: Implement a queue to send all the tokenized items from the data received! Bug when using strtok twice for different split criteria ...

        // Find the colon in the received data
        char *pair = strtok(data, ",");

        while (pair != NULL) {
            ESP_LOGI("WRITE GATT SERVICE","pair result: %s\n", pair);
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

                // set the values in customer_info and print the new values
                if (set_customer_info(key, value)) {
                    ESP_LOGI("WRITE GATT SERVICE", "New %s set: %s\n", key, value);
                } else {
                    ESP_LOGI("WRITE GATT SERVICE","Unknown key: %s\n", key);
                }
            }

            // Get the next key-value pair
            pair = strtok(NULL, ",");
            ESP_LOGI("WRITE GATT SERVICE","Next pair result: %s\n", pair);
        }
        free(data);
        update_buffer();
        xSemaphoreGive(buffer_mutex);
    }
    return 0;
}

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    if (xSemaphoreTake(buffer_mutex, (TickType_t)portMAX_DELAY) == pdTRUE) {
        ESP_LOGI("READ GATT SERVICE", "Read requested by a bluetooth connection!");

        os_mbuf_append(ctxt->om, cached_device_info_buffer, INFO_BUFFER_SIZE);

        ESP_LOGI("READ GATT SERVICE", "%d bytes.\n", strlen(cached_device_info_buffer));
        
        // ESP_LOGI("READ GATT SERVICE", "%d bytes, context buffer: %s\n", strlen(cached_device_info_buffer), cached_device_info_buffer);
        // ESP_LOGI("READ GATT SERVICE", "%d bytes, context buffer: %.*s\n", ctxt->om->om_len, ctxt->om->om_len, (char *)(ctxt->om->om_data));

        xSemaphoreGive(buffer_mutex);

        //for debugging
        // esp_timer_dump(stdout);
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

    // Set the device type, flags
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    //TODO: manufacturer data...
    // Set the manufacturer data
    // const char *manufacturer_name = "Bodil Energy ApS";

    // fields.mfg_data =  manufacturer_name;
    // fields.mfg_data_len = MANUFACTURER_DATA_SIZE;

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
    ESP_LOGI("BLE ADDRESS TYPE", "%d /n", ble_address_type);
    ble_advertisement();
}


// main ble process
void ble_task(void *param){
    nimble_port_run(); // returns only when nimble_port_stop() is executed
}

int initialize_bluetooth_service(char *BLE_device_name){
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