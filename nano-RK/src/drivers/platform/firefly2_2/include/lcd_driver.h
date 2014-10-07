/******************************************************************************
*
*  Filename: lcd_driver.h
*  Author: Vikram Rajkumar
*  Last Updated: 2009-07-08
*
*  This driver interfaces the FireFly 2.2 with a Sitronix ST7066U LCD
*  controller using either an 8-bit or 4-bit data interface.
*
*  The Sitronix ST7066U is compatible with the Hitachi HD44780 controller.
*  Software written for modules that use the HD44780 should work without
*  modification.
*
*  Function definitions and pin defines can be found in files 'ff_lcd.c' and
*  'lcd_driver.c'.
*
*******************************************************************************/

#ifndef _LCD_DRIVER_H_
#define _LCD_DRIVER_H_

// Escape Characters
#define LCD_CHAR_DEGREE 223
#define LCD_CHAR_ARROW_LEFT 127
#define LCD_CHAR_ARROW_RIGHT 126

// Functions
void lcd_control_pins_set(uint8_t rs, uint8_t rw);
void lcd_enable_cycle();
void lcd_data_pins_set(uint8_t d7, uint8_t d6, uint8_t d5, uint8_t d4, uint8_t d3, uint8_t d2, uint8_t d1, uint8_t d0);

void lcd_initialize();
void lcd_function_set_4bit();
void lcd_function_set();

void lcd_display_on();
void lcd_display_off();

void lcd_display_clear();
void lcd_entry_mode_set();

void lcd_line_set(uint8_t line);
void lcd_char_send(char c);

void lcd_string_display_line(char* s, uint8_t line);
void lcd_line_clear(uint8_t line);

void lcd_string_display_wrap(char* s);
void lcd_string_display_escape(char* s);

void lcd_string_array_load(char* s);
void lcd_string_array_load_helper(char* s);
void lcd_string_array_add(char* s);

void lcd_string_array_cycle();
void lcd_string_array_cycle_reverse();

#endif
