#ifndef I2C_LCD_H
#define I2C_LCD_H

#include <xc.h>
#include <stdint.h>

// LCD address (change if needed, usually 0x27 or 0x3F)
#define LCD_ADDR 0x78
#define _XTAL_FREQ 16000000 // Define system clock

// I2C Functions
void I2C1_Init();
void I2C1_Start();
void I2C1_Stop();
void I2C1_Write(uint8_t data);

// LCD Control Functions
void LCD_Init();
void LCD_Send_Cmd(uint8_t cmd);
void LCD_Send_Char(char data);
void LCD_Send_String(const char *str);
void LCD_Set_Cursor(uint8_t row, uint8_t col);
void LCD_Send_Byte(uint8_t data, uint8_t mode);

#endif


