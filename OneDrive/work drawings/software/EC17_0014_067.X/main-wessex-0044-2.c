/**
  Generated main.c file from MPLAB Code Configurator

  @Company
 Wessex Lift Co Ltd

  @File Name
    main.c

  @Summary
    This is the generated main.c using PIC24 / dsPIC33 / PIC32MM MCUs.
 * 
 * Version 58 -     

  @Description
    This source file provides main entry point for system initialization and application code development.
    Generation Information :
        Product Revision  :  PIC24 / dsPIC33 / PIC32MM MCUs - 1.171.5
        Device            :  PIC24FJ128GA310
 * 
 * Wessex Ec17 0014  rev 058  
 * 
    The generated drivers are tested against the following:
        Compiler          :  XC16 v2.10
        MPLAB 	          :  MPLAB X v6.25
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
#define BAUDRATE 38400
#define BRGVAL ((FCY / BAUDRATE)/16)-1

#include <libpic30.h>
#define LIFT_NOT_AT_FLOOR ((PORTAbits.RA6 ==1)&& (PORTAbits.RA9 == 1))
#define  DOOR_SAFE_TO_OPEN_AT_TOP ((PORTAbits.RA6 ==0)&&(PORTCbits.RC4 == 0))  // RC4 should be 1 - temp cheged
#define  DOOR_SAFE_TO_OPEN_AT_BOTTOM ((PORTAbits.RA9 == 0)&&(PORTCbits.RC4 == 0))
#define DOOR_CALL_ACTIVE   (PORTAbits.RA5 == 1)
#define LIFT_AT_FLOOR  ((PORTAbits.RA6 == 0) || (PORTAbits.RA9 == 0))
#define DOOR_UNLOCKABLE   ((door_lock_flag == 0)&&((PORTAbits.RA6==0)||(PORTAbits.RA9 == 0))

#define MAX_PWM 4095
#define DELAY_MS 10

//========== SAFE MODE ================//
int safe_mode_active = 0;
volatile int safe_mode_alarm_count = 0;
#define SAFE_MODE_ALARM_ON_TICKS  2   // 0.2 seconds (2 x 100ms)
#define SAFE_MODE_ALARM_OFF_TICKS 98  // 9.8 seconds off = 10 second cycle


//====================================//
//========== LEVELLING ================//
typedef enum {
    LEV_IDLE = 0,
    LEV_RUNNING,
    LEV_COMPLETE
} LevellingState;

LevellingState levelling_state = LEV_IDLE;
//int levelling_active = 0;

//========== ANTI-CREEP ================//
typedef enum {
    AC_IDLE = 0,
    AC_RUNNING,
    AC_COMPLETE
} AntiCreepState;

AntiCreepState anticreep_state = AC_IDLE;
int anticreep_active = 0;


//====================================
typedef unsigned char uint8_t;
typedef unsigned int  uint16_t;

// Global configuration variables
int overtravel_time = 90;
int autohome_time = 300;
char serial [10] = "0000000000";
char lift_type[8]  = "IClass";
char install_date [20] = "12/2/26";
char firmware[20] = "EC17_0014_058";  // adjusting door and lighting timings
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


//========== DOOR LOCK PWM (OC2) ================//
int lock_ramp_current_speed  = 0;
int lock_ramp_target_speed   = 0;   // set from uart_door_speed or fixed value
int lock_ramp_step_size      = 5;
int lock_ramp_delay_count    = 0;
int lock_ramp_delay_target   = 0;   // 0 x 100ms ticks between steps
volatile int lock_ramp_interrupt_flag = 0;


//========== SHOOTBOLT PWM (OC3) ================//
typedef enum {
    SB_OFF = 0,
    SB_RAMPING_UP,
    SB_ON,
    SB_RAMPING_DOWN
} ShootboltState;

ShootboltState shootbolt_state       = SB_OFF;
int            sb_ramp_current_speed = 0;
int            sb_ramp_target_speed  = 255;  // adjust to suit solenoid
int            sb_ramp_step_size     = 5;
int            sb_ramp_delay_count   = 0;
int            sb_ramp_delay_target  = 2;


volatile int sb_ramp_interrupt_flag   = 0;




//========== LIGHTING CONTROL ================//
typedef enum {
    LIGHT_OFF = 0,
    LIGHT_RAMPING_UP,
    LIGHT_ON,
    LIGHT_RAMPING_DOWN
} LightState;

LightState light_state       = LIGHT_OFF;
int        light_brightness  = 0;      // 0-255 current brightness multiplier
int        light_ramp_step   = 3;      // brightness steps per 100ms tick
volatile uint32_t light_on_timer = 0;  // counts up while lights are on
int        lights_triggered  = 0;      // flag to request lights on


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
uint32_t uart_door_autoclose_timer = 100;
int uart_door_autoclose = 1;
uint32_t uart_autohome_timer = 300;
int uart_autohome = 0;
int uart_autohome_direction = 0;
uint32_t uart_overttravel_timer = 90;
uint32_t uart_top_overrrun= 5;  // 0.5 seconds
uint32_t uart_bottom_overrrun= 5;   // 0.5 seconds
uint32_t lighting_timer = 3000;   //5 minutes
uint32_t uart_red = 0;
uint32_t uart_green = 0;
uint32_t uart_blue = 0;
uint32_t uart_white = 0;

// Deferred flash save variables
volatile int config_dirty = 0;
volatile int config_save_countdown = 0;
#define CONFIG_SAVE_DELAY 50  // 5 seconds at 100ms ticks

void handle_safe_mode(void);
void handle_levelling(void);
void handle_anticreep(void);
void handle_doorlock_pwm(void);   
void handle_shootbolt(void);  
void handle_up_overrun(void);
void handle_down_overrun(void);
void handle_lighting(void);





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
    lights_I2C_Write(0x40, 0x00, 0x10);
    lights_I2C_Write(0x40, 0xFE, 3);
    lights_I2C_Write(0x40, 0x00, 0x00);
    lights_I2C_Write(0x40, 0x01, 0x04);
    
    
    
    
    
}

void PCA9685_SetPWM(uint8_t channel, uint16_t on, uint16_t off) {
    uint8_t pwm_base = (0x06 + 4 * channel);
    if (off == 0) {
        //UART1_WriteString("FULLOFF ch:");  // temp debug
        //UART1_WriteUInt(channel);
        //UART1_WriteString("\r\n");
        //lights_I2C_Write(0x40, pwm_base,     0x00);
        lights_I2C_Write(0x40, pwm_base + 1, 0x00);
        lights_I2C_Write(0x40, pwm_base + 2, 0x00);
        lights_I2C_Write(0x40, pwm_base + 3, 0x10);
    } else {
       // lights_I2C_Write(0x40, pwm_base,     on & 0xFF);
        lights_I2C_Write(0x40, pwm_base + 1, on >> 8);
        lights_I2C_Write(0x40, pwm_base + 2, off & 0xFF);
        lights_I2C_Write(0x40, pwm_base + 3, off >> 8);
    }
}
int blue_fade = 0;

void FadeLights(void) {
    for (uint16_t i = 0; i <= 4096; i += 16) {
       // PCA9685_SetPWM(0, 0, i);
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
  //  PCA9685_SetPWM(0, 0, r);
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


//==========safe mode===================

void handle_safe_mode(void) {
    if (!safe_mode_active) {
        alarm_out_SetLow();
        return;
    }

    safe_mode_alarm_count++;

    // 0-1  = alarm ON  (0.2s)
    // 2-99 = alarm OFF (9.8s)
    // 100  = cycle reset
    if (safe_mode_alarm_count <= SAFE_MODE_ALARM_ON_TICKS) {
        alarm_out_SetHigh();
    } else {
        alarm_out_SetLow();
    }

    if (safe_mode_alarm_count >= (SAFE_MODE_ALARM_ON_TICKS + SAFE_MODE_ALARM_OFF_TICKS)) {
        safe_mode_alarm_count = 0;
    }
}


// Door state machine  this works !!
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
int door_close_request = 0;
int uart_door_request= 0;

// Door ramping variables
int door_ramp_target_speed = 0;
int door_ramp_current_speed = 0;
int door_ramp_step = 0;
int door_ramp_delay_count = 0;
int door_ramp_delay_target = 2;
int ramp_step_size = 4;

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
int door_lock_timer = 500;  // 5 seconds
volatile int door_lock_on = 0;
volatile int door_lock_count = 0;
volatile int floor_arrival_unlock_timer = 0;
volatile int floor_arrival_unlock_flag = 0;
int call_unlock_request = 0;

// Helper function to check if we should unlock due to call button
int shouldUnlockForCall(void) {
    // Check if at bottom floor AND down call is active
    if ((PORTAbits.RA9 == 0) && (PORTAbits.RA6 == 1)) { // At bottom floor
        if (PORTAbits.RA0 == 1) { // Down call active (assuming RA0 is DOWN button)    - THIS WONT WORK _ RA0 is output !!!
            return 1;
        }
    }
    // Check if at top floor AND up call is active
    if ((PORTAbits.RA6 == 0) && (PORTAbits.RA9 == 1)) { // At top floor
        if (PORTAbits.RA1 == 1) { // Up call active (assuming same button for testing)    - THIS WONT WORK _ RA1 is output !!!
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
     door_ramp_interrupt_flag = 1;  // door speed ramping up or down]
     fire_interrupt_flag = 1; //  im not entirely sure why this is here
     

     
    delay_flag = 1;
    door_auto_open_timer_flag = 1;  // 100ms for door opening
       // moved from TMR3
    level_interrupt_flag = 1;  // ADDED FOR LEVELLING
    
    fire_count++;
    lights_count++;
    

    
   // overtravel_timer++;

            if (PORTEbits.RE0 == 1) overtravel_timer++;  // only count during up travel
            else overtravel_timer = 0;                    // reset when not travelling up
    
    
    open_delay++;
    
    tick_counter++;
    if (tick_counter >= 100)
    {
        tick_counter = 0; // Reset every 10 seconds 
    }

        // Safe mode alarm pulse
    handle_safe_mode();
    
    
    
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

    
    handle_doorlock_pwm();
    handle_shootbolt();
    handle_up_overrun();
    handle_down_overrun();
    
    
}

void handle_tmr4_scheduler_tick(void) {
    TMR4_SoftwareCounterClear();
    
}

void handle_lighting(void) {

    switch (light_state) {

        case LIGHT_OFF:
            light_brightness = 0;
            if (lights_triggered) {
                lights_triggered = 0;
                light_state      = LIGHT_RAMPING_UP;
                //UART1_WriteString("Lights on\r\n");
            }
            break;

        case LIGHT_RAMPING_UP:
            light_brightness += (light_ramp_step*2);
            if (light_brightness >= 255) {
                light_brightness = 255;
                light_state      = LIGHT_ON;
                light_on_timer   = 0;
            }
            // If re-triggered during ramp, reset timer
            if (lights_triggered) {
                lights_triggered = 0;
                light_on_timer   = 0;
            }
            break;
            
case LIGHT_ON:
    light_brightness = 255;
    light_on_timer++;                    // ? must be here
    if (lights_triggered) {
        lights_triggered = 0;
        light_on_timer = 0;
    }
    if (light_on_timer >= lighting_timer) {
        light_state    = LIGHT_RAMPING_DOWN;
        light_on_timer = 0;
        UART1_WriteString("Lights ramping down\r\n");
    }
    break;

        case LIGHT_RAMPING_DOWN:
            if (lights_triggered) {
                // Activity during ramp down - go back up
                lights_triggered = 0;
                light_state      = LIGHT_RAMPING_UP;
                break;
            }
            light_brightness -= light_ramp_step;
            if (light_brightness <= 0) {
                light_brightness = 0;
                light_state      = LIGHT_OFF;
            }
            break;
    }

    // Apply brightness scaling to all channels
    uint16_t r = (uint16_t)(((uint32_t)uart_red   * (uint32_t)light_brightness) / 255);
    uint16_t g = (uint16_t)(((uint32_t)uart_green * (uint32_t)light_brightness) / 255);
    uint16_t b = (uint16_t)(((uint32_t)uart_blue  * (uint32_t)light_brightness) / 255);
    uint16_t w = (uint16_t)(((uint32_t)uart_white * (uint32_t)light_brightness) / 255);

    PCA9685_SetPWM(2, 0, (uint16_t)(r * 16));
    PCA9685_SetPWM(1, 0, (uint16_t)(g * 16));
   // PCA9685_SetPWM(0, 0, (uint16_t)(b * 16));
    PCA9685_SetPWM(3, 0, (uint16_t)(w * 16));
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

void UART1_WriteHex(uint16_t value) {
    char buffer[8];
    sprintf(buffer, "%02X", value);
    UART1_WriteString(buffer);
}

//================ levelling ========================

void handle_levelling(void) {
    // RA15 high = levelling switch activated
    // Run DOWN until RA6 goes HIGH (top limit deactivates)
    // RA6 low = at top floor, RA6 high = left top floor

    switch (levelling_state) {
        case LEV_IDLE:
            if (PORTAbits.RA15 == 1) {
                // Levelling switch activated - start running down
                levelling_active = 1;
                levelling_state  = LEV_RUNNING;
                down_call_SetHigh();   // command down travel
                up_call_SetLow();      // cancel any up call
                UART1_WriteString("Levelling started\r\n");
                if (strcmp(lift_state, "ST 23") != 0) {
                    strcpy(lift_state, "ST 23");
                    change_lift_state = 1;
                }
            }
            break;

        case LEV_RUNNING:
            // Keep commanding down
            down_call_SetHigh();

            // Stop when RA6 goes HIGH (lift has moved away from top limit)
            // OR when levelling switch deactivates
            if (PORTAbits.RA6 == 1 || PORTAbits.RA15 == 0) {
                down_call_SetLow();
                levelling_active = 0;
                levelling_state  = LEV_COMPLETE;
                UART1_WriteString("Levelling complete\r\n");
            }
            break;

        case LEV_COMPLETE:
            // Wait for levelling switch to deactivate before re-arming
            if (PORTAbits.RA15 == 0) {
                levelling_state = LEV_IDLE;
            }
            break;
    }
}

void handle_anticreep(void) {
    // RA14 high = anti-creep switch activated
    // Only act if no call currently in progress
    // Run UP until RA6 goes HIGH (top limit deactivates)

    // Check if any call is in progress
    int call_in_progress = (uart_up_call_request   == 1) ||
                           (uart_dn_call_request   == 1) ||
                           (PORTAbits.RA0          == 1) ||   // down call button
                           (PORTAbits.RA1          == 1) ||   // up call button
                           (levelling_active           == 1);     // levelling takes priority

    switch (anticreep_state) {
        case AC_IDLE:
            if (PORTAbits.RA14 == 1 && !call_in_progress) {
                anticreep_active = 1;
                anticreep_state  = AC_RUNNING;
                up_call_SetHigh();     // command up travel
                down_call_SetLow();    // cancel any down call
                UART1_WriteString("Anti-creep started\r\n");
                if (strcmp(lift_state, "ST 24") != 0) {
                    strcpy(lift_state, "ST 24");
                    change_lift_state = 1;
                }
            }
            break;

        case AC_RUNNING:
            // Keep commanding up
            up_call_SetHigh();

            // Abort immediately if a call comes in or levelling activates
            if (call_in_progress) {
                up_call_SetLow();
                anticreep_active = 0;
                anticreep_state  = AC_IDLE;
                UART1_WriteString("Anti-creep interrupted\r\n");
                break;
            }

            // Stop when RA6 goes HIGH (lift moved away from top limit)
            // OR when anti-creep switch deactivates
            if (PORTAbits.RA6 == 1 || PORTAbits.RA14 == 0) {
                up_call_SetLow();
                anticreep_active = 0;
                anticreep_state  = AC_COMPLETE;
                UART1_WriteString("Anti-creep complete\r\n");
            }
            break;

        case AC_COMPLETE:
            // Wait for anti-creep switch to deactivate before re-arming
            if (PORTAbits.RA14 == 0) {
                anticreep_state = AC_IDLE;
            }
            break;
    }
}


void handle_up_overrun(void) {
    static int last_up_limit = 1;
    int current_up_limit = PORTAbits.RA6;

    if (last_up_limit == 1 && current_up_limit == 0 && !up_overrun_flag) {
        up_overrun_flag  = 1;
        up_overrun_count = 0;
        up_limit_arrived = 0;
    }

    if (up_overrun_flag == 1) {
        up_call_SetHigh();
        down_call_SetLow();
        up_safe_fault_LED_SetHigh();
        up_overrun_count++;
        if (up_overrun_count >= (int)uart_top_overrrun) {
            up_call_SetLow();
            up_limit_arrived  = 1;
            up_overrun_count  = 0;
            up_overrun_flag   = 0;
            up_safe_fault_LED_SetLow();
        }
    } else {
        if (!anticreep_active) up_call_SetLow();
    }

    if (current_up_limit == 1) up_limit_arrived = 0;
    last_up_limit = current_up_limit;
}

void handle_down_overrun(void) {
    static int last_down_limit = 1;
    int current_down_limit = PORTAbits.RA9;

    if (last_down_limit == 1 && current_down_limit == 0 && !down_overrun_flag) {
        down_overrun_flag  = 1;
        down_overrun_count = 0;
        down_limit_arrived = 0;
        dn_safe_fault_LED_SetHigh();
    }

    if (down_overrun_flag == 1) {
        //down_call_SetHigh();
        PCA9685_SetPWM(0, 0,4096);
        up_call_SetLow();
        down_overrun_count++;
        if (down_overrun_count >= (int)uart_bottom_overrrun) {
            //down_call_SetHigh();
          PCA9685_SetPWM(0, 0,4096);   
            down_limit_arrived  = 1;
            down_overrun_count  = 0;
            down_overrun_flag   = 0;
            dn_safe_fault_LED_SetLow();
        }
    } else {
        if (!levelling_active)  down_call_SetHigh();
    }

    if (current_down_limit == 1) down_limit_arrived = 0;
    last_down_limit = current_down_limit;
}

//========== DOOR LOCK PWM RAMP (OC2) ================//
// Ramps up when door_lock_on goes high, ramps down when it goes low
void handle_doorlock_pwm(void) {
    if (!lock_ramp_interrupt_flag) return; 
    lock_ramp_interrupt_flag = 0;
    // Note: flag is cleared by door motor handler - use a separate copy
    // Actually use lights_interrupt_flag timing instead - already cleared above
    // So call this BEFORE lights_interrupt_flag is cleared, or add a dedicated flag
    
    if (door_lock_on) {
        // Ramp up
        if (lock_ramp_current_speed < lock_ramp_target_speed) {
            lock_ramp_delay_count++;
            if (lock_ramp_delay_count >= lock_ramp_delay_target) {
                lock_ramp_current_speed += lock_ramp_step_size;
                if (lock_ramp_current_speed > lock_ramp_target_speed)
                    lock_ramp_current_speed = lock_ramp_target_speed;
                lock_ramp_delay_count = 0;
            }
        }
    } else {
        // Ramp down
        if (lock_ramp_current_speed > 0) {
            lock_ramp_delay_count++;
            if (lock_ramp_delay_count >= lock_ramp_delay_target) {
                lock_ramp_current_speed -= lock_ramp_step_size;
                if (lock_ramp_current_speed < 0)
                    lock_ramp_current_speed = 0;
                lock_ramp_delay_count = 0;
            }
        }
    }
    OC2_PrimaryValueSet((uint16_t)lock_ramp_current_speed);
}

//========== SHOOTBOLT PWM RAMP (OC3) ================//
// Only operates when RC3 == 1 (FP override zone active)
// Ramps up when down call starts in FP zone
// Ramps down when FP override deactivates
void handle_shootbolt(void) {
    if (!sb_ramp_interrupt_flag) return;
    sb_ramp_interrupt_flag = 0;
    int fp_zone_active   = (PORTCbits.RC3 == 1);
    int down_call_active = (uart_dn_call_request == 1) || (PORTAbits.RA0 == 1);

    switch (shootbolt_state) {

        case SB_OFF:
            sb_ramp_current_speed = 0;
            OC3_PrimaryValueSet(0);
            // Only arm if FP zone is active AND a down call starts
            if (fp_zone_active && down_call_active) {
                shootbolt_state = SB_RAMPING_UP;
                sb_ramp_delay_count = 0;
                UART1_WriteString("Shootbolt ramping up\r\n");
            }
            break;

        case SB_RAMPING_UP:
            // Cancel immediately if FP zone deactivates
            if (!fp_zone_active) {
                shootbolt_state = SB_RAMPING_DOWN;
                UART1_WriteString("Shootbolt: FP zone off, ramping down\r\n");
                break;
            }
            sb_ramp_delay_count++;
            if (sb_ramp_delay_count >= sb_ramp_delay_target) {
                sb_ramp_current_speed += sb_ramp_step_size;
                if (sb_ramp_current_speed >= sb_ramp_target_speed) {
                    sb_ramp_current_speed = sb_ramp_target_speed;
                    shootbolt_state       = SB_ON;
                    UART1_WriteString("Shootbolt on\r\n");
                }
                sb_ramp_delay_count = 0;
            }
            OC3_PrimaryValueSet((uint16_t)sb_ramp_current_speed);
            break;

        case SB_ON:
            OC3_PrimaryValueSet((uint16_t)sb_ramp_target_speed);
            // Ramp down when FP zone deactivates
            if (!fp_zone_active) {
                shootbolt_state     = SB_RAMPING_DOWN;
                sb_ramp_delay_count = 0;
                UART1_WriteString("Shootbolt ramping down\r\n");
            }
            break;

        case SB_RAMPING_DOWN:
            sb_ramp_delay_count++;
            if (sb_ramp_delay_count >= sb_ramp_delay_target) {
                sb_ramp_current_speed -= sb_ramp_step_size;
                if (sb_ramp_current_speed <= 0) {
                    sb_ramp_current_speed = 0;
                    shootbolt_state       = SB_OFF;
                    UART1_WriteString("Shootbolt off\r\n");
                }
                sb_ramp_delay_count = 0;
            }
            OC3_PrimaryValueSet((uint16_t)sb_ramp_current_speed);
            break;
    }
}


// ================  UART PROCESS COMMANDS ====================





void UART1_ProcessCommand(const char *cmd) {
    //__delay_ms(10); // this is a finger in the air delay - speeds all need looking at thoroughly
    
      if (strstr(cmd, "AA UP") != NULL) {
        uart_up_call_request = 1;
        uart_dn_call_request = 0;
        up_call_SetHigh();
        down_call_SetLow();
        UART1_WriteString("UP call requested\r\n");

    } else if (strstr(cmd, "AA DN") != NULL) {
        uart_dn_call_request = 1;
        uart_up_call_request = 0;
        down_call_SetHigh();
        up_call_SetLow();
        UART1_WriteString("DN call requested\r\n");

    } else if (strstr(cmd, "AA XX") != NULL) {
        uart_up_call_request = 0;
        uart_dn_call_request = 0;
        up_call_SetLow();
        down_call_SetLow();
        UART1_WriteString("All calls cancelled\r\n");
        
        
        } else if (strcmp(cmd, "DR") == 0) {
        uart_up_call_request = 0;
        uart_dn_call_request = 0;
        up_call_SetLow();       // cancel up call
        down_call_SetLow();     // cancel down call
        uart_door_request = 1;
        UART1_WriteString("All calls cancelled\r\n");    
        
        //====================safe mode==========================
        
        } else if (strcmp(cmd, "SF") == 0) {
        safe_mode_active = 1;
        safe_mode_alarm_count = 0;
        UART1_WriteString("Safe mode activated\r\n");

    } else if (strcmp(cmd, "SC") == 0) {
        // SC = Safe mode Cancel
        safe_mode_active = 0;
        safe_mode_alarm_count = 0;
        alarm_out_SetLow();
        UART1_WriteString("Safe mode cancelled\r\n");
        
        
    //=================config messages============================

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
        
      } else if (strncmp(cmd, "LT ", 3) == 0) {
    int ltvalue;
    if (sscanf(cmd, "LT %d", &ltvalue) == 1) {
        lighting_timer = ltvalue;
        UART1_WriteString("Lighting timer updated: ");
        UART1_WriteUInt(lighting_timer);
        UART1_WriteString("\r\n");
        Config_MarkDirty();
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
       UART1_WriteString("LR "); UART1_WriteHex((uint16_t)uart_red);   UART1_WriteString("\r\n");
UART1_WriteString("LG "); UART1_WriteHex((uint16_t)uart_green); UART1_WriteString("\r\n");
UART1_WriteString("LB "); UART1_WriteHex((uint16_t)uart_blue);  UART1_WriteString("\r\n");
UART1_WriteString("LW "); UART1_WriteHex((uint16_t)uart_white); UART1_WriteString("\r\n");
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
    
// Set door disable HIGH first, before touching PWM
door_disable_SetHigh();          // ? disable motor driver FIRST
door_direction_SetLow();         // ? set direction before enabling

// Now safe to initialise PWM'S  with zero

//===========DOOR MOTOR PWM SETUP================
OC1_Initialize();
OC1_Start();
OC1_PrimaryValueSet(0);          // ? start at zero, not uart_door_speed
OC1_SecondaryValueSet(0x100);
door_disable_SetHigh();   // repeat after PWM init just to be sure

//===========DOOR LOCK PWM SETUP================
OC2_Initialize(); OC2_Start();
OC2_PrimaryValueSet(0);
OC2_SecondaryValueSet(0x100);
lock_ramp_target_speed = 0xFF;    //set to fully on


//===========SHOOTBOLT PWM SETUP================
OC3_Initialize(); OC3_Start();
OC3_PrimaryValueSet(0);
OC3_SecondaryValueSet(0x100);



// Now safe to do the long delay
__delay_ms(500);
PCA9685_Init();
    __delay_ms(200);
// Boot light test - full white for 2 seconds
// After PCA9685_Init() - test each channel one at a time
UART1_WriteString("Ch0\r\n");
//PCA9685_SetPWM(0, 0, 4095);
__delay_ms(2000);
//PCA9685_SetPWM(0, 0, 0);

UART1_WriteString("Ch1\r\n");
PCA9685_SetPWM(1, 0, 4095);
__delay_ms(2000);
PCA9685_SetPWM(1, 0, 0);

UART1_WriteString("Ch2\r\n");
PCA9685_SetPWM(2, 0, 4095);
__delay_ms(2000);
PCA9685_SetPWM(2, 0, 0);

UART1_WriteString("Ch3\r\n");
PCA9685_SetPWM(3, 0, 4095);
__delay_ms(2000);
PCA9685_SetPWM(3, 0, 0);
UART1_WriteString("Boot light test done\r\n");
    
    
    
    
    
    
    // Digital outputs initializing
    down_call_SetLow();                   //A0  set highj to put a down call in
    up_call_SetLow();                        //A1  set high to put an up call in
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
    keyswitch_block_out_SetLow(); // F12 - Set high to allow travel control from inside the lift
    delay_24v_3s_SetHigh();            // G7 - set low to disable lift up or down travel
    door_open_led_SetLow();           //G14 Set high to turn on the door open LED
    door_close_led_SetLow();           // G15 Set high to turn on the door close LED
  
__delay_ms(500);
    
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

    

    
    UART1_WriteString("System ready\r\n");
    
    OC1_PrimaryValueSet(0);
    
    int last_floor_state = -1;  // chnged from 0
    int current_floor_state;
    
    
    door_ramp_target_speed = uart_door_speed;
    
    
    
    //========== MAIN PROGRAM LOOP ================//
    while (1) {
        
         ClrWdt();  // add this as first line of main loop
        
        // Lighting control
        /*
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
        */
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
  
        if ((PORTAbits.RA6 == 0) && (PORTAbits.RA9 == 1)) {
    // At top floor
    current_floor_state = 0;
    if (last_floor_state != 0) {
        // NEW ARRIVAL at top floor
        start_door_unlock();      // Unlock immediately
        door_auto_open_request = 1;  // Request door to open immediately
    }
    if (strcmp(lift_state, "ST 21") != 0) {
        strcpy(lift_state, "ST 21");
        change_lift_state = 1;
    }
    
} else if ((PORTAbits.RA9 == 0) && (PORTAbits.RA6 == 1)) {
    // At bottom floor
    current_floor_state = 1;
    if (last_floor_state != 1) {
        // NEW ARRIVAL at bottom floor
        start_door_unlock();      // Unlock immediately
        door_auto_open_request = 1;  // Request door to open immediately
    }
    if (strcmp(lift_state, "ST 1F") != 0) {
        strcpy(lift_state, "ST 1F");
        change_lift_state = 1;
    }
    
} else if ((PORTAbits.RA9 == 1) && (PORTAbits.RA6 == 1)) {
    // Between floors - reset
    current_floor_state = 2;
    door_auto_open_request = 0;
    if (strcmp(lift_state, "ST 27") != 0) {
        strcpy(lift_state, "ST 27");
        change_lift_state = 1;
    }
    
} else if ((PORTAbits.RA9 == 0) && (PORTAbits.RA6 == 0)) {
    // Floor switch fault
    current_floor_state = 3;
    if (strcmp(lift_state, "ST 16") != 0) {
        strcpy(lift_state, "ST 16");
        change_lift_state = 1;
    }
}
        
  //========== LEVELLING & ANTI-CREEP ==========//
        handle_levelling();
        handle_anticreep();      
        
        
  //========== RA5 DOOR CALL BUTTON ==========//
{
    static int last_ra5 = 0;
    int current_ra5 = PORTAbits.RA5;
    
    if (current_ra5 == 1 && last_ra5 == 0) {
        // Rising edge - treat exactly like "DR" command
        uart_door_request = 1;
        UART1_WriteString("ST 07\r\n");  // door call status
    }
    last_ra5 = current_ra5;
}      
        
        
        //========== DOOR CONTROL STATE MACHINE ==========//
        
        //if (DOOR_SAFE_TO_OPEN_AT_TOP || DOOR_SAFE_TO_OPEN_AT_BOTTOM)  // As it says
            if ((PORTAbits.RA6 == 0) || (PORTAbits.RA9 == 0))  // temp replaced
                
        {

            switch (door_status) {
                case DOOR_IDLE:
                   // UART1_WriteString (" door idle \r\n");
                    // Only respond to door_auto_open_request, NOT isDoorCallActive
                    if ((door_auto_open_request)||(PORTBbits.RB13==0 && uart_door_request))
                    {
                        start_door_unlock();  // added 
                        door_disable_SetLow(); // enable the door
                        door_direction_SetLow();  // set direction to opening
                        door_status = DOOR_OPENING;  // change the door status
                        door_ramp_current_speed = 0;  // set the ramp speed to 0
                        door_ramp_step = 0;
                        door_ramp_delay_count = 0;  // reset the delay before ramping up
                        OC1_PrimaryValueSet(0);  // speed sent to the PWM  //  was uart_door_speed
                           //OC1_PrimaryValueSet(door_ramp_current_speed);
                        door_auto_open_request = 0;  // reset the auto door request
                        
                        door_open_led_SetHigh();  // turn on the door open led
                        door_close_led_SetLow();  // and turn off the door closed   - tremp on as well 
                        
                        if (strcmp(door_state, "ST 08") != 0) 
                        {
                            strcpy(door_state, "ST 08");
                            change_door_state = 1;
                        }
                    }
                    break;
                    
                case DOOR_OPENING:
                    //UART1_WriteString (" door opening\r\n");
                   if (door_ramp_interrupt_flag) {
    door_ramp_interrupt_flag = 0;
    door_ramp_delay_count++;
    if (door_ramp_delay_count >= door_ramp_delay_target) {
        if (door_ramp_current_speed < door_ramp_target_speed) {
            door_ramp_current_speed += ramp_step_size;
            if (door_ramp_current_speed > door_ramp_target_speed) {
                door_ramp_current_speed = door_ramp_target_speed;
            }
        }
        OC1_PrimaryValueSet(door_ramp_current_speed);
        door_ramp_delay_count = 0;
    }
}
                    if (PORTBbits.RB12 == 0) {
                        //UART1_WriteString("DOOR: Fully open, stopping motor\r\n");
                        for(int i = door_ramp_current_speed; i >= 0; i -= 100) {
                            if(i < 0) i = 0;
                            OC1_PrimaryValueSet(i);
                           // __delay_ms(10);
                        }
                        door_direction_SetHigh(); // set door direction to close
                        OC1_PrimaryValueSet(0);
                        door_status = DOOR_OPEN;
                        door_auto_open_request = 0; 
                        uart_door_request = 0;
                        
        
                        if (strcmp(door_state, "ST 09") != 0) 
                        {
                            strcpy(door_state, "ST 09");
                            change_door_state = 1;
                        }
                        door_open_led_SetLow();
                    }
                    break;
                    
                case DOOR_OPEN:
                   if ((uart_door_autoclose && ADC_flag) || door_close_request||uart_door_request) {
                        //UART1_WriteString("DOOR: Starting closing sequence\r\n");
                        door_disable_SetLow();  // enable the motor
                        door_direction_SetHigh(); // set door direction to close
                        door_status = DOOR_CLOSING;
                        door_ramp_current_speed = 0;
                        door_ramp_step = 0;
                        door_ramp_delay_count = 0;
                        OC1_PrimaryValueSet(0);  // start ramp from zero
                        
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
        }
        OC1_PrimaryValueSet(door_ramp_current_speed);
        door_ramp_delay_count = 0;
    }
}
                    if (PORTBbits.RB13 == 0) {
                        //UART1_WriteString("DOOR: Fully closed, stopping motor\r\n");
                        for(int i = door_ramp_current_speed; i >= 0; i -= 100) {
                            if(i < 0) i = 0;
                            OC1_PrimaryValueSet(i);
                            __delay_ms(10);
                        }
                        OC1_PrimaryValueSet(0);
                        uart_door_request = 0;
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
            OC1_PrimaryValueSet(0);
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
            
            // 50 ticks * 100ms = 5 seconds
            if (door_lock_count >= 50) {
                door_lock_on = 0;   // turns off the door lock
                door_lock_flag = 0;   // reset the flag
                door_lock_count = 0;  // reset the clock
            }
        }
        
        // Update door lock LED/indicator
        if (door_lock_on) {
            door_lock_SetHigh(); // Unlocked - solenoid activated
         
        } else {
            door_lock_SetLow(); // Locked - solenoid deactivated
          
        }

//========== LIGHTING TRIGGER DETECTION ==========//
{
   static int last_t_travel = 0;
static int last_t_door   = 0;
static int last_t_lock   = 0;
static int last_t_upcall = 0;
static int last_t_dncall = 0;
static int last_t_doorcall = 0;

int lift_travelling_now = (PORTAbits.RA6 == 1) && (PORTAbits.RA9 == 1);

int t_travel = lift_travelling_now;
int t_door   = (door_status == DOOR_OPENING) ||
               (door_status == DOOR_OPEN)    ||
               (door_status == DOOR_CLOSING);
int t_lock   = (door_lock_on == 1);
int t_upcall = (uart_up_call_request == 1);
int t_dncall = (uart_dn_call_request == 1);
int t_doorcall = (uart_door_request == 1);


// Rising edge on ANY source triggers lights
if ((t_travel && !last_t_travel) ||
    (t_door   && !last_t_door)   ||
    (t_lock   && !last_t_lock)   ||
      (t_doorcall   && !last_t_doorcall)   ||  
    (t_upcall && !last_t_upcall) ||
    (t_dncall && !last_t_dncall)) {
    lights_triggered = 1;
    UART1_WriteString("LTrig:");
    if (t_travel) UART1_WriteString("TRAV ");
    if (t_door||t_doorcall)   UART1_WriteString("DOOR ");
    if (t_lock)   UART1_WriteString("LOCK ");
    if (t_upcall) UART1_WriteString("UP ");
    if (t_dncall) UART1_WriteString("DN ");
    UART1_WriteString("\r\n");
}

last_t_travel = t_travel;
last_t_door   = t_door;
last_t_lock   = t_lock;
last_t_upcall = t_upcall;
last_t_dncall = t_dncall;
last_t_doorcall = t_doorcall;
}

        
      //========== LIGHTING STATE MACHINE ==========//
        
if (lights_interrupt_flag) {
    lights_interrupt_flag = 0;
    handle_lighting();
}
        
        
        //========== AUTO-CLOSE TIMER HANDLING ==========//
        if (door_status == DOOR_OPEN && uart_door_autoclose) {
            if (door_delay_interrupt_flag) {
                door_delay_interrupt_flag = 0;
                ADC_count++;
                if (ADC_count >= uart_door_autoclose_timer) {
                    ADC_flag = 1;
                    ADC_count = 0;
                    
                }
            }
        } else {
            ADC_count = 0;
            ADC_flag = 0;
        }
        
        
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
        int currently_travelling = (PORTEbits.RE0 == 1) ||
                                                (PORTGbits.RG12 == 1)||
                                                (down_overrun_flag == 1)||  // v062 added
                                                (up_overrun_flag == 1);      // v062 added
        
        
        ;  // changed from RA0 and RA1 as these are outputs !!
        
        //==============================
        // can these be used to hold on one touch ?
        //==============================
        
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
        
        
        //========== UP/DOWN CONTROLS ==========//
        
        up_sw_led_SetHigh();  //  illuminate the up button
        dn_sw_led_SetHigh();  // illuminate the down button
        stop_sw_led_SetHigh();  // illuminate the stop button
        
// ?? Alarm ?????????????????????????????????????????
if (!safe_mode_active) {
    // Normal alarm operation - driven by RE1 input
    if (PORTEbits.RE1 == 1) alarm_out_SetHigh();
    else                     alarm_out_SetLow();
}
// Safe mode alarm is handled in handle_safe_mode() via TMR2
        
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
     /*   
        //========== UP OVERRUN ==========//
// When lift arrives at top (RA6 goes low), keep running UP
// for uart_top_overrrun x 10ms ticks, then stop
{
    static int last_up_limit = 1;
    int current_up_limit = PORTAbits.RA6;

    // Edge detect: limit switch just activated
    if (last_up_limit == 1 && current_up_limit == 0 && !up_overrun_flag) {
        up_overrun_flag  = 1;
        up_overrun_count = 0;
        up_limit_arrived = 0;
        UART1_WriteString("Up overrun started\r\n");
    }

    if (up_overrun_flag == 1) {
        up_call_SetLow();    // keep running UP past the limit
        down_call_Sethigh();   // ensure no conflicting call
        up_safe_fault_LED_SetHigh();  // temp debug added

        if (up_overrun_interrupt_flag == 1) {
            up_overrun_interrupt_flag = 0;
            up_overrun_count++;

            if (up_overrun_count >= (int)uart_top_overrrun) {
                up_call_SetHigh();     // stop up travel
                up_limit_arrived  = 1;
                up_overrun_count  = 0;
                up_overrun_flag   = 0;
                up_safe_fault_LED_SetLow(); // temp debug added
                UART1_WriteString("Up overrun complete\r\n");
            }
        }
    } else {
        // Not in overrun and not anticreep - ensure up call is clear
        if (!anticreep_active) up_call_SetHigh();
    }

    // Reset arrived flag when lift leaves top floor
    if (current_up_limit == 1) {
        up_limit_arrived = 0;
    }

    last_up_limit = current_up_limit;
}
*/
        /*
//========== DOWN OVERRUN ==========//
// When lift arrives at bottom (RA9 goes low), keep running DOWN
// for uart_bottom_overrrun x 10ms ticks, then stop
{
    static int last_down_limit = 1;
    int current_down_limit = PORTAbits.RA9;

    // Edge detect: limit switch just activated
    if (last_down_limit == 1 && current_down_limit == 0 && !down_overrun_flag) {
        down_overrun_flag  = 1;
        down_overrun_count = 0;
        down_limit_arrived = 0;
          dn_safe_fault_LED_SetHigh(); // temp debug added
        UART1_WriteString("Down overrun started\r\n");
    }

    if (down_overrun_flag == 1) {
        down_call_SetHigh();  // keep running DOWN past the limit
        up_call_SetHigh();     // ensure no conflicting call

        if (down_overrun_interrupt_flag == 1) {
            down_overrun_interrupt_flag = 0;
            down_overrun_count++;

            if (down_overrun_count >= (int)uart_bottom_overrrun) {
                down_call_SetLow();    // stop down travel
                down_limit_arrived  = 1;
                down_overrun_count  = 0;
                down_overrun_flag   = 0;
                dn_safe_fault_LED_SetLow(); // temp debug added
                UART1_WriteString("Down overrun complete\r\n");
            }
        }
    } else {
        // Not in overrun and not levelling - ensure down call is clear
        if (!levelling_active) down_call_SetLow();
    }

    // Reset arrived flag when lift leaves bottom floor
    if (current_down_limit == 1) {
        down_limit_arrived = 0;
    }

    last_down_limit = current_down_limit;
}
   */     
        //========== INTERRUPT TICKS ==========//
        if (TMR2_SoftwareCounterGet() >= 1) {
            handle_tmr2_scheduler_tick();
        }
        if (TMR3_SoftwareCounterGet() >= 1) {
            handle_tmr3_scheduler_tick();
        }
        
//========== OVERTRAVEL TIMER ==========//
{
    int currently_up = (PORTEbits.RE0 == 1);

    if (currently_up && !overtravel_flag) {
        if (overtravel_timer >= overtravel_time) {
            overtravel_flag  = 1;
            overtravel_timer = 0;
            UART1_WriteString("ST 17\r\n");  // overtravel fault status
            // Save to flash so it survives power off
            Config_MarkDirty();
        }
    } else if (!currently_up) {
        overtravel_timer = 0;  // reset timer when not going up
    }

    // RD13 = engineer reset
    if (PORTDbits.RD13 == 1) {
        if (overtravel_flag) {
            UART1_WriteString("Overtravel reset\r\n");
            Config_MarkDirty();
        }
        overtravel_flag  = 0;
        overtravel_timer = 0;
    }

    // Apply to outputs
    if (overtravel_flag == 1) {
        overtravel_led_SetHigh();  // LED ON = fault active
        UP_control_SetLow();       // Block up travel
    } else {
        overtravel_led_SetLow();   // LED OFF = clear
        UP_control_SetHigh();      // Allow up travel
    }
}
        
        //========== DOWN LIMIT & FP ZONE ==========//
        if ((PORTCbits.RC3 == 1)) {
            dn_limit_override_SetHigh();
           // down_fire_inhib_LED_SetLow();
        } else {
            dn_limit_override_SetLow();
           // down_fire_inhib_LED_SetHigh();
        }
        
        //========== STATUS MESSAGE SENDING ==========//
        //  only send status messages when they change
        if (change_door_state == 1) {
            UART1_WriteString(door_state);UART1_WriteString("\r\n");
            change_door_state = 0;
        }
        if (change_lock_state == 1) {
            UART1_WriteString(lock_state);UART1_WriteString("\r\n");
            change_lock_state = 0;
        }
        if (change_lift_state == 1) {
            UART1_WriteString(lift_state);UART1_WriteString("\r\n");
            change_lift_state = 0;
        }
        if (change_fire_state == 1) {
            UART1_WriteString(fire_state);UART1_WriteString("\r\n");
            change_fire_state = 0;
        }






    } // End of while loop
    
} // End of main

/**
 End of File
*/
