#include "ble_scanner.h"
#include "esp_gap_ble_api.h"
#include "tags.h"

static ble_device_found_callback on_discovery_callback = NULL;

void gap_scan_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            ESP_LOGI(GATTS_TAG, "Scan parameters set, starting scan...");
            esp_ble_gap_start_scanning(0); // 0 oznacza nieograniczony czas skanowania
            break;

        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TAG, "Failed to start scanning");
            } else {
                ESP_LOGI(GATTS_TAG, "Scanning started successfully");
            }
            break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            // Poprawna deklaracja zmiennej scan_result
            struct ble_scan_result_evt_param *scan_result = &param->scan_rst;

            if (scan_result->search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
				int rssi = scan_result->rssi;
				char addr_str[18];
				
				// Formatuj adres MAC jako string szesnastkowy z dwukropkami
				snprintf(addr_str, sizeof(addr_str),
				         "%02x:%02x:%02x:%02x:%02x:%02x",
				         scan_result->bda[0], scan_result->bda[1], scan_result->bda[2],
				         scan_result->bda[3], scan_result->bda[4], scan_result->bda[5]);         
			                
                uint8_t length = 0;
				uint8_t* name = esp_ble_resolve_adv_data(scan_result->ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &length);
				
                if (name) {
					ESP_LOGI(GATTS_TAG, "Device found: RSSI = %d, Addr: %s", rssi, addr_str);
					
                    ESP_LOGI(GATTS_TAG, "Device name: %s", name);
                    
                    on_discovery_callback((char*)name, addr_str, rssi);                        
                }
            }
            break;
        }

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TAG, "Failed to stop scanning");
            } else {
                ESP_LOGI(GATTS_TAG, "Scanning stopped successfully");
            }
            break;

        default:
            break;
    }
}

void initialize_ble_scanner(ble_device_found_callback on_discovery) {
	on_discovery_callback = on_discovery;
	
	ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_scan_event_handler));
	
	esp_ble_scan_params_t scan_params = {
        .scan_type              = BLE_SCAN_TYPE_PASSIVE,
        .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval          = 0x50,
        .scan_window            = 0x30,
        .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
    };

    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));
}