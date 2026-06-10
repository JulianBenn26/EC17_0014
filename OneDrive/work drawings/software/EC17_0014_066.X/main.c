/**
  Generated main.c file from MPLAB Code Configurator

  @Company
    Wessex Lift Co Ltd

  @File Name
    main.c

  @Summary
    This is the generated main.c using PIC24 / dsPIC33 / PIC32MM MCUs.

  @Description
    Device : PIC24FJ128GA310
    Compiler : XC16 v2.10
    MPLAB    : MPLAB X v6.25

    Wessex EC17 0014  rev 066 - with 5 colour lighting  10/6/26
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

#include "adc_an9.h"

// Function prototypes
void handle_lighting(void);
void handle_overtravel(void);
void handle_fire_test(void);
void handle_travel_mode(void);

#define ST7036_ADDR 0x3C
#define LCD_CMD  0x00
#define LCD_DATA 0x40
#define FCY      4000000UL
#define BAUDRATE 38400
#define BRGVAL   ((FCY / BAUDRATE)/16)-1

#include <libpic30.h>
#define LIFT_NOT_AT_FLOOR           ((PORTAbits.RA6 == 1) && (PORTAbits.RA9 == 1))
#define DOOR_SAFE_TO_OPEN_AT_TOP    ((PORTAbits.RA6 == 0) && (PORTCbits.RC4 == 0))
#define DOOR_SAFE_TO_OPEN_AT_BOTTOM ((PORTAbits.RA9 == 0) && (PORTCbits.RC4 == 0))
#define DOOR_CALL_ACTIVE            (PORTAbits.RA5 == 1)
#define LIFT_AT_FLOOR               ((PORTAbits.RA6 == 0) || (PORTAbits.RA9 == 0))
#define DOOR_UNLOCKABLE             ((door_lock_flag == 0) && ((PORTAbits.RA6 == 0) || (PORTAbits.RA9 == 0)))

#define MAX_PWM  4095
#define DELAY_MS 10

//========== SAFE MODE ================//
int safe_mode_active = 0;
volatile int safe_mode_alarm_count = 0;
#define SAFE_MODE_ALARM_ON_TICKS  2
#define SAFE_MODE_ALARM_OFF_TICKS 98

//========== LEVELLING ================//
typedef enum { LEV_IDLE = 0, LEV_RUNNING, LEV_COMPLETE } LevellingState;
LevellingState levelling_state = LEV_IDLE;

//========== ANTI-CREEP ================//
typedef enum { AC_IDLE = 0, AC_RUNNING, AC_COMPLETE } AntiCreepState;
AntiCreepState anticreep_state = AC_IDLE;
int anticreep_active = 0;

typedef unsigned char uint8_t;
typedef unsigned int  uint16_t;

// Global configuration variables
int  overtravel_time = 90;
int  autohome_time   = 300;
char serial[10]       = "0000000000";
char lift_type[8]     = "IClass";
char install_date[20] = "12/2/26";
char firmware[20]     = "EC17_0014_064";
int  rb = 0;

#define RX_LEN 8
volatile char    rx_buffer[RX_LEN];
volatile uint8_t rx_index  = 0;
volatile bool    rx_ready  = false;
volatile bool    send_boo  = false;
volatile uint16_t __attribute__((address(0x0900), persistent)) boot_magic_flag;

#define FLASH_PAGE_SIZE 512
#define FLASH_ROW_SIZE   64

void Flash_ErasePage(uint32_t addr) {
    NVMCON = 0x4042;
    TBLPAG = (addr >> 16) & 0xFF;
    __builtin_tblwtl(addr & 0xFFFF, 0xFFFF);
    __builtin_disi(5);
    __builtin_write_NVM();
    while (NVMCONbits.WR);
}

void Flash_WriteRow(uint32_t addr, uint16_t *data) {
    NVMCON = 0x4001;
    TBLPAG = (addr >> 16) & 0xFF;
    uint32_t offset = addr & 0xFFFF;
    for (int i = 0; i < 64; i++) {
        __builtin_tblwtl(offset, data[i*2]);
        __builtin_tblwth(offset, data[i*2+1]);
        offset += 2;
    }
    NVMCON = 0x4003;
    __builtin_disi(5);
    __builtin_write_NVM();
    while (NVMCONbits.WR);
}

#define UART_RX_BUFFER_SIZE 64
char    uart_rx_buffer[UART_RX_BUFFER_SIZE];
uint8_t uart_rx_index = 0;

void UART1_WriteUInt(uint16_t value) {
    char buffer[20];
    sprintf(buffer, "%u", value);
    UART1_WriteString(buffer);
}

//========== DOOR LOCK PWM (OC2) ================//
int lock_ramp_current_speed = 0;
int lock_ramp_target_speed  = 0;
int lock_ramp_step_size     = 5;
int lock_ramp_delay_count   = 0;
int lock_ramp_delay_target  = 0;
volatile int lock_ramp_interrupt_flag = 0;

//========== SHOOTBOLT PWM (OC3) ================//
typedef enum { SB_OFF = 0, SB_RAMPING_UP, SB_ON, SB_RAMPING_DOWN } ShootboltState;
ShootboltState shootbolt_state       = SB_OFF;
int            sb_ramp_current_speed = 0;
int            sb_ramp_target_speed  = 255;
int            sb_ramp_step_size     = 5;
int            sb_ramp_delay_count   = 0;
int            sb_ramp_delay_target  = 2;
volatile int   sb_ramp_interrupt_flag = 0;

//========== LIGHTING ================//
typedef enum { LIGHT_OFF = 0, LIGHT_RAMPING_UP, LIGHT_ON, LIGHT_RAMPING_DOWN } LightState;
LightState light_state      = LIGHT_OFF;
int        light_brightness = 0;
int        light_ramp_step  = 3;
volatile uint32_t light_on_timer = 0;
int        lights_triggered = 0;

uint32_t red, blue, green, white;

// UART travel request flags
int uart_up_call_request = 0;
int uart_dn_call_request = 0;

// Refresh pulse flags.
// Set each time AA UP / AA DN is received (i.e. once per UART message).
// Cleared by handle_travel_mode() on the 100ms tick.
// This allows the handler to detect whether a new command arrived within
// the last 100ms tick window, without relying on the persistent request flag.
volatile int up_call_refresh = 0;
volatile int dn_call_refresh = 0;

char     uart_serial_number[10] = "0000000000";
char     uart_install_date[10]  = "0000000000";
char     uart_lift_type[5]      = "00000";
int      power_door = 0;

uint32_t uart_door_speed           = 0x7F;
uint32_t uart_door_open_delay      = 0x00;
uint32_t uart_door_autoclose_timer = 100;
int      uart_door_autoclose       = 1;
uint32_t uart_autohome_timer       = 300;
int      uart_autohome             = 0;
int      uart_autohome_direction   = 0;
uint32_t uart_overttravel_timer    = 90;
uint32_t uart_top_overrrun         = 5;
uint32_t uart_bottom_overrrun      = 5;
uint32_t lighting_timer            = 3000;
uint32_t uart_red   = 0;
uint32_t uart_green = 0;
uint32_t uart_blue  = 0;
uint32_t uart_white = 0;
uint32_t uart_warm_white = 0;

volatile int config_dirty          = 0;
volatile int config_save_countdown = 0;
#define CONFIG_SAVE_DELAY 50

//==============================================================
//  TRAVEL MODE
//==============================================================
//
//  TRAVEL_MODE_HOLD (0) - Hold to run
//    The ESP32 must send "AA UP" or "AA DN" every 100ms while
//    the button is held.  Each receipt sets the refresh flag.
//    If no refresh is seen within HTR_TIMEOUT ticks (200ms),
//    the call is cancelled and the lift stops.
//
//  TRAVEL_MODE_ONETOUCH (1) - One touch
//    Works identically to hold-to-run until the button has been
//    held continuously for OT_LATCH_TIME ticks (500ms).
//    After that, the call is latched: the lift continues running
//    even if the button is released.
//    The latch is cleared when:
//      - The floor limit switch fires (RA6 or RA9 goes low)
//      - E-stop (RD14 low)
//      - "AA XX" is received over UART
//      - (Add physical stop pin check in handle_travel_mode if needed)
//
//  Set mode with UART command:  TT 0  or  TT 1
//  Value is saved to flash.
//
//  NOTE: levelling, anti-creep, and overrun handlers call
//  up_call_SetHigh / down_call_SetLow directly and are NOT
//  affected by this handler - they are safety / mechanical functions.
//==============================================================

#define TRAVEL_MODE_HOLD     0
#define TRAVEL_MODE_ONETOUCH 1
int travel_mode = TRAVEL_MODE_HOLD;

#define HTR_TIMEOUT  2    // 2 x 100ms = 200ms without refresh = stop
volatile int htr_watchdog_up = 0;
volatile int htr_watchdog_dn = 0;

#define OT_LATCH_TIME 5   // 5 x 100ms = 500ms hold required to latch
int ot_timer_up   = 0;
int ot_timer_dn   = 0;
int ot_latched_up = 0;
int ot_latched_dn = 0;


void handle_safe_mode(void);
void handle_levelling(void);
void handle_anticreep(void);
void handle_doorlock_pwm(void);
void handle_shootbolt(void);
void handle_up_overrun(void);
void handle_down_overrun(void);
void handle_lighting(void);
void handle_travel_mode(void);


void UART1_WriteString(const char *text) {
    while (*text) { while (U1STAbits.UTXBF); U1TXREG = *text++; }
}

void UART1_WriteChar(char c) {
    while (U1STAbits.UTXBF); U1TXREG = c;
}

void lights_I2C_Write(uint8_t addr, uint8_t reg, uint8_t data) {
    I2C1CONbits.SEN = 1; while (I2C1CONbits.SEN);
    I2C1TRN = addr << 1; while (I2C1STATbits.TRSTAT);
    I2C1TRN = reg;       while (I2C1STATbits.TRSTAT);
    I2C1TRN = data;      while (I2C1STATbits.TRSTAT);
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

//===========================  FLASH / OTA  ====================
#define HEX_DATA     0x00
#define HEX_EOF      0x01
#define HEX_EXT_ADDR 0x04

uint8_t OTA_ProcessHexLine(const char *line) {
    if (line[0] != ':') return 0;
    uint8_t buf[32]; int len = 0;
    const char *p = line + 1;
    while (*p && len < 32) { char h[3]={p[0],p[1],'\0'}; buf[len++]=(uint8_t)strtol(h,NULL,16); p+=2; }
    uint8_t sum = 0;
    for (int i = 0; i < len-1; i++) sum += buf[i];
    sum = (~sum)+1;
    if (sum != buf[len-1]) return 0;
    uint8_t  byteCount = buf[0];
    uint16_t address   = ((uint16_t)buf[1]<<8)|buf[2];
    uint8_t  recType   = buf[3];
    static uint32_t extAddr = 0;
    switch (recType) {
        case HEX_EOF: return 2;
        case HEX_EXT_ADDR: extAddr=((uint32_t)buf[4]<<24)|((uint32_t)buf[5]<<16); return 1;
        case HEX_DATA: {
            uint32_t flashAddr = (extAddr|address)>>1;
            if ((flashAddr%FLASH_PAGE_SIZE)==0) Flash_ErasePage(flashAddr);
            uint16_t rowBuf[128]; memset(rowBuf,0xFF,sizeof(rowBuf));
            for (int i=0;i<byteCount;i+=4) { int idx=(i/4)*2; rowBuf[idx]=((uint16_t)buf[4+i+1]<<8)|buf[4+i]; rowBuf[idx+1]=buf[4+i+2]; }
            Flash_WriteRow(flashAddr,rowBuf); return 1;
        }
    }
    return 0;
}

void OTA_ReceiveAndProgram(void) {
    char hexLine[80]; int idx=0,lineCount=0,errorCount=0;
    UART1_WriteString("OTA_ACK\r\n"); __delay_ms(600); UART1_WriteChar('?');
    char feedBuf[8]; int feedIdx=0; uint32_t timeout=0;
    while(1){
        if(UART1_IsRxReady()){char c=UART1_Read(); if(feedIdx<7)feedBuf[feedIdx++]=c; feedBuf[feedIdx]='\0'; if(strstr(feedBuf,"FEED"))break;}
        __delay_us(100); if(++timeout>30000){UART1_WriteString("OTA_TIMEOUT\r\n");return;}
    }
    UART1_WriteString("BLD\r\n"); __delay_ms(100);
    while(1){
        ClrWdt(); if(!UART1_IsRxReady())continue;
        char c=UART1_Read();
        if(c=='\n'||c=='\r'){
            if(idx==0)continue; hexLine[idx]='\0'; idx=0;
            uint8_t result=OTA_ProcessHexLine(hexLine); lineCount++;
            if(result==2){UART1_WriteChar(0x06);__delay_ms(100);UART1_WriteString("OTA_OK\r\n");__delay_ms(200);asm("RESET");return;}
            else if(result==1){UART1_WriteChar(0x06);errorCount=0;}
            else{errorCount++;UART1_WriteChar(0x15);if(errorCount>5){UART1_WriteString("OTA_FAIL\r\n");return;}}
        } else {if(idx<78)hexLine[idx++]=c;}
    }
}

//=========== PCA9685 ==================================
void PCA9685_Init(void) {
    lights_I2C_Write(0x40,0x00,0x10); lights_I2C_Write(0x40,0xFE,3);
    lights_I2C_Write(0x40,0x00,0x00); lights_I2C_Write(0x40,0x01,0x04);
}

void PCA9685_SetPWM(uint8_t channel, uint16_t on, uint16_t off) {
    uint8_t pwm_base = 0x06 + 4*channel;
    if (off == 0) {
        lights_I2C_Write(0x40,pwm_base+1,0x00);
        lights_I2C_Write(0x40,pwm_base+2,0x00);
        lights_I2C_Write(0x40,pwm_base+3,0x10);
    } else {
        lights_I2C_Write(0x40,pwm_base+1,on>>8);
        lights_I2C_Write(0x40,pwm_base+2,off&0xFF);
        lights_I2C_Write(0x40,pwm_base+3,off>>8);
    }
}

int blue_fade = 0;

void setRGB(uint16_t r, uint16_t g, uint16_t b) { PCA9685_SetPWM(1,0,g); PCA9685_SetPWM(2,0,b); }

void lcd_sendCommand(uint8_t cmd) {
    I2C2_Start(); I2C2_Write((ST7036_ADDR<<1)|0); I2C2_Write(LCD_CMD); I2C2_Write(cmd); I2C2_Stop(); __delay_us(2);
}
void lcd_sendData(uint8_t data) {
    I2C2_Start(); I2C2_Write((ST7036_ADDR<<1)|0); I2C2_Write(LCD_DATA); I2C2_Write(data); I2C2_Stop(); __delay_us(200);
}
void lcd_init(void) {
    __delay_ms(100); lcd_sendCommand(0x38); lcd_sendCommand(0x39); __delay_ms(100);
    lcd_sendCommand(0x14); lcd_sendCommand(0x7F); lcd_sendCommand(0x5F); __delay_ms(500);
    lcd_sendCommand(0x6C); __delay_ms(20); lcd_sendCommand(0x38); __delay_ms(20);
    lcd_sendCommand(0x0D); __delay_ms(20); lcd_sendCommand(0x01); __delay_ms(20);
    lcd_sendCommand(0x06); __delay_ms(10); lcd_sendCommand(0x40); __delay_ms(500);
}
void lcd_print(const char *str) { while (*str) lcd_sendData(*str++); }
void flash_speed(void) { ADC1_SoftwareTriggerDisable(); }

//==========safe mode===================
void handle_safe_mode(void) {
    if (!safe_mode_active) { alarm_out_SetLow(); return; }
    safe_mode_alarm_count++;
    if (safe_mode_alarm_count <= SAFE_MODE_ALARM_ON_TICKS) alarm_out_SetHigh();
    else alarm_out_SetLow();
    if (safe_mode_alarm_count >= (SAFE_MODE_ALARM_ON_TICKS + SAFE_MODE_ALARM_OFF_TICKS))
        safe_mode_alarm_count = 0;
}

typedef enum { DOOR_IDLE=0, DOOR_OPENING, DOOR_OPEN, DOOR_CLOSING, DOOR_CLOSED } DoorState;
DoorState door_status = DOOR_IDLE;

int VDC_in=0, VDC_low=0, VDC_OK=0, VDC_high=0;
float old_actual_supply_volt=27.5, actual_supply_volt=27.5;
int supply_volt=0;
int battery_volt=0, old_battery_volt=0, battery_low=0, battery_ok=0, battery_high=0;

typedef enum { TRAVEL_IDLE=0, TRAVEL_ACTIVE, TRAVEL_DELAY_PENDING } TravelState;
TravelState travel_state = TRAVEL_IDLE;
int travel_delay_count=0, travel_delay_timer=30, travel_delay_active=0;

typedef enum { LEVEL_IDLE=0, LEVEL_LOWERING, LEVEL_COMPLETE, LEVEL_ERROR } LevelState;
LevelState level_state = LEVEL_IDLE;
int level_timer_count=0, level_timeout=20, level_error_flag=0;
volatile int level_interrupt_flag=0;
int levelling_active=0;

int full_speed=0, slow_speed_flag=0, up_ramp_speed=0, dn_ramp_speed=0;
volatile int door_delay_interrupt_flag=0, door_ramp_interrupt_flag=0;
int ls_flag=0, delay_open=1, delay_open_flag=1, delay_close=0, delay_close_flag=0;
int open_delay=0, close_delay=0, close_overrun=0, close_overrun_count=100;
int slow_speed=40, ok_to_run=0, slow_ramp=0, slow_ramp_speed=150;
int door_motor_speed=0, door_opening=0, door_closing=0, ramp_speed=0, max_speed=255;
int ls_delay_close=0, ls_delay_open=0, door_motor_state=0, door_allowed=0;
int door_ramp_direction=0, door_close_request=0, uart_door_request=0;

// Door toggle - pulse set on each new DR / button press edge, cleared once consumed
volatile int door_toggle_request = 0;

int door_ramp_target_speed=0, door_ramp_current_speed=0, door_ramp_step=0;
int door_ramp_delay_count=0, door_ramp_delay_target=2, ramp_step_size=4;
int door_auto_open_delay=0, door_auto_open_request=0, door_auto_open_timer_flag=0;
volatile int floor_arrival_timer=0;
int floor_arrival_flag=0, Power_door=0, door_delay=0;

int ADC_flag=0, ADC_time=300;
volatile int ADC_count=0;
int door_call_request=0, autohome_flag=0, autohome_count=0;

int door_lock_flag=0;
volatile int door_lock_interrupt_flag=0;
int door_lock_timer=500;
volatile int door_lock_on=0, door_lock_count=0;
volatile int floor_arrival_unlock_timer=0, floor_arrival_unlock_flag=0;
int call_unlock_request=0;

// v64 added these
uint8_t  cfg_lock_ramp_up    = 50;
uint8_t  cfg_lock_ramp_down  = 50;
uint8_t  cfg_lock_pwm        = 80;
typedef enum { LOCK_OFF=0, LOCK_RAMPING_UP, LOCK_HOLDING, LOCK_RELEASE_DELAY } LockPWMState;
LockPWMState lock_pwm_state      = LOCK_OFF;
uint16_t     lock_ramp_timer     = 0;
uint16_t     lock_release_timer  = 0;
uint8_t      lock_door_was_closed = 0;



int shouldUnlockForCall(void) {
    if ((PORTAbits.RA9==0)&&(PORTAbits.RA6==1)) { if (PORTAbits.RA0==1) return 1; }
    if ((PORTAbits.RA6==0)&&(PORTAbits.RA9==1)) { if (PORTAbits.RA1==1) return 1; }
    return 0;
}

void start_door_unlock(void) {
    if (door_lock_on==0) { door_lock_on=1; door_lock_count=0; door_lock_flag=1; }
}

int lights_flag=1, lights_ramp_up_speed=0, lights_ramp_dn_speed=0, lights_delay=3000;
volatile int lights_count=0;
int lights_on=1, lights_ramp_down=2000;
int lights_red=0, lights_green=0, lights_blue=0, lights_white=0, lights_warm_white=0, lighting_brightness=0;

int overtravel_flag=0;
volatile int overtravel_timer=0;
int overtravel_reset=0;

int up_limit_no=0, up_limit_nc=1, up_overrun_flag=0, up_overrun_count=0, up_overrun_timer=25;
int up_overrun_timer_reset=0;
volatile int up_overrun_interrupt_flag=0;
int up_limit_arrived=0;

int down_limit_no=0, down_limit_nc=1, down_overrun_flag=0, down_overrun_count=0, down_overrun_timer=4;
int down_overrun_complete=0, down_limit_arrived=0;

int fire_up_inhibit=0, fire_down_inhibit=0, fire_toggle=0;
volatile int fire_count=0;
int fire_time=10;

int estop_ok=0, up_limit_fault=0, down_limit_fault=0;
char message2[20]="Wessex Lift co Ltd";
char Rxcommand[20]="blank";

char door_state[6]="ST 00", lock_state[6]="ST 00", lift_state[6]="ST 00", fire_state[6]="ST 00";
int change_door_state=0, change_lock_state=0, change_lift_state=0, change_fire_state=0;

volatile int run_delay_interrupt_flag=0;
int run_delay=0, run_timer=30, run_count=0, delay_flag=0, lift_travelling=0;

volatile int fire_interrupt_flag=0, lights_interrupt_flag=0;
volatile int overtravel_interrupt_flag=0, down_overrun_interrupt_flag=0;

int safe_mode_on=0, safe_mode_off=1, child_lock=0, chime=0;
int SDA1=0, SCL1=0, IC_error=0, timestamp=0, realtime=0;
char received[34]="RECEIVED";


// ======================================================================
//  TRAVEL MODE HANDLER  -  called from the 100ms TMR2 tick
//
//  Hold to run:
//    up_call_refresh / dn_call_refresh are set by AA UP / AA DN.
//    Each tick this function checks the flag.  If set, watchdog resets.
//    If not set within HTR_TIMEOUT ticks, call cancelled, output low.
//
//  One touch:
//    Same as hold-to-run until OT_LATCH_TIME ticks of continuous refresh.
//    After latch: call stays active regardless of button state.
//    Latch cleared by: floor limit, e-stop, AA XX.
//
//  Overrun / levelling / anticreep call SetHigh/Low directly
//  and are not affected by this function.
// ======================================================================
void handle_travel_mode(void) {

    int at_top    = (PORTAbits.RA6 == 0);
    int at_bottom = (PORTAbits.RA9 == 0);
    int estop     = (PORTDbits.RD14 == 0);

    if (travel_mode == TRAVEL_MODE_HOLD) {

        //--- HOLD TO RUN : UP ---
        if (uart_up_call_request) {
            if (up_call_refresh) {
                up_call_refresh = 0;
                htr_watchdog_up = 0;
                // up_call_SetHigh() already called by AA UP handler
            } else {
                htr_watchdog_up++;
                if (htr_watchdog_up >= HTR_TIMEOUT) {
                    htr_watchdog_up      = 0;
                    uart_up_call_request = 0;
                    up_call_SetLow();
                    UART1_WriteString("HTR: UP stopped\r\n");
                }
            }
        } else {
            up_call_refresh = 0;
            htr_watchdog_up = 0;
        }

        //--- HOLD TO RUN : DN ---
        if (uart_dn_call_request) {
            if (dn_call_refresh) {
                dn_call_refresh = 0;
                htr_watchdog_dn = 0;
            } else {
                htr_watchdog_dn++;
                if (htr_watchdog_dn >= HTR_TIMEOUT) {
                    htr_watchdog_dn      = 0;
                    uart_dn_call_request = 0;
                    down_call_SetLow();
                    UART1_WriteString("HTR: DN stopped\r\n");
                }
            }
        } else {
            dn_call_refresh = 0;
            htr_watchdog_dn = 0;
        }

    } else {

        //--- ONE TOUCH : UP ---
        if (!ot_latched_up) {
            if (uart_up_call_request && up_call_refresh) {
                // Refreshed this tick - keep timing
                up_call_refresh = 0;
                ot_timer_up++;
                if (ot_timer_up >= OT_LATCH_TIME) {
                    ot_latched_up = 1;
                    ot_timer_up   = 0;
                    UART1_WriteString("UP one-touch latched\r\n");
                }
                up_call_SetHigh();   // keep running while timing
            } else if (uart_up_call_request && !up_call_refresh) {
                // Request set but no refresh this tick = button released before latch
                up_call_refresh      = 0;
                ot_timer_up          = 0;
                uart_up_call_request = 0;
                up_call_SetLow();
                UART1_WriteString("OT: UP released before latch\r\n");
            } else {
                up_call_refresh = 0;
                ot_timer_up     = 0;
            }
        } else {
            // Latched - watch for cancel conditions
            up_call_refresh = 0;
            if (at_top || estop) {
                ot_latched_up        = 0;
                ot_timer_up          = 0;
                uart_up_call_request = 0;
                up_call_SetLow();
                UART1_WriteString("UP one-touch cancelled\r\n");
            } else {
                up_call_SetHigh();   // keep running
            }
        }

        //--- ONE TOUCH : DN ---
        if (!ot_latched_dn) {
            if (uart_dn_call_request && dn_call_refresh) {
                dn_call_refresh = 0;
                ot_timer_dn++;
                if (ot_timer_dn >= OT_LATCH_TIME) {
                    ot_latched_dn = 1;
                    ot_timer_dn   = 0;
                    UART1_WriteString("DN one-touch latched\r\n");
                }
                down_call_SetHigh();
            } else if (uart_dn_call_request && !dn_call_refresh) {
                dn_call_refresh      = 0;
                ot_timer_dn          = 0;
                uart_dn_call_request = 0;
                down_call_SetLow();
                UART1_WriteString("OT: DN released before latch\r\n");
            } else {
                dn_call_refresh = 0;
                ot_timer_dn     = 0;
            }
        } else {
            dn_call_refresh = 0;
            if (at_bottom || estop) {
                ot_latched_dn        = 0;
                ot_timer_dn          = 0;
                uart_dn_call_request = 0;
                down_call_SetLow();
                UART1_WriteString("DN one-touch cancelled\r\n");
            } else {
                down_call_SetHigh();
            }
        }
    }

    // Hard e-stop cancel regardless of mode
    if (estop) {
        ot_latched_up=0; ot_latched_dn=0; ot_timer_up=0; ot_timer_dn=0;
        htr_watchdog_up=0; htr_watchdog_dn=0; up_call_refresh=0; dn_call_refresh=0;
    }
}


//========== TIMER INTERRUPT HANDLERS ================//
void handle_tmr2_scheduler_tick(void) {
    TMR2_SoftwareCounterClear();
    static int tick_counter = 0;

    door_lock_interrupt_flag  = 1;
    door_delay_interrupt_flag = 1;
    run_delay_interrupt_flag  = 1;
    door_ramp_interrupt_flag  = 1;
    fire_interrupt_flag       = 1;
    delay_flag                = 1;
    door_auto_open_timer_flag = 1;
    level_interrupt_flag      = 1;
    lights_interrupt_flag     = 1;

    fire_count++;
    lights_count++;

    if (PORTEbits.RE0 == 1) overtravel_timer++;
    else                    overtravel_timer = 0;

    open_delay++;
    tick_counter++;
    if (tick_counter >= 100) tick_counter = 0;

    handle_safe_mode();
    handle_travel_mode();   // watchdog / one-touch latch runs every 100ms

    if (config_dirty && config_save_countdown > 0) {
        config_save_countdown--;
        if (config_save_countdown == 0) { Config_SaveAll(); config_dirty = 0; }
    }
}

void handle_tmr3_scheduler_tick(void) {   // 10ms
    TMR3_SoftwareCounterClear();
    lock_ramp_interrupt_flag = 1;   // v64 ADDed THIS
    sb_ramp_interrupt_flag   = 1;   // v64 ADDed THIS (shootbolt was also never being set)
    
    handle_doorlock_pwm();
    handle_shootbolt();
    handle_up_overrun();
    handle_down_overrun();
}

void handle_tmr4_scheduler_tick(void) { TMR4_SoftwareCounterClear(); }

void handle_lighting(void) {
    switch (light_state) {
        case LIGHT_OFF:
            light_brightness = 0;
            if (lights_triggered) { lights_triggered=0; light_state=LIGHT_RAMPING_UP; }
            break;
        case LIGHT_RAMPING_UP:
            light_brightness += (light_ramp_step*2);
            if (light_brightness >= 255) { light_brightness=255; light_state=LIGHT_ON; light_on_timer=0; }
            if (lights_triggered) { lights_triggered=0; light_on_timer=0; }
            break;
        case LIGHT_ON:
            light_brightness = 255; light_on_timer++;
            if (lights_triggered) { lights_triggered=0; light_on_timer=0; }
            if (light_on_timer >= lighting_timer) { light_state=LIGHT_RAMPING_DOWN; light_on_timer=0; UART1_WriteString("Lights ramping down\r\n"); }
            break;
        case LIGHT_RAMPING_DOWN:
            if (lights_triggered) { lights_triggered=0; light_state=LIGHT_RAMPING_UP; break; }
            light_brightness -= light_ramp_step;
            if (light_brightness <= 0) { light_brightness=0; light_state=LIGHT_OFF; }
            break;
    }
 uint16_t r =(uint16_t)(((uint32_t)uart_red       *(uint32_t)light_brightness)/255);
uint16_t g =(uint16_t)(((uint32_t)uart_green      *(uint32_t)light_brightness)/255);
uint16_t b =(uint16_t)(((uint32_t)uart_blue       *(uint32_t)light_brightness)/255);
uint16_t w =(uint16_t)(((uint32_t)uart_white      *(uint32_t)light_brightness)/255);
uint16_t ww=(uint16_t)(((uint32_t)uart_warm_white *(uint32_t)light_brightness)/255);
PCA9685_SetPWM(2,0,(uint16_t)(r *16));
PCA9685_SetPWM(1,0,(uint16_t)(g *16));
PCA9685_SetPWM(3,0,(uint16_t)(w *16));
PCA9685_SetPWM(4,0,(uint16_t)(ww*16));
}

void handle_fire_test(void) {
    if (fire_count>=fire_time) { fire_count=0; stop_sw_led_SetHigh(); dn_sw_led_SetHigh(); }
}

void Config_MarkDirty(void) { config_dirty=1; config_save_countdown=CONFIG_SAVE_DELAY; }

void UART1_WriteHex(uint16_t value) {
    char buffer[8]; sprintf(buffer,"%02X",value); UART1_WriteString(buffer);
}

//================ LEVELLING ========================
void handle_levelling(void) {
    switch (levelling_state) {
        case LEV_IDLE:
            if (PORTAbits.RA15==1) {
                levelling_active=1; levelling_state=LEV_RUNNING;
                down_call_SetHigh(); up_call_SetLow();
                UART1_WriteString("Levelling started\r\n");
                if (strcmp(lift_state,"ST 23")!=0){strcpy(lift_state,"ST 23");change_lift_state=1;}
            }
            break;
        case LEV_RUNNING:
            down_call_SetHigh();
            if (PORTAbits.RA6==1||PORTAbits.RA15==0) {
                down_call_SetLow(); levelling_active=0; levelling_state=LEV_COMPLETE;
                UART1_WriteString("Levelling complete\r\n");
            }
            break;
        case LEV_COMPLETE:
            if (PORTAbits.RA15==0) levelling_state=LEV_IDLE;
            break;
    }
}

void handle_anticreep(void) {
    int call_in_progress=(uart_up_call_request==1)||(uart_dn_call_request==1)||
                         (PORTAbits.RA0==1)||(PORTAbits.RA1==1)||(levelling_active==1);
    switch (anticreep_state) {
        case AC_IDLE:
            if (PORTAbits.RA14==1&&!call_in_progress) {
                anticreep_active=1; anticreep_state=AC_RUNNING;
                up_call_SetHigh(); down_call_SetLow();
                UART1_WriteString("Anti-creep started\r\n");
                if (strcmp(lift_state,"ST 24")!=0){strcpy(lift_state,"ST 24");change_lift_state=1;}
            }
            break;
        case AC_RUNNING:
            up_call_SetHigh();
            if (call_in_progress){up_call_SetLow();anticreep_active=0;anticreep_state=AC_IDLE;UART1_WriteString("Anti-creep interrupted\r\n");break;}
            if (PORTAbits.RA6==1||PORTAbits.RA14==0){up_call_SetLow();anticreep_active=0;anticreep_state=AC_COMPLETE;UART1_WriteString("Anti-creep complete\r\n");}
            break;
        case AC_COMPLETE:
            if (PORTAbits.RA14==0) anticreep_state=AC_IDLE;
            break;
    }
}

void handle_up_overrun(void) {
    static int last_up_limit=1;
    int current_up_limit=PORTAbits.RA6;
    if (last_up_limit==1&&current_up_limit==0&&!up_overrun_flag){up_overrun_flag=1;up_overrun_count=0;up_limit_arrived=0;}
    if (up_overrun_flag==1){
        up_call_SetHigh(); down_call_SetLow(); up_safe_fault_LED_SetHigh();
        up_overrun_count++;
        if (up_overrun_count>=(int)uart_top_overrrun){up_call_SetLow();up_limit_arrived=1;up_overrun_count=0;up_overrun_flag=0;up_safe_fault_LED_SetLow();}
    } else { if (!anticreep_active) up_call_SetLow(); }
    if (current_up_limit==1) up_limit_arrived=0;
    last_up_limit=current_up_limit;
}

void handle_down_overrun(void) {
    static int last_down_limit=1;
    int current_down_limit=PORTAbits.RA9;
    if (last_down_limit==1&&current_down_limit==0&&!down_overrun_flag){down_overrun_flag=1;down_overrun_count=0;down_limit_arrived=0;dn_safe_fault_LED_SetHigh();}
    if (down_overrun_flag==1){
        PCA9685_SetPWM(0,0,4096); up_call_SetLow();
        down_overrun_count++;
        if (down_overrun_count>=(int)uart_bottom_overrrun){PCA9685_SetPWM(0,0,4096);down_limit_arrived=1;down_overrun_count=0;down_overrun_flag=0;dn_safe_fault_LED_SetLow();}
    } else { if (!levelling_active) down_call_SetHigh(); }
    if (current_down_limit==1) down_limit_arrived=0;
    last_down_limit=current_down_limit;
}

void handle_doorlock_pwm(void) {
    if (!lock_ramp_interrupt_flag) return;
    lock_ramp_interrupt_flag = 0;

    // Convert % config values to OC2 counts (OC2 range 0-255 based on your existing code)
    uint16_t hold_duty = (uint16_t)(((uint32_t)cfg_lock_pwm * 255) / 100);

    uint8_t door_is_closed = (door_status == DOOR_CLOSED || door_status == DOOR_IDLE);

    // Trigger state machine when lock is requested
    if (door_lock_on && lock_pwm_state == LOCK_OFF) {
        lock_pwm_state   = LOCK_RAMPING_UP;
        lock_ramp_timer  = 0;
        lock_ramp_current_speed = 0;
        OC2_PrimaryValueSet(0);
    }

    // Release if door_lock_on cleared externally
    if (!door_lock_on && lock_pwm_state != LOCK_OFF) {
        lock_pwm_state   = LOCK_OFF;
        lock_ramp_timer  = 0;
        lock_release_timer = 0;
        lock_ramp_current_speed = 0;
        OC2_PrimaryValueSet(0);
    }

    switch (lock_pwm_state) {

        case LOCK_OFF:
            OC2_PrimaryValueSet(0);
            lock_ramp_current_speed = 0;
            lock_ramp_timer         = 0;
            lock_release_timer      = 0;
            lock_door_was_closed    = door_is_closed;
            break;

        case LOCK_RAMPING_UP:
            // Ramp 0 to 100% over cfg_lock_ramp_up ticks (each tick = 10ms)
            if (cfg_lock_ramp_up == 0) {
                lock_ramp_current_speed = 255;
                OC2_PrimaryValueSet(255);
                lock_pwm_state = LOCK_HOLDING;
                lock_ramp_timer = 0;
            } else {
                lock_ramp_timer++;
                uint32_t duty = ((uint32_t)lock_ramp_timer * 255) / cfg_lock_ramp_up;
                if (duty > 255) duty = 255;
                lock_ramp_current_speed = (int)duty;
                OC2_PrimaryValueSet((uint16_t)duty);
                if (lock_ramp_timer >= cfg_lock_ramp_up) {
                    lock_ramp_current_speed = 255;
                    OC2_PrimaryValueSet(255);
                    lock_pwm_state = LOCK_HOLDING;
                    lock_ramp_timer = 0;
                }
            }
            lock_door_was_closed = door_is_closed;
            break;

        case LOCK_HOLDING:
            // Drop to hold duty
            OC2_PrimaryValueSet(hold_duty);
            lock_ramp_current_speed = (int)hold_duty;

            // Detect door closed -> open transition (door opened)
            if (lock_door_was_closed && !door_is_closed) {
                lock_release_timer = 0;
                lock_pwm_state     = LOCK_RELEASE_DELAY;
            }
            lock_door_was_closed = door_is_closed;
            break;

        case LOCK_RELEASE_DELAY:
            // Keep holding during delay after door opens
            OC2_PrimaryValueSet(hold_duty);
            lock_release_timer++;
            if (lock_release_timer >= cfg_lock_ramp_down) {
                // Delay expired - release lock
                OC2_PrimaryValueSet(0);
                lock_ramp_current_speed = 0;
                lock_pwm_state          = LOCK_OFF;
                lock_release_timer      = 0;
                door_lock_on            = 0;
                door_lock_flag          = 0;
                door_lock_count         = 0;
            }
            lock_door_was_closed = door_is_closed;
            break;
    }
}
void handle_shootbolt(void) {
    if (!sb_ramp_interrupt_flag) return;
    sb_ramp_interrupt_flag=0;
    int fp_zone_active=(PORTCbits.RC3==1);
    int down_call_active=(uart_dn_call_request==1)||(PORTAbits.RA0==1);
    switch (shootbolt_state){
        case SB_OFF:
            sb_ramp_current_speed=0; OC3_PrimaryValueSet(0);
            if (fp_zone_active&&down_call_active){shootbolt_state=SB_RAMPING_UP;sb_ramp_delay_count=0;UART1_WriteString("Shootbolt ramping up\r\n");}
            break;
        case SB_RAMPING_UP:
            if (!fp_zone_active){shootbolt_state=SB_RAMPING_DOWN;UART1_WriteString("Shootbolt: FP zone off\r\n");break;}
            sb_ramp_delay_count++;
            if (sb_ramp_delay_count>=sb_ramp_delay_target){
                sb_ramp_current_speed+=sb_ramp_step_size;
                if (sb_ramp_current_speed>=sb_ramp_target_speed){sb_ramp_current_speed=sb_ramp_target_speed;shootbolt_state=SB_ON;UART1_WriteString("Shootbolt on\r\n");}
                sb_ramp_delay_count=0;
            }
            OC3_PrimaryValueSet((uint16_t)sb_ramp_current_speed);
            break;
        case SB_ON:
            OC3_PrimaryValueSet((uint16_t)sb_ramp_target_speed);
            if (!fp_zone_active){shootbolt_state=SB_RAMPING_DOWN;sb_ramp_delay_count=0;UART1_WriteString("Shootbolt ramping down\r\n");}
            break;
        case SB_RAMPING_DOWN:
            sb_ramp_delay_count++;
            if (sb_ramp_delay_count>=sb_ramp_delay_target){
                sb_ramp_current_speed-=sb_ramp_step_size;
                if (sb_ramp_current_speed<=0){sb_ramp_current_speed=0;shootbolt_state=SB_OFF;UART1_WriteString("Shootbolt off\r\n");}
                sb_ramp_delay_count=0;
            }
            OC3_PrimaryValueSet((uint16_t)sb_ramp_current_speed);
            break;
    }
}


// ================  UART PROCESS COMMANDS  ====================

void UART1_ProcessCommand(const char *cmd) {

    if (strstr(cmd, "AA UP") != NULL) {
        uart_up_call_request = 1;
        uart_dn_call_request = 0;
        up_call_refresh      = 1;   // refresh pulse for travel mode handler
        dn_call_refresh      = 0;
        up_call_SetHigh();
        down_call_SetLow();
        UART1_WriteString("UP call requested\r\n");

    } else if (strstr(cmd, "AA DN") != NULL) {
        uart_dn_call_request = 1;
        uart_up_call_request = 0;
        dn_call_refresh      = 1;   // refresh pulse for travel mode handler
        up_call_refresh      = 0;
        down_call_SetHigh();
        up_call_SetLow();
        UART1_WriteString("DN call requested\r\n");

    } else if (strstr(cmd, "AA XX") != NULL) {
        // Cancel all travel - also clear mode state and latches
        uart_up_call_request = 0;
        uart_dn_call_request = 0;
        up_call_SetLow();
        down_call_SetLow();
        up_call_refresh  = 0;
        dn_call_refresh  = 0;
        ot_latched_up    = 0;  ot_latched_dn    = 0;
        ot_timer_up      = 0;  ot_timer_dn      = 0;
        htr_watchdog_up  = 0;  htr_watchdog_dn  = 0;
        UART1_WriteString("All calls cancelled\r\n");

    } else if (strcmp(cmd, "DR") == 0) {
        uart_up_call_request = 0;
        uart_dn_call_request = 0;
        up_call_SetLow();
        down_call_SetLow();
        uart_door_request   = 1;
        door_toggle_request = 1;
        UART1_WriteString("Door requested\r\n");

    } else if (strcmp(cmd, "OTA") == 0) {
       boot_magic_flag = 0xBEEF;
        UART1_WriteString("OTA_ACK\r\n"); __delay_ms(200); asm("RESET");

    } else if (strcmp(cmd, "SF") == 0) {
        safe_mode_active=1; safe_mode_alarm_count=0; UART1_WriteString("Safe mode activated\r\n");

    } else if (strcmp(cmd, "SC") == 0) {
        safe_mode_active=0; safe_mode_alarm_count=0; alarm_out_SetLow(); UART1_WriteString("Safe mode cancelled\r\n");

    } else if (strcmp(cmd, "RESET") == 0) {
        // System_Reset();

    // ---- Travel mode select ----
    } else if (strncmp(cmd, "TT ", 3) == 0) {
        int ttvalue;
        if (sscanf(cmd, "TT %d", &ttvalue) == 1) {
            travel_mode = (ttvalue == 1) ? TRAVEL_MODE_ONETOUCH : TRAVEL_MODE_HOLD;
            // Clear all state on mode change
            ot_latched_up=0; ot_latched_dn=0; ot_timer_up=0; ot_timer_dn=0;
            htr_watchdog_up=0; htr_watchdog_dn=0;
            uart_up_call_request=0; uart_dn_call_request=0;
            up_call_SetLow(); down_call_SetLow();
            UART1_WriteString(travel_mode==TRAVEL_MODE_ONETOUCH ?
                "Mode: one-touch\r\n" : "Mode: hold-to-run\r\n");
            Config_MarkDirty();
        }

    } else if (strncmp(cmd, "OT ", 3) == 0) {
        int otvalue;
        if (sscanf(cmd,"OT %d",&otvalue)==1){
            uart_overttravel_timer=(uint32_t)otvalue*10; overtravel_time=otvalue*10;
            UART1_WriteString("OT "); UART1_WriteUInt(otvalue); UART1_WriteString("\r\n"); Config_MarkDirty();
        }

    } else if (strncmp(cmd, "TO ", 3) == 0) {
        int tovalue;
        if (sscanf(cmd,"TO %d",&tovalue)==1){
            uart_top_overrrun=(uint32_t)tovalue;
            UART1_WriteString("TO "); UART1_WriteUInt(uart_top_overrrun); UART1_WriteString("\r\n"); Config_MarkDirty();
        }

    } else if (strncmp(cmd, "BT ", 3) == 0) {
        int btvalue;
        if (sscanf(cmd,"BT %d",&btvalue)==1){
            uart_bottom_overrrun=(uint32_t)btvalue;
            UART1_WriteString("BT "); UART1_WriteUInt(uart_bottom_overrrun); UART1_WriteString("\r\n"); Config_MarkDirty();
        }

    } else if (strcmp(cmd, "DM") == 0) {
        UART1_WriteUInt(autohome_time); UART1_WriteString("\r\n");

    } else if (strcmp(cmd, "FW") == 0) {
        UART1_WriteString(firmware); UART1_WriteString("\r\n");

    } else if (strcmp(cmd, "serial") == 0) {
        UART1_WriteString(firmware); UART1_WriteString("\r\n");

    } else if ((strcmp(cmd,"RB00")==0)&&(rb==0)) { rb=1; UART1_WriteString("rainbow lights off ");
    } else if ((strcmp(cmd,"RB01")==0)&&(rb==1)) { rb=0; UART1_WriteString("rainbow lights on");

    } else if (strncmp(cmd, "LR ", 3) == 0) {
        UART1_WriteString("Raw red input: "); UART1_WriteString(cmd); UART1_WriteString("\r\n");
        int rvalue;
        if (sscanf(cmd,"LR %2x",&rvalue)==1){uart_red=rvalue;UART1_WriteString("redPWM updated\r\n");UART1_WriteUInt(uart_red);Config_MarkDirty();}
        else UART1_WriteString("Invalid red hex value\r\n");

    } else if (strncmp(cmd, "LG ", 3) == 0) {
        int gvalue;
        if (sscanf(cmd,"LG %2x",&gvalue)==1){uart_green=gvalue;UART1_WriteString("Green PWM updated\r\n");UART1_WriteUInt(uart_green);Config_MarkDirty();}
        else UART1_WriteString("Invalid green hex value\r\n");

    } else if (strncmp(cmd, "LB ", 3) == 0) {
        int bvalue;
        if (sscanf(cmd,"LB %2x",&bvalue)==1){uart_blue=bvalue;UART1_WriteString("blue PWM updated\r\n");UART1_WriteUInt(uart_blue);Config_MarkDirty();}
        else UART1_WriteString("Invalid blue hex value\r\n");

    } else if (strncmp(cmd, "LW ", 3) == 0) {
        int wvalue;
        if (sscanf(cmd,"LW %2x",&wvalue)==1){uart_white=wvalue;UART1_WriteString("white PWM updated\r\n");UART1_WriteUInt(uart_white);Config_MarkDirty();}
        else UART1_WriteString("Invalid white hex value\r\n");
        
           } else if (strncmp(cmd, "LC ", 3) == 0) {
        int cvalue;
        if (sscanf(cmd,"LC %2x",&cvalue)==1){uart_warm_white=cvalue;UART1_WriteString("warm white PWM updated\r\n");UART1_WriteUInt(uart_warm_white);Config_MarkDirty();}
        else UART1_WriteString("Invalid warm white hex value\r\n"); 
        
        

    } else if (strncmp(cmd, "LT ", 3) == 0) {
        int ltvalue;
        if (sscanf(cmd,"LT %d",&ltvalue)==1){lighting_timer=(uint32_t)ltvalue*10;UART1_WriteString("LT ");UART1_WriteUInt(ltvalue);UART1_WriteString("\r\n");Config_MarkDirty();}

    } else if (strncmp(cmd, "DM ", 3) == 0) {
        int dmvalue;
        if (sscanf(cmd,"DM %d",&dmvalue)==1){uart_door_speed=(uint32_t)dmvalue;door_ramp_target_speed=(int)uart_door_speed;UART1_WriteString("DM ");UART1_WriteUInt(uart_door_speed);UART1_WriteString("\r\n");Config_MarkDirty();}

    } else if (strncmp(cmd, "DT ", 3) == 0) {
        int dtvalue;
        if (sscanf(cmd,"DT %d",&dtvalue)==1){uart_door_autoclose_timer=(uint32_t)dtvalue*10;UART1_WriteString("DT ");UART1_WriteUInt(dtvalue);UART1_WriteString("\r\n");Config_MarkDirty();}

    } else if (strncmp(cmd, "DC ", 3) == 0) {
        int dcvalue;
        if (sscanf(cmd,"DC %d",&dcvalue)==1){uart_door_autoclose=dcvalue;UART1_WriteString("DC ");UART1_WriteUInt(uart_door_autoclose);UART1_WriteString("\r\n");Config_MarkDirty();}

    } else if (strcmp(cmd, "LOCKSTATUS") == 0) {
        if (door_lock_on){UART1_WriteString("LOCK: UNLOCKED (");UART1_WriteUInt(30-door_lock_count);UART1_WriteString(" ticks remaining)\r\n");}
        else UART1_WriteString("LOCK: LOCKED\r\n");
        
  } else if (strncmp(cmd, "KR ", 3) == 0) {
    int val;
    if (sscanf(cmd,"KR %d",&val)==1){
        if(val<0)val=0; if(val>100)val=100;
        cfg_lock_ramp_up=(uint8_t)val;
        UART1_WriteString("KR "); UART1_WriteUInt(cfg_lock_ramp_up);
        UART1_WriteString("\r\n"); Config_MarkDirty();
    }

} else if (strncmp(cmd, "KD ", 3) == 0) {
    int val;
    if (sscanf(cmd,"KD %d",&val)==1){
        if(val<0)val=0; if(val>100)val=100;
        cfg_lock_ramp_down=(uint8_t)val;
        UART1_WriteString("KD "); UART1_WriteUInt(cfg_lock_ramp_down);
        UART1_WriteString("\r\n"); Config_MarkDirty();
    }

} else if (strncmp(cmd, "KP ", 3) == 0) {
    int val;
    if (sscanf(cmd,"KP %d",&val)==1){
        if(val<0)val=0; if(val>100)val=100;
        cfg_lock_pwm=(uint8_t)val;
        UART1_WriteString("KP "); UART1_WriteUInt(cfg_lock_pwm);
        UART1_WriteString("\r\n"); Config_MarkDirty();
    }      
        
        
        
        
        
        
        

    } else if (strcmp(cmd, "SAVE") == 0) {
        Config_SaveAll(); config_dirty=0; config_save_countdown=0;

    } else if (strcmp(cmd, "DEFAULTS") == 0) {
        Config_SetDefaults(); Config_SaveAll(); config_dirty=0; config_save_countdown=0;
        UART1_WriteString("Factory defaults restored\r\n");

    } else if (strcmp(cmd, "CONFIG") == 0) {
        UART1_WriteString("--- CONFIG ---\r\n");
        UART1_WriteString("LR "); UART1_WriteHex((uint16_t)uart_red);            UART1_WriteString("\r\n");
        UART1_WriteString("LG "); UART1_WriteHex((uint16_t)uart_green);          UART1_WriteString("\r\n");
        UART1_WriteString("LB "); UART1_WriteHex((uint16_t)uart_blue);           UART1_WriteString("\r\n");
        UART1_WriteString("LW "); UART1_WriteHex((uint16_t)uart_white);          UART1_WriteString("\r\n");
        UART1_WriteString("LC "); UART1_WriteHex((uint16_t)uart_warm_white);          UART1_WriteString("\r\n");
        UART1_WriteString("DM "); UART1_WriteUInt(uart_door_speed);              UART1_WriteString("\r\n");
        UART1_WriteString("DT "); UART1_WriteUInt(uart_door_autoclose_timer);    UART1_WriteString("\r\n");
        UART1_WriteString("DC "); UART1_WriteUInt(uart_door_autoclose);          UART1_WriteString("\r\n");
        UART1_WriteString("HT "); UART1_WriteUInt(uart_autohome_timer);          UART1_WriteString("\r\n");
        UART1_WriteString("HO "); UART1_WriteUInt(uart_autohome);                UART1_WriteString("\r\n");
        UART1_WriteString("HD "); UART1_WriteUInt(uart_autohome_direction);      UART1_WriteString("\r\n");
        UART1_WriteString("OT "); UART1_WriteUInt(uart_overttravel_timer);       UART1_WriteString("\r\n");
        UART1_WriteString("TO "); UART1_WriteUInt(uart_top_overrrun);            UART1_WriteString("\r\n");
        UART1_WriteString("BT "); UART1_WriteUInt(uart_bottom_overrrun);         UART1_WriteString("\r\n");
        UART1_WriteString("LT "); UART1_WriteUInt(lighting_timer);               UART1_WriteString("\r\n");
        UART1_WriteString("TT "); UART1_WriteUInt(travel_mode);                  UART1_WriteString("\r\n");
        UART1_WriteString("SN "); UART1_WriteString(serial);                       UART1_WriteString("\r\n");
        UART1_WriteString("ID "); UART1_WriteString(install_date);                 UART1_WriteString("\r\n");
        UART1_WriteString("TY "); UART1_WriteString(lift_type);                    UART1_WriteString("\r\n");
        UART1_WriteString("PD "); UART1_WriteUInt(Power_door);                   UART1_WriteString("\r\n");
        UART1_WriteString("DD "); UART1_WriteUInt(door_delay);                   UART1_WriteString("\r\n");
        UART1_WriteString("LD "); UART1_WriteUInt(lighting_brightness);          UART1_WriteString("\r\n");
        UART1_WriteString("SF "); UART1_WriteUInt(safe_mode_on);                 UART1_WriteString("\r\n");
        UART1_WriteString("SO "); UART1_WriteUInt(safe_mode_off);                UART1_WriteString("\r\n");
        UART1_WriteString("CL "); UART1_WriteUInt(child_lock);                   UART1_WriteString("\r\n");
        UART1_WriteString("CH "); UART1_WriteUInt(chime);                        UART1_WriteString("\r\n");
        UART1_WriteString("KR "); UART1_WriteUInt(cfg_lock_ramp_up);   UART1_WriteString("\r\n");
        UART1_WriteString("KD "); UART1_WriteUInt(cfg_lock_ramp_down);  UART1_WriteString("\r\n");
        UART1_WriteString("KP "); UART1_WriteUInt(cfg_lock_pwm);        UART1_WriteString("\r\n");
        
        
        
        
        
        UART1_WriteString("Dirty: "); UART1_WriteUInt(config_dirty);             UART1_WriteString("\r\n");
        UART1_WriteString("--- END ---\r\n");
    }
}

int isDoorCallActive(void) {
    return (PORTAbits.RA5 == 1) || (door_auto_open_request == 1);
}


//====================================================================//
int main(void) {

    SYSTEM_Initialize();
    __delay_ms(100);
    ADC1_Initialize();
    UART1_Initialize();
    __delay_ms(100);

    TMR1_Initialize(); TMR2_Initialize(); TMR3_Initialize(); TMR4_Initialize();
    TMR1_Start();      TMR2_Start();      TMR3_Start();      TMR4_Start();

    ADC1_SoftwareTriggerDisable();
    
    adc_an9_init();
    
    TMR3_SetInterruptHandler(flash_speed);

    door_disable_SetHigh();
    door_direction_SetLow();

    OC1_Initialize(); OC1_Start(); OC1_PrimaryValueSet(0); OC1_SecondaryValueSet(0x100);
    door_disable_SetHigh();

    OC2_Initialize(); OC2_Start(); OC2_PrimaryValueSet(0); OC2_SecondaryValueSet(0x100);
    lock_ramp_target_speed = 0xFF;

    OC3_Initialize(); OC3_Start(); OC3_PrimaryValueSet(0); OC3_SecondaryValueSet(0x100);

    __delay_ms(500);
    PCA9685_Init();
    __delay_ms(200);

    UART1_WriteString("Ch0\r\n"); __delay_ms(2000);
    UART1_WriteString("Ch1\r\n"); PCA9685_SetPWM(1,0,4095); __delay_ms(2000); PCA9685_SetPWM(1,0,0);
    UART1_WriteString("Ch2\r\n"); PCA9685_SetPWM(2,0,4095); __delay_ms(2000); PCA9685_SetPWM(2,0,0);
    UART1_WriteString("Ch3\r\n"); PCA9685_SetPWM(3,0,4095); __delay_ms(2000); PCA9685_SetPWM(3,0,0);
    UART1_WriteString("Ch4\r\n"); PCA9685_SetPWM(4,0,4095); __delay_ms(2000); PCA9685_SetPWM(4,0,0);

    UART1_WriteString("Boot light test done\r\n");

    down_call_SetLow();           // A0
    up_call_SetLow();             // A1
    stop_sw_led_SetHigh();        // A4
    dn_sw_led_SetHigh();          // C2
    dn_limit_override_SetLow();   // C4
    up_sw_led_SetHigh();          // D0
    SPI1_PD_SetLow();             // D6
    door_disable_SetHigh();       // E5
    down_fire_inhib_LED_SetLow(); // E7
    Up_fire_inhib_LED_SetLow();   // E6
    UP_control_SetHigh();         // F0
    down_control_SetHigh();       // F1
    dn_safe_fault_LED_SetLow();   // F2
    up_safe_fault_LED_SetLow();   // F3
    door_lock_SetLow();           // F4
    top_lock_SetLow();            // F5
    door_direction_SetLow();      // F6
    alarm_out_SetLow();           // F7
    overtravel_led_SetLow();      // F8
    keyswitch_block_out_SetLow(); // F12
    delay_24v_3s_SetHigh();       // G7
    door_open_led_SetLow();       // G14
    door_close_led_SetLow();      // G15

    __delay_ms(500);

    __builtin_write_OSCCONL(OSCCON & ~(1 << 6));
    RPOR1bits.RP2R    = 3;
    RPINR18bits.U1RXR = 3;
    __builtin_write_OSCCONL(OSCCON | (1 << 6));

    UART1_WriteString("UART Ready\r\n");
    Config_LoadAll();
    UART1_WriteString("System ready\r\n");
    UART1_WriteString(travel_mode == TRAVEL_MODE_ONETOUCH ?
        "Travel: one-touch\r\n" : "Travel: hold-to-run\r\n");

    OC1_PrimaryValueSet(0);

    int last_floor_state = -1;
    int current_floor_state;

    door_ramp_target_speed = uart_door_speed;


    //========== MAIN PROGRAM LOOP ================//
    while (1) {

        ClrWdt();
        
        adc_an9_task();

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

        //========== FLOOR ARRIVAL DETECTION ==========//
        if ((PORTAbits.RA6==0)&&(PORTAbits.RA9==1)) {
            current_floor_state=0;
            if (last_floor_state!=0){start_door_unlock();door_auto_open_request=1;}
            if (strcmp(lift_state,"ST 21")!=0){strcpy(lift_state,"ST 21");change_lift_state=1;}

        } else if ((PORTAbits.RA9==0)&&(PORTAbits.RA6==1)) {
            current_floor_state=1;
            if (last_floor_state!=1){start_door_unlock();door_auto_open_request=1;}
            if (strcmp(lift_state,"ST 1F")!=0){strcpy(lift_state,"ST 1F");change_lift_state=1;}

        } else if ((PORTAbits.RA9==1)&&(PORTAbits.RA6==1)) {
            current_floor_state=2;
            door_auto_open_request=0;
            if (strcmp(lift_state,"ST 27")!=0){strcpy(lift_state,"ST 27");change_lift_state=1;}

        } else {
            current_floor_state=3;
            if (strcmp(lift_state,"ST 16")!=0){strcpy(lift_state,"ST 16");change_lift_state=1;}
        }

        handle_levelling();
        handle_anticreep();

        //========== RA5 DOOR CALL BUTTON ==========//
        {
            static int last_ra5=0;
            int current_ra5=PORTAbits.RA5;
            if (current_ra5==1&&last_ra5==0){uart_door_request=1;door_toggle_request=1;UART1_WriteString("ST 07\r\n");}
            last_ra5=current_ra5;
        }

        //========== DOOR CONTROL STATE MACHINE ==========//
        if ((PORTAbits.RA6==0)||(PORTAbits.RA9==0)) {

            switch (door_status) {

                case DOOR_IDLE:
                    if ((door_auto_open_request)||(PORTBbits.RB13==0&&uart_door_request)) {
                        door_toggle_request=0;
                        start_door_unlock();
                        door_disable_SetLow(); door_direction_SetLow();
                        door_status=DOOR_OPENING;
                        door_ramp_current_speed=0; door_ramp_step=0; door_ramp_delay_count=0;
                        OC1_PrimaryValueSet(0); door_auto_open_request=0;
                        door_open_led_SetHigh(); door_close_led_SetLow();
                        if (strcmp(door_state,"ST 08")!=0){strcpy(door_state,"ST 08");change_door_state=1;}
                    }
                    break;

                case DOOR_OPENING:
                    // New press while opening - reverse to closing
                    if (door_toggle_request) {
                        door_toggle_request=0; uart_door_request=0;
                        for(int i=door_ramp_current_speed;i>=0;i-=100){if(i<0)i=0;OC1_PrimaryValueSet(i);}
                        OC1_PrimaryValueSet(0); door_direction_SetHigh();
                        door_ramp_current_speed=0; door_ramp_delay_count=0;
                        door_status=DOOR_CLOSING; door_open_led_SetLow(); door_close_led_SetHigh();
                        UART1_WriteString("Door reversed: now closing\r\n");
                        if (strcmp(door_state,"ST 0A")!=0){strcpy(door_state,"ST 0A");change_door_state=1;}
                        break;
                    }
                    if (door_ramp_interrupt_flag) {
                        door_ramp_interrupt_flag=0; door_ramp_delay_count++;
                        if (door_ramp_delay_count>=door_ramp_delay_target){
                            if (door_ramp_current_speed<door_ramp_target_speed){
                                door_ramp_current_speed+=ramp_step_size;
                                if (door_ramp_current_speed>door_ramp_target_speed) door_ramp_current_speed=door_ramp_target_speed;
                            }
                            OC1_PrimaryValueSet(door_ramp_current_speed); door_ramp_delay_count=0;
                        }
                    }
                    if (PORTBbits.RB12==0) {
                        for(int i=door_ramp_current_speed;i>=0;i-=100){if(i<0)i=0;OC1_PrimaryValueSet(i);}
                        door_direction_SetHigh(); OC1_PrimaryValueSet(0);
                        door_status=DOOR_OPEN; door_auto_open_request=0; uart_door_request=0;
                        door_open_led_SetLow();
                        if (strcmp(door_state,"ST 09")!=0){strcpy(door_state,"ST 09");change_door_state=1;}
                    }
                    break;

                case DOOR_OPEN:
                    if ((uart_door_autoclose&&ADC_flag)||door_close_request||uart_door_request) {
                        door_toggle_request=0;
                        door_disable_SetLow(); door_direction_SetHigh();
                        door_status=DOOR_CLOSING;
                        door_ramp_current_speed=0; door_ramp_step=0; door_ramp_delay_count=0;
                        OC1_PrimaryValueSet(0); door_open_led_SetLow(); door_close_led_SetHigh();
                        ADC_flag=0;
                        if (strcmp(door_state,"ST 0A")!=0){strcpy(door_state,"ST 0A");change_door_state=1;}
                    }
                    break;

                case DOOR_CLOSING:
                    // New press while closing - reverse to opening
                    if (door_toggle_request) {
                        door_toggle_request=0; uart_door_request=0;
                        for(int i=door_ramp_current_speed;i>=0;i-=100){if(i<0)i=0;OC1_PrimaryValueSet(i);}
                        OC1_PrimaryValueSet(0); door_direction_SetLow();
                        door_ramp_current_speed=0; door_ramp_delay_count=0;
                        door_status=DOOR_OPENING; door_open_led_SetHigh(); door_close_led_SetLow();
                        UART1_WriteString("Door reversed: now opening\r\n");
                        if (strcmp(door_state,"ST 08")!=0){strcpy(door_state,"ST 08");change_door_state=1;}
                        break;
                    }
                    if (door_ramp_interrupt_flag) {
                        door_ramp_interrupt_flag=0; door_ramp_delay_count++;
                        if (door_ramp_delay_count>=door_ramp_delay_target){
                            if (door_ramp_current_speed<door_ramp_target_speed){
                                door_ramp_current_speed+=ramp_step_size;
                                if (door_ramp_current_speed>door_ramp_target_speed) door_ramp_current_speed=door_ramp_target_speed;
                            }
                            OC1_PrimaryValueSet(door_ramp_current_speed); door_ramp_delay_count=0;
                        }
                    }
                    if (PORTBbits.RB13==0) {
                        for(int i=door_ramp_current_speed;i>=0;i-=100){if(i<0)i=0;OC1_PrimaryValueSet(i);__delay_ms(10);}
                        OC1_PrimaryValueSet(0); uart_door_request=0; door_status=DOOR_CLOSED;
                        if (strcmp(door_state,"ST 0B")!=0){strcpy(door_state,"ST 0B");change_door_state=1;}
                    }
                    break;

                case DOOR_CLOSED:
                    door_status=DOOR_IDLE; door_auto_open_request=0;
                    break;
            }

        } else {
            door_disable_SetHigh(); OC1_PrimaryValueSet(0);
            door_allowed=0; ramp_speed=0; full_speed=0;
            door_status=DOOR_IDLE; door_auto_open_request=0;
            door_toggle_request=0;
            floor_arrival_timer=0; floor_arrival_flag=0;
        }

        last_floor_state=current_floor_state;

        //========== DOOR LOCK ==========//
        if ((DOOR_SAFE_TO_OPEN_AT_TOP==1)||(DOOR_SAFE_TO_OPEN_AT_BOTTOM==1)) {
            if (door_status==DOOR_CLOSED||door_status==DOOR_IDLE){
                if (shouldUnlockForCall()){if(call_unlock_request==0){call_unlock_request=1;start_door_unlock();}}
                else call_unlock_request=0;
            }
        } else { floor_arrival_unlock_flag=0; floor_arrival_unlock_timer=0; call_unlock_request=0; }

 // door lock now handled entirely by handle_doorlock_pwm() via TMR3
// door_lock_SetHigh/Low no longer needed - OC2 drives the lock directly

 

        //========== LIGHTING TRIGGER DETECTION ==========//
        {
            static int last_t_travel=0,last_t_door=0,last_t_lock=0;
            static int last_t_upcall=0,last_t_dncall=0,last_t_doorcall=0;
            int t_travel  =(PORTAbits.RA6==1)&&(PORTAbits.RA9==1);
            int t_door    =(door_status==DOOR_OPENING)||(door_status==DOOR_OPEN)||(door_status==DOOR_CLOSING);
            int t_lock    =(door_lock_on==1);
            int t_upcall  =(uart_up_call_request==1);
            int t_dncall  =(uart_dn_call_request==1);
            int t_doorcall=(uart_door_request==1);
            if((t_travel&&!last_t_travel)||(t_door&&!last_t_door)||(t_lock&&!last_t_lock)||
               (t_doorcall&&!last_t_doorcall)||(t_upcall&&!last_t_upcall)||(t_dncall&&!last_t_dncall)){
                lights_triggered=1;
                UART1_WriteString("LTrig:");
                if(t_travel)           UART1_WriteString("TRAV ");
                if(t_door||t_doorcall) UART1_WriteString("DOOR ");
                if(t_lock)             UART1_WriteString("LOCK ");
                if(t_upcall)           UART1_WriteString("UP ");
                if(t_dncall)           UART1_WriteString("DN ");
                UART1_WriteString("\r\n");
            }
            last_t_travel=t_travel; last_t_door=t_door; last_t_lock=t_lock;
            last_t_upcall=t_upcall; last_t_dncall=t_dncall; last_t_doorcall=t_doorcall;
        }

        if (lights_interrupt_flag){lights_interrupt_flag=0;handle_lighting();}

        //========== AUTO-CLOSE TIMER ==========//
        if (door_status==DOOR_OPEN&&uart_door_autoclose){
            if(door_delay_interrupt_flag){door_delay_interrupt_flag=0;ADC_count++;if(ADC_count>=uart_door_autoclose_timer){ADC_flag=1;ADC_count=0;}}
        } else {ADC_count=0;ADC_flag=0;}

        //========== ANALOGUE INPUTS ==========//
        supply_volt=ADC1_ConversionResultGet(power_in_monitor);
        float actual_supply_volt=(supply_volt*190.0)/4095.0;
        if((actual_supply_volt<old_actual_supply_volt-0.2)||(actual_supply_volt>old_actual_supply_volt+0.2))
            old_actual_supply_volt=actual_supply_volt;
        VDC_in=ADC1_ConversionResultGet(power_in_monitor);

        //========== EMERGENCY STOP ==========//
        estop_ok=(PORTDbits.RD14==1)?1:0;

        //========== 3-SECOND TRAVEL DELAY ==========//
        int currently_travelling=(PORTEbits.RE0==1)||(PORTGbits.RG12==1)||
                                 (down_overrun_flag==1)||(up_overrun_flag==1);
        switch (travel_state){
            case TRAVEL_IDLE:
                travel_delay_active=0; delay_24v_3s_SetHigh();
                if(currently_travelling) travel_state=TRAVEL_ACTIVE;
                break;
            case TRAVEL_ACTIVE:
                travel_delay_active=0; delay_24v_3s_SetHigh();
                if(!currently_travelling){travel_state=TRAVEL_DELAY_PENDING;travel_delay_count=0;travel_delay_active=1;delay_24v_3s_SetLow();}
                break;
            case TRAVEL_DELAY_PENDING:
                travel_delay_active=1; delay_24v_3s_SetLow();
                if(run_delay_interrupt_flag==1){run_delay_interrupt_flag=0;travel_delay_count++;}
                if(travel_delay_count>=travel_delay_timer){travel_state=TRAVEL_IDLE;travel_delay_count=0;travel_delay_active=0;delay_24v_3s_SetHigh();}
                break;
        }
        if(travel_delay_active) dn_safe_fault_LED_SetHigh();
        else                    dn_safe_fault_LED_SetLow();

        up_sw_led_SetHigh(); dn_sw_led_SetHigh(); stop_sw_led_SetHigh();

        if(!safe_mode_active){if(PORTEbits.RE1==1)alarm_out_SetHigh();else alarm_out_SetLow();}

        //========== KEYSWITCH ==========//
        if(PORTDbits.RD15==1){/* turn off in-car controls */}
        if(PORTDbits.RD15==0){/* turn on in-car controls  */}

        //========== FIRE ALARM ==========//
        if(PORTFbits.RF13==1&&PORTAbits.RA6==1&&PORTAbits.RA9==1){if(strcmp(fire_state,"ST 28")!=0){strcpy(fire_state,"ST 28");change_fire_state=1;}}
        if(PORTFbits.RF13==1&&PORTAbits.RA6==0){down_control_SetLow();down_fire_inhib_LED_SetHigh();if(strcmp(fire_state,"ST 29")!=0){strcpy(fire_state,"ST 29");change_fire_state=1;}}
        if((PORTFbits.RF13==1)&&(PORTAbits.RA9==0)&&(PORTCbits.RC3==0)){fire_up_inhibit=1;Up_fire_inhib_LED_SetHigh();if(strcmp(fire_state,"ST 2A")!=0){strcpy(fire_state,"ST 2A");change_fire_state=1;}}
        if(PORTFbits.RF13==0){fire_up_inhibit=0;fire_down_inhibit=0;Up_fire_inhib_LED_SetLow();down_fire_inhib_LED_SetLow();if(strcmp(fire_state,"ST 2B")!=0){strcpy(fire_state,"ST 2B");change_fire_state=1;}}

        if(TMR2_SoftwareCounterGet()>=1) handle_tmr2_scheduler_tick();
        if(TMR3_SoftwareCounterGet()>=1) handle_tmr3_scheduler_tick();

        //========== OVERTRAVEL TIMER ==========//
        {
            int currently_up=(PORTEbits.RE0==1);
            if(currently_up&&!overtravel_flag){if(overtravel_timer>=overtravel_time){overtravel_flag=1;overtravel_timer=0;UART1_WriteString("ST 17\r\n");Config_MarkDirty();}}
            else if(!currently_up) overtravel_timer=0;
            if(PORTDbits.RD13==1){if(overtravel_flag){UART1_WriteString("Overtravel reset\r\n");Config_MarkDirty();}overtravel_flag=0;overtravel_timer=0;}
            if(overtravel_flag==1){overtravel_led_SetHigh();UP_control_SetLow();}
            else                  {overtravel_led_SetLow(); UP_control_SetHigh();}
        }

        if(PORTCbits.RC3==1) dn_limit_override_SetHigh();
        else                 dn_limit_override_SetLow();

        //========== STATUS MESSAGES ==========//
        if(change_door_state==1){UART1_WriteString(door_state);UART1_WriteString("\r\n");change_door_state=0;}
        if(change_lock_state==1){UART1_WriteString(lock_state);UART1_WriteString("\r\n");change_lock_state=0;}
        if(change_lift_state==1){UART1_WriteString(lift_state);UART1_WriteString("\r\n");change_lift_state=0;}
        if(change_fire_state==1){UART1_WriteString(fire_state);UART1_WriteString("\r\n");change_fire_state=0;}

    } // End of while loop

} // End of main

/**
 End of File
*/