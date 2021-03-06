//
//  ble_device.c
//  nsec16
//
//  Created by Marc-Etienne M.Léveillé on 2015-11-22.
//
//

#include "nsec_ble_internal.h"
#include <ble_conn_params.h>
/*
#include <ble_bas.h>
#include <ble_hci.h>*/
#include <ble_advdata.h>
#include <ble_dis.h>
#include <app_timer.h>
#include <peer_manager.h>
#include <app_scheduler.h>
#include <nrf_ble_gatt.h>

#include "../boards.h"
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include "../led_effects.h"
#include "../logs.h"
#include "nrf_log.h"
#include "gap_configuration.h"
#include "nsec_ble.h"
#include "vendor_service.h"
#include "service_characteristic.h"


#define APP_BLE_OBSERVER_PRIO 3
#define PEER_ADDRESS_SIZE 6
#define MAX_VENDOR_SERVICE_COUNT 8
#define LONG_WRITE_MAX_LENGTH 200
NRF_BLE_GATT_DEF(m_gatt);


typedef struct{
    const char* device_name;
    ble_gap_adv_params_t advertising_parameters;
    uint16_t vendor_service_count;
    VendorService* vendor_services[MAX_VENDOR_SERVICE_COUNT];
} BleDevice;

typedef struct{
	uint16_t characteristic_handle;
	uint16_t write_offset;
	uint16_t write_length;
	uint8_t write_buffer[1];
} QueuedWrite;

static BleDevice* ble_device = NULL;

static uint8_t* buffer = NULL;

static nrf_sdh_ble_evt_handler_t _nsec_ble_event_handlers[NSEC_BLE_LIMIT_MAX_EVENT_HANDLER];
static nsec_ble_adv_uuid_provider _nsec_ble_adv_uuid_providers[NSEC_BLE_LIMIT_MAX_UUID_PROVIDER];
static uint8_t _nsec_ble_is_enabled = 0;
static nsec_ble_found_nsec_badge_callback _nsec_ble_scan_callback = NULL;

//static void nsec_ble_scan_start();
static void _nsec_ble_softdevice_init();
static void gatt_init();
//static void init_connection_parameters();
//static void add_device_information_service(char * manufacturer_name, char * model, char * serial_number,
//        char * hw_revision, char * fw_revision, char * sw_revision);
static void on_characteristic_write_command_event(const ble_gatts_evt_write_t * write_event);
static void on_execute_queued_write_commands();
static void on_characteristic_write_request_event(const ble_gatts_evt_write_t * write_event, uint16_t connection_handle);
static void on_characteristic_read_request_event(const ble_gatts_evt_read_t * read_event, uint16_t connection_handle);
static ServiceCharacteristic* get_characteristic_from_uuid(uint16_t uuid);
static void reply_to_client_request(uint8_t operation, uint16_t status_code, uint16_t connection_handle,
        const uint8_t* data_buffer, uint16_t data_length);
static void on_prepare_write_request(const ble_gatts_evt_write_t * write_event, uint16_t connection_handle);
static void on_execute_queued_write_requests(uint16_t connection_handle);
static uint16_t get_characteristic_handle_for_queued_writes();
static ServiceCharacteristic* get_characteristic_from_handle(uint16_t handle);
static uint16_t parse_queued_write_events(CharacteristicWriteEvent* event);


ret_code_t create_ble_device(char* device_name){
    if(ble_device == NULL){
        _nsec_ble_softdevice_init();
        pm_init();
        nsec_ble_init();
        ble_device = malloc(sizeof(BleDevice));
        ble_device->device_name = device_name;
        ble_device->vendor_service_count = 0;
        for(int i = 0; i < MAX_VENDOR_SERVICE_COUNT; i++){
            ble_device->vendor_services[i] = NULL;
        }
        return NRF_SUCCESS;
    }
    return -1;
}

void configure_advertising(){
    if(ble_device == NULL)
        return;
    gatt_init();
    set_default_gap_parameters(ble_device->device_name, &(ble_device->advertising_parameters));
}


void start_advertising(){
    if(ble_device == NULL)
        return;
    APP_ERROR_CHECK(sd_ble_gap_adv_start(&(ble_device->advertising_parameters), BLE_COMMON_CFG_VS_UUID));
}

static void ble_event_handler(ble_evt_t const * p_ble_evt, void * p_context){
    //pm_on_ble_evt(p_ble_evt);
    switch (p_ble_evt->header.evt_id){
        case BLE_GAP_EVT_DISCONNECTED:
            start_advertising();
            break;
        case BLE_GATTS_EVT_SYS_ATTR_MISSING: {
            const uint16_t conn = p_ble_evt->evt.gatts_evt.conn_handle;
            APP_ERROR_CHECK(sd_ble_gatts_sys_attr_set(conn, NULL, 0, 0));
            }
            break;

        case BLE_GAP_EVT_ADV_REPORT: {
            if(_nsec_ble_scan_callback == NULL) {
                break;
            }
            const ble_gap_evt_adv_report_t * rp = &p_ble_evt->evt.gap_evt.params.adv_report;
            int8_t i = 0;
            while((rp->dlen - i) >= 2) {
                const uint8_t len = rp->data[i++] - 1; // The type is included in the length
                const uint8_t type = rp->data[i++];
                const uint8_t * data = &rp->data[i];
                i += len;

                if(type == BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME) {
                    if(len == 8 && data[0] == 'N' && data[1] == 'S' && data[2] == 'E' && data[3] == 'C') {
                        uint16_t other_id = 0;
                        if(sscanf((const char *) &data[4], "%04hx", &other_id) == 1) {
                            _nsec_ble_scan_callback(other_id, rp->peer_addr.addr, rp->rssi);
                        }
                    }
                }
            }
            }
            break;
        case BLE_GATTS_EVT_WRITE:
        {
            const ble_gatts_evt_write_t * event = &p_ble_evt->evt.gatts_evt.params.write;
            if(event->op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW){
            	on_execute_queued_write_commands();
            }
            else{
            	on_characteristic_write_command_event(event);
            }
            break;
        }
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            uint8_t type = p_ble_evt->evt.gatts_evt.params.authorize_request.type;
            if(type == BLE_GATTS_AUTHORIZE_TYPE_READ){
                const ble_gatts_evt_read_t * event = &p_ble_evt->evt.gatts_evt.params.authorize_request.request.read;
                on_characteristic_read_request_event(event, p_ble_evt->evt.gatts_evt.conn_handle);
            }
            else if(type == BLE_GATTS_AUTHORIZE_TYPE_WRITE){
                const ble_gatts_evt_write_t * event = &p_ble_evt->evt.gatts_evt.params.authorize_request.request.write;
                uint16_t connection_handle = p_ble_evt->evt.gatts_evt.conn_handle;
                switch(event->op){
                	case BLE_GATTS_OP_WRITE_REQ:
                		on_characteristic_write_request_event(event, connection_handle);
                		break;
                	case BLE_GATTS_OP_PREP_WRITE_REQ:
                		on_prepare_write_request(event, connection_handle);
                		break;
                	case BLE_GATTS_OP_EXEC_WRITE_REQ_NOW:
                		on_execute_queued_write_requests(connection_handle);
                		break;
                	default:
                		break;
                }
            }
            break;
        }
        case BLE_EVT_USER_MEM_REQUEST:
        {
            buffer = malloc(LONG_WRITE_MAX_LENGTH);
            ble_user_mem_block_t memory_block;
            memory_block.p_mem = buffer;
            memory_block.len = LONG_WRITE_MAX_LENGTH;
            volatile uint32_t error_code = sd_ble_user_mem_reply(p_ble_evt->evt.common_evt.conn_handle, &memory_block);
            APP_ERROR_CHECK(error_code);
        }
            break;
        case BLE_EVT_USER_MEM_RELEASE:
            free(p_ble_evt->evt.common_evt.params.user_mem_release.mem_block.p_mem);
            buffer = NULL;
            break;
        default:
            break;
    }
}

uint32_t add_vendor_service(VendorService* service){
    if(ble_device->vendor_service_count >= MAX_VENDOR_SERVICE_COUNT)
        return 1;
    create_uuid_for_vendor_service(&service->uuid, ble_device->vendor_service_count);
    uint32_t error_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &service->uuid, &service->handle);
    APP_ERROR_CHECK(error_code);
    ble_device->vendor_services[ble_device->vendor_service_count] = service;
    ble_device->vendor_service_count++;
    return 0;
}

static void gatt_init(){
    APP_ERROR_CHECK(nrf_ble_gatt_init(&m_gatt, NULL));
}

/*static void _nsec_pm_evt_handler(pm_evt_t const * event) {

}*/

/*static void on_connection_params_event(ble_conn_params_evt_t * p_evt){
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED){
        NRF_LOG_ERROR("BLE connection params event failed.");
    }
}*/

/*static void connection_params_error_handler(uint32_t nrf_error){
    log_error_code("connection params error handler", nrf_error);
}*/

static void on_characteristic_write_command_event(const ble_gatts_evt_write_t * write_event){
    ServiceCharacteristic* characteristic = get_characteristic_from_uuid(write_event->uuid.uuid);
    if(characteristic != NULL && characteristic->on_write_command != NULL){
        CharacteristicWriteEvent event = {
            .write_offset = write_event->offset,
            .data_length = write_event->len,
            .data_buffer = write_event->data
        };
        characteristic->on_write_command(&event);
    }
}

static void on_execute_queued_write_commands(){
	APP_ERROR_CHECK(buffer == NULL);
	CharacteristicWriteEvent event;
	uint16_t characteristic_handle = parse_queued_write_events(&event);
	if(characteristic_handle != BLE_GATT_HANDLE_INVALID){
		ServiceCharacteristic* characteristic = get_characteristic_from_handle(characteristic_handle);
		if(characteristic != NULL && characteristic->on_write_command != NULL){
			characteristic->on_write_command(&event);
		}
	}
}

static void on_characteristic_write_request_event(const ble_gatts_evt_write_t * write_event, uint16_t connection_handle){
    ServiceCharacteristic* characteristic = get_characteristic_from_uuid(write_event->uuid.uuid);
    if(characteristic != NULL && characteristic->on_write_request != NULL){
        CharacteristicWriteEvent event = {
            .write_offset = write_event->offset,
            .data_length = write_event->len,
            .data_buffer = write_event->data
        };
        uint16_t status_code = characteristic->on_write_request(&event);
        const uint8_t* data_buffer = status_code == BLE_GATT_STATUS_SUCCESS ? event.data_buffer: NULL;
        reply_to_client_request(BLE_GATTS_AUTHORIZE_TYPE_WRITE, status_code, connection_handle, data_buffer,
                event.data_length);
    }
    else{
        reply_to_client_request(BLE_GATTS_AUTHORIZE_TYPE_WRITE, BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED,
                        connection_handle, NULL, 0);
    }
}

static void on_characteristic_read_request_event(const ble_gatts_evt_read_t * read_event, uint16_t connection_handle){
    ServiceCharacteristic* characteristic = get_characteristic_from_uuid(read_event->uuid.uuid);
    if(characteristic != NULL && characteristic->on_read_request != NULL){
        CharacteristicReadEvent event = {
            .read_offset = read_event->offset,
        };
        uint16_t status_code = characteristic->on_read_request(&event);
        reply_to_client_request(BLE_GATTS_AUTHORIZE_TYPE_READ, status_code, connection_handle, NULL, characteristic->value_length);
    }
    else{
        reply_to_client_request(BLE_GATTS_AUTHORIZE_TYPE_READ, BLE_GATT_STATUS_ATTERR_READ_NOT_PERMITTED, connection_handle, NULL, 0);
    }
}

static void on_prepare_write_request(const ble_gatts_evt_write_t * write_event, uint16_t connection_handle){
	if(get_characteristic_handle_for_queued_writes() == write_event->handle){
		reply_to_client_request(BLE_GATTS_AUTHORIZE_TYPE_WRITE, BLE_GATT_STATUS_SUCCESS, connection_handle, NULL, 0);
	}
	else{
		/* Queuing multiple writes to different characteristics should be allowed, but it would be difficult to handle properly and simply. */
		reply_to_client_request(BLE_GATTS_AUTHORIZE_TYPE_WRITE, BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED, connection_handle, write_event->data, write_event->len);
	}
}

static void on_execute_queued_write_requests(uint16_t connection_handle){
	APP_ERROR_CHECK(buffer == NULL);
	uint16_t status_code = BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
	CharacteristicWriteEvent event;
	uint16_t characteristic_handle = parse_queued_write_events(&event);
	if(characteristic_handle != BLE_GATT_HANDLE_INVALID){
		ServiceCharacteristic* characteristic = get_characteristic_from_handle(characteristic_handle);
		if(characteristic != NULL && characteristic->on_write_request != NULL){
			status_code = characteristic->on_write_request(&event);
		}
	}
	if(status_code == BLE_GATT_STATUS_SUCCESS){
		reply_to_client_request(BLE_GATTS_AUTHORIZE_TYPE_WRITE, status_code, connection_handle, NULL, 0);
	}
	else{
		reply_to_client_request(BLE_GATTS_AUTHORIZE_TYPE_WRITE, BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED,
				connection_handle, NULL, 0);
	}
}

static uint16_t parse_queued_write_events(CharacteristicWriteEvent* event){
	uint8_t* current_read_index_in_buffer = buffer;
	QueuedWrite* write = (QueuedWrite*)current_read_index_in_buffer;
	event->data_length = 0;
	uint16_t characteristic_handle = write->characteristic_handle;

	while(write->characteristic_handle != BLE_GATT_HANDLE_INVALID){
		if(current_read_index_in_buffer - buffer > LONG_WRITE_MAX_LENGTH){
			event->data_buffer = NULL;
			event->data_length = 0;
			event->write_offset = 0;
			return BLE_GATT_HANDLE_INVALID;
		}
		if(characteristic_handle != write->characteristic_handle){
			// Writing to different characteristic in a queued requests should be supported but it is not for the moment, so reject the write.
			// It should not happen to often.
			event->data_buffer = NULL;
			event->data_length = 0;
			return BLE_GATT_HANDLE_INVALID;
		}

		current_read_index_in_buffer += sizeof(write->characteristic_handle) + sizeof(write->write_length)
				+ sizeof(write->write_offset);
		characteristic_handle = write->characteristic_handle;
		uint16_t copy_size = write->write_length;
		uint8_t* copy_buffer = malloc(copy_size);
		// beyond this point, write doesn't point to a valid structure anymore, as the memcpy to defrag the data overwrite it.
		memcpy(copy_buffer, current_read_index_in_buffer, copy_size);
		memcpy(buffer + event->data_length, copy_buffer, copy_size);
		free(copy_buffer);
		event->data_length += copy_size;
		current_read_index_in_buffer += copy_size;
		write = (QueuedWrite*)current_read_index_in_buffer;
	}
	event->data_buffer = buffer;
	event->write_offset = 0;
	return characteristic_handle;
}

static void reply_to_client_request(uint8_t operation, uint16_t status_code, uint16_t connection_handle,
        const uint8_t* data_buffer, uint16_t data_length){
    ble_gatts_rw_authorize_reply_params_t reply;
    reply.type = operation;
    reply.params.read.gatt_status = status_code;
    reply.params.read.len = data_length;
    reply.params.read.offset = 0;
    reply.params.read.update = data_buffer == NULL ? false: true;
    reply.params.read.p_data = data_buffer;
    APP_ERROR_CHECK(sd_ble_gatts_rw_authorize_reply(connection_handle, &reply));
}

static ServiceCharacteristic* get_characteristic_from_uuid(uint16_t uuid){
    for(int i = 0; i < ble_device->vendor_service_count; i++){
        VendorService* service = ble_device->vendor_services[i];
        if(service->uuid.uuid == (uuid & UUID_SERVICE_PART_MASK))
            return get_characteristic(service, uuid);
    }
    return NULL;
}

static ServiceCharacteristic* get_characteristic_from_handle(uint16_t handle){
    for(int i = 0; i < ble_device->vendor_service_count; i++){
        VendorService* service = ble_device->vendor_services[i];
        for(int j = 0; j < service->characteristic_count; j++){
        	ServiceCharacteristic* characteristic = service->characteristics[j];
        	if(characteristic->handle == handle){
        		return characteristic;
        	}
        }
    }
    return NULL;
}

static uint16_t get_characteristic_handle_for_queued_writes(){
	if(buffer == NULL){
		return BLE_GATT_HANDLE_INVALID;
	}
	uint16_t handle = *((uint16_t*)buffer);
	return handle;
}

static void _nsec_ble_softdevice_init() {
    uint32_t ram_start = 0;
    nrf_sdh_ble_default_cfg_set(BLE_COMMON_CFG_VS_UUID, &ram_start);

#if DEBUG_PRINT_RAM_USAGE
    uint32_t ram_start_copy = 0;
    nrf_sdh_ble_enable(&ram_start_copy);
    //TO DO
#else
    APP_ERROR_CHECK(nrf_sdh_ble_enable(&ram_start));
#endif
    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_event_handler, NULL);
}
/*
static void nsec_ble_disable_task(void * context, uint16_t size) {
    sd_ble_gap_scan_stop();
    sd_ble_gap_adv_stop();
}

static void nsec_ble_enable_task(void * context, uint16_t size) {
    _nsec_ble_advertising_start();
    nsec_ble_scan_start();
}

uint8_t nsec_ble_toggle(void) {
    if(_nsec_ble_is_enabled) {
        app_sched_event_put(NULL, 0, nsec_ble_disable_task);
        _nsec_ble_is_enabled = 0;
    }
    else {
        app_sched_event_put(NULL, 0, nsec_ble_enable_task);
        _nsec_ble_is_enabled = 1;
    }
    return _nsec_ble_is_enabled;
}
*/
uint8_t nsec_ble_toggle(void) { return 0; } // FIXME

void nsec_ble_register_evt_handler(nrf_sdh_ble_evt_handler_t handler) {
    for(int i = 0; i < NSEC_BLE_LIMIT_MAX_EVENT_HANDLER; i++) {
        if(_nsec_ble_event_handlers[i] == NULL) {
            _nsec_ble_event_handlers[i] = handler;
            break;
        }
    }
}
/*
void nsec_ble_set_scan_callback(nsec_ble_found_nsec_badge_callback callback) {
    _nsec_ble_scan_callback = callback;
}
*/
void nsec_ble_register_adv_uuid_provider(nsec_ble_adv_uuid_provider provider) {
    for(int i = 0; i < NSEC_BLE_LIMIT_MAX_UUID_PROVIDER; i++) {
        if(_nsec_ble_adv_uuid_providers[i] == NULL) {
            _nsec_ble_adv_uuid_providers[i] = provider;
            break;
        }
    }
    sd_ble_gap_adv_stop();
}
/*
static void nsec_ble_scan_start(void) {
    ble_gap_scan_params_t scan_params;
    scan_params.active = 0;
    scan_params.adv_dir_report = 0;
    scan_params.use_whitelist = 0;
    scan_params.timeout = 0;
    scan_params.window = MSEC_TO_UNITS(40, UNIT_0_625_MS);
    scan_params.interval = MSEC_TO_UNITS(240, UNIT_0_625_MS);

    log_error_code("sd_ble_gap_scan_start", sd_ble_gap_scan_start(&scan_params));
}
*/
int nsec_ble_init() {
    bzero(_nsec_ble_event_handlers, sizeof(_nsec_ble_event_handlers));
    bzero(_nsec_ble_adv_uuid_providers, sizeof(_nsec_ble_adv_uuid_providers));

    //nsec_ble_scan_start();
    _nsec_ble_is_enabled = 1;

    return NRF_SUCCESS;
}
