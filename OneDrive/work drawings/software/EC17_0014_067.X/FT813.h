#ifndef FT813_H
#define FT813_H

#include <xc.h>
#include <stdint.h>

// Pin controls (adjust for your actual pin mappings)
#define FT813_CS_LAT   LATDbits.LATD11
#define FT813_CS_TRIS  TRISDbits.TRISD11
//#define FT813_PD_LAT   LATBbits.LATB1
//#define FT813_PD_TRIS  TRISBbits.TRISB1
//#define FT813_INT_PORT PORTBbits.RB2
//#define FT813_INT_TRIS TRISBbits.TRISB2

void FT813_Init(void);
void FT813_SendCommand(uint8_t cmd);
uint8_t FT813_Read(uint32_t addr);
void FT813_Write(uint32_t addr, uint8_t data);
void FT813_DisplayMessage(const char *msg);

#endif
