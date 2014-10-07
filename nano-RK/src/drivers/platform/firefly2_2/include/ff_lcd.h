/******************************************************************************
*
*  Filename: ff_lcd.h
*  Author: Vikram Rajkumar
*  Last Updated: 2009-07-08
*
*  This driver interfaces the FireFly 2.2 with the FireFly LCD v1 board.
*
*  The FireFly LCD v1 board includes a Sitronix ST7066U LCD controller using
*  either an 8-bit or 4-bit data interface.
*
*  Function definitions and pin defines can be found in files 'ff_lcd.c' and
*  'lcd_driver.c'.
*
*  Note: If possible, do not call nrk_setup_uart() in main() because the board
*  uses all of the FireFly's UART ports.
*
*******************************************************************************/

#ifndef _FF_LCD_H_
#define _FF_LCD_H_

// Hardware
#define LCD_4BIT_BUS_MODE         // Comment out to run using 8-bit interface
#define LCD_MAX_CHARS_PER_LINE 16 // Change to max characters per line of display

// Functions
void lcd_wait_us(uint32_t micro_secs);

uint8_t lcd_pin_get(uint8_t pin);
void lcd_pin_set(uint8_t pin, uint8_t value);

void lcd_setup();

uint8_t lcd_switch_get(uint8_t sw);
uint8_t lcd_switch_pressed(uint8_t sw);
uint8_t lcd_switch_released(uint8_t sw);

void lcd_led_set(uint8_t led, uint8_t value);

#endif
