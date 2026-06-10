// =============================================================================
// adc_an9.h  ?  PIC24 AN9 voltage monitor
// =============================================================================
#ifndef ADC_AN9_H
#define ADC_AN9_H

#include <stdint.h>
#include <stdbool.h>

// ?? Public API ????????????????????????????????????????????????????????????????

// Call once from main() after oscillator / peripheral init
void     adc_an9_init(void);

// Call from main loop ? non-blocking, sends UART only on significant change
void     adc_an9_task(void);

// Returns last sampled input voltage in millivolts (e.g. 27300 = 27.3 V)
uint16_t an9_get_mv(void);

// Force an immediate UART send regardless of threshold
// input_mv: value from an9_get_mv(), or pass 0 to re-read
void     an9_send_uart(uint16_t input_mv);

// ?? Scaling summary (for reference) ??????????????????????????????????????????
//
//   Input range  :  0 ? 36 V  (absolute max; resistors limit ADC to 3.3 V)
//   Nominal range:  24 ? 30 V
//   At 24 V      :  ADC ? 2609 mV on pin  ?  count ? 3238 / 4095
//   At 30 V      :  ADC ? 3261 mV on pin  ?  count ? 4047 / 4095
//   Resolution   :  3300 × 92 / (4095 × 10) ÷ 1000  ?  7.4 mV per count
//                   i.e. ~0.007 V resolution at input
//   UART format  :  "VI 27.3\r\n"

#endif // ADC_AN9_H