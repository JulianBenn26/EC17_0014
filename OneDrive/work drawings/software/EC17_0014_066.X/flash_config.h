// flash_config.h
// Flash-based configuration storage for PIC24FJ128GA310
// Wessex Lift Co - EC17 0014

#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#include <stdint.h>

void Config_LoadAll(void);      // Call once at boot - loads flash or sets defaults
void Config_SaveAll(void);      // Erases page and writes all current values
void Config_SetDefaults(void);  // Sets RAM variables to factory defaults (no flash write)

#endif // FLASH_CONFIG_H
