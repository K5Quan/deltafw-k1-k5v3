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
#include "helper/trng.h"

// Cortex-M SysTick register (24-bit down-counter)
#include "py32f0xx.h"

#define CSMA_CCA_THRESHOLD  (-100) // dBm
#define CSMA_MAX_ATTEMPTS     5
#define CSMA_TICKS_PER_MS    48000 // 48MHz sysclk

// ──── Non-blocking millisecond delay ────
// Polls SysTick->VAL (24-bit downcounter) to measure elapsed time
// without blocking interrupts or the main loop.
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

bool HERMES_CSMA_IsChannelClear(void) {
    return (HERMES_PHY_GetRSSI() < CSMA_CCA_THRESHOLD);
}

bool HERMES_CSMA_Transmit(const uint8_t *frame, uint16_t len) {
    if (!frame || len == 0) return false;

    uint16_t backoff = HM_BACKOFF_MIN_MS;
    uint32_t ms_elapsed = 0;

    for (uint8_t attempt = 0; attempt < CSMA_MAX_ATTEMPTS; attempt++) {
        // CCA window: sample RSSI for 5ms
        bool clear = true;
        for (uint8_t t = 0; t < 5; t++) {
            hermes_yield_ms(1);
            ms_elapsed++;
            BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, (ms_elapsed / 50) % 2);

            if (!HERMES_CSMA_IsChannelClear()) { clear = false; break; }
        }

        if (clear) {
            // DIFS
            for (uint8_t t = 0; t < HM_DIFS_MS; t++) {
                hermes_yield_ms(1);
                ms_elapsed++;
                BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, (ms_elapsed / 50) % 2);
            }

            if (HERMES_CSMA_IsChannelClear()) {
                BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false); // Turn off blink before solid TX starts
                HERMES_PHY_SaveRadioState();
                bool ok = HERMES_PHY_Transmit(frame, len);
                HERMES_PHY_RestoreRadioState();
                HERMES_PHY_StartRx();
                return ok;
            }
        }

        // Exponential backoff with jitter via TRNG
        uint8_t jitter = (uint8_t)(TRNG_GetU32()) & 0x1F;
        uint16_t wait_ms = backoff + jitter;

        for (uint16_t t = 0; t < wait_ms; t++) {
            hermes_yield_ms(1);
            ms_elapsed++;
            BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, (ms_elapsed / 50) % 2);
        }

        backoff *= 2;
        if (backoff > HM_BACKOFF_MAX_MS) backoff = HM_BACKOFF_MAX_MS;
    }

    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
    return false;
}

#endif // ENABLE_MESH_NETWORK
