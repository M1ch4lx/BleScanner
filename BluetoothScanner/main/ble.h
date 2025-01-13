#ifndef BLE_H_
#define BLE_H_

#include "common.h"

#define PROFILE_NUM 1
#define PROFILE_APP_ID 0
#define GATTS_NUM_HANDLE 10
#define PREPARE_BUF_MAX_SIZE 1024

#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

typedef void (*configuration_received_callback)(const char*);

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t ssid_char_handle;
    uint16_t pass_char_handle;
    uint16_t broker_ip_char_handle;
    uint16_t board_name_char_handle;
};

typedef struct {
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;

esp_ble_adv_params_t* get_adv_params();

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

void exec_write_event_env(prepare_type_env_t *prepare_write_env, 
                                         esp_ble_gatts_cb_param_t *param);

void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

void gatts_event_handler(esp_gatts_cb_event_t event, 
                                esp_gatt_if_t gatts_if, 
                                esp_ble_gatts_cb_param_t *param);
                                
void initialize_ble(configuration_received_callback ssid_callback, 
					configuration_received_callback password_callback,
					configuration_received_callback _broker_ip_callback, 
					configuration_received_callback _board_name_callback);

#endif