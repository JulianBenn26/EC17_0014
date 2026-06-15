/*
 * File:   i2c_lcd.c
 * Author: Julianb
 *
 * Created on May 27, 2025, 10:19 AM
 */
#include "i2c_lcd.h"

// Low-level I2C
void I2C1_Init() {
    I2C1BRG = 157; // For 100kHz assuming Fcy = 16MHz
    //I2C1CONbits.ON = 1;
}

void I2C1_Start() {
    I2C1CONbits.SEN = 1;
    while(I2C1CONbits.SEN);
}

void I2C1_Stop() {
    I2C1CONbits.PEN = 1;
    while(I2C1CONbits.PEN);
}

void I2C1_Write(uint8_t data) {
    I2C1TRN = data;
    while(I2C1STATbits.TRSTAT);
    while(I2C1STATbits.ACKSTAT); // Wait for ACK
}

// Send byte to LCD via PCF8574
void LCD_Send_Byte(uint8_t data, uint8_t mode) {
    uint8_t high = data & 0xF0;
    uint8_t low = (data << 4) & 0xF0;
    
    uint8_t en = 0x04; // Enable bit
    uint8_t rs = (mode ? 0x01 : 0x00); // RS bit

uint8_t parts[2] = {high, low};
for (int i = 0; i < 2; i++) {
    uint8_t part = parts[i];
    I2C1_Start();
    I2C1_Write(LCD_ADDR << 1);
    I2C1_Write(part | rs | 0x08);         // Backlight on
    I2C1_Write(part | rs | en | 0x08);    // EN high
    I2C1_Write(part | rs | 0x08);         // EN low
    I2C1_Stop();
}
}

void LCD_Send_Cmd(uint8_t cmd) {
    LCD_Send_Byte(cmd, 0);
    __delay_ms(2);
}

void LCD_Send_Char(char data) {
    LCD_Send_Byte(data, 1);
    __delay_ms(2);
}

void LCD_Send_String(const char *str) {
    while (*str) {
        LCD_Send_Char(*str++);
    }
}

void LCD_Set_Cursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? 0x80 + col : 0xC0 + col;
    LCD_Send_Cmd(addr);
}

void LCD_Init() {
    __delay_ms(50);
    LCD_Send_Cmd(0x30);
    __delay_ms(5);
    LCD_Send_Cmd(0x30);
    __delay_us(150);
    LCD_Send_Cmd(0x30);
    LCD_Send_Cmd(0x20); // Set to 4-bit
    LCD_Send_Cmd(0x28); // 2 line, 5x8 font
    LCD_Send_Cmd(0x0C); // Display on, cursor off
    LCD_Send_Cmd(0x06); // Entry mode
    LCD_Send_Cmd(0x01); // Clear display
    __delay_ms(2);
}
