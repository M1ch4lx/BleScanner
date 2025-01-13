#include "ble.h"

static uint8_t adv_config_done = 0;

static configuration_received_callback wifi_ssid_callback;
static configuration_received_callback wifi_password_callback;
static configuration_received_callback broker_ip_callback;
static configuration_received_callback board_name_callback;

// UUIDs for BLE
static const uint8_t WIFI_CONFIG_SERVICE_UUID_128[16] = {
    0x00, 0x00, 0xEE, 0x00, 0x00, 0x10, 0x00, 0x80,
    0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB, 0x00, 0x00
};

static const uint8_t WIFI_SSID_CHAR_UUID_128[16] = {
    0x00, 0x00, 0xFF, 0x01, 0x00, 0x10, 0x00, 0x80,
    0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB, 0x00, 0x00
};

static const uint8_t WIFI_PASS_CHAR_UUID_128[16] = {
    0x00, 0x00, 0xFF, 0x02, 0x00, 0x10, 0x00, 0x80,
    0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB, 0x00, 0x00
};

static const uint8_t BROKER_IP_CHAR_UUID_128[16] = {
    0x00, 0x00, 0xFF, 0x03, 0x00, 0x10, 0x00, 0x80,
    0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB, 0x00, 0x00
};

static const uint8_t BOARD_NAME_CHAR_UUID_128[16] = {
    0x00, 0x00, 0xFF, 0x04, 0x00, 0x10, 0x00, 0x80,
    0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB, 0x00, 0x00
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x40,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

esp_ble_adv_params_t* get_adv_params() {
	return &adv_params;
}

static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM];

static prepare_type_env_t a_prepare_write_env;

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0){
            // ESP_LOGI(GATTS_TAG, "Start advertising...");
            // esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0){
            // ESP_LOGI(GATTS_TAG, "Start advertising...");
            // esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising start failed");
        } else {
            ESP_LOGI(GATTS_TAG, "Advertising started successfully");
        }
        break;
    default:
        break;
    }
}

void exec_write_event_env(prepare_type_env_t *prepare_write_env, 
                                         esp_ble_gatts_cb_param_t *param) {
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        ESP_LOGI(GATTS_TAG, "GATTS_REG_EVT");
        gl_profile_tab[PROFILE_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_128;
        memcpy(gl_profile_tab[PROFILE_APP_ID].service_id.id.uuid.uuid.uuid128,
               WIFI_CONFIG_SERVICE_UUID_128, ESP_UUID_LEN_128);

        esp_ble_gap_set_device_name("ESP_WIFI_CONFIG");
        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret) {
            ESP_LOGE(GATTS_TAG, "config adv data failed: %x", ret);
        }
        adv_config_done |= adv_config_flag;

        ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (ret) {
            ESP_LOGE(GATTS_TAG, "config scan rsp data failed: %x", ret);
        }
        adv_config_done |= scan_rsp_config_flag;

        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_APP_ID].service_id, GATTS_NUM_HANDLE);
        break;
    }
    
    case ESP_GATTS_CREATE_EVT:
        gl_profile_tab[PROFILE_APP_ID].service_handle = param->create.service_handle;
        {
			esp_attr_value_t attr_value = {
	            .attr_max_len = 32,
	            .attr_len = 0,   
	            .attr_value = NULL,
	        };
			
            esp_bt_uuid_t ssid_uuid = { .len = ESP_UUID_LEN_128 };
            memcpy(ssid_uuid.uuid.uuid128, WIFI_SSID_CHAR_UUID_128, ESP_UUID_LEN_128);
            esp_ble_gatts_add_char(gl_profile_tab[PROFILE_APP_ID].service_handle, &ssid_uuid,
                                   ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE,
                                   &attr_value, NULL);
        }

        {
			esp_attr_value_t attr_value = {
	            .attr_max_len = 32,
	            .attr_len = 0,   
	            .attr_value = NULL,
	        };
			
            esp_bt_uuid_t pass_uuid = { .len = ESP_UUID_LEN_128 };
            memcpy(pass_uuid.uuid.uuid128, WIFI_PASS_CHAR_UUID_128, ESP_UUID_LEN_128);
            esp_ble_gatts_add_char(gl_profile_tab[PROFILE_APP_ID].service_handle, &pass_uuid,
                                   ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE,
                                   &attr_value, NULL);
        }
        
        {
			esp_attr_value_t attr_value = {
	            .attr_max_len = 32,
	            .attr_len = 0,   
	            .attr_value = NULL,
	        };
			
		    esp_bt_uuid_t broker_ip_uuid = { .len = ESP_UUID_LEN_128 };
		    memcpy(broker_ip_uuid.uuid.uuid128, BROKER_IP_CHAR_UUID_128, ESP_UUID_LEN_128);
		    esp_ble_gatts_add_char(gl_profile_tab[PROFILE_APP_ID].service_handle, &broker_ip_uuid,
		                           ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE,
		                           &attr_value, NULL);
		}
		
		{		
			esp_attr_value_t attr_value = {
	            .attr_max_len = 32,
	            .attr_len = 0,   
	            .attr_value = NULL,
	        };
			
		    esp_bt_uuid_t board_name_uuid = { .len = ESP_UUID_LEN_128 };
		    memcpy(board_name_uuid.uuid.uuid128, BOARD_NAME_CHAR_UUID_128, ESP_UUID_LEN_128);
		    esp_ble_gatts_add_char(gl_profile_tab[PROFILE_APP_ID].service_handle, &board_name_uuid,
		                           ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE,
		                           &attr_value, NULL);
		}

        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_APP_ID].service_handle);
        break;
        
    case ESP_GATTS_ADD_CHAR_EVT:
        if (memcmp(param->add_char.char_uuid.uuid.uuid128, WIFI_SSID_CHAR_UUID_128, ESP_UUID_LEN_128) == 0) {
            gl_profile_tab[PROFILE_APP_ID].ssid_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "SSID CHAR HANDLE = %d", param->add_char.attr_handle);
        } else if (memcmp(param->add_char.char_uuid.uuid.uuid128, WIFI_PASS_CHAR_UUID_128, ESP_UUID_LEN_128) == 0) {
            gl_profile_tab[PROFILE_APP_ID].pass_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "PASS CHAR HANDLE = %d", param->add_char.attr_handle);
        }
        if (memcmp(param->add_char.char_uuid.uuid.uuid128, BROKER_IP_CHAR_UUID_128, ESP_UUID_LEN_128) == 0) {
		    gl_profile_tab[PROFILE_APP_ID].broker_ip_char_handle = param->add_char.attr_handle;
		    ESP_LOGI(GATTS_TAG, "BROKER IP CHAR HANDLE = %d", param->add_char.attr_handle);
		} else if (memcmp(param->add_char.char_uuid.uuid.uuid128, BOARD_NAME_CHAR_UUID_128, ESP_UUID_LEN_128) == 0) {
		    gl_profile_tab[PROFILE_APP_ID].board_name_char_handle = param->add_char.attr_handle;
		    ESP_LOGI(GATTS_TAG, "BOARD NAME CHAR HANDLE = %d", param->add_char.attr_handle);
		}
        break;
        
    case ESP_GATTS_WRITE_EVT: {
        char *str_value = (char*)malloc(param->write.len + 1);
        if (str_value) {
            memcpy(str_value, param->write.value, param->write.len);
            str_value[param->write.len] = '\0';

            if (param->write.handle == gl_profile_tab[PROFILE_APP_ID].ssid_char_handle) {
                ESP_LOGI(GATTS_TAG, "Received SSID: %s", str_value);
                wifi_ssid_callback(str_value);
            } else if (param->write.handle == gl_profile_tab[PROFILE_APP_ID].pass_char_handle) {
                ESP_LOGI(GATTS_TAG, "Received Password: %s", str_value);
                wifi_password_callback(str_value);
            }
            if (param->write.handle == gl_profile_tab[PROFILE_APP_ID].broker_ip_char_handle) {
			    ESP_LOGI(GATTS_TAG, "Received Broker IP: %s", str_value);
			    broker_ip_callback(str_value);
			} else if (param->write.handle == gl_profile_tab[PROFILE_APP_ID].board_name_char_handle) {
			    ESP_LOGI(GATTS_TAG, "Received Board Name: %s", str_value);
			    board_name_callback(str_value);
			}

            free(str_value);
        }
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        esp_ble_gatts_send_response(gatts_if, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, NULL);
        exec_write_event_env(&a_prepare_write_env, param);
        break;
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client connected, conn_id=%d", param->connect.conn_id);
        gl_profile_tab[PROFILE_APP_ID].conn_id = param->connect.conn_id;
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client disconnected, restarting advertising...");
        esp_ble_gap_start_advertising(&adv_params);
        break;
    default:
        break;
    }
}

void gatts_event_handler(esp_gatts_cb_event_t event, 
                                esp_gatt_if_t gatts_if, 
                                esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_REG_EVT && param->reg.status == ESP_GATT_OK) {
        gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
    }
    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile_tab[PROFILE_APP_ID].gatts_if) {
        if (gl_profile_tab[PROFILE_APP_ID].gatts_cb) {
            gl_profile_tab[PROFILE_APP_ID].gatts_cb(event, gatts_if, param);
        }
    }
}

void initialize_ble(configuration_received_callback ssid_callback, 
					configuration_received_callback password_callback,
					configuration_received_callback _broker_ip_callback, 
					configuration_received_callback _board_name_callback) {
								
	wifi_ssid_callback = ssid_callback;
	wifi_password_callback = password_callback; 					
	broker_ip_callback = _broker_ip_callback;
	board_name_callback = _board_name_callback;
								
	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    gl_profile_tab[PROFILE_APP_ID].gatts_cb = gatts_profile_event_handler;
    gl_profile_tab[PROFILE_APP_ID].gatts_if = ESP_GATT_IF_NONE;

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_APP_ID));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));
}