// flash_config.c
// Flash-based configuration storage for PIC24FJ128GA310
// Wessex Lift Co - EC17 0014
//
// ============================================================
// LINKER SCRIPT NOTE - IMPORTANT
// You must reserve this page so the compiler never puts code here.
// In MPLAB X, open your project's .gld linker file and add this
// line inside the MEMORY { } block:
//
//   config_page (r)  : ORIGIN = 0x15000, LENGTH = 0x400
//
// This prevents XC16 from placing code at the config page.
// ============================================================

#include "flash_config.h"
#include <xc.h>
#include <stdint.h>

// ============================================================
// Flash page address - must be 0x400-aligned for PIC24FJ128GA310
// 0x15000 = second-to-last page (safe distance from end of flash at 0x157FE)
// ============================================================
#define CONFIG_FLASH_PAGE   0x15000UL
#define CONFIG_MAGIC        0xA55A      // Sentinel - if this is present, flash is valid

// ============================================================
// Word index map - each entry is one uint16_t stored in flash
// ============================================================
#define W_MAGIC             0
#define W_RED_LO            1
#define W_RED_HI            2
#define W_GREEN_LO          3
#define W_GREEN_HI          4
#define W_BLUE_LO           5
#define W_BLUE_HI           6
#define W_WHITE_LO          7
#define W_WHITE_HI          8
#define W_DOOR_SPD_LO       9
#define W_DOOR_SPD_HI       10
#define W_AUTOCLOSE_T_LO    11
#define W_AUTOCLOSE_T_HI    12
#define W_AUTOCLOSE         13
#define W_AUTOHOME_T_LO     14
#define W_AUTOHOME_T_HI     15
#define W_AUTOHOME          16
#define W_AUTOHOME_DIR      17
#define W_OT_LO             18
#define W_OT_HI             19
#define W_TOP_OR_LO         20
#define W_TOP_OR_HI         21
#define W_BOT_OR_LO         22
#define W_BOT_OR_HI         23
#define W_LIGHT_T_LO        24
#define W_LIGHT_T_HI        25
// Total: 26 words used out of 64 available in one row

// ============================================================
// Factory default values - used when flash is blank
// ============================================================
#define DEFAULT_RED                 0x00
#define DEFAULT_GREEN               0x00
#define DEFAULT_BLUE                0x00
#define DEFAULT_WHITE               0xFF
#define DEFAULT_DOOR_SPEED          0x7F
#define DEFAULT_DOOR_AUTOCLOSE_T    100
#define DEFAULT_DOOR_AUTOCLOSE      1
#define DEFAULT_AUTOHOME_T          300
#define DEFAULT_AUTOHOME            0
#define DEFAULT_AUTOHOME_DIR        0
#define DEFAULT_OVERTRAVEL_T        90
#define DEFAULT_TOP_OVERRUN         25 // should be about 15mm @60mm/s speed
#define DEFAULT_BOT_OVERRUN        25  // should be about 15mm @60mm/s speed
#define DEFAULT_LIGHTING_T          3000

// ============================================================
// External variables declared in main.c
// ============================================================
extern uint32_t uart_red;
extern uint32_t uart_green;
extern uint32_t uart_blue;
extern uint32_t uart_white;
extern uint32_t uart_door_speed;
extern uint32_t uart_door_autoclose_timer;
extern int      uart_door_autoclose;
extern uint32_t uart_autohome_timer;
extern int      uart_autohome;
extern int      uart_autohome_direction;
extern uint32_t uart_overttravel_timer;
extern uint32_t uart_top_overrrun;
extern uint32_t uart_bottom_overrrun;
extern uint32_t lighting_timer;

// ============================================================
// Low-level flash primitives
// ============================================================

// Read one 16-bit word from program flash
static uint16_t Flash_ReadWord(uint32_t addr) {
    TBLPAG = (uint8_t)(addr >> 16);
    return __builtin_tblrdl((uint16_t)(addr & 0xFFFF));
}

// Erase the entire config page (all bits -> 1)
// Must be done before any write
static void Flash_ErasePage(void) {
    NVMCON = 0x4042;                                            // Page erase command
    TBLPAG = (uint8_t)(CONFIG_FLASH_PAGE >> 16);
    __builtin_tblwtl((uint16_t)(CONFIG_FLASH_PAGE & 0xFFFF), 0xFFFF);
    __builtin_disi(5);                                          // Disable interrupts for key sequence
    __builtin_write_NVM();
    while (NVMCONbits.WR);                                      // Wait for erase to complete
}

// Write a full 64-word row starting at CONFIG_FLASH_PAGE
// Unused words are filled with 0xFFFF (erased state)
static void Flash_WriteRow(uint16_t *words) {
    uint16_t offset = (uint16_t)(CONFIG_FLASH_PAGE & 0xFFFF);

    NVMCON = 0x4001;                                            // Row write command
    TBLPAG = (uint8_t)(CONFIG_FLASH_PAGE >> 16);

    for (uint16_t i = 0; i < 64; i++) {
        __builtin_tblwtl(offset + (i * 2), words[i]);
        __builtin_tblwth(offset + (i * 2), 0xFF);              // Phantom byte - always 0xFF
    }

    __builtin_disi(5);
    __builtin_write_NVM();
    while (NVMCONbits.WR);
}

// ============================================================
// Public API
// ============================================================

// Set all config variables in RAM to factory defaults
// Does NOT write to flash
void Config_SetDefaults(void) {
    uart_red                  = DEFAULT_RED;
    uart_green                = DEFAULT_GREEN;
    uart_blue                 = DEFAULT_BLUE;
    uart_white                = DEFAULT_WHITE;
    uart_door_speed           = DEFAULT_DOOR_SPEED;
    uart_door_autoclose_timer = DEFAULT_DOOR_AUTOCLOSE_T;
    uart_door_autoclose       = DEFAULT_DOOR_AUTOCLOSE;
    uart_autohome_timer       = DEFAULT_AUTOHOME_T;
    uart_autohome             = DEFAULT_AUTOHOME;
    uart_autohome_direction   = DEFAULT_AUTOHOME_DIR;
    uart_overttravel_timer    = DEFAULT_OVERTRAVEL_T;
    uart_top_overrrun         = DEFAULT_TOP_OVERRUN;
    uart_bottom_overrrun      = DEFAULT_BOT_OVERRUN;
    lighting_timer            = DEFAULT_LIGHTING_T;
}

// Load config from flash into RAM variables.
// If flash is blank (magic mismatch), loads defaults instead.
// Call once at startup in main() before using any config variables.
void Config_LoadAll(void) {
    uint16_t magic = Flash_ReadWord(CONFIG_FLASH_PAGE + (W_MAGIC * 2));

    if (magic != CONFIG_MAGIC) {
        // Flash has never been written, or is corrupt - fall back to defaults
        Config_SetDefaults();
        return;
    }

    // Helper macro to reassemble a uint32_t from two consecutive flash words
    #define READ32(lo_idx) \
        ( (uint32_t)Flash_ReadWord(CONFIG_FLASH_PAGE + ((lo_idx)     * 2))        | \
          (uint32_t)Flash_ReadWord(CONFIG_FLASH_PAGE + ((lo_idx + 1) * 2)) << 16 )

    uart_red                  = READ32(W_RED_LO);
    uart_green                = READ32(W_GREEN_LO);
    uart_blue                 = READ32(W_BLUE_LO);
    uart_white                = READ32(W_WHITE_LO);
    uart_door_speed           = READ32(W_DOOR_SPD_LO);
    uart_door_autoclose_timer = READ32(W_AUTOCLOSE_T_LO);
    uart_door_autoclose       = (int)Flash_ReadWord(CONFIG_FLASH_PAGE + (W_AUTOCLOSE    * 2));
    uart_autohome_timer       = READ32(W_AUTOHOME_T_LO);
    uart_autohome             = (int)Flash_ReadWord(CONFIG_FLASH_PAGE + (W_AUTOHOME     * 2));
    uart_autohome_direction   = (int)Flash_ReadWord(CONFIG_FLASH_PAGE + (W_AUTOHOME_DIR * 2));
    uart_overttravel_timer    = READ32(W_OT_LO);
    uart_top_overrrun         = READ32(W_TOP_OR_LO);
    uart_bottom_overrrun      = READ32(W_BOT_OR_LO);
    lighting_timer            = READ32(W_LIGHT_T_LO);

    #undef READ32
}

// Save all current RAM config values to flash.
// Erases the page first, then writes all values in one row.
// Takes ~20ms - do not call from an ISR.
void Config_SaveAll(void) {
    uint16_t words[64];

    // Pre-fill with 0xFFFF (blank/erased state for any unused words)
    for (int i = 0; i < 64; i++) {
        words[i] = 0xFFFF;
    }

    // Pack all values into the word array
    words[W_MAGIC]           = CONFIG_MAGIC;
    words[W_RED_LO]          = (uint16_t)(uart_red                  & 0xFFFF);
    words[W_RED_HI]          = (uint16_t)(uart_red                  >> 16);
    words[W_GREEN_LO]        = (uint16_t)(uart_green                & 0xFFFF);
    words[W_GREEN_HI]        = (uint16_t)(uart_green                >> 16);
    words[W_BLUE_LO]         = (uint16_t)(uart_blue                 & 0xFFFF);
    words[W_BLUE_HI]         = (uint16_t)(uart_blue                 >> 16);
    words[W_WHITE_LO]        = (uint16_t)(uart_white                & 0xFFFF);
    words[W_WHITE_HI]        = (uint16_t)(uart_white                >> 16);
    words[W_DOOR_SPD_LO]     = (uint16_t)(uart_door_speed           & 0xFFFF);
    words[W_DOOR_SPD_HI]     = (uint16_t)(uart_door_speed           >> 16);
    words[W_AUTOCLOSE_T_LO]  = (uint16_t)(uart_door_autoclose_timer & 0xFFFF);
    words[W_AUTOCLOSE_T_HI]  = (uint16_t)(uart_door_autoclose_timer >> 16);
    words[W_AUTOCLOSE]       = (uint16_t) uart_door_autoclose;
    words[W_AUTOHOME_T_LO]   = (uint16_t)(uart_autohome_timer       & 0xFFFF);
    words[W_AUTOHOME_T_HI]   = (uint16_t)(uart_autohome_timer       >> 16);
    words[W_AUTOHOME]        = (uint16_t) uart_autohome;
    words[W_AUTOHOME_DIR]    = (uint16_t) uart_autohome_direction;
    words[W_OT_LO]           = (uint16_t)(uart_overttravel_timer    & 0xFFFF);
    words[W_OT_HI]           = (uint16_t)(uart_overttravel_timer    >> 16);
    words[W_TOP_OR_LO]       = (uint16_t)(uart_top_overrrun         & 0xFFFF);
    words[W_TOP_OR_HI]       = (uint16_t)(uart_top_overrrun         >> 16);
    words[W_BOT_OR_LO]       = (uint16_t)(uart_bottom_overrrun      & 0xFFFF);
    words[W_BOT_OR_HI]       = (uint16_t)(uart_bottom_overrrun      >> 16);
    words[W_LIGHT_T_LO]      = (uint16_t)(lighting_timer            & 0xFFFF);
    words[W_LIGHT_T_HI]      = (uint16_t)(lighting_timer            >> 16);

    Flash_ErasePage();
    Flash_WriteRow(words);
}
