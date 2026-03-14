/* Hermes Physical Layer — BK4819 FFSK1200 Modem
 * Re-implemented from scratch per kamil/matoz FSK1200 reference
 * and Hermes RFC §1–§4.
 *
 * Reference implementations:
 *   kamil: app/messenger.c → MSG_ConfigureFSK(), MSG_FSKSendData(),
 *          MSG_StorePacket(), MSG_EnableRX()
 *   matoz: apps/messenger.c → MSG_FSKSendData(), MSG_EnableRX(),
 *          MSG_StorePacket()
 *
 * FFSK 1200/1800 Hz, 1200 baud, 128-byte fixed frames.
 * Uses the BK4819's hardware FSK modem with 4-byte sync word,
 * 16-byte preamble (TX), and FIFO-based transfers.
 *
 * Passive RX is interrupt-driven: hermes.c HERMES_HandleFSKInterrupt()
 * receives raw interrupt bits from the main radio interrupt loop,
 * calls HERMES_PHY_ReadFIFO() on FIFO_ALMOST_FULL, and calls
 * HERMES_PHY_StartRx() on RX_FINISHED to re-arm for next packet.
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/physical/phy.h"
#include "apps/hermes/hermes_types.h"
#include "apps/hermes/hermes.h"
#include "drivers/bsp/bk4819.h"
#include "drivers/bsp/system.h"
#include "features/storage/storage.h"
#include "features/radio/frequencies.h"
#include "features/audio/audio.h"
#include "py32f0xx.h"

// ──── Saved Radio State ── for restore after FSK TX ────
static struct {
    uint16_t reg_51;  // CTCSS/CDCSS
    uint16_t reg_40;  // FM deviation
    uint16_t reg_2B;  // AF filters
} phy_saved;

// ──── REG_59 base config (persistent, no enable/clear bits) ────
// Matches kamil MSG_ConfigureFSK() and matoz MSG_FSKSendData():
//   No scramble, no RX, no TX, no invert
//   Sync = 4 bytes
//   Reserved bits = 0
// Preamble length is set per-context (0 for RX, 15 for TX).
static uint16_t phy_reg59_base_rx;
static uint16_t phy_reg59_base_tx;

// ──── Full FSK modem configuration ────
// Called once on init and once before each TX.
// Matches kamil MSG_ConfigureFSK(rx) exactly.
//
// When rx=true:  RX mode setup (preamble=0, gain=3, threshold=2)
// When rx=false: TX mode setup (preamble=15, gain=0, threshold untouched)
static void HERMES_PHY_ConfigureFSK(bool rx) {

    // ── REG_70: Enable TONE2 for FSK baudrate, gain=96 ──
    // Kamil: (0u<<15)|(0u<<8)|(1u<<7)|(96u<<0)
    // Matoz: identical
    BK4819_WriteRegister(BK4819_REG_70,
        (0u << 15) |    // TONE1 disable
        (0u <<  8) |    // TONE1 gain = 0
        (1u <<  7) |    // TONE2 enable
        (96u << 0));    // TONE2/FSK gain = 96

    // ── REG_72: Tone2 = 1200Hz baudrate ──
    // freq_word = 1200 * 10.32444 ≈ 0x3065 (for 26MHz XTAL)
    BK4819_WriteRegister(BK4819_REG_72, 0x3065);

    // ── REG_58: FSK modem mode config ──
    // Kamil AFSK_1200 / Matoz: TX=1(FFSK1200/1800), RX=7(FFSK1200/1800)
    // Kamil: RX gain=3, matoz TX: RX gain=0
    // Both: preamble type=0, BW=1(FFSK1200/1800), enable=1
    BK4819_WriteRegister(BK4819_REG_58,
        (1u << 13) |             // TX: FFSK 1200/1800
        (7u << 10) |             // RX: FFSK 1200/1800
        ((rx ? 3u : 0u) << 8) | // RX gain: 3 for RX, 0 for TX (per matoz)
        (0u <<  6) |             // reserved
        (0u <<  4) |             // preamble type: auto (0xAA/0x55)
        (1u <<  1) |             // RX BW: FFSK 1200/1800
        (1u <<  0));             // FSK enable

    // ── REG_5A/5B: 4-byte sync word ──
    uint16_t sync_01 = ((uint16_t)gHermesConfig.sync_word[0] << 8) | gHermesConfig.sync_word[1];
    uint16_t sync_23 = ((uint16_t)gHermesConfig.sync_word[2] << 8) | gHermesConfig.sync_word[3];
    BK4819_WriteRegister(BK4819_REG_5A, sync_01);
    BK4819_WriteRegister(BK4819_REG_5B, sync_23);

    // ── REG_5C: Disable HW CRC (handled by L2 software FEC) ──
    // 0x5625 in both kamil and matoz
    BK4819_WriteRegister(BK4819_REG_5C, 0x5625);

    // ── REG_5E: Almost-full threshold ──
    // kamil: (64u << 3) | (1u << 0) — 64 word almost-full threshold
    if (rx)
        BK4819_WriteRegister(BK4819_REG_5E, (64u << 3) | (1u << 0));

    // ── REG_5D: Packet size ──
    // kamil RX: rounds up to even + 2 for RX, else BK4819 FSK RX fails
    // kamil TX: exact packet size
    // matoz: uses (TX_MSG_LENGTH + 2) << 8 for both
    uint16_t size = HM_FRAME_SIZE;
    if (rx)
        size = (((size + 1) / 2) * 2) + 2;  // round up to even + 2

    BK4819_WriteRegister(BK4819_REG_5D, (size << 8));

    // ── REG_59: FIFO control + preamble/sync config ──
    // kamil: preamble = (rx ? 0 : 15), sync = 4 bytes, no scramble/invert
    // matoz TX: preamble = 15, matoz RX: preamble = 0
    uint16_t reg59_base =
        (0u << 13) |                    // no scramble
        (0u << 12) |                    // RX off initially
        (0u << 11) |                    // TX off initially
        (0u << 10) |                    // no RX invert
        (0u <<  9) |                    // no TX invert
        (0u <<  8) |                    // reserved
        ((rx ? 0u : 15u) << 4) |        // preamble: 0 for RX, 15 for TX
        (1u <<  3) |                    // 4-byte sync
        (0u <<  0);                     // reserved

    // Store base for later use in StartRx/Transmit
    if (rx)
        phy_reg59_base_rx = reg59_base;
    else
        phy_reg59_base_tx = reg59_base;

    // Clear both FIFOs (pulse bits 15, 14)
    // kamil BK4819_FskClearFifo(): fsk_reg59 |= (1<<15)|(1<<14)
    BK4819_WriteRegister(BK4819_REG_59, (1u << 15) | (1u << 14) | reg59_base);
    BK4819_WriteRegister(BK4819_REG_59, reg59_base);

    // Clear pending interrupts (kamil: end of MSG_ConfigureFSK)
    BK4819_WriteRegister(BK4819_REG_02, 0);
}

// ──── Initialize PHY (called once on Hermes startup) ────
// Sets frequency, configures FSK for RX, and arms the receiver.
void HERMES_PHY_Init(void) {
    // Set RX frequency
    BK4819_SetFrequency(gHermesConfig.frequency);
    BK4819_PickRXFilterPathBasedOnFrequency(gHermesConfig.frequency);

    // Full FSK configuration for RX
    HERMES_PHY_ConfigureFSK(true);

    // Arm RX (enable RX bit)
    // kamil BK4819_FskEnableRx(): fsk_reg59 |= (1<<12)
    BK4819_WriteRegister(BK4819_REG_59, (1u << 12) | phy_reg59_base_rx);
}

// ──── Start/Re-arm RX ────
// Called from RADIO_SetupRegisters() in radio.c whenever the radio
// state is rebuilt (dual watch toggle, power save wake, etc).
// Must do FULL modem reconfigure because RADIO_SetupRegisters()
// resets all BK4819 registers before calling us.
//
// This matches kamil's integration: RADIO_SetupRegisters() calls
// MSG_EnableRX(true) which does MSG_ConfigureFSK(true) + BK4819_FskEnableRx().
void HERMES_PHY_StartRx(void) {
    // Full FSK modem configuration for RX
    HERMES_PHY_ConfigureFSK(true);

    // Enable RX (kamil BK4819_FskEnableRx(): fsk_reg59 |= (1<<12))
    BK4819_WriteRegister(BK4819_REG_59, (1u << 12) | phy_reg59_base_rx);
}

// ──── Stop RX / Disable FSK (called when leaving Hermes) ────
// Matches kamil/matoz MSG_EnableRX(false):
//   REG_70 = 0, REG_58 = 0
void HERMES_PHY_StopRx(void) {
    BK4819_WriteRegister(BK4819_REG_70, 0);
    BK4819_WriteRegister(BK4819_REG_58, 0);
}

// ──── Transmit 128-byte frame ────
// Complete TX flow matching kamil MSG_SendPacket()+MSG_FSKSendData()
// and matoz MSG_Send()+MSG_FSKSendData().
//
// Kamil flow:
//   MSG_SendPacket():
//     1. RADIO_PrepareTX() → checks TX allowed, calls FUNCTION_Select(FUNCTION_TRANSMIT)
//     2. Red LED on, green LED off
//     3. BK4819_DisableDTMF(), gMuteMic=true
//     4. SYSTEM_DelayMs(50)
//     5. MSG_FSKSendData() — the actual FSK TX
//     6. SYSTEM_DelayMs(50)
//     7. APP_EndTransmission(), FUNCTION_Select(FUNCTION_FOREGROUND)
//     8. MSG_EnableRX(true) — re-arm passive RX
//   MSG_FSKSendData():
//     1. Save REG_51/40/2B
//     2. Disable CTCSS, set deviation, disable HPF/pre-emphasis
//     3. MSG_ConfigureFSK(false) — full FSK TX config
//     4. SYSTEM_DelayMs(100)
//     5. Load FIFO
//     6. BK4819_FskEnableTx()
//     7. Poll REG_0C/REG_02 for FSK_TX_FINISHED
//     8. SYSTEM_DelayMs(100)
//     9. MSG_ConfigureFSK(true) — re-arm FSK for RX
//    10. Restore REG_40/2B/51
//
// Matoz flow:
//   MSG_Send():
//     1. IsTXAllowed() check
//     2. Red LED on
//     3. BK4819_DisableDTMF()
//     4. RADIO_SetTxParameters() — sets freq, PA, filter, BK4819_PrepareTransmit()
//     5. SYSTEM_DelayMs(500)
//     6. BK4819_ExitTxMute()
//     7. MSG_FSKSendData()
//     8. APP_EndTransmission()
//     9. Red LED off
//    10. MSG_EnableRX(true)
bool HERMES_PHY_Transmit(const uint8_t *frame, uint16_t len) {
    if (!frame || len > HM_FRAME_SIZE) return false;

    // ── LED feedback ──
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);

    // ── 1. Save radio state ──
    phy_saved.reg_51 = BK4819_ReadRegister(BK4819_REG_51);
    phy_saved.reg_40 = BK4819_ReadRegister(BK4819_REG_40);
    phy_saved.reg_2B = BK4819_ReadRegister(BK4819_REG_2B);

    // ── 2. Disable CTCSS/CDCSS during FSK ──
    BK4819_WriteRegister(BK4819_REG_51, 0);

    // ── 3. Set FM deviation for FFSK ──
    BK4819_WriteRegister(BK4819_REG_40, (phy_saved.reg_40 & 0xF000) | 1050);

    // ── 4. Disable HPF + pre-emphasis ──
    BK4819_WriteRegister(BK4819_REG_2B, (1u << 2) | (1u << 0));

    // ── 5. Disable RX path, disable audio (matches matoz/kamil) ──
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);
    AUDIO_AudioPathOff();

    // ── 6. Prepare BK4819 for TX (PTT keying, like RADIO_SetTxParameters) ──
    BK4819_PrepareTransmit();
    SYSTEM_DelayMs(10);

    // ── 7. Set TX frequency (Hermes uses its own frequency, not VFO) ──
    BK4819_SetFrequency(gHermesConfig.frequency);
    BK4819_PickRXFilterPathBasedOnFrequency(gHermesConfig.frequency);

    // ── 8. PA Enable + calibrated power ──
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, true);
    SYSTEM_DelayMs(5);

    uint8_t Band = FREQUENCY_GetBand(gHermesConfig.frequency);
    if (Band != BAND_NONE) {
        uint8_t Txp[3];
        Storage_ReadRecordIndexed(REC_CALIB_TX_POWER, (Band << 8) | gHermesConfig.tx_power, Txp, 0, 3);
        uint8_t bias = FREQUENCY_CalculateOutputPower(Txp[0], Txp[1], Txp[2],
            frequencyBandTable[Band].lower,
            (frequencyBandTable[Band].lower + frequencyBandTable[Band].upper) / 2,
            frequencyBandTable[Band].upper, gHermesConfig.frequency);
        BK4819_SetupPowerAmplifier(bias, gHermesConfig.frequency);
    }
    SYSTEM_DelayMs(10);

    // ── 9. Unmute transmitter (critical — matoz: BK4819_ExitTxMute()) ──
    BK4819_ExitTxMute();

    // ── 10. Full FSK modem config for TX (preamble=15, gain=0) ──
    HERMES_PHY_ConfigureFSK(false);

    // ── 11. PA stabilization delay ──
    SYSTEM_DelayMs(100);

    // ── 12. Load frame into TX FIFO ──
    for (uint16_t i = 0; i < len; i += 2) {
        uint16_t word = frame[i] | ((uint16_t)frame[i + 1] << 8);
        BK4819_WriteRegister(BK4819_REG_5F, word);
    }

    // ── 13. Enable FSK TX ──
    BK4819_WriteRegister(BK4819_REG_59, (1u << 11) | phy_reg59_base_tx);

    // ── 14. Poll for TX complete ──
    bool success = false;
    unsigned int timeout = 1000 / 5;  // up to 1 second

    while (timeout-- > 0) {
        SYSTEM_DelayMs(5);
        if (BK4819_ReadRegister(BK4819_REG_0C) & (1u << 0)) {
            BK4819_WriteRegister(BK4819_REG_02, 0);
            if (BK4819_ReadRegister(BK4819_REG_02) & BK4819_REG_02_FSK_TX_FINISHED) {
                success = true;
                break;
            }
        }
    }

    // ── 15. Post-TX tail delay ──
    SYSTEM_DelayMs(100);

    // ── 16. Restore radio state ──
    BK4819_WriteRegister(BK4819_REG_40, phy_saved.reg_40);
    BK4819_WriteRegister(BK4819_REG_2B, phy_saved.reg_2B);
    BK4819_WriteRegister(BK4819_REG_51, phy_saved.reg_51);

    // ── 17. Clean up TX, re-enable RX path ──
    BK4819_SetupPowerAmplifier(0, 0);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);

    // ── 18. Re-arm FSK RX (full reconfigure + enable) ──
    HERMES_PHY_StartRx();

    return success;
}

// ──── Read RX FIFO ────
// Called from ISR on FSK_FIFO_ALMOST_FULL.
// Reads available words from the hardware FIFO.
//
// kamil MSG_StorePacket():
//   count = BK4819_ReadRegister(REG_5E) & (7u << 0)
//   word = BK4819_ReadRegister(REG_5F)
//   buf[idx++] = (word >> 0) & 0xFF
//   buf[idx++] = (word >> 8) & 0xFF
//
// matoz MSG_StorePacket(): identical pattern
uint16_t HERMES_PHY_ReadFIFO(uint8_t *buf, uint16_t max_len, uint16_t current_len) {
    uint16_t count = BK4819_ReadRegister(BK4819_REG_5E) & (7u << 0);
    uint16_t bytes_read = 0;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t word = BK4819_ReadRegister(BK4819_REG_5F);
        if ((current_len + bytes_read) < max_len) buf[bytes_read++] = (word >> 0) & 0xFF;
        if ((current_len + bytes_read) < max_len) buf[bytes_read++] = (word >> 8) & 0xFF;
    }

    SYSTEM_DelayMs(10);
    return bytes_read;
}

// ──── Hardware Telemetry ────
int16_t HERMES_PHY_GetRSSI(void) {
    return BK4819_ReadRegister(BK4819_REG_67) & 0x01FF;
}

uint16_t HERMES_PHY_GetExNoise(void) {
    return BK4819_ReadRegister(BK4819_REG_65) & 0x007F;
}

uint16_t HERMES_PHY_GetGlitch(void) {
    return BK4819_ReadRegister(BK4819_REG_63) & 0x00FF;
}

#endif // ENABLE_MESH_NETWORK
