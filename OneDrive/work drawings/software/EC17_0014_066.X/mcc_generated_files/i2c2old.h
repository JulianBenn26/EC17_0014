#ifndef I2C2_H
#define I2C2_H

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

void I2C2_Init(uint32_t clockHz);
void I2C2_Start(void);
void I2C2_Stop(void);
void I2C2_Restart(void);
bool I2C2_Write(uint8_t data);
uint8_t I2C2_Read(bool ack);

#endif
