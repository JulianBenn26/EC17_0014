
#define FCY 4000000UL

#include "FT813.h"
//#include "spi1.h"
#include <libpic30.h> // For __delay_ms

#define REG_ID         0x302000UL



void FT813_Init() {

 // cycle PD 
   
 LATDbits.LATD6 = 0;     // Power  off display
    
    __delay_ms(50); // 
    
 LATDbits.LATD6 = 1;     // Power up display    

  __delay_ms(50);
 
 FT813_SendCommand(0x44);  // set ext clock
  __delay_ms(1);
 
 FT813_SendCommand(0x00);  // send active command 
  
 __delay_ms(100); // wait for self diagnostics
 __delay_ms(100); // wait for self diagnostics
  __delay_ms(100); // wait for self diagnostics
   __delay_ms(100); // wait for self diagnostics
}

void FT813_SendCommand(uint8_t cmd) {
   
    
     SPI1_Exchange8bit(0x00); // was 'exhangeByte"
     SPI1_Exchange8bit(0x00); // was 'exhangeByte"
     SPI1_Exchange8bit(0x00); // was 'exhangeByte"
    SPI1_Exchange8bit(cmd); // was 'exhangeByte"
   // lcd_print  ("FT813_send command"); // temp indicator
   
   
    
}

uint8_t FT813_Read(uint32_t addr) {
    LATDbits.LATD11 = 0;
    SPI1_Exchange8bit(0x00);  // Dummy read
 
    return SPI1_Exchange8bit(0x00); // Return dummy byte (example)
    __delay_ms(1);
      LATDbits.LATD11 = 1; 
      
      
}

uint32_t FT813_Read24(uint32_t addr) {
    LATDbits.LATD11 = 0;  // CS LOW
    __delay_us(50);
    SPI1_Exchange8bit(0x00);  // just added
    SPI1_Exchange8bit(((addr >> 16) & 0xFF) );  // MSB with read bit added - 0x3F was 0xFF removed| 0x80
    SPI1_Exchange8bit((addr >> 8) & 0xFF);            // MID
    SPI1_Exchange8bit(addr & 0xFF);                   // LSB
    SPI1_Exchange8bit(0x00);                          // Dummy-
    SPI1_Exchange8bit(0x00);                          // Dummy-

    uint8_t b1 = SPI1_Exchange8bit(0x00);
    uint8_t b2 = SPI1_Exchange8bit(0x00);
    uint8_t b3 = SPI1_Exchange8bit(0x00);
    
      __delay_us(50);


    LATDbits.LATD11 = 1;  // CS HIGH

    return (b1 << 16) | (b2 << 8) | b3;
}





void FT813_Write(uint32_t addr, uint8_t data) {
    LATDbits.LATD11= 0;
    
    SPI1_Exchange((addr >> 16) | 0x80 & 0x3F);  // MSB with write flag - & 0x3F added
    SPI1_Exchange((addr >> 8) & 0xFF);
    SPI1_Exchange(addr & 0xFF);
    SPI1_Exchange8bit(data);

    LATDbits.LATD11 = 1;
}

void FT813_DisplayMessage(const char *msg) {
    // Stub: Create a display list with text (requires full FT813 graphics lib)
}

void FT813_SetClock(uint8_t multiplier) {
    LATDbits.LATD11 = 0;
    __delay_us(50);

    SPI1_Exchange8bit(0x61);        // CLKSEL
    SPI1_Exchange8bit(multiplier);  // Multiplier (1?6)
    SPI1_Exchange8bit(0x00);        // Padding

    __delay_us(50);
    LATDbits.LATD11 = 1;
}
