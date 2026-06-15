/**
  Generated main.c file from MPLAB Code Configurator

  @Company
 Wessex Lift Co Ltd

  @File Name
    main.c

  @Summary
    This is the generated main.c using PIC24 / dsPIC33 / PIC32MM MCUs.

  @Description
    This source file provides main entry point for system initialization and application code development.
    Generation Information :
        Product Revision  :  PIC24 / dsPIC33 / PIC32MM MCUs - 1.171.5
        Device            :  PIC24FJ128GA310
 * 
 * Wessex Ec17 0014  rev 033
 * 
    The generated drivers are tested against the following:
        Compiler          :  XC16 v2.10
        MPLAB 	          :  MPLAB X v6.05
*/

/*
    (c) 2020 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
    WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

    MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
    TERMS.
*/

/**
  Section: Included Files
*/
#include <stdint.h>
#include "mcc_generated_files/system.h"
#include "mcc_generated_files/delay.h"
#include "mcc_generated_files/adc1.h"
#include "mcc_generated_files/mcc.h" 
#include "mcc_generated_files/uart1.h"
#include "mcc_generated_files/i2c1.h"
#include <xc.h>
#include <stdbool.h>
#include <string.h>
#include "ble.h"
#include <math.h>
#include <stdio.h>
#include "flash_config.h"

// Function prototypes
void handle_lighting(void);
void handle_overtravel(void);
void handle_fire_test(void);

#define ST7036_ADDR 0x3C
#define LCD_CMD 0x00
#define LCD_DATA 0x40
#define FCY 4000000UL
#define BAUDRATE 9600
#define BRGVAL ((FCY / BAUDRATE)/16)-1

#include <libpic30.h>
#define LIFT_NOT_AT_FLOOR ((PORTAbits.RA6 ==1)&& (PORTAbits.RA9 == 1))
#define  DOOR_SAFE_TO_OPEN_AT_TOP ((PORTAbits.RA6 ==0)&&(PORTCbits.RC4 == 1))
#define  DOOR_SAFE_TO_OPEN_AT_BOTTOM ((PORTAbits.RA9 == 0)&&(PORTCbits.RC4 == 0))
#define DOOR_CALL_ACTIVE   (PORTAbits.RA5 == 1)
#define LIFT_AT_FLOOR  ((PORTAbits.RA6 == 0) || (PORTAbits.RA9 == 0))
#define DOOR_UNLOCKABLE   ((door_lock_flag == 0)&&((LIFT_SAFE_TO_OPEN_AT_TOP))

#define MAX_PWM 4095
#define DELAY_MS 10

typedef unsigned char uint8_t;
typedef unsigned int  uint16_t;

// Global configuration variables
int overtravel_time = 90;
int autohome_time = 300;
char serial [10] = "0000000000";
char lift_type[8]  = "IClass";
char install_date [20] = "12/2/26";
char firmware[20] = "EC17_0014_033";
int rb = 0;

// Flash memory setup
#define RX_LEN 8
volatile char rx_buffer[RX_LEN];
volatile uint8_t rx_index = 0;
volatile bool rx_ready = false;
volatile bool send_boo = false;

// UART buffer
#define UART_RX_BUFFER_SIZE 64
char uart_rx_buffer[UART_RX_BUFFER_SIZE];
uint8_t uart_rx_index = 0;

void UART1_WriteUInt(uint16_t value) {
    char buffer[20];
    sprintf(buffer, "%u", value);
    UART1_WriteString(buffer);
}

// Lighting colors
uint32_t red;
uint32_t blue;
uint32_t green;
uint32_t white;

// UART parameters
int uart_up_call_request = 0;
int uart_dn_call_request = 0;

char uart_serial_number [10]= "0000000000";
char uart_install_date[10] = "0000000000";
char uart_lift_type [5]= "00000";
int power_door = 0;

uint32_t uart_door_speed = 0x7F;
uint32_t uart_door_open_delay = 0x00;
uint32_t uart_door_autoclose_timer = 300;
int uart_door_autoclose = 0;
uint32_t uart_autohome_timer = 300;
int uart_autohome = 0;
int uart_autohome_direction = 0;
uint32_t uart_overttravel_timer = 90;
uint32_t uart_top_overrrun= 5;  // 0.5 seconds
uint32_t uart_bottom_overrrun= 5;   // 0.5 seconds
uint32_t lighting_timer = 300;
uint32_t uart_red = 0;
uint32_t uart_green = 0;
uint32_t uart_blue = 0;
uint32_t uart_white = 0;

// Deferred flash save variables
volatile int config_dirty = 0;
volatile int config_save_countdown = 0;
#define CONFIG_SAVE_DELAY 50  // 5 seconds at 100ms ticks

void UART1_WriteString(const char *text) {
    while(*text) {
        while(U1STAbits.UTXBF);
        U1TXREG = *text++;
    }
}

void UART1_WriteChar(char c) {
    while (U1STAbits.UTXBF);
    U1TXREG = c;
}

// I2C1 lighting definitions
void lights_I2C_Write(uint8_t addr, uint8_t reg, uint8_t data) {
    I2C1CONbits.SEN = 1; while (I2C1CONbits.SEN);
    I2C1TRN = addr << 1; while (I2C1STATbits.TRSTAT);
    I2C1TRN = reg; while (I2C1STATbits.TRSTAT);
    I2C1TRN = data; while (I2C1STATbits.TRSTAT);
    I2C1CONbits.PEN = 1; while (I2C1CONbits.PEN);
}

uint8_t lights_I2C_Read(uint8_t device_addr, uint8_t reg_addr) {
    uint8_t data;
    I2C1_Start();
    I2C1_Write(device_addr << 1);
    I2C1_Write(reg_addr);
    I2C1_Write((device_addr << 1) | 0x01);
    data = I2C1_Read(0);
    I2C1_Stop();
    return data;
}
   
void PCA9685_Init(void) {
    lights_I2C_Write(0x40, 0x003, 0x10);
    lights_I2C_Write(0x40, 0xFE, 3);
    lights_I2C_Write(0x40, 0x00, 0x00);
    lights_I2C_Write(0x40, 0x01, 0x04);
}

void PCA9685_SetPWM(uint8_t channel, uint16_t on, uint16_t off) {
    uint8_t pwm_base = (0x06 + 4 * channel);
    lights_I2C_Write(0x40, pwm_base, on & 0xFF);
    lights_I2C_Write(0x40, pwm_base + 1, on >> 8);
    lights_I2C_Write(0x40, pwm_base + 2, off & 0xFF);
    lights_I2C_Write(0x40, pwm_base + 3, off >> 8);
}

int blue_fade = 0;

void FadeLights(void) {
    for (uint16_t i = 0; i <= 4096; i += 16) {
        PCA9685_SetPWM(0, 0, i);
        __delay_ms(10);
        PCA9685_SetPWM(1, 0, i);
        __delay_ms(10);
        PCA9685_SetPWM(2, 0, i);
        __delay_ms(10);
        PCA9685_SetPWM(3, 0, i);
        __delay_ms(10);
        if(i>=4096) {i = 0;}
    }
}

void setRGB(uint16_t r, uint16_t g, uint16_t b) {
    PCA9685_SetPWM(0, 0, r);
    PCA9685_SetPWM(1, 0, g);
    PCA9685_SetPWM(2, 0, b);
}

void rainbowCycle() {
    float angle = 0.0;
    while (1) {
        uint16_t r = (sinf(angle) + 1.0) * (MAX_PWM / 2);
        uint16_t g = (sinf(angle + 2.0944) + 1.0) * (MAX_PWM / 2);
        uint16_t b = (sinf(angle + 4.1888) + 1.0) * (MAX_PWM / 2);
        setRGB(r, g, b);
        __delay_ms(DELAY_MS);
        angle += 0.01;
        if (angle > 6.2832) angle -= 6.2832;
    }
}

// I2C display definitions for LCD
void lcd_sendCommand(uint8_t cmd) {
    I2C2_Start();
    I2C2_Write((ST7036_ADDR << 1) | 0);
    I2C2_Write(LCD_CMD);
    I2C2_Write(cmd);
    I2C2_Stop();
    __delay_us(2);
}

void lcd_sendData(uint8_t data) {
    I2C2_Start();
    I2C2_Write((ST7036_ADDR << 1) | 0);
    I2C2_Write(LCD_DATA);
    I2C2_Write(data);
    I2C2_Stop();
    __delay_us(200);
}

void lcd_init(void) {
    __delay_ms(100);
    lcd_sendCommand(0x38);
    lcd_sendCommand(0x39);
    __delay_ms(100);
    lcd_sendCommand(0x14);
    lcd_sendCommand(0x7F);
    lcd_sendCommand(0x5F);
    __delay_ms(500);
    lcd_sendCommand(0x6C);
    __delay_ms(20);
    lcd_sendCommand(0x38);
    __delay_ms(20);
    lcd_sendCommand(0x0D);
    __delay_ms(20);
    lcd_sendCommand(0x01);
    __delay_ms(20);
    lcd_sendCommand(0x06);
    __delay_ms(10);
    lcd_sendCommand(0x40);
    __delay_ms(500);
}

void lcd_print(const char *str) {
    while (*str) {
        lcd_sendData(*str++);
    }
}

void flash_speed(void) {
    ADC1_SoftwareTriggerDisable();
}

// Door state machine
typedef enum {
    DOOR_IDLE = 0,
    DOOR_OPENING,
    DOOR_OPEN,
    DOOR_CLOSING,
    DOOR_CLOSED
} DoorState;

DoorState door_status = DOOR_IDLE;

//========== SUPPLY VOLTAGE MONITORING ================//
int VDC_in = 0;
int VDC_low = 0;
int VDC_OK = 0;
int VDC_high = 0;
float old_actual_supply_volt = 27.5;
float actual_supply_volt = 27.5;
int supply_volt = 0;

//========== BATTERY VOLTAGE MONITORING ================//
int battery_volt = 0;
int old_battery_volt = 0;
int battery_low = 0;
int battery_ok = 0;
int battery_high = 0;

//========== 3 SECOND TRAVEL DELAY ================//
typedef enum {
    TRAVEL_IDLE = 0,
    TRAVEL_ACTIVE,
    TRAVEL_DELAY_PENDING
} TravelState;

TravelState travel_state = TRAVEL_IDLE;
int travel_delay_count = 0;
int travel_delay_timer = 30;   // on a 100ms timer TMR2 = 3 seconds
int travel_delay_active = 0;

//========== LEVELLING ================//
typedef enum {
    LEVEL_IDLE = 0,
    LEVEL_LOWERING,
    LEVEL_COMPLETE,
    LEVEL_ERROR
} LevelState;

LevelState level_state = LEVEL_IDLE;
int level_timer_count = 0;
int level_timeout = 20;
int level_error_flag = 0;
volatile int level_interrupt_flag = 0;
int levelling_active = 0;

//========== DOOR MOTOR CONTROL ================//
int full_speed = 0;
int slow_speed_flag = 0;
int up_ramp_speed = 0;
int dn_ramp_speed = 0;
volatile int door_delay_interrupt_flag = 0;
volatile int door_ramp_interrupt_flag = 0;
int ls_flag = 0;
int delay_open = 1;
int delay_open_flag = 1;
int delay_close = 0;
int delay_close_flag = 0;
int open_delay = 0;
int close_delay = 0;
int close_overrun = 0;
int close_overrun_count = 100;
int slow_speed = 40;
int ok_to_run = 0;
int slow_ramp = 0;
int slow_ramp_speed = 150;
int door_motor_speed = 0;
int door_opening = 0;
int door_closing = 0;
int ramp_speed = 0;
int max_speed = 255;
int ls_delay_close = 0;
int ls_delay_open = 0;
int door_motor_state = 0;
int door_allowed = 0;
int door_ramp_direction = 0;

// Door ramping variables
int door_ramp_target_speed = 127;
int door_ramp_current_speed = 0;
int door_ramp_step = 0;
int door_ramp_delay_count = 0;
int door_ramp_delay_target = 2;
int ramp_step_size = 2;

int door_auto_open_delay = 0;
int door_auto_open_request = 0;
int door_auto_open_timer_flag = 0;
volatile int floor_arrival_timer = 0;
int floor_arrival_flag = 0;

//========== AUTO DOOR CLOSE ================//
int ADC_flag = 0;
int ADC_time = 300;
volatile int ADC_count = 0;
int door_call_request = 0;
int autohome_flag = 0;
int autohome_count = 0;

//========== DOOR LOCK SECTION ================//
// Door lock variables - FIXED IMPLEMENTATION
int door_lock_flag = 0;
volatile int door_lock_interrupt_flag = 0;
int door_lock_timer = 100;
volatile int door_lock_on = 0;
volatile int door_lock_count = 0;
volatile int floor_arrival_unlock_timer = 0;
volatile int floor_arrival_unlock_flag = 0;
int call_unlock_request = 0;

// Helper function to check if we should unlock due to call button
int shouldUnlockForCall(void) {
    // Check if at bottom floor AND down call is active
    if ((PORTAbits.RA9 == 0) && (PORTAbits.RA6 == 1)) { // At bottom floor
        if (PORTAbits.RA0 == 1) { // Down call active (assuming RA0 is DOWN button)
            return 1;
        }
    }
    // Check if at top floor AND up call is active
    if ((PORTAbits.RA6 == 0) && (PORTAbits.RA9 == 1)) { // At top floor
        if (PORTAbits.RA1 == 1) { // Up call active (assuming same button for testing)
            return 1;
        }
    }
    return 0;
}

// Function to start door unlock sequence
void start_door_unlock(void) {
    if (door_lock_on == 0)  // if the lock isnt already on
    {
        door_lock_on = 1;  // set door_lock on (this triggers the door lock to open)
        door_lock_count = 0;  // reset the timer count
        door_lock_flag = 1;  // flag that the door is unlocked
    }
}

//========== LIGHTS ================//
int lights_flag = 1;
int lights_ramp_up_speed = 0;
int lights_ramp_dn_speed = 0;
int lights_delay = 3000;
volatile int lights_count = 0;
int lights_on = 1;
int lights_ramp_down = 2000;
int lights_red = 0;
int lights_green = 0;
int lights_blue = 0;
int lights_white = 0;

//========== OVERTRAVEL ================//
int overtravel_flag = 0;
volatile int overtravel_timer = 0;
int overtravel_reset = 0;

//========== UP OVERRUN ================//
int up_limit_no = 0;
int up_limit_nc = 1;
int up_overrun_flag = 0;
int up_overrun_count = 0;
int up_overrun_timer = 25;
int up_overrun_timer_reset = 0;
volatile int up_overrun_interrupt_flag = 0;
int up_limit_arrived = 0;

//========== DOWN OVERRUN ================//
int down_limit_no = 0;
int down_limit_nc = 1;
int down_overrun_flag = 0;
int down_overrun_count = 0;
int down_overrun_timer = 4;
int down_overrun_complete = 0;
int down_limit_arrived = 0;

//========== SMOKE ALARM ================//
int fire_up_inhibit = 0;
int fire_down_inhibit = 0;
int fire_toggle = 0;
volatile int fire_count = 0;
int fire_time = 10;

//========== FAULT AND ERROR MONITORING ================//
int estop_ok = 0;
int up_limit_fault = 0;
int down_limit_fault = 0;
char message2[20] = "Wessex Lift co Ltd";
char Rxcommand[20] = "blank";

//========== LIFT STATES ================//
char door_state[6] = "ST 00";
char lock_state[6] = "ST 00";
char lift_state[6] = "ST 00";
char fire_state[6] = "ST 00";
int change_door_state = 0;
int change_lock_state = 0;
int change_lift_state = 0;
int change_fire_state = 0;

//========== 3 SECOND TRAVEL DELAY ================//
volatile int run_delay_interrupt_flag = 0;
int run_delay = 0;
int run_timer = 30;/// this sets the 3 second delay-  30 x 0.1s
int run_count = 0;
int delay_flag = 0;
int lift_travelling = 0;

// Interrupt flags
volatile int fire_interrupt_flag = 0;
volatile int lights_interrupt_flag = 0;
volatile int overtravel_interrupt_flag = 0;
volatile int down_overrun_interrupt_flag = 0;

//========== LIGHTS I2C ================//
int SDA1 = 0;
int SCL1 = 0;

//========== LCD DISPLAY I2C ================//
int IC_error = 0;

int timestamp = 0;
int realtime = 0;
char received[34] = "RECEIVED";

//========== TIMER INTERRUPT HANDLERS ================//
void handle_tmr2_scheduler_tick(void) {
    TMR2_SoftwareCounterClear();
    
    // all 100ms tasks.
    static int tick_counter = 0;
    
    // every 100ms
     door_lock_interrupt_flag = 1;   // 100ms timer for door lock timing
     door_delay_interrupt_flag = 1;  // 100ms timer for delaying door opening
     run_delay_interrupt_flag = 1;  // 100ms timer for 3 seconds delay between runs
     
     fire_interrupt_flag = 1; //  im not entirely sure why this is here
     door_ramp_interrupt_flag = 1;  // door speed ramping up or down]
     lights_interrupt_flag = 1;  // lights timer
    delay_flag = 1;
    door_auto_open_timer_flag = 1;  // 100ms for door opening
       // moved from TMR3
    level_interrupt_flag = 1;  // ADDED FOR LEVELLING
    
    fire_count++;
    lights_count++;
    overtravel_timer++;
    open_delay++;
    
    tick_counter++;
    if (tick_counter >= 100)
    {
        tick_counter = 0; // Reset every 10 seconds 
    }

    // Deferred flash save - counts down in 100ms ticks
    if (config_dirty && config_save_countdown > 0) {
        config_save_countdown--;
        if (config_save_countdown == 0) {
            Config_SaveAll();
            config_dirty = 0;
        }
    }
}

void handle_tmr3_scheduler_tick(void)  // TMR3 = 10ms timer
{
    TMR3_SoftwareCounterClear();
    down_overrun_interrupt_flag = 1;  // 10ms timer for down overrun
    up_overrun_interrupt_flag = 1;      // 10ms timer for up overrun
}

void handle_tmr4_scheduler_tick(void) {
    TMR4_SoftwareCounterClear();
   // run_delay_interrupt_flag = 1;
    handle_lighting();
    handle_overtravel();
}

void handle_lighting(void) {
    if (lights_count >= lights_delay) {
        lights_flag = 0;
        lights_count = 0;
    }
    if (lights_flag == 1) {
        // DO SOMETHING
    } else {
        // DO SOMETHING ELSE
    }
}

void handle_overtravel(void) {
    if (overtravel_timer >= overtravel_time) {
        overtravel_flag = 1;
        overtravel_timer = 0;
    }
    if (overtravel_flag == 1) {
          overtravel_led_SetLow();
        UP_control_SetHigh();
    } else {
          overtravel_led_SetHigh();
        UP_control_SetLow();
    }
    if (PORTDbits.RD13 == 1) {
        overtravel_flag = 0;
        overtravel_timer = 0;
    }
}

void handle_fire_test(void) {
    if (fire_count >= fire_time) {
        fire_count = 0;
        stop_sw_led_SetHigh();
        dn_sw_led_SetHigh();
    }
}

// Helper to mark config as dirty and start deferred save countdown
void Config_MarkDirty(void) {
    config_dirty = 1;
    config_save_countdown = CONFIG_SAVE_DELAY;
}

void UART1_ProcessCommand(const char *cmd) {
    __delay_ms(10); // this is a finger in the air delay - speeds all need looking at thoroughly
    
    if (strcmp(cmd, "UP") == 0)
    {
        uart_up_call_request = 1;
        uart_dn_call_request = 0;
        up_call_SetHigh();      // put an up call in
        down_call_SetLow();     // cancel any down call
        UART1_WriteString("UP call requested\r\n");
        
    } else if (strcmp(cmd, "DN") == 0) {
        uart_dn_call_request = 1;
        uart_up_call_request = 0;
        down_call_SetHigh();    // put a down call in
        up_call_SetLow();       // cancel any up call
        UART1_WriteString("DN call requested\r\n");
        
    } else if (strcmp(cmd, "XX") == 0) {
        uart_up_call_request = 0;
        uart_dn_call_request = 0;
        up_call_SetLow();       // cancel up call
        down_call_SetLow();     // cancel down call
        UART1_WriteString("All calls cancelled\r\n");

    } else if (strcmp(cmd, "RESET") == 0) {
        //System_Reset();
        
    } else if (strcmp(cmd, "OT") == 0) {
        UART1_WriteUInt(overtravel_time);
        UART1_WriteString("\r\n");
        
    } else if (strcmp(cmd, "DM") == 0) {
        UART1_WriteUInt(autohome_time);
        UART1_WriteString("\r\n");
        
    } else if (strcmp(cmd, "FW") == 0) {
        UART1_WriteString(firmware);
        UART1_WriteString("\r\n");
        
    } else if (strcmp(cmd, "serial") == 0) {
        UART1_WriteString(firmware);
        UART1_WriteString("\r\n");
        
    } else if ((strcmp(cmd, "RB00") == 0)&&(rb == 0)) {
        rb = 1;
        UART1_WriteString("rainbow lights off ");
        
    } else if ((strcmp(cmd, "RB01") == 0)&&(rb == 1)) {
        rb = 0;
        UART1_WriteString("rainbow lights on");
        
    } else if(strncmp(cmd, "LR ", 3) == 0) {
        char temp[3];
        strncpy(temp, cmd + 3, 2);
        temp[2] = '\0';
        UART1_WriteString("Raw red input: ");
        UART1_WriteString(cmd);
        UART1_WriteString("\r\n");
        int rvalue;
        if (sscanf(cmd, "LR %2x", &rvalue) == 1) {
            uart_red = rvalue;
            UART1_WriteString("redPWM updated\r\n");
            UART1_WriteUInt(uart_red);
            Config_MarkDirty();
        } else {
            UART1_WriteString("Invalid red hex value\r\n");
        }
        
    } else if(strncmp(cmd, "LG ", 3) == 0) {
        char temp[3];
        strncpy(temp, cmd + 3, 2);
        temp[2] = '\0';
        int gvalue;
        if (sscanf(cmd, "LG %2x", &gvalue) == 1) {
            uart_green = gvalue;
            UART1_WriteString("Green PWM updated\r\n");
            UART1_WriteUInt(uart_green);
            Config_MarkDirty();
        } else {
            UART1_WriteString("Invalid green hex value\r\n");
        }
        
    } else if(strncmp(cmd, "LB ", 3) == 0) {
        char temp[3];
        strncpy(temp, cmd + 3, 2);
        temp[2] = '\0';
        int bvalue;
        if (sscanf(cmd, "LB %2x", &bvalue) == 1) {
            uart_blue = bvalue;
            UART1_WriteString("blue PWM updated\r\n");
            UART1_WriteUInt(uart_blue);
            Config_MarkDirty();
        } else {
            UART1_WriteString("Invalid blue hex value\r\n");
        }
        
    } else if(strncmp(cmd, "LW ", 3) == 0) {
        char temp[3];
        strncpy(temp, cmd + 3, 2);
        temp[2] = '\0';
        int wvalue;
        if (sscanf(cmd, "LW %2x", &wvalue) == 1) {
            uart_white = wvalue;
            UART1_WriteString("white PWM updated\r\n");
            UART1_WriteUInt(uart_white);
            Config_MarkDirty();
        } else {
            UART1_WriteString("Invalid white hex value\r\n");
        }
        
    } else if(strncmp(cmd, "DS ", 3) == 0) {
        char temp[3];
        strncpy(temp, cmd + 3, 2);
        temp[2] = '\0';
        int dsvalue;
        if (sscanf(cmd, "DS %2x", &dsvalue) == 1) {
            uart_door_speed = dsvalue;
            UART1_WriteString("door speed updated\r\n");
            UART1_WriteUInt(uart_door_speed);
            Config_MarkDirty();
        } else {
            UART1_WriteString("Invalid door speed hex value\r\n");
        }
        
    } else if (strcmp(cmd, "LOCKSTATUS") == 0) {
        if (door_lock_on) {
            UART1_WriteString("LOCK: UNLOCKED (");
            UART1_WriteUInt(30 - door_lock_count);
            UART1_WriteString(" ticks remaining)\r\n");
        } else {
            UART1_WriteString("LOCK: LOCKED\r\n");
        }
        
    } else if (strcmp(cmd, "SAVE") == 0) {
        // Force immediate save to flash
        Config_SaveAll();
        config_dirty = 0;
        config_save_countdown = 0;
        
    } else if (strcmp(cmd, "DEFAULTS") == 0) {
        // Restore factory defaults and save
        Config_SetDefaults();
        Config_SaveAll();
        config_dirty = 0;
        config_save_countdown = 0;
        UART1_WriteString("Factory defaults restored\r\n");
        
    } else if (strcmp(cmd, "CONFIG") == 0) {
        // Dump all stored config values over UART
        UART1_WriteString("--- CONFIG ---\r\n");
        UART1_WriteString("Red:    "); UART1_WriteUInt(uart_red);                 UART1_WriteString("\r\n");
        UART1_WriteString("Green:  "); UART1_WriteUInt(uart_green);               UART1_WriteString("\r\n");
        UART1_WriteString("Blue:   "); UART1_WriteUInt(uart_blue);                UART1_WriteString("\r\n");
        UART1_WriteString("White:  "); UART1_WriteUInt(uart_white);               UART1_WriteString("\r\n");
        UART1_WriteString("DoorSpd:"); UART1_WriteUInt(uart_door_speed);          UART1_WriteString("\r\n");
        UART1_WriteString("ACTimer:"); UART1_WriteUInt(uart_door_autoclose_timer); UART1_WriteString("\r\n");
        UART1_WriteString("ACEn:   "); UART1_WriteUInt(uart_door_autoclose);      UART1_WriteString("\r\n");
        UART1_WriteString("AHTimer:"); UART1_WriteUInt(uart_autohome_timer);      UART1_WriteString("\r\n");
        UART1_WriteString("AHEn:   "); UART1_WriteUInt(uart_autohome);            UART1_WriteString("\r\n");
        UART1_WriteString("AHDir:  "); UART1_WriteUInt(uart_autohome_direction);  UART1_WriteString("\r\n");
        UART1_WriteString("OT:     "); UART1_WriteUInt(uart_overttravel_timer);   UART1_WriteString("\r\n");
        UART1_WriteString("TopOR:  "); UART1_WriteUInt(uart_top_overrrun);        UART1_WriteString("\r\n");
        UART1_WriteString("BotOR:  "); UART1_WriteUInt(uart_bottom_overrrun);     UART1_WriteString("\r\n");
        UART1_WriteString("LightT: "); UART1_WriteUInt(lighting_timer);           UART1_WriteString("\r\n");
        UART1_WriteString("Dirty:  "); UART1_WriteUInt(config_dirty);             UART1_WriteString("\r\n");
        UART1_WriteString("--- END ---\r\n");
    }
}

int isDoorCallActive(void) {
    return (PORTAbits.RA5 == 1) || (door_auto_open_request == 1);
}



//====================================================================//
int main(void) {
    
    // Initialize the device
    SYSTEM_Initialize();
    
    __delay_ms(100);
     
    ADC1_Initialize();
     
    UART1_Initialize(); 
//    I2C1_Initialize(100000UL); //   already initialized in i2c1.c ?? and system.c ???
    
    __delay_ms(100);

    // Start timers
    TMR1_Initialize();
    TMR2_Initialize();
    TMR3_Initialize();
    TMR4_Initialize();
    TMR1_Start();
    TMR2_Start();
    TMR3_Start();
    TMR4_Start();
    
    ADC1_SoftwareTriggerDisable();
    
    TMR3_SetInterruptHandler(flash_speed);
    
    // Initialize door PWM
    OC1_Initialize();
    OC1_Start();
    OC1_PrimaryValueSet(door_speed);
    OC1_SecondaryValueSet(0x100);
    
    // Initialize lighting I2C and controller
    __delay_ms(500);
    PCA9685_Init();
    
    // Digital outputs initializing
    down_call_SetLow();                   //A0  set high to put a down call in
    up_call_SetLow();                        //A1  set lhigh to put an up call in
    stop_sw_led_SetHigh();              //A4  set high to turn the stop switch illumunation on
    dn_sw_led_SetHigh();                 //C2 set high to turn down switch illumination on
    dn_limit_override_SetLow();        //C4 turn on to bypass the down limit switch (for FP override etc)
    
    up_sw_led_SetHigh();                 // D0  set high to turn on the up switch illumination
    SPI1_PD_SetLow();                    //D6  - not used 
    
    
 
    //up_safe_edge_monitor_SetLow();   // E0 - this is an input !!!!
    door_disable_SetHigh();             //E5 - Set low to enable the door motor Driver
    down_fire_inhib_LED_SetLow();// E7 - Set high to turn on the down inhibit LED
    Up_fire_inhib_LED_SetLow();    // E6 - Set high to turn on the up inhibit LED
    UP_control_SetHigh();                // F0 - turn this off to isolate the "up" output from the board including shoot bolt
    down_control_SetHigh();            // F1 - Set low to isolate the "down" output from the board
    dn_safe_fault_LED_SetLow();    //F2 - Set high to turn on down safe fault LED
    up_safe_fault_LED_SetLow();    //F3- Set high to turn on up safe fault LED
    door_lock_SetLow() ;                   // F4 - Set high to unlock the door (activate the solenoid)
    top_lock_SetLow();                     // F5 - Set high to unlock the upper door (activate the solenoid)  optional upper lock for LR/OP etc 
    door_direction_SetLow();            // F6 - Set Low to open the door, set high to close the door (reversing direction)
    alarm_out_SetLow();                   // F7 - SEt high to turn on the alarm
    overtravel_led_SetLow();            // F8 -  set high to turn on overtravel fault led - default high to test led
    keyswitch_block_out_SetHigh(); // F12 - Set high to allow travel control from inside the lift
    delay_24v_3s_SetHigh();            // G7 - set low to disable lift up or down travel
    door_open_led_SetLow();           //G14 Set high to turn on the door open LED
    door_close_led_SetLow();           // G15 Set high to turn on the door close LED
  

    
    //========================lift status messages==========================
/*    
    Mains Power on              0x01
Mains power off                 0x02
Battery low                         0x03
e/stop ok                            0x04
e/stop off                          0x05
Interlock fail                          0x06
Door call                               0x07
Door opening                        0x08
Door Open (actuator)            0x09
Door closing                        0x0A
Door closed (actuator)	0x0B
Door solenoid activated	0x0C
Door solenoid deativated	 0x0D
Up call                                 0x0E
Up FP fault                         0x0F
UP FP override  fsault	0x10
UP safety fault                     0x11
Up trap fault                       0x12
up travel fault                     0x13
Up travel ok                        0x14
Up 0V out                           0x15
floor limit fault                      0x16
                                            0x17
Down call                               0x18
Down FP Fault                       0x19
Down Trap fault             	0x1A
Down Trap override fault	0x1B
Down travel ok                      0x1C
	
	
Down 0V out                         0x1D
Travel 24V out                      0x1E
Bottom Floor Arrival                0x1F
Bottom Floor Departure	0x20
	
Top Floor Arrival                   0x21
Top Floor Departure             0x22
Levelling                               0x23
Anti Creep                              0x24
Lights on                               0x25
Lights off                              0x26
 *                           0x27
 *                           0x28
 *                           0x29
 *                           0x2A
 *                              0x2B
 *                      0x2C
 * 0X2D
 * 0x2E
 * 0x2F 
 */   
    
    // Set up UART1
    __builtin_write_OSCCONL(OSCCON & ~(1 << 6));
    RPOR1bits.RP2R = 3;
    RPINR18bits.U1RXR = 3;
    __builtin_write_OSCCONL(OSCCON | (1 << 6));
    
    UART1_WriteString("UART Ready\r\n");
    
    //========== LOAD SAVED CONFIGURATION FROM FLASH ==========//
    Config_LoadAll();  // Loads saved values or sets defaults if flash is blank
    
    // Apply loaded lighting values to the PCA9685 immediately
    PCA9685_SetPWM(2, 0, uart_red * 16);
    PCA9685_SetPWM(1, 0, uart_green * 16);
    PCA9685_SetPWM(0, 0, uart_blue * 16);
    PCA9685_SetPWM(3, 0, uart_white * 16);
    
    // Sync the "current" shadow values so main loop doesn't re-trigger updates
    red   = uart_red;
    green = uart_green;
    blue  = uart_blue;
    white = uart_white;
    
    UART1_WriteString("System ready\r\n");
    
    OC1_PrimaryValueSet(10);
    
    int last_floor_state = 0;
    int current_floor_state;
    
    //========== MAIN PROGRAM LOOP ================//
    while (1) {
        
        // Lighting control
        if (red != uart_red) {
            red = uart_red;
            UART1_WriteString("red changed\r\n");
            PCA9685_SetPWM(2, 0, red*16);
        }
        if (green != uart_green) {
            green = uart_green;
            UART1_WriteString("green changed\r\n");
            PCA9685_SetPWM(1, 0, green*16);
        }
        if (blue != uart_blue) {
            blue = uart_blue;
            UART1_WriteString("blue changed\r\n");
            PCA9685_SetPWM(0, 0, blue*16);
        }
        if (white != uart_white) {
            white = uart_white;
            UART1_WriteString("white changed\r\n");
            PCA9685_SetPWM(3, 0, white*16);
        }
        
        // UART command processing
        if (UART1_IsRxReady()) {
            char received = UART1_Read();
            if (received == '\n' || received == '\r') {
                uart_rx_buffer[uart_rx_index] = '\0';
                UART1_WriteString(uart_rx_buffer);
                UART1_WriteString("\r\n");
                UART1_ProcessCommand(uart_rx_buffer);
                uart_rx_index = 0;
            } else if (uart_rx_index < UART_RX_BUFFER_SIZE - 1) {
                uart_rx_buffer[uart_rx_index++] = received;
            } else {
                uart_rx_index = 0;
            }
        }
        
        //========== FLOOR ARRIVAL DETECTION & TIMING ==========//
        if ((PORTAbits.RA6 == 0) && (PORTAbits.RA9 == 1)){ //&& (floor_arrival_flag == 0)) {
            // At top floor and the arrival flag isn't set
            current_floor_state = 0;
            if (last_floor_state != 0) {
                floor_arrival_timer = 0;  // Start the 1 second delay to unlock
                floor_arrival_unlock_timer = 0;  // Reset door open timer
                floor_arrival_flag = 1;
            }
            if (strcmp(lift_state, "ST 21") != 0) {
                strcpy(lift_state, "ST 21");
                change_lift_state = 1;
            }
        } else if ((PORTAbits.RA9 == 0) && (PORTAbits.RA6 == 1)){ //&& (floor_arrival_flag == 0)) {
            // At bottom floor
            current_floor_state = 1;
            if (last_floor_state != 1) {
                floor_arrival_timer = 0;  // Start the 1 second delay to unlock
                floor_arrival_unlock_timer = 0;  // Reset door open timer
                floor_arrival_flag = 1;
            }
            if (strcmp(lift_state, "ST 1F") != 0) {
                strcpy(lift_state, "ST 1F");
                change_lift_state = 1;
            }
        } else if ((PORTAbits.RA9 == 1) && (PORTAbits.RA6 == 1)) {
            // Between floors - reset everything
            current_floor_state = 2;
            floor_arrival_flag = 0;
            floor_arrival_timer = 0;
            floor_arrival_unlock_timer = 0;
            floor_arrival_unlock_flag = 0;
            door_auto_open_request = 0;
            if (strcmp(lift_state, "ST 27") != 0) {
                strcpy(lift_state, "ST 27");
                change_lift_state = 1;
            }
        } else if ((PORTAbits.RA9 == 0) && (PORTAbits.RA6 == 0)) {
            // Floor switch fault
            current_floor_state = 3;
            floor_arrival_flag = 0;
            floor_arrival_timer = 0;
            floor_arrival_unlock_timer = 0;
            floor_arrival_unlock_flag = 0;
            up_safe_fault_LED_SetHigh();
            if (strcmp(lift_state, "ST 16") != 0) {
                strcpy(lift_state, "ST 16");  // floor switch fault
                change_lift_state = 1;
            }
        }

        // STEP 1: Wait 1 second after arrival, then unlock
        if (floor_arrival_flag && !floor_arrival_unlock_flag && door_delay_interrupt_flag) {
            // floor arrival flag set when the lift has arrived at a floor
            //floor arrival unlock flack not set
            //door delay interrupt flag set every 100ms
            //door_delay_interrupt_flag = 0; // reset the door delay increment flag  - AI deleteed
            floor_arrival_timer++; // increment the arrival timer (100ms))
            
            if (floor_arrival_timer >= 10) {  // 10 ticks * 100ms = 1 second
                start_door_unlock();  // This unlocks for 3 seconds
                floor_arrival_unlock_flag = 1;  // Mark that unlock has started
                floor_arrival_unlock_timer = 0;  // Start timer for door opening
                floor_arrival_flag = 0;  // reset the arrival flag
                
            }
        }

        // STEP 2: Wait another 1 second after unlock starts, then request door open
        if (floor_arrival_unlock_flag && door_delay_interrupt_flag) {
           
            floor_arrival_unlock_timer++;
            
            if (floor_arrival_unlock_timer >= 10) {  // 10 ticks * 100ms = 1 second
                door_auto_open_request = 1;  // Request door to open
                floor_arrival_unlock_flag = 0;  // Reset flag
                floor_arrival_flag = 0;  // Reset arrival flag
            }
        }
         door_delay_interrupt_flag = 0;
        //========== DOOR CONTROL STATE MACHINE ==========//
        
        if (DOOR_SAFE_TO_OPEN_AT_TOP || DOOR_SAFE_TO_OPEN_AT_BOTTOM ||(PORTAbits.RA1 = 1) )// As it says
        {
            switch (door_status) {
                case DOOR_IDLE:
                    // Only respond to door_auto_open_request, NOT isDoorCallActive
                    if ((PORTBbits.RB13 == 0) && door_auto_open_request)
                    {
                        door_disable_SetLow(); // enable the door
                        door_direction_SetLow();  // set direction to opening
                        door_status = DOOR_OPENING;  // change the door status
                        door_ramp_current_speed = 0;  // set the ramp speed to 0
                        door_ramp_step = 0;
                        door_ramp_delay_count = 0;  // reset the delay before ramping up
                        OC1_PrimaryValueSet(door_ramp_current_speed);  // speed sent to the PWM
                        door_auto_open_request = 0;  // reset the auto door request
                        
                        //door_open_led_SetHigh();  // turn on the door open led
                        //door_close_led_SetLow();  // and turn off the door closed
                        
                        if (strcmp(door_state, "ST 08") != 0) 
                        {
                            strcpy(door_state, "ST 08");  // door status = opening
                            change_door_state = 1;
                        }
                    }
                    break;
                    
                case DOOR_OPENING:
                        door_open_led_SetHigh();  // turn on the door open led
                        door_close_led_SetLow();  // and turn off the door closed
                    if (door_ramp_interrupt_flag) {
                        door_ramp_interrupt_flag = 0;
                        door_ramp_delay_count++;
                        if (door_ramp_delay_count >= door_ramp_delay_target) {
                            if (door_ramp_current_speed <= door_ramp_target_speed) {
                                door_ramp_current_speed += ramp_step_size;
                                if (door_ramp_current_speed >= door_ramp_target_speed) {
                                    door_ramp_current_speed = door_ramp_target_speed;
                                }
                                OC1_PrimaryValueSet(door_ramp_current_speed);
                                int percentage = (door_ramp_current_speed * 100) / 4095;
                                UART1_WriteString("OPENING - PWM: ");
                                UART1_WriteUInt(door_ramp_current_speed);
                                UART1_WriteString(" (");
                                UART1_WriteUInt(percentage);
                                UART1_WriteString("%)\r\n");
                            } else {
                                UART1_WriteString("RAMP: Reached target PWM\r\n");
                            }
                            door_ramp_delay_count = 0;
                        }
                    }
                    if (PORTBbits.RB12 == 0) {
                        UART1_WriteString("DOOR: Fully open, stopping motor\r\n");
                        for(int i = door_ramp_current_speed; i >= 0; i -= 100) {
                            if(i < 0) i = 0;
                            OC1_PrimaryValueSet(i);
                            __delay_ms(10);
                        }
                        door_direction_SetHigh(); // set door direction to close
                        OC1_PrimaryValueSet(0);
                        door_status = DOOR_OPEN;
                        door_auto_open_request = 0;
        
                        if (strcmp(door_state, "ST 09") != 0) 
                        {
                            strcpy(door_state, "ST 09");
                            change_door_state = 1;
                        }
                        door_open_led_SetLow();
                    }
                    break;
                    
                case DOOR_OPEN:
                    if ((isDoorCallActive() && PORTBbits.RB12 == 0) || 
                        (uart_door_autoclose && ADC_flag)) {
                        UART1_WriteString("DOOR: Starting closing sequence\r\n");
                        door_disable_SetLow();  // enable the motor
                        door_direction_SetHigh(); // set door direction to close
                        door_status = DOOR_CLOSING;
                        door_ramp_current_speed = 0;
                        door_ramp_step = 0;
                        door_ramp_delay_count = 0;
                        OC1_PrimaryValueSet(door_ramp_current_speed);
                        
                          door_open_led_SetLow();  // turn off the door open led
                        door_close_led_SetHigh();  // and turn on the door closed
                       
                        ADC_flag = 0;
                        if (strcmp(door_state, "ST 0A") != 0)
                        {
                            strcpy(door_state, "ST 0A");
                            change_door_state = 1;
                        }
                    }
                    break;
                    
                case DOOR_CLOSING:
                    if (door_ramp_interrupt_flag) {
                        door_ramp_interrupt_flag = 0;
                        door_ramp_delay_count++;
                        if (door_ramp_delay_count >= door_ramp_delay_target) {
                            if (door_ramp_current_speed < door_ramp_target_speed) {
                                door_ramp_current_speed += ramp_step_size;
                                if (door_ramp_current_speed > door_ramp_target_speed) {
                                    door_ramp_current_speed = door_ramp_target_speed;
                                }
                                OC1_PrimaryValueSet(door_ramp_current_speed);
                           //     int percentage = (door_ramp_current_speed * 100) / 4095;
                            } 
                            door_ramp_delay_count = 0;
                        }
                    }
                    if (PORTBbits.RB13 == 0) {
                        UART1_WriteString("DOOR: Fully closed, stopping motor\r\n");
                        for(int i = door_ramp_current_speed; i >= 0; i -= 100) {
                            if(i < 0) i = 0;
                            OC1_PrimaryValueSet(i);
                            __delay_ms(10);
                        }
                        OC1_PrimaryValueSet(0);
                        door_status = DOOR_CLOSED;
                        if (strcmp(door_state, "ST 0B") != 0)
                        {
                            strcpy(door_state, "ST 0B");
                            change_door_state = 1;
                        }
                    }
                    break;
                    
                case DOOR_CLOSED:
                    door_status = DOOR_IDLE;
                    door_auto_open_request = 0;
                    break;
            }
        } else {
            // Lift not at floor - disable door operation
            door_disable_SetHigh();
            OC1_PrimaryValueSet(10);
            door_allowed = 0;
            ramp_speed = 0;
            full_speed = 0;
            door_status = DOOR_IDLE;
            door_auto_open_request = 0;
            floor_arrival_timer = 0;
            floor_arrival_flag = 0;
        }
        
        last_floor_state = current_floor_state;
        
        //========== DOOR LOCK IMPLEMENTATION ==========//
        // This section implements the door lock functionality
        // Unlock is triggered by the floor arrival sequence above
        
        if ((DOOR_SAFE_TO_OPEN_AT_TOP == 1) || (DOOR_SAFE_TO_OPEN_AT_BOTTOM == 1)) {   
            // Check for call button unlock request
            // Only if door is CLOSED
            if (door_status == DOOR_CLOSED || door_status == DOOR_IDLE) {
                if (shouldUnlockForCall()) {
                    if (call_unlock_request == 0) {
                        call_unlock_request = 1;
                        start_door_unlock();
                    }
                } else {
                    call_unlock_request = 0;
                }
            }
        } else {
            // Not at floor - reset door lock timers
            floor_arrival_unlock_flag = 0;
            floor_arrival_unlock_timer = 0;
            call_unlock_request = 0;
        }
        
        // Handle the 3-second unlock duration
        if (door_lock_on && door_lock_interrupt_flag) {
            door_lock_interrupt_flag = 0;
            door_lock_count++;  // increment the clock every 100ms
            
            // 30 ticks * 100ms = 3 seconds
            if (door_lock_count >= 30) {
                door_lock_on = 0;   // turns off the door lock
                door_lock_flag = 0;   // reset the flag
                door_lock_count = 0;  // reset the clock
            }
        }
        
        // Update door lock LED/indicator
        if (door_lock_on) {
            door_lock_SetHigh(); // Unlocked - solenoid activated
           //up_safe_fault_LED_SetHigh();
        } else {
            door_lock_SetLow(); // Locked - solenoid deactivated
            up_safe_fault_LED_SetLow();
        }
        
        //========== AUTO-CLOSE TIMER HANDLING ==========//
        /*if (door_status == DOOR_OPEN && uart_door_autoclose) {
            if (door_delay_interrupt_flag) {
                door_delay_interrupt_flag = 0;
                ADC_count++;
                if (ADC_count >= uart_door_autoclose_timer) {
                    ADC_flag = 1;
                    ADC_count = 0;
                    UART1_WriteString("Auto-close timer expired\r\n");
                }
            }
        } else {
            ADC_count = 0;
            ADC_flag = 0;
        }
        */
        
        //========== ANALOGUE INPUTS ==========//
        supply_volt = ADC1_ConversionResultGet(power_in_monitor);
        float actual_supply_volt = (supply_volt * 190.0) / 4095.0;
        if ((actual_supply_volt < old_actual_supply_volt - 0.2) ||
            (actual_supply_volt > old_actual_supply_volt + 0.2)) {
            old_actual_supply_volt = actual_supply_volt;
        }
        VDC_in = ((ADC1_ConversionResultGet(power_in_monitor)));
        
        //========== EMERGENCY STOP ==========//
        if (PORTDbits.RD14 == 0) {
            estop_ok = 0;
        }
        if (PORTDbits.RD14 == 1) {
            estop_ok = 1;
        }
        
        //========== 3-SECOND TRAVEL DELAY CONTROL ==========//
        int currently_travelling = (PORTEbits.RE0 == 1) || (PORTEbits.RE8 == 1);
        
        switch (travel_state) {
            case TRAVEL_IDLE:
                travel_delay_active = 0;
                delay_24v_3s_SetHigh(); // turn on 24V to allow travel
                if (currently_travelling) {
                    travel_state = TRAVEL_ACTIVE;
                }
                break;
                
            case TRAVEL_ACTIVE:
                travel_delay_active = 0;
                delay_24v_3s_SetHigh(); // turn on 24V to allow travel
                if (!currently_travelling) {
                    travel_state = TRAVEL_DELAY_PENDING;
                    travel_delay_count = 0;
                    travel_delay_active = 1;
                    delay_24v_3s_SetLow();  // block 24V to up and down travel
                }
                break;
                
            case TRAVEL_DELAY_PENDING:
                travel_delay_active = 1;
                delay_24v_3s_SetLow(); // block 24V to up and down travel
                if (run_delay_interrupt_flag == 1) {
                    run_delay_interrupt_flag = 0;
                    travel_delay_count++;
                }
                if (travel_delay_count >= travel_delay_timer) {
                    travel_state = TRAVEL_IDLE;
                    travel_delay_count = 0;
                    travel_delay_active = 0;
                    delay_24v_3s_SetHigh();  // turn on 24V to allow travel
                }
                break;
        }
        
        if (travel_delay_active) {
            dn_safe_fault_LED_SetHigh();
        } else {
            dn_safe_fault_LED_SetLow();
        }
        
        //========== LEVELLING FUNCTION ==========//
        

        //========== UP/DOWN CONTROLS ==========//
        
        up_sw_led_SetHigh();  //  illuminate the up button
        dn_sw_led_SetHigh();  // illuminate the down button
        stop_sw_led_SetHigh();  // illuminate the stop button
        
        //========== ALARM ==========//
        if (PORTEbits.RE1 == 1) {
            alarm_out_SetHigh();
        }
        if (PORTEbits.RE1 == 0) {
            alarm_out_SetLow();
        }
        
        //========== KEYSWITCH ==========//
        if (PORTDbits.RD15 == 1) {
            // Turn off in car controls
        }
        if (PORTDbits.RD15 == 0) {
            // Turn on in car controls
        }
        
        //========== FIRE ALARM SECTION ==========//
        if (PORTFbits.RF13 == 1 && PORTAbits.RA6 == 1 && PORTAbits.RA9 == 1) // fire alarm active and lift between floors
        {
            if (strcmp(fire_state, "ST 28") != 0) {
                strcpy(fire_state, "ST 28");
                change_fire_state = 1;
            } // ST28 is "fire alarm active"
        }
        if (PORTFbits.RF13 == 1 && PORTAbits.RA6 == 0) //  fire alarm active and lift at the top floor - block down travel
        {
            down_control_SetLow();
            down_fire_inhib_LED_SetHigh();
            if (strcmp(fire_state, "ST 29") != 0) {
                strcpy(fire_state, "ST 29");
                change_fire_state = 1;
            }  // ST 29 is fire alarm blocking down travel
        }
        if ((PORTFbits.RF13 == 1) && (PORTAbits.RA9 == 0) && (PORTCbits.RC3 == 0))// smoke alarm active and lift at the bottom floor - block up travel
        {
            fire_up_inhibit = 1;
            Up_fire_inhib_LED_SetHigh();
            if (strcmp(fire_state, "ST 2A") != 0) {
                strcpy(fire_state, "ST 2A");
                change_fire_state = 1;
            } //ST 2A is "fire alarm blocking up travel
        }
        if (PORTFbits.RF13 == 0) {
            fire_up_inhibit = 0;
            fire_down_inhibit = 0;
            Up_fire_inhib_LED_SetLow();
            down_fire_inhib_LED_SetLow();
            if (strcmp(fire_state, "ST 2B") != 0) {
                strcpy(fire_state, "ST 2B");
                change_fire_state = 1;
            } //  ST 2B is "fire alarm cleared"
        }
        
        //========== UP OVERRUN ==========//
        if (!up_limit_arrived && PORTAbits.RA6 == 0) {
            up_overrun_flag = 1;
        }
        if (up_overrun_flag == 1) {
            down_call_SetHigh();  // put a down call in until arrival
            if(up_overrun_interrupt_flag == 1) {
                up_overrun_count++;
                up_overrun_interrupt_flag = 0;
                if(up_overrun_count >= up_overrun_timer) {
                    up_limit_arrived = 1;
                    up_overrun_count = 0;
                    up_overrun_flag = 0;
                }
            }
        } else {
            up_call_SetLow();   // turn off down call when finally arrived
        }
        
        //========== DOWN OVERRUN ==========//
        if (!down_limit_arrived && PORTAbits.RA9 == 0) // down limit switch has turned off but arrival status not complete
        {  
            down_overrun_flag = 1;  // turn on the down overrun timer
        }
        if (down_overrun_flag == 1)   // if down overrun timer is active
        {
            down_call_SetHigh();  // put a down call in until arrival
            if (down_overrun_interrupt_flag == 1)  // wait for 10ms tick
            {
                down_overrun_count++;  // increment the overrun count
                down_overrun_interrupt_flag = 0;  // reset the overrun interrupt flag
                if(down_overrun_count >= down_overrun_timer) { // if overrun time reached
                    down_overrun_count = 0;  // reset the count
                    down_overrun_flag = 0;  // turn the overun flag off
                    down_limit_arrived = 1;  // turn the arrived flag on.   
                }
            }
        } else {
            down_call_SetLow();   // turn off down call when finally arrived.. 
        }
        
        if (PORTAbits.RA9 == 1)  // if not at the lower floor, turn off down limit arrived
        {down_limit_arrived = 0;}
        
        //========== INTERRUPT TICKS ==========//
        if (TMR2_SoftwareCounterGet() == 1) {
            handle_tmr2_scheduler_tick();
        }
        if (TMR3_SoftwareCounterGet() == 1) {
            handle_tmr3_scheduler_tick();
        }
        
        //========== OVERTRAVEL TIMER ==========//
        if (overtravel_timer >= overtravel_time) {
            overtravel_flag = 1;
            overtravel_timer = 0;
        }
        if (overtravel_flag == 1) {
            overtravel_led_SetLow();
            UP_control_SetHigh();
        }
        if (overtravel_flag == 0) {
            overtravel_led_SetHigh();
            UP_control_SetLow();
        }
        if (PORTDbits.RD13 == 1) {
            overtravel_flag = 0;
            overtravel_timer = 0;
        }
        
        //========== DOWN LIMIT & FP ZONE ==========//
        if ((PORTCbits.RC3 == 1)) {
            dn_limit_override_SetHigh();
            down_fire_inhib_LED_SetLow();
        } else {
            dn_limit_override_SetLow();
            down_fire_inhib_LED_SetHigh();
        }
        
        //========== STATUS MESSAGE SENDING ==========//
        //  only send status messages when they change
        if (change_door_state == 1) {
            UART1_WriteString(door_state);
            change_door_state = 0;
        }
        if (change_lock_state == 1) {
            UART1_WriteString(lock_state);
            change_lock_state = 0;
        }
        if (change_lift_state == 1) {
            UART1_WriteString(lift_state);
            change_lift_state = 0;
        }
        if (change_fire_state == 1) {
            UART1_WriteString(fire_state);
            change_fire_state = 0;
        }
        
    } // End of while loop
    
} // End of main

/**
 End of File
*/
