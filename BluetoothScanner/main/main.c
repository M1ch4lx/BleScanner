#include "common.h"
#include "ble.h"
#include "lcd_i2c.h"
#include "esp_system.h"
#include "mqtt_client.h"
#include "ble_scanner.h"

#define NVS_NAMESPACE "wifi_config"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "password"

#define NVS_KEY_BROKER "broker"
#define NVS_KEY_BOARD_NAME "board_name"

#define DEFAULT_BOARD_NAME "pokoj_1"

// Button configuration
#define BUTTON_GPIO        GPIO_NUM_0
#define BUTTON_ACTIVE_LEVEL 0

#define DEFAULT_MQTT_BROKER_IP "mqtt://192.168.241.246"

// Wi-Fi variables
static EventGroupHandle_t wifi_event_group = NULL;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_DISCONNECTED_BIT = BIT1;
static bool wifi_connected = false;
static bool wifi_mode = false; // Wi-Fi connection mode ON/OFF
static bool mqtt_connected = false;

static char board_name[30] = {0};
static char broker_ip[30] = {0};

static void wifi_init_sta(const char *ssid, const char *pass);
static void wifi_stop(void);
static void wifi_try_connect_from_nvs(void);

////////////////////////////////////////
// NVS Handling
////////////////////////////////////////

static esp_err_t save_str_to_nvs(const char* key, const char* value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_str(nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "Error writing to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "Error committing to NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(GATTS_TAG, "Saved %s to NVS", key);
    }
    return err;
}

static bool print_current_nvs_creds(void) {
    nvs_handle_t nvs_handle;
    char ssid[64] = {0}, pass[64] = {0};
    size_t len;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) != ESP_OK) {
        ESP_LOGI(GATTS_TAG, "No NVS Namespace found");
        return false;
    }
    len = sizeof(ssid);
    if (nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid, &len) != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGI(GATTS_TAG, "No SSID in NVS");
        return false;
    }
    len = sizeof(pass);
    if (nvs_get_str(nvs_handle, NVS_KEY_PASS, pass, &len) != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGI(GATTS_TAG, "No PASS in NVS");
        return false;
    }
    nvs_close(nvs_handle);

    ESP_LOGI(GATTS_TAG, "Current NVS creds: SSID=%s PASS=%s", ssid, pass);
    
    lcd_clear();
    
    lcd_first_line();
    
    lcd_send_string(ssid);
    
    lcd_second_line();
    
    lcd_send_string(pass);
    
    return true;
}

static bool get_broker_ip_from_nvs(char* broker_ip, size_t len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Otwarcie przestrzeni nazw NVS w trybie tylko do odczytu
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(GATTS_TAG, "No NVS Namespace found");
        snprintf(broker_ip, len, DEFAULT_MQTT_BROKER_IP);  // Domyślny adres IP brokera
        return false;
    }

    // Odczytanie wartości IP brokera z NVS
    err = nvs_get_str(nvs_handle, NVS_KEY_BROKER, broker_ip, &len);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGI(GATTS_TAG, "No broker IP in NVS, using default");
        snprintf(broker_ip, len, DEFAULT_MQTT_BROKER_IP);  // Domyślny adres IP brokera
        return false;
    }

    ESP_LOGI(GATTS_TAG, "Broker IP from NVS: %s", broker_ip);
    return true;
}

// Funkcja do odczytu nazwy płytki
static bool get_board_name_from_nvs(char* board_name, size_t len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Otwarcie przestrzeni nazw NVS w trybie tylko do odczytu
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(GATTS_TAG, "No NVS Namespace found");
        snprintf(board_name, len, DEFAULT_BOARD_NAME);  // Domyślna nazwa płytki
        return false;
    }

    // Odczytanie nazwy płytki z NVS
    err = nvs_get_str(nvs_handle, NVS_KEY_BOARD_NAME, board_name, &len);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGI(GATTS_TAG, "No board name in NVS, using default");
        snprintf(board_name, len, DEFAULT_BOARD_NAME);  // Domyślna nazwa płytki
        return false;
    }

    ESP_LOGI(GATTS_TAG, "Board name from NVS: %s", board_name);
    return true;
}

////////////////////////////////////////
// Wi-Fi Handling
////////////////////////////////////////

static int wifi_connection_attempt_count = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, 
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START && wifi_mode) {
        ESP_LOGI(WIFI_TAG, "Wi-Fi STA start event, attempting to connect...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
       	if(wifi_connection_attempt_count == 0) {
			   xEventGroupSetBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
		   }
        
        ESP_LOGI(WIFI_TAG, "Wi-Fi disconnected event received.");
        if (wifi_mode) {
            ESP_LOGI(WIFI_TAG, "Wi-Fi mode is ON, retrying connection...");
            
            wifi_connection_attempt_count++;
            
            lcd_clear();
            lcd_first_line();
            lcd_send_string("Connecting wifi");
            lcd_second_line();
            lcd_send_string("Attempt: ");
            lcd_send_int(wifi_connection_attempt_count);       
            
            esp_wifi_connect();
        } else {
            ESP_LOGI(WIFI_TAG, "Wi-Fi mode is OFF, not retrying.");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "Wi-Fi connected successfully!");
        ESP_LOGI(WIFI_TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        wifi_connection_attempt_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        lcd_clear();
        lcd_first_line();
        lcd_send_string("Connected Wi-Fi");
    }
}

static esp_err_t unregister_wifi_event_handlers(void) {
    esp_err_t err = ESP_OK;
    // Usuń istniejące event handlery
    err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to unregister Wi-Fi event handler: %s", esp_err_to_name(err));
    }

    err = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to unregister IP event handler: %s", esp_err_to_name(err));
    }

    return err;
}

static void wifi_init_sta(const char *ssid, const char *pass) {
	esp_netif_create_default_wifi_sta();
	
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    unregister_wifi_event_handlers();
    
    esp_event_handler_instance_t instance_any_id, instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, &instance_got_ip));
	
    ESP_LOGI(WIFI_TAG, "Starting Wi-Fi with SSID:%s PASS:%s", ssid, pass);

    if (wifi_connected) {
        wifi_stop();
    }
	
	if(wifi_event_group != NULL) {
		vEventGroupDelete(wifi_event_group);
		wifi_event_group = NULL;
	}
	
    wifi_event_group = xEventGroupCreate();
    
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);       
	
	//lcd_clear();
	//lcd_first_line();
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "Connected Wi-Fi.");
        //lcd_send_string("Connected Wi-Fi");
    } else {
        ESP_LOGI(WIFI_TAG, "Failed to connect Wi-Fi.");
        //lcd_send_string("Connection failed");
    }
}

static void wifi_stop(void) {
    if (wifi_connected) {
        ESP_LOGI(WIFI_TAG, "Stopping Wi-Fi");
        esp_wifi_disconnect();
        esp_wifi_stop();
        wifi_connected = false;
    }
}

static void wifi_try_connect_from_nvs(void) {
    if (!wifi_mode) {
        ESP_LOGI(WIFI_TAG, "Wi-Fi mode is OFF, not connecting");
        return;
    }

    nvs_handle_t nvs_handle;
    char ssid[64] = {0}, pass[64] = {0};
    size_t len;
    bool creds_found = false;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        len = sizeof(ssid);
        if (nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid, &len) == ESP_OK) {
			ESP_LOGI(WIFI_TAG, "Got SSID: %s", ssid);
            len = sizeof(pass);
            if (nvs_get_str(nvs_handle, NVS_KEY_PASS, pass, &len) == ESP_OK) {
				ESP_LOGI(WIFI_TAG, "Got password: %s", pass);
                creds_found = true;
            }
        }
        nvs_close(nvs_handle);
    }

    if (creds_found) {
        ESP_LOGI(WIFI_TAG, "Connecting Wi-Fi using NVS creds");
        wifi_init_sta(ssid, pass);
    } else {
        ESP_LOGI(WIFI_TAG, "No creds in NVS, cannot connect Wi-Fi");
    }
}

////////////////////////////////////////
// Button Handling (Short press toggles Wi-Fi)
////////////////////////////////////////

static void button_task(void *arg) {
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_pullup_en(BUTTON_GPIO);
    gpio_pulldown_dis(BUTTON_GPIO);

    int last_state = 1;
    while (1) {
        int current_state = gpio_get_level(BUTTON_GPIO);
        if (current_state != last_state) {
            last_state = current_state;
            if (current_state == BUTTON_ACTIVE_LEVEL) {
                ESP_LOGI(BUTTON_TAG, "Button pressed");
            } else {
                ESP_LOGI(BUTTON_TAG, "Button released, toggling Wi-Fi mode");
                wifi_mode = !wifi_mode;
                ESP_LOGI(BUTTON_TAG, "Wi-Fi mode is now %s", wifi_mode ? "ON" : "OFF");
                if (wifi_mode) {
                    ESP_LOGI(BUTTON_TAG, "Attempting to connect Wi-Fi with stored creds...");
                    esp_restart();
                } else {
                    ESP_LOGI(BUTTON_TAG, "Wi-Fi mode OFF, stopping Wi-Fi if running");     
                                
                    lcd_clear();
                    lcd_first_line();
    				lcd_send_string("Configuration");
    				lcd_second_line();
    				lcd_send_string("mode"); 	            
                                   
                    wifi_stop();
                    esp_ble_gap_start_advertising(get_adv_params());
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

////////////////////////////////////////
// Main Application
////////////////////////////////////////

void on_ssid_received(const char* ssid) {
	save_str_to_nvs(NVS_KEY_SSID, ssid);
	
	print_current_nvs_creds();
}

void on_password_received(const char* password) {
	save_str_to_nvs(NVS_KEY_PASS, password);
	
	print_current_nvs_creds();
}

void on_broker_ip_received(const char* broker_ip) {
	char prefixed_broker_ip[64];

    snprintf(prefixed_broker_ip, sizeof(prefixed_broker_ip), "mqtt://%s", broker_ip);

    save_str_to_nvs(NVS_KEY_BROKER, prefixed_broker_ip);

    ESP_LOGI(MAIN_TAG, "Saved broker IP: %s", prefixed_broker_ip);
}

void on_board_name_received(const char* board_name) {
	save_str_to_nvs(NVS_KEY_BOARD_NAME, board_name);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(MAIN_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static esp_mqtt_client_handle_t mqtt_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    mqtt_client = event->client;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id = 0;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MAIN_TAG, "MQTT_EVENT_CONNECTED");
        //msg_id = esp_mqtt_client_publish(client, "/topic/ble_devices", "device:smartphone1;rssi:-45", 0, 1, 0);
        //msg_id = esp_mqtt_client_publish(client, "/topic/ble_devices", "device:smartphone2;rssi:-42", 0, 1, 0);
        //msg_id = esp_mqtt_client_publish(client, "/topic/ble_devices", "device:smartphone3;rssi:-48", 0, 1, 0);
        mqtt_connected = true;
        
        msg_id = esp_mqtt_client_subscribe(client, "/boards_command", 0);  // 0 to QoS (Quality of Service)
        ESP_LOGI(MAIN_TAG, "Subscribed to /boards_command, msg_id=%d", msg_id);
        
        msg_id = esp_mqtt_client_publish(client, "/boards", board_name, 0, 1, 0);
        ESP_LOGI(MAIN_TAG, "Published board name '%s' to topic '%s', msg_id=%d", board_name, "boards", msg_id);
        
        ESP_LOGI(MAIN_TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MAIN_TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MAIN_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MAIN_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MAIN_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
    	ESP_LOGI(MAIN_TAG, "MQTT_EVENT_DATA");
    
    	// Wyświetlenie topicu i wiadomości
    	printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    	printf("DATA=%.*s\r\n", event->data_len, event->data);
    
        ESP_LOGI(MAIN_TAG, "MQTT_EVENT_DATA");
        
        if (event->topic_len == strlen("/boards_command") && 
            strncmp(event->topic, "/boards_command", event->topic_len) == 0 &&
            event->data_len == strlen("introduce") && 
            strncmp((char*)event->data, "introduce", event->data_len) == 0) {
            
            char topic[50];
            snprintf(topic, sizeof(topic), "/boards");
            
            msg_id = esp_mqtt_client_publish(client, topic, board_name, 0, 1, 0);
            ESP_LOGI(MAIN_TAG, "Published board name '%s' to topic '%s', msg_id=%d", board_name, topic, msg_id);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MAIN_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(MAIN_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(MAIN_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
	if (get_broker_ip_from_nvs(broker_ip, sizeof(board_name))) {
        ESP_LOGI(MAIN_TAG, "Got broker IP: %s", broker_ip);
    } else {
		ESP_LOGI(MAIN_TAG, "Using default broker IP: %s", broker_ip);
	}
	
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_ip,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

static void mqtt_task() {
	while(1) {
		if(mqtt_connected) {
			// int msg_id = esp_mqtt_client_publish(mqtt_client, "/topic/ble_devices", "device:smartphone3;rssi:-48", 0, 1, 0);
        	// ESP_LOGI(MAIN_TAG, "sent publish successful, msg_id=%d", msg_id);
		}
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}

static void on_ble_device_discovery(const char* name, const char* address, int rssi) {
	ESP_LOGI(GATTS_TAG, "Device discovered:");
    ESP_LOGI(GATTS_TAG, "Name: %s", name);   
    ESP_LOGI(GATTS_TAG, "Address: %s", address); 
    ESP_LOGI(GATTS_TAG, "RSSI: %d", rssi);
    
    if(mqtt_connected) {
        char topic[50];
        snprintf(topic, sizeof(topic), "/%s/devices", board_name);

        char message[200];
        snprintf(message, sizeof(message),
                 "{\"name\": \"%s\", \"address\": \"%s\", \"rssi\": %d}",
                 name, address, rssi);

        int msg_id = esp_mqtt_client_publish(mqtt_client, topic, message, 0, 1, 0);
        ESP_LOGI(GATTS_TAG, "Published message '%s' to topic '%s', msg_id=%d", message, topic, msg_id);
	}
}

void app_main(void)
{
	i2c_master_init();
    lcd_init();
    
    lcd_send_string("Initializing");
	
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Initialize netif and event loop once
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
	
	if (get_board_name_from_nvs(board_name, sizeof(board_name))) {
        ESP_LOGI(MAIN_TAG, "Got board name: %s", board_name);
    } else {
		ESP_LOGI(MAIN_TAG, "Using default board name: %s", board_name);
	}
	
    // Start with Wi-Fi mode ON
    wifi_mode = true;

    // Initialize Wi-Fi system once
    //initialize_wifi();
	
	ESP_LOGI(MAIN_TAG, "Attempting to connect Wi-Fi with stored creds...");
	wifi_try_connect_from_nvs();
	
	initialize_ble(&on_ssid_received,
	&on_password_received,
	&on_broker_ip_received,
	&on_board_name_received);
	
	initialize_ble_scanner(on_ble_device_discovery);

    // Create a task to handle the button (short press toggles Wi-Fi mode)
    xTaskCreate(button_task, "button_task", 8192, NULL, 5, NULL);
    
    // Create a task to send data through mqtt broker
    xTaskCreate(mqtt_task, "mqtt_task", 8192, NULL, 4, NULL);
    
    mqtt_app_start();

    ESP_LOGI(MAIN_TAG, "Application started, waiting for button input and BLE writes.");
    // BLE is active and advertising
    // Press and release the button to toggle Wi-Fi mode ON/OFF
    // Update SSID/PASS via BLE anytime
}
