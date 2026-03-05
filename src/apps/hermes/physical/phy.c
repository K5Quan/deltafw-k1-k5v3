/* Hermes Physical Layer — BK4819 FFSK1200 Modem Implementation
 * Written from scratch per Hermes RFC §1–§4.
 *
 * FFSK 1200/1800 Hz, 1200 baud, 128-byte fixed frames.
 * Uses the BK4819's hardware FSK modem with 4-byte sync word,
 * 16-byte preamble, and FIFO-based DMA-free transfers.
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/physical/phy.h"
#include "apps/hermes/hermes_types.h"
#include "apps/hermes/hermes.h"
#include "drivers/bsp/bk4819.h"
#include "drivers/bsp/system.h"
#include "features/storage/storage.h"
#include "features/radio/frequencies.h"

// ──── Saved Radio State ────for restore after FSK ops ────
static struct {
    uint16_t reg_51;  // CTCSS/CDCSS
    uint16_t reg_40;  // FM deviation
    uint16_t reg_2B;  // AF filters
} phy_saved;

// ──── Configure BK4819 for Hermes FFSK ────
void HERMES_PHY_Init(const uint8_t sync_word[4]) {
    // REG_70: Enable TONE2 generator for FSK, gain=96
    BK4819_WriteRegister(BK4819_REG_70,
        (0u << 15) |   // TONE1 disable
        (0u <<  8) |   // TONE1 gain
        (1u <<  7) |   // TONE2 enable
        (96u << 0));   // TONE2/FSK gain

    // REG_72: TONE2 frequency = 1200 Hz
    // For 26 MHz XTAL: freq_word = 1200 * 10.32444 ≈ 0x3065
    BK4819_WriteRegister(BK4819_REG_72, 0x3065);

    // REG_58: FSK mode configuration
    //   TX mode = FFSK 1200/1800 (mode 1)
    //   RX mode = FFSK 1200/1800 (mode 7)
    //   RX gain = 3 (max sensitivity)
    //   Preamble type = 0 (auto from sync byte MSB)
    //   RX bandwidth = FFSK 1200/1800 (mode 1)
    //   FSK enable
    BK4819_WriteRegister(BK4819_REG_58,
        (1u << 13) |   // TX: FFSK 1200/1800
        (7u << 10) |   // RX: FFSK 1200/1800
        (3u <<  8) |   // RX gain max
        (0u <<  6) |   // reserved
        (0u <<  4) |   // preamble type: auto
        (1u <<  1) |   // RX BW: FFSK 1200/1800
        (1u <<  0));   // FSK enable

    // REG_5A/5B: 4-byte sync word (MSB first)
    uint16_t sync_01 = ((uint16_t)sync_word[0] << 8) | sync_word[1];
    uint16_t sync_23 = ((uint16_t)sync_word[2] << 8) | sync_word[3];
    BK4819_WriteRegister(BK4819_REG_5A, sync_01);
    BK4819_WriteRegister(BK4819_REG_5B, sync_23);

    // REG_5C: Disable CRC (we use RS FEC instead)
    BK4819_WriteRegister(BK4819_REG_5C, 0x0000);

    // Set initial RX frequency
    BK4819_SetFrequency(gHermesConfig.frequency);
    BK4819_PickRXFilterPathBasedOnFrequency(gHermesConfig.frequency);

    // REG_5D: Packet size = 128 bytes
    BK4819_WriteRegister(BK4819_REG_5D, (HM_FRAME_SIZE << 8));

    // REG_5E: FIFO almost-full threshold = 64 words (for RX)
    BK4819_WriteRegister(BK4819_REG_5E, (64u << 3) | (1u << 0));

    // REG_59: Configure preamble + sync lengths, clear FIFOs
    uint16_t reg59_base =
        (0u << 13) |   // no scramble
        (0u << 12) |   // RX off initially
        (0u << 11) |   // TX off initially
        (0u << 10) |   // no RX invert
        (0u <<  9) |   // no TX invert
        (0u <<  8) |   // reserved
        (15u << 4) |   // 16-byte preamble
        (1u <<  3) |   // 4-byte sync
        (0u <<  0);    // reserved

    // Clear both FIFOs
    BK4819_WriteRegister(BK4819_REG_59, (1u << 15) | (1u << 14) | reg59_base);
    BK4819_WriteRegister(BK4819_REG_59, reg59_base);
}

// ──── Arm RX ────
void HERMES_PHY_StartRx(void) {
    uint16_t reg59 = BK4819_ReadRegister(BK4819_REG_59);
    // Clear FIFOs first
    BK4819_WriteRegister(BK4819_REG_59, reg59 | (1u << 15) | (1u << 14));
    // Enable RX
    reg59 &= ~((1u << 15) | (1u << 14) | (1u << 11)); // clear TX/clear bits
    reg59 |= (1u << 12);  // enable RX
    BK4819_WriteRegister(BK4819_REG_59, reg59);
}

// ──── Stop RX ────
void HERMES_PHY_StopRx(void) {
    uint16_t reg59 = BK4819_ReadRegister(BK4819_REG_59);
    reg59 &= ~(1u << 12);  // disable RX
    BK4819_WriteRegister(BK4819_REG_59, reg59);
}

// ──── Transmit 128-byte frame ────
bool HERMES_PHY_Transmit(const uint8_t *frame, uint16_t len) {
    if (!frame || len > HM_FRAME_SIZE) return false;

    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
    
    // Enable Power Amplifier
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, true);
    SYSTEM_DelayMs(5);

    uint8_t Band = FREQUENCY_GetBand(gHermesConfig.frequency);
    if (Band != BAND_NONE) {
        uint8_t Txp[3];
        Storage_ReadRecordIndexed(REC_CALIB_TX_POWER, (Band << 8) | gHermesConfig.tx_power, Txp, 0, 3);
        
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
        static const uint8_t dividers_band2[6] = { 20, 15, 10, 8, 6, 4 };
        static const uint8_t dividers_band5[6] = { 25, 19, 13, 9, 6, 4 };
        const uint8_t *dividers = (Band == BAND2_108MHz) ? dividers_band2 : dividers_band5;
        for (uint8_t p = 0; p < 3; p++) {
            if (gHermesConfig.tx_power < 2) Txp[p] = (Txp[p] * 4) / dividers[gHermesConfig.tx_power * 2 + 1]; // approximate mid/low from user mapping
            else Txp[p] += 24;
        }
#endif
        uint8_t bias = FREQUENCY_CalculateOutputPower(Txp[0], Txp[1], Txp[2], 
            frequencyBandTable[Band].lower, 
            (frequencyBandTable[Band].lower + frequencyBandTable[Band].upper) / 2, 
            frequencyBandTable[Band].upper, gHermesConfig.frequency);
            
        BK4819_SetupPowerAmplifier(bias, gHermesConfig.frequency);
    } else {
        BK4819_SetupPowerAmplifier((gHermesConfig.tx_power + 1) * 35, gHermesConfig.frequency); // Fallback
    }

    uint16_t reg59 = BK4819_ReadRegister(BK4819_REG_59);

    // Disable RX while transmitting
    reg59 &= ~(1u << 12);
    BK4819_WriteRegister(BK4819_REG_59, reg59);

    // Reconfigure for TX preamble (16 bytes)
    uint16_t reg59_tx =
        (0u << 13) |   // no scramble
        (0u << 12) |   // RX off
        (0u << 11) |   // TX off (will enable after FIFO load)
        (0u << 10) | (0u << 9) | (0u << 8) |
        (15u << 4) |   // 16-byte preamble
        (1u <<  3) |   // 4-byte sync
        (0u <<  0);

    // Clear FIFOs
    BK4819_WriteRegister(BK4819_REG_59, (1u << 15) | (1u << 14) | reg59_tx);
    BK4819_WriteRegister(BK4819_REG_59, reg59_tx);

    SYSTEM_DelayMs(10);

    // Load frame into TX FIFO (16 bits at a time, little-endian word order)
    for (uint16_t i = 0; i < len; i += 2) {
        uint16_t word = frame[i];
        if (i + 1 < len) word |= ((uint16_t)frame[i + 1] << 8);
        BK4819_WriteRegister(BK4819_REG_5F, word);
    }

    // Enable FSK TX
    BK4819_WriteRegister(BK4819_REG_59, (1u << 11) | reg59_tx);

    // Wait for TX complete with timeout (~1500ms for 128B @ 1200bps + preamble)
    bool success = false;
    for (uint16_t t = 0; t < 300; t++) {
        SYSTEM_DelayMs(5);
        if (BK4819_ReadRegister(BK4819_REG_0C) & (1u << 0)) {
            BK4819_WriteRegister(BK4819_REG_02, 0);
            uint16_t irq = BK4819_ReadRegister(BK4819_REG_02);
            if (irq & BK4819_REG_02_FSK_TX_FINISHED) {
                success = true;
                break;
            }
        }
    }

    SYSTEM_DelayMs(20);

    // Disable TX, re-arm RX
    BK4819_WriteRegister(BK4819_REG_59, reg59_tx);

    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
    BK4819_SetupPowerAmplifier(0, 0);

    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);

    return success;
}

// ──── Read RX FIFO ────
uint16_t HERMES_PHY_ReadFIFO(uint8_t *buf, uint16_t max_len) {
    uint16_t count = BK4819_ReadRegister(BK4819_REG_5E) & 0x7F; // FIFO count
    uint16_t bytes_read = 0;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t word = BK4819_ReadRegister(BK4819_REG_5F);
        if (bytes_read < max_len) buf[bytes_read++] = (word >> 0) & 0xFF;
        if (bytes_read < max_len) buf[bytes_read++] = (word >> 8) & 0xFF;
    }
    return bytes_read;
}

// ──── Hardware Telemetry ────
int16_t HERMES_PHY_GetRSSI(void) {
    return BK4819_GetRSSI_dBm();
}

uint16_t HERMES_PHY_GetExNoise(void) {
    return BK4819_ReadRegister(BK4819_REG_65) & 0x007F;
}

uint16_t HERMES_PHY_GetGlitch(void) {
    return BK4819_ReadRegister(BK4819_REG_63) & 0x00FF;
}

// ──── Radio State Save/Restore ────
void HERMES_PHY_SaveRadioState(void) {
    phy_saved.reg_51 = BK4819_ReadRegister(BK4819_REG_51);
    phy_saved.reg_40 = BK4819_ReadRegister(BK4819_REG_40);
    phy_saved.reg_2B = BK4819_ReadRegister(BK4819_REG_2B);

    // Disable CTCSS/CDCSS during FSK
    BK4819_WriteRegister(BK4819_REG_51, 0);
    // Disable HPF + pre-emphasis for FSK
    BK4819_WriteRegister(BK4819_REG_2B, (1u << 2) | (1u << 0));
    // Set deviation for FFSK
    BK4819_WriteRegister(BK4819_REG_40, (phy_saved.reg_40 & 0xF000) | (850u & 0xFFF));
}

void HERMES_PHY_RestoreRadioState(void) {
    BK4819_WriteRegister(BK4819_REG_51, phy_saved.reg_51);
    BK4819_WriteRegister(BK4819_REG_40, phy_saved.reg_40);
    BK4819_WriteRegister(BK4819_REG_2B, phy_saved.reg_2B);
}

#endif // ENABLE_MESH_NETWORK
