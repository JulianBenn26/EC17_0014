/*******************************************************************************
  MPLAB Code Configurator (MCC) Application Source File

  
  Company:
    Microchip Technology Inc.
  
  File Name:
    app.c

  Summary:
     This file contains the source code for the application Lab used in:

	Getting Started using PIC16F1xxx Class of Microchip University
	
	**************This code is for the Part 4 section**************

  Description:
    This file contains the source code for the MCC application.  It 
    implements the logic of the application's state machine and it may call 
    API routines of other MCC modules in the system, such as drivers,
    system services, etc.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
Copyright (c) 2013-2014 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/
// DOM-IGNORE-END


// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include "mcc_generated_files/mcc.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
*/

APP_LED_DATA appledData;
APP_KEY_DATA appkeyData;
APP_UART_DATA appuartData;


// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary callback funtions.
*/


// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary local functions.
*/


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************



/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_LED_Initialize ( void )
{
    /* Place the App LED state machine in its initial state. */
    appledData.state = APP_LED_STATE_INIT;

    /* TODO: Initialize your application's LED state machine and other
     * parameters.
     */
    appledData.TimerCount = 0;
}


void APP_KEY_Initialize ( void )
{
    /* Place the App S2 state machine in its initial state. */
    appkeyData.state = APP_KEY_STATE_INIT;

    /* TODO: Initialize your application's S2 state machine and other
     * parameters.
     */
    appkeyData.DebounceCount = 0;
}


void APP_UART_Initialize ( void )
{
    // Place the App KEY state machine in its initial state.
    appuartData.state = APP_UART_STATE_INIT;

    // TODO: Initialize your application's KEY state machine and other
    // parameters.

    appuartData.RxChar = 0;
    appuartData.TxChar = 0;
}


/******************************************************************************
  Function:
    void APP_LED_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_LED_Tasks ( void )
{
    /* Check the application's current state. */
    switch ( appledData.state )
    {
        /* Application's initial state. */
        case APP_LED_STATE_INIT:
        {

            appledData.state = APP_LED_STATE_WAIT;
            break;
        }

        /* TODO: implement your application state machine.*/
        case APP_LED_STATE_WAIT:
        {
            if (appledData.TimerCount >= 500) // 500 mS done?
                // yes then Blink LED
                appledData.state = APP_LED_STATE_BLINK_LED;
            break;
        }
        case APP_LED_STATE_BLINK_LED:
        {
            D7_Toggle(); //Toggle LED D2
            appledData.state = APP_LED_STATE_WAIT; // return to Wait state
            appledData.TimerCount = 0;              // Clear TimerCount    
            break;
        }
        case APP_LED_STATE_STOP:
        {
            D7_SetLow();    // Turn off LED D2
            break;
        }

        /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}

/* The APP_S2_Tasks checks for a valid keypress on S2.
 * The 4 states are INIT, HIGH, LOW and DEBOUNCE
 * INIT state initializes the start of the Task
 * HIGH is the normal default when S2 is not pressed. In it
 * we check for a high to low transition. If that occurs, we go to the
 * DEBOUNCE state.
 * LOW state is when the key is pressed. Here we look for a low to high transition
 * If that occurs then we go to the DEBOUNCE state.
 * In the DEBOUNCE state, a 20 mS delay is executed to take care of contact jitter
 * Once the delay is competed we look for a high to low or low to high transition.
 * If a low to high transition occurs then a valid keypress is acknowledged and 
 * blinking LED is stopped if LED blinking is ON or started if LED blinking is OFF.*/

void APP_KEY_Tasks ( void )
{
    /* Check the application's current state. */
    switch ( appkeyData.state )
    {
        /* Application's initial state. */
        case APP_KEY_STATE_INIT:
        {
            if (S1_GetValue() == 1) // Input high?
                // yes then go to High state
                appkeyData.state = APP_KEY_STATE_HIGH;
                // no then go to Low state
            else appkeyData.state = APP_KEY_STATE_LOW;
            break;
        }

        /* TODO: implement your application state machine.*/
        case APP_KEY_STATE_HIGH:
        {
            if (S1_GetValue() == 0) // low transition?
            {
                // yes then go to Debounce state 
                appkeyData.state = APP_KEY_STATE_DEBOUNCE;
                appkeyData.DebounceCount = 0;
            }
            break;
        }
        case APP_KEY_STATE_LOW:
        {
            if (S1_GetValue() == 1)  // high transition?
            {
                // yes then go to debounce state
                appkeyData.state = APP_KEY_STATE_DEBOUNCE;
                appkeyData.DebounceCount = 0;
            }
            break;
        }        
        case APP_KEY_STATE_DEBOUNCE:
        {
            if (appkeyData.DebounceCount >= 20) // 20 mS debounce time over?
                if (S1_GetValue() == 0)         // yes then check if input low
                    appkeyData.state = APP_KEY_STATE_LOW; // yes, go to low state
                else { // no then go to high state
                    appkeyData.state = APP_KEY_STATE_HIGH;
                    if (appledData.state == APP_LED_STATE_STOP) // blinking stopped?
                        // yes then resume blinking
                        appledData.state = APP_LED_STATE_BLINK_LED;
                    else    // no then stop blinking
                        appledData.state = APP_LED_STATE_STOP;
                    }
            // 20 mS debounce time not over then keep checking in next cycle
            break;
        }

        /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}



/* DecodeSerial is a function which decodes the serial input and respond 
 * appropriately.
 * If an 'A' is received then the latest ADC value is transmitted 
 * if an 'H' is received the "Hello World!" is transmitted */

void DecodeSerial( void )
{

    switch ( appuartData.RxChar )
    {
        // 'H' print Hello World!
        case 'H':
        case 'h':
        {
        printf("Hello World! \n\r");
        break;
        }

        default :
            break;
    }
}

/* The UART Task has 4 States:
 INIT where the hardware is initialized if needed
 SEND state where if the transmit buffer is empty the TXREG is 
 loaded with the transmit value
 GET state where the RCREG is read and  
 the input value is then decoded in the DecodeSerial routine.
 WAIT state where a check for a received char is done. If yes then the
 state goes to GET.
 */
void APP_UART_Tasks ( void )
{
    // Check the application's current state.
    switch ( appuartData.state )
    {
        // Application's initial state.
        case APP_UART_STATE_INIT:
        {
            appuartData.state = APP_UART_STATE_WAIT;
            break;
        }

        // TODO: implement your application state machine.
        case APP_UART_STATE_SEND:
        {
            if (EUSART_is_tx_ready()) // TXREG is empty?
            {// yes then load value to be transmitted in TXREG
                TXREG = appuartData.TxChar;
                appuartData.state = APP_UART_STATE_WAIT;
            }
            break;
        }
        case APP_UART_STATE_GET:
        {
            appuartData.RxChar = RCREG;  // Read RCREG
            RCREG = 0;                  // Clear RCREG
            DecodeSerial();             // Decode value received
            appuartData.state = APP_UART_STATE_WAIT; // goto WAIT
            break;
        }
        case APP_UART_STATE_WAIT:
        {
            if (EUSART_is_rx_ready()) // is a char received?
                // yes then go to GET state
                appuartData.state = APP_UART_STATE_GET;
            break;
        }

        // The default state should never be executed.
        default:
        {
            // TODO: Handle error in application's state machine.
            break;
        }
    }
}




/*******************************************************************************
 End of File
 */

