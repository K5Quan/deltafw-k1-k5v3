/* Hermes Transport — CSMA/CA with Exponential Backoff (RFC §9)
 *
 * Uses non-blocking yield delay so UI/LED/key updates continue
 * during CCA and backoff periods. Polls SysTick->VAL directly
 * for microsecond-precision timing without blocking the main loop.
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/transport/csma.h"
#include "apps/hermes/physical/phy.h"
#include "apps/hermes/hermes_types.h"
#include "drivers/bsp/system.h"
#include "drivers/bsp/bk4819.h"
#include "features/radio/functions.h"
#include "helper/trng.h"

// Cortex-M SysTick register (24-bit down-counter)
#include "py32f0xx.h"

#define CSMA_CCA_THRESHOLD  (-110) // dBm per Hermes RFC
#define CSMA_MAX_ATTEMPTS      6    // Attempt 0-5
#define CSMA_TICKS_PER_MS    48000 // 48MHz sysclk

#define CW_BASE_MIN    50    // ms
#define CW_BASE_MAX    200   // ms
#define CW_CLAMP_MIN   1000  // ms
#define CW_CLAMP_MAX   5000  // ms
#define JITTER_MAX     100   // ms

// ──── Non-blocking millisecond delay ────
static void hermes_yield_ms(uint16_t ms) {
    uint32_t target = (uint32_t)ms * CSMA_TICKS_PER_MS;
    uint32_t elapsed = 0;
    uint32_t prev = SysTick->VAL;

    while (elapsed < target) {
        uint32_t now = SysTick->VAL;
        if (now <= prev)
            elapsed += prev - now;
        else
            elapsed += prev + (SysTick->LOAD + 1 - now); // wrap
        prev = now;
    }
}

// ──── CCA Window Sampling ────
// Requires 5 consecutive clear samples at 1ms intervals
static bool hermes_wait_for_clear(uint32_t max_wait_ms, uint32_t *ms_counter) {
    uint8_t clear_count = 0;
    for (uint32_t i = 0; i < max_wait_ms; i++) {
        hermes_yield_ms(1);
        (*ms_counter)++;
        
        // Blink LED while waiting (4Hz = 125ms phase)
        BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, ((*ms_counter) / 125) % 2);

        if (HERMES_CSMA_IsChannelClear()) {
            clear_count++;
            if (clear_count >= 5) return true;
        } else {
            clear_count = 0;
        }
    }
    return false;
}

bool HERMES_CSMA_IsChannelClear(void) {
    // Use same busy-channel detection as the firmware's BUSY_CHANNEL_LOCK.
    // When the BK4819's hardware squelch opens (calibrated RSSI + noise + glitch
    // thresholds from EEPROM), gCurrentFunction transitions to FUNCTION_RECEIVE.
    // This is the exact check at radio.c RADIO_PrepareTX() line 1314.
    return (gCurrentFunction != FUNCTION_RECEIVE);
}

bool HERMES_CSMA_Transmit(const uint8_t *frame, uint16_t len, Hermes_Priority_t priority) {
    if (!frame || len == 0) return false;

    uint32_t ms_elapsed = 0;

    for (uint8_t attempt = 0; attempt < CSMA_MAX_ATTEMPTS; attempt++) {
        // 1. Initial CCA
        if (hermes_wait_for_clear(50, &ms_elapsed)) {
            BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
            bool ok = HERMES_PHY_Transmit(frame, len);
            BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
            HERMES_PHY_StartRx();
            return ok;
        }

        // 2. Channel busy - calculate backoff
        uint32_t cw_min = CW_BASE_MIN << attempt;
        uint32_t cw_max = CW_BASE_MAX << attempt;

        // Apply priority divisors/multipliers
        if (priority == HM_PRIO_CRITICAL) { cw_min /= 4; cw_max /= 4; }
        else if (priority == HM_PRIO_LOW) { cw_min *= 2; cw_max *= 2; }

        if (cw_min > CW_CLAMP_MIN) cw_min = CW_CLAMP_MIN;
        if (cw_max > CW_CLAMP_MAX) cw_max = CW_CLAMP_MAX;

        uint32_t backoff = cw_min + (TRNG_GetU32() % (cw_max - cw_min + 1));
        uint32_t jitter = TRNG_GetU32() % (JITTER_MAX + 1);
        uint32_t total_wait = backoff + jitter;

        // 3. Backoff wait with early-exit
        BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
        uint32_t waited = 0;
        while (waited < total_wait) {
            // Check every 10ms for early exit
            if (hermes_wait_for_clear(10, &ms_elapsed)) {
                BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
                bool ok = HERMES_PHY_Transmit(frame, len);
                BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
                HERMES_PHY_StartRx();
                return ok;
            }
            waited += 10;
        }
    }

    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
    return false;
}

#endif // ENABLE_MESH_NETWORK
