#include "ble.h"
#include <xc.h>
#include <string.h>


#define FCY 4000000UL
#define BAUDRATE 9600
#define BRGVAL ((FCY / BAUDRATE) / 16) - 1
#include <libpic30.h> // For __delay_ms


void UART1_Init(void) {
    // Unlock PPS
    __builtin_write_OSCCONL(OSCCON & ~(1 << 6));

    // Map UART1 TX/RX to RP pins (adjust as needed)
    RPOR1bits.RP2R = 3;   // RP2 -> U1TX
    RPINR18bits.U1RXR = 3; // RP3 -> U1RX

    // Lock PPS
    __builtin_write_OSCCONL(OSCCON | (1 << 6));

    // UART1 configuration
    U1MODEbits.UARTEN = 0;
    U1MODEbits.BRGH = 0;
    U1BRG = BRGVAL;
//LATFbits.LATF2  = 1;   // E6 temp indicator

//DELAY_milliseconds(200);
    __delay_ms(200);
    U1MODEbits.PDSEL = 0;
    U1MODEbits.STSEL = 0;
    
//LATFbits.LATF2  = 0 ;   // E6 temp indicator
//DELAY_milliseconds(200);
__delay_ms(200);

    U1STAbits.UTXEN = 1;
    U1MODEbits.UARTEN = 1;
    LATFbits.LATF2  = 1;   // E6 temp indicator  
}

void UART1_WriteChar(char c) {
    while (U1STAbits.UTXBF);
    U1TXREG = c;
}

void UART1_WriteString(const char *str) {
    while (*str) {
        UART1_WriteChar(*str++);
    }
}

char UART1_ReadChar(void) {
    while (!U1STAbits.URXDA);
    return U1RXREG;
}

void UART1_ReadLine(char *buffer, int maxLen) {
    int i = 0;
    char c;
    do {
        c = UART1_ReadChar();
        if (i < maxLen - 1) buffer[i++] = c;
    } while (c != '\n' && c != '\r');
    buffer[i] = '\0';
}

void BLE_SendCommand(const char *cmd) {
    UART1_WriteString(cmd);
    UART1_WriteString("\r");
}

void BLE_ReadResponse(char *response, int maxLen) {
    UART1_ReadLine(response, maxLen);
}

void BLE_Init(void) {
    UART1_Init();

    // Configure CMD and WKE pins
    TRISDbits.TRISD11 = 0; // WKE
    TRISDbits.TRISD10 = 0; // CMD
    LATDbits.LATD11 = 1;   // Wake
    LATDbits.LATD10 = 0;   // Command mode

    //DELAY_milliseconds(100);
    __delay_ms(100);

    BLE_SendCommand("SF,1"); // Factory reset
    //DELAY_milliseconds(100);
    __delay_ms(100);

    BLE_SendCommand("SR,3200000"); // Enable device info service
    //DELAY_milliseconds(100);
    __delay_ms(100);

    BLE_SendCommand("SN,JulianBLE"); // Set device name
    //DELAY_milliseconds(100);
    __delay_ms(100);

    BLE_SendCommand("R,1"); // Reboot
    //DELAY_milliseconds(500);
    __delay_ms(500);

    BLE_SendCommand("A"); // Start advertising
}

