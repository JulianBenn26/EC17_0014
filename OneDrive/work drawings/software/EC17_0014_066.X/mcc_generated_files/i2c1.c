#include "i2c1.h"


#define FCY 4000000UL  // Change to your actual instruction clock (Fosc / 2)
#include <libpic30.h>  // For __delay_xx functions


void I2C1_Initialize(uint32_t clockHz) {
  
    I2C1CONbits.I2CEN = 0;   // Disable I2C1 module
    I2C1BRG = (FCY / (2 * clockHz)) - 1;  // Baud rate generator
    I2C1CONbits.I2CEN = 1;   // Enable I2C1 module
   //DELAY_milliseconds(1);
    __delay_ms(1);
}

void I2C1_Start(void) {
    I2C1CONbits.SEN = 1;
    while (I2C1CONbits.SEN); // Wait for start to complete
}

void I2C1_Stop(void) {
    I2C1CONbits.PEN = 1;
    while (I2C1CONbits.PEN); // Wait for stop to complete
}

void I2C1_Restart(void) {
    I2C1CONbits.RSEN = 1;
    while (I2C1CONbits.RSEN); // Wait for restart to complete
}

bool I2C1_Write(uint8_t data) {
    I2C1TRN = data;
    while (I2C1STATbits.TRSTAT);         // Wait for transmission to complete
    return !I2C1STATbits.ACKSTAT;        // Return true if ACK received
}

uint8_t I2C1_Read(bool ack) {
    I2C1CONbits.RCEN = 1;                // Enable receive
    while (!I2C1STATbits.RBF);           // Wait for buffer full
    uint8_t data = I2C1RCV;
    I2C1CONbits.ACKDT = ack ? 0 : 1;     // 0 = ACK, 1 = NACK
    I2C1CONbits.ACKEN = 1;               // Send ACK/NACK
    while (I2C1CONbits.ACKEN);           // Wait until ACK/NACK sent
    return data;
}
