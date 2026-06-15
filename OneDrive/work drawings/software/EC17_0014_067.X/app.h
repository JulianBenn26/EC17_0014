/*******************************************************************************
  MPLAB Code Configurator (MCC) Application Source File


  Company:
    Microchip Technology Inc.

  File Name:
    app.h

  Summary:
     This file contains the source code for the application Lab used in:

	Getting Started using PIC16F1xxx Class of Microchip University
	
	**************This code is for the Part 4 section**************

  Description:
    This header file provides function prototypes and data type definitions for
    the application.  Some of these are required by the system (such as the
    "APP_Initialize" and "APP_Tasks" prototypes) and some of them are only used
    internally by the application (such as the "APP_STATES" definition).  Both
    are defined here for convenience.
*******************************************************************************/

//DOM-IGNORE-BEGIN
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
//DOM-IGNORE-END

#ifndef _APP_H
#define _APP_H





// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>



// *****************************************************************************
// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application states

  Summary:
    Application states enumeration

  Description:
    This enumeration defines the valid LED application states.  These states
    determine the behavior of the blinky LED at various times.
*/

typedef enum
{
	/* Application's state machine's initial state. */
	APP_LED_STATE_INIT=0,

	/* TODO: Define states used by the application state machine. */
                APP_LED_STATE_WAIT=1,
                APP_LED_STATE_BLINK_LED=2,
                APP_LED_STATE_STOP=3
            
} APP_LED_STATES;


typedef enum
{
	/* Application's state machine's initial state. */
	APP_KEY_STATE_INIT=0,

	/* TODO: Define states used by the application state machine. */
                APP_KEY_STATE_HIGH=1,
                APP_KEY_STATE_LOW=2,            
                APP_KEY_STATE_DEBOUNCE=3
} APP_KEY_STATES;


typedef enum
{
	/* Application's state machine's initial state. */
	APP_UART_STATE_INIT=0,

	/* TODO: Define states used by the application state machine. */
                APP_UART_STATE_SEND=1,
                APP_UART_STATE_GET=2,
                APP_UART_STATE_WAIT=3
} APP_UART_STATES;

// *****************************************************************************
/* Application Data

  Summary:
    Holds application LED data

  Description:
    This structure holds the application's LED data.

  Remarks:
    Application strings and buffers are be defined outside this structure.
 */

typedef struct
{
    /* The application's current state */
    APP_LED_STATES state;

    /* TODO: Define any additional data used by the application. */
    int TimerCount;

} APP_LED_DATA;

extern APP_LED_DATA appledData; 

typedef struct
{
    /* The application's current state */
    APP_KEY_STATES state;

    /* TODO: Define any additional data used by the application. */
    unsigned char DebounceCount;

} APP_KEY_DATA;

extern APP_KEY_DATA appkeyData;



typedef struct
{
    /* The application's current state */
    APP_UART_STATES state;

    /* TODO: Define any additional data used by the application. */
    unsigned char RxChar;
    unsigned char TxChar;

} APP_UART_DATA;

extern APP_UART_DATA appuartData;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Routines
// *****************************************************************************
// *****************************************************************************
/* These routines are called by drivers when certain events occur.
*/

	
// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Summary:
     MPLAB Code Configurator (MCC) application initialization routine.

  Description:
    This function initializes the MCC application.  It places the 
    application in its initial state and prepares it to run so that its 
    APP_Tasks function can be called.

  Precondition:
    All other system initialization routines should be called before calling
    this routine (in "SYS_Initialize").

  Parameters:
    None.

  Returns:
    None.

  Example:
    <code>
    APP_Initialize();
    </code>

  Remarks:
    These routine usually called from the SYS_Initialize function.
*/

void APP_LED_Initialize ( void );
void APP_KEY_Initialize ( void );
void APP_UART_Initialize ( void );

/*******************************************************************************
  Function:
    void APP_Tasks ( void )

  Summary:
    MPLAB Code Configurator (MCC) application tasks function

  Description:
    This routine is the MCC Demo application's tasks function.  It
    defines the application's state machine and core logic.

  Precondition:
    The system and application initialization ("SYS_Initialize") should be
    called before calling this.

  Parameters:
    None.

  Returns:
    None.

  Example:
    <code>
    APP_Tasks();
    </code>

  Remarks:
    These routine may be called from SYS_Tasks() routine.
 */

void APP_LED_Tasks ( void );
void APP_KEY_Tasks ( void );
void APP_UART_Tasks ( void );

#endif /* _APP_H */
/******** Copy and Paste data for Lab1 Part B*************
 
 In MCC file tmr1.c in the callback function:
 if (appkeyData.state == APP_KEY_STATE_DEBOUNCE)
            appkeyData.DebounceCount++;
 * 
 In main.c file add below SYSTEM_Initialize() function:
 APP_KEY_Initialize();
 In main.c in the while loop add:
 APP_KEY_Tasks();
 * 
 *  */
/*******************************************************************************
 End of File
 */

