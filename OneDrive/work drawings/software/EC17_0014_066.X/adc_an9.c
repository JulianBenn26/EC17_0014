// =============================================================================
// adc_an9.c  ?  PIC24FJ128GA310  AN9 voltage monitor
//
// Hardware:
//   AN9 pin  ?  82k?  ?  input voltage (24?30 VDC nominal)
//                          10k? to GND at AN9
//
// Divider:   V_AN9 = V_IN × 10 / (82 + 10)  =  V_IN × 0.108696
// Inverse:   V_IN  = V_AN9 × 92 / 10
//
// ADC:       10-bit (matches MCC Easy Setup), AVDD = 3.3 V
//            LSB  = 3.3 / 1023  = 3.226 mV/count  (at AN9 pin)
//            Input resolution ? 29.7 mV/count
//            At 24 V ? count ? 809
//            At 30 V ? count ? 1011
//
// ADC clock: FOSC/2, ADCS=3 ? TAD = 500 ns  (matches MCC)
//            Trigger: manual (SSRC=000), scan mode
//            Acquisition: manual SAMP pulse ? 1 TAD (500 ns) before convert
//
// Output:    UART string  "VI xx.x\r\n"
//            Sent only when reading changes by ? 0.2 V from last sent value
//
// Integration:
//   1. In MCC ADC Easy Setup, scroll the channel list to AN9,
//      tick "Scan Enable", give it custom name "input_voltage", Generate.
//   2. Call adc_an9_init() AFTER ADC1_Initialize() in main().
//   3. Call adc_an9_task() from your main loop.
//   4. Wire uart_send_string() and millis() to your existing functions.
// =============================================================================

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "adc_an9.h"

// ?? Scaling constants (10-bit) ????????????????????????????????????????????????
#define DIVIDER_R_LOW        10UL       // k?
#define DIVIDER_R_HIGH       82UL       // k?
#define DIVIDER_DEN          (DIVIDER_R_HIGH + DIVIDER_R_LOW)  // 92

#define AVDD_MV              3300UL     // mV
#define ADC_COUNTS           1023UL     // 10-bit full scale

// ?? Thresholds ????????????????????????????????????????????????????????????????
#define SEND_THRESHOLD_MV    200UL      // 0.2 V change triggers send

// Rate limiting: adc_an9_task() is called every main loop iteration.
// We only want to send at most every ~2 seconds.
// If your main loop runs at ~1 ms, set this to 2000.
// If your main loop runs at ~10 ms, set this to 200.
// Adjust to match your loop speed.
#define SEND_RATE_LIMIT      500UL      // task calls between sends

// ?? Module state ??????????????????????????????????????????????????????????????
static uint16_t  an9_last_sent_mv   = 0xFFFF;  // 0xFFFF = never sent
static uint32_t  an9_call_counter   = 0;        // incremented each task() call
static uint32_t  an9_last_send_call = 0;        // call count at last send
static uint16_t  an9_current_mv     = 0;

// Use the same top-level MCC include that main.c uses ? covers all peripherals
#include "mcc_generated_files/mcc.h"

// UART1_WriteString may not exist in all MCC versions ? provide it if needed.
// MCC uart1.h always has UART1_Write(uint8_t) so we wrap it here.
static void _uart1_write_str(const char *s) {
    while (*s) {
        while (!UART1_IsTxReady());   // wait for TX buffer space
        UART1_Write((uint8_t)*s++);
    }
}

// ?? Private helpers ???????????????????????????????????????????????????????????
static uint16_t  adc_read_an9(void);
static uint16_t  adc_to_input_mv(uint16_t adc_count);

// =============================================================================
// adc_an9_init()
//
// IMPORTANT: call this AFTER MCC-generated ADC1_Initialize() ? we only
// add AN9 to the scan list and adjust the channel mux.  All other ADC
// registers are left as MCC configured them.
// =============================================================================
void adc_an9_init(void) {

    // ?? Pin: AN9 = RB9 ????????????????????????????????????????????????????????
    TRISBbits.TRISB9  = 1;      // input
    // MCC ADC1_Initialize() will have cleared PCFG9 if you ticked AN9 in
    // Easy Setup.  If not, uncomment the line that matches your DFP headers.
    // AN9 analog select ? write register directly to avoid header name issues
    // AD1PCFGL bit 9 = 0 means analog. Clear it regardless of header naming.
    //AD1PCFGL &= ~(1u << 9);

    // ?? Add AN9 to scan list ??????????????????????????????????????????????????
    // PIC24FJ128GA310: scan select bits named CSS9 (not CSSL9)
    AD1CSSLbits.CSS9 = 1;

    // ?? ADC clock: match MCC settings ?????????????????????????????????????????
    // MCC: FOSC/2, Conversion Clock = 2 TCY, TAD = 500 ns
    // ADCS = 3 ? TAD = (3+1) × Tcy = 4 × 125 ns = 500 ns  ?
    AD1CON3bits.ADRC  = 0;      // Fcy clock source
    AD1CON3bits.ADCS  = 3;      // TAD = 500 ns @ 8 MHz Fcy

    // ?? Trigger mode: manual (SSRC=000) ? matches MCC ?????????????????????????
    // "Clearing SAMP bit ends sampling and starts conversion"
    AD1CON1bits.SSRC  = 0b000;  // manual trigger
    AD1CON1bits.ASAM  = 0;      // manual sampling start
    AD1CON1bits.FORM  = 0b00;   // unsigned integer, right-justified

    // PIC24FJ128GA310 is 10-bit only ? no AD12B bit exists, always 10-bit

    // ?? Scan mode: enable so AN9 is swept with other channels ?????????????????
    AD1CON2bits.CSCNA = 1;      // scan inputs (uses AD1CSSL)

    // ?? SMPI: interrupt/flag after N+1 conversions ????????????????????????????
    // Set to match however many channels MCC has in the scan list.
    // Safe default: 0 = interrupt after every conversion (poll DONE instead).
    // MCC will have set this ? don't override here unless needed.

    // ?? Channel mux default (used when not scanning) ??????????????????????????
    AD1CHSbits.CH0SA  = 9;      // AN9 positive
    AD1CHSbits.CH0NA  = 0;      // AVSS negative

    // ADC already on from ADC1_Initialize() ? no need to set ADON again
    // If calling before ADC1_Initialize(), uncomment:
    // AD1CON1bits.ADON = 1;

    an9_last_sent_mv   = 0xFFFF;
    an9_call_counter   = 0;
    an9_last_send_call = 0;
}

// =============================================================================
// adc_an9_task()
// Non-blocking.  Call from main loop every cycle.
// Rate-limited by SEND_RATE_LIMIT (call count).
// =============================================================================
void adc_an9_task(void) {

    an9_call_counter++;

    uint16_t raw      = adc_read_an9();
    uint16_t input_mv = adc_to_input_mv(raw);
    an9_current_mv    = input_mv;

    // Threshold check
    uint32_t delta = (input_mv > an9_last_sent_mv)
                   ? (input_mv - an9_last_sent_mv)
                   : (an9_last_sent_mv - input_mv);

    bool first_read  = (an9_last_sent_mv == 0xFFFF);
    bool changed     = (delta >= SEND_THRESHOLD_MV);
    bool rate_ok     = ((an9_call_counter - an9_last_send_call) >= SEND_RATE_LIMIT);

    if ((first_read || changed) && rate_ok) {
        an9_send_uart(input_mv);
        an9_last_sent_mv   = input_mv;
        an9_last_send_call = an9_call_counter;
    }
}

// =============================================================================
// an9_send_uart()  ?  transmits "VI xx.x\r\n"
// =============================================================================
void an9_send_uart(uint16_t input_mv) {
    char buf[16];
    uint16_t whole  = input_mv / 1000;
    uint16_t tenths = (input_mv % 1000) / 100;
    snprintf(buf, sizeof(buf), "VI %u.%u\r\n", whole, tenths);
    _uart1_write_str(buf);
}

// =============================================================================
// an9_get_mv()  ?  returns last reading in millivolts, no side effects
// =============================================================================
uint16_t an9_get_mv(void) {
    return an9_current_mv;
}

// =============================================================================
// Private: trigger one manual conversion on AN9 and return 10-bit count
//
// Sequence (SSRC=000 manual trigger):
//   1. Point mux at AN9
//   2. SAMP=1 ? start sampling
//   3. Wait ? 1 TAD (500 ns) for the 82k source to charge pin capacitance
//      31 NOPs @ 8 MHz = ~31 × 125 ns = 3875 ns ? well within requirement
//   4. SAMP=0 ? end sampling, conversion starts automatically
//   5. Poll DONE
// =============================================================================
static uint16_t adc_read_an9(void) {
    // Point mux at AN9 (in case scan last left it elsewhere)
    AD1CHSbits.CH0SA = 9;

    AD1CON1bits.SAMP = 1;               // start sampling

    // Sample hold: ~31 NOPs ? 3.9 µs  (more than enough for 82k? source)
    __asm__ volatile (
        "repeat #30  \n"
        "nop         \n"
    );

    AD1CON1bits.SAMP = 0;               // end sampling ? conversion starts

    while (!AD1CON1bits.DONE);          // wait for conversion (~12 TAD = 6 µs)
    AD1CON1bits.DONE = 0;               // clear flag

    return (uint16_t)ADC1BUF0;         // 10-bit result, right-justified
}

// =============================================================================
// Private: convert 10-bit count ? input voltage in millivolts
//
//   V_IN (mV) = count × AVDD_MV × DIVIDER_DEN
//               ?????????????????????????????????
//               ADC_COUNTS × DIVIDER_R_LOW
//
//   Max numerator: 1023 × 3300 × 92 = 310,557,600  (fits in uint32_t)
// =============================================================================
static uint16_t adc_to_input_mv(uint16_t adc_count) {
    uint32_t num = (uint32_t)adc_count * AVDD_MV * DIVIDER_DEN;
    uint32_t den = ADC_COUNTS * DIVIDER_R_LOW;
    return (uint16_t)(num / den);
}