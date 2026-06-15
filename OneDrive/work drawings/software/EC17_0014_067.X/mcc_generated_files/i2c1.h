#ifndef I2C_H
#define I2C_H

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

void I2C1_Init(uint32_t clockHz);
void I2C1_Start(void);
void I2C1_Stop(void);
void I2C1_Restart(void);
bool I2C1_Write(uint8_t data);
uint8_t I2C1_Read(bool ack);

#endif
