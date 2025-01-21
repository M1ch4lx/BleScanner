#include "ble_scanner.h"
#include "esp_gap_ble_api.h"
#include "tags.h"
#include <ctype.h>

static ble_device_found_callback on_discovery_callback = NULL;

#define SCAN_DURATION 10   // Czas skanowania w sekundach
#define SCAN_INTERVAL 5    // Czas między kolejnymi skanowaniami w sekundach

void start_scanner(void) {
    ESP_LOGI(GATTS_TAG, "Starting BLE scan for %d seconds", SCAN_DURATION);
    esp_ble_gap_start_scanning(SCAN_DURATION);
}

// Task do cyklicznego uruchamiania i zatrzymywania skanera
void scanner_task(void *param) {
    while (1) {
        start_scanner();
        vTaskDelay((SCAN_DURATION + SCAN_INTERVAL) * 1000 / portTICK_PERIOD_MS);
    }
}

void sanitize_name(char* name, char* sanitized_name, size_t max_length) {
    size_t i, j = 0;
    for (i = 0; name[i] != '\0' && j < max_length - 1; i++) {
        if (isprint((unsigned char)name[i])) {
            sanitized_name[j++] = name[i];
        } else {
            sanitized_name[j++] = ' ';
        }
    }
    sanitized_name[j] = '\0';
}

void gap_scan_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            ESP_LOGI(GATTS_TAG, "Scan parameters set, starting scan...");
            xTaskCreate(scanner_task, "scanner_task", 4096, NULL, 5, NULL);
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
				
				if(name) {
					char sanitized_name[ESP_BLE_ADV_DATA_LEN_MAX] = {0};
					sanitize_name((char*)name, sanitized_name, ESP_BLE_ADV_DATA_LEN_MAX);
				
					on_discovery_callback(sanitized_name, addr_str, rssi);
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
        .scan_type              = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0xA0,
		.scan_window = 0xA0,
        .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
    };

    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));
}