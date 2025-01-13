#ifndef MAIN_BLE_SCANNER_H_
#define MAIN_BLE_SCANNER_H_

#include "common.h"

typedef void (*ble_device_found_callback)(const char*, const char*, int);

void gap_scan_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

void initialize_ble_scanner(ble_device_found_callback on_discovery);

#endif 