/* Hermes Physical Layer — BK4819 FFSK1200 Modem
 * RFC §1–§4: Modulation, pulse shaping, preamble/sync, register map
 *
 * Re-implemented from scratch per kamil/matoz FSK1200 reference.
 */
#ifndef HERMES_PHY_H
#define HERMES_PHY_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>

// Configure BK4819 for Hermes FFSK 1200/1800 mode and arm RX.
// Reads sync word from gHermesConfig.sync_word.
void HERMES_PHY_Init(void);

// Re-arm RX: clear FIFOs + enable RX (lightweight, called from ISR on rx_finished)
void HERMES_PHY_StartRx(void);

// Disable FSK modem entirely (REG_70=0, REG_58=0)
void HERMES_PHY_StopRx(void);

// Full TX sequence: save state → configure → PA → FIFO load → TX → restore
// Handles radio state save/restore internally (caller must NOT double-save).
bool HERMES_PHY_Transmit(const uint8_t *frame, uint16_t len);

// Read RX FIFO words into buffer, returns bytes read
uint16_t HERMES_PHY_ReadFIFO(uint8_t *buf, uint16_t max_len, uint16_t current_len);

// Hardware telemetry reads
int16_t HERMES_PHY_GetRSSI(void);
uint16_t HERMES_PHY_GetExNoise(void);
uint16_t HERMES_PHY_GetGlitch(void);

#endif // ENABLE_MESH_NETWORK
#endif // HERMES_PHY_H
