#include "lcd_i2c.h"

void app_main(void)
{
	i2c_master_init();
    lcd_init();
    
    lcd_send_string("Hello world!");
    
    lcd_second_line();
    
    lcd_send_string(" --------------");
    
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    lcd_clear();
    
    lcd_first_line();
    
    lcd_scroll_text("Bluetooth scanner LCD demo");
    
    vTaskDelay(pdMS_TO_TICKS(5000)); 
    
    for(int i=1; i<=10; i++) {
		lcd_clear();
    
    	lcd_first_line();
    
    	lcd_send_string("Loop example");
    
    	lcd_second_line();
    	
    	lcd_send_string("i = ");
    	
    	lcd_send_int(i);
    	
    	vTaskDelay(pdMS_TO_TICKS(1000));
	}
    
    vTaskDelay(pdMS_TO_TICKS(5000));
}
