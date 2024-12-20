#ifndef MAIN_LCD_I2C_H_
#define MAIN_LCD_I2C_H_

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO 22       // GPIO dla SCL
#define I2C_MASTER_SDA_IO 21       // GPIO dla SDA
#define I2C_MASTER_NUM I2C_NUM_0   // Numer portu I2C
#define I2C_MASTER_FREQ_HZ 100000  // Częstotliwość I2C
#define LCD_ADDR 0x27              // Adres I2C adaptera PCF8574
#define I2C_MASTER_TIMEOUT_MS 1000 // Timeout dla I2C
#define LCD_BACKLIGHT 0x08  // Podświetlenie ON
#define LCD_ENABLE 0x04     // Bit EN
#define LCD_RW 0x02         // Bit RW
#define LCD_RS 0x01         // Bit RS

#define LCD_CHARACTERS 16
#define LCD_LINES 2

#define LCD_SCROLL_LEFT 0x1C
#define LCD_SCROLL_RIGHT 0x18

#define LCD_DELAY() vTaskDelay(500 / portTICK_PERIOD_MS)

void i2c_master_init();

void lcd_send_nibble(uint8_t nibble);

void lcd_send_byte(uint8_t data, uint8_t mode);

void lcd_init();

void lcd_send_char(char character);

void lcd_send_string(const char *str);

void lcd_send_char_array(char* str, int size);

void lcd_first_line();

void lcd_second_line();

void lcd_clear();

void lcd_scroll_text(const char *text);

void lcd_scroll_left();

void lcd_scroll_right();

void lcd_scroll_by(int characters, int direction);

void lcd_send_int(int value);

#endif