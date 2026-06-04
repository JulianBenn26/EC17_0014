#include "i2c2.h"


#define FCY 4000000UL  // Change to your actual instruction clock (Fosc / 2)
#include <libpic30.h>  // For __delay_xx functions



void I2C2_Initialize(uint32_t clockHz) {
    I2C2CONbits.I2CEN = 0;   // Disable I2C2 module
    //I2C2BRG = 0x27;  // Baud rate generator
    I2C2BRG = (FCY / (2 * clockHz)) - 1;  // Baud rate generator
    I2C2CONbits.I2CEN = 1;   // Enable I2C2 module
    //DELAY_milliseconds(100);
     __delay_ms(100);
    #define FCY 4000000UL
}

void I2C2_Start(void) {
    I2C2CONbits.SEN = 1;
    while (I2C2CONbits.SEN); // Wait for start to complete
}

void I2C2_Stop(void) {
    I2C2CONbits.PEN = 1;    
    while (I2C2CONbits.PEN); // Wait for stop to complete
}

void I2C2_Restart(void) {
    I2C2CONbits.RSEN = 1;
    while (I2C2CONbits.RSEN); // Wait for restart to complete
}

bool I2C2_Write(uint8_t data) {
    I2C2TRN = data;
    while (I2C2STATbits.TRSTAT);         // Wait for transmission to complete
    return !I2C2STATbits.ACKSTAT;        // Return true if ACK received
}

uint8_t I2C2_Read(bool ack) {
    I2C2CONbits.RCEN = 1;                // Enable receive
    while (!I2C2STATbits.RBF);           // Wait for buffer full
    uint8_t data = I2C1RCV;
    I2C2CONbits.ACKDT = ack ? 0 : 1;     // 0 = ACK, 1 = NACK
    I2C2CONbits.ACKEN = 1;               // Send ACK/NACK
    while (I2C2CONbits.ACKEN);           // Wait until ACK/NACK sent
    return data;
}
