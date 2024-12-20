#include "lcd_i2c.h"

void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

void lcd_send_nibble(uint8_t nibble) {
    uint8_t data = nibble | LCD_BACKLIGHT | LCD_ENABLE;
    i2c_master_write_to_device(I2C_MASTER_NUM, LCD_ADDR, &data, 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    data &= ~LCD_ENABLE;  // Wyłączenie EN
    i2c_master_write_to_device(I2C_MASTER_NUM, LCD_ADDR, &data, 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

void lcd_send_byte(uint8_t data, uint8_t mode) {
    uint8_t high_nibble = (data & 0xF0) | mode;
    uint8_t low_nibble = ((data << 4) & 0xF0) | mode;

    lcd_send_nibble(high_nibble);
    lcd_send_nibble(low_nibble);
}

void lcd_init() {
    vTaskDelay(pdMS_TO_TICKS(50));  // Czekanie na start wyświetlacza

    lcd_send_nibble(0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x30);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_nibble(0x30);

    lcd_send_nibble(0x20);  // Tryb 4-bitowy
    lcd_send_byte(0x28, 0); // 2 linie, 5x8 punktów
    lcd_send_byte(0x08, 0); // Wyłącz wyświetlacz
    lcd_send_byte(0x01, 0); // Wyczyść wyświetlacz
    lcd_send_byte(0x06, 0); // Przesuwanie kursora w prawo
    lcd_send_byte(0x0C, 0); // Włącz wyświetlacz, wyłącz kursor
}

void lcd_first_line() {
	lcd_send_byte(0x80, 0);
}

void lcd_second_line() {
	lcd_send_byte(0xC0, 0);
}

void lcd_clear() {
	lcd_send_byte(0x01, 0);
}

void lcd_scroll_text(const char *text) {
    lcd_send_string(text);
	
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	
	lcd_scroll_by(strlen(text) - LCD_CHARACTERS, LCD_SCROLL_RIGHT);
}

void lcd_scroll_left() {
	lcd_send_byte(0x1C, 0);
}

void lcd_scroll_right() {
	lcd_send_byte(0x18, 0);
}

void lcd_scroll(int direction) {
	lcd_send_byte(direction, 0);
}

void lcd_scroll_by(int characters, int direction) {
    for (int i = 0; i < characters; i++) {
		vTaskDelay(500 / portTICK_PERIOD_MS);
        lcd_send_byte(direction, 0);  // Przesuń w lewo
    }
}

void lcd_send_char(char character) {
    lcd_send_byte(character, LCD_RS);
}

void lcd_send_string(const char *str) {
    while (*str) {
        lcd_send_char(*str++);
    }
}

void lcd_send_char_array(char* str, int size) {
	for(int i=0; i<size; i++) {
		lcd_send_char(str[i]);
	}
}

void lcd_send_int(int value) {
	char str[12];

	itoa(value, str, 10);
	
	lcd_send_string(str);
}