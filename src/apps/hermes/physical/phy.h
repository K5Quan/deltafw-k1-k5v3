/* Hermes Physical Layer — BK4819 FFSK1200 Modem
 * RFC §1–§4: Modulation, pulse shaping, preamble/sync, register map
 */
#ifndef HERMES_PHY_H
#define HERMES_PHY_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>

// Configure BK4819 for Hermes FFSK 1200/1800 mode
void HERMES_PHY_Init(const uint8_t sync_word[4]);

// Arm FIFO and enable RX FSK interrupts
void HERMES_PHY_StartRx(void);

// Disable FSK RX, clear FIFO
void HERMES_PHY_StopRx(void);

// Load 128-byte frame into TX FIFO and transmit (blocking until TX done or timeout)
bool HERMES_PHY_Transmit(const uint8_t *frame, uint16_t len);

// Read RX FIFO words into buffer, returns bytes read
uint16_t HERMES_PHY_ReadFIFO(uint8_t *buf, uint16_t max_len);

// Hardware telemetry reads
int16_t HERMES_PHY_GetRSSI(void);
uint16_t HERMES_PHY_GetExNoise(void);
uint16_t HERMES_PHY_GetGlitch(void);

// Save/restore radio state around FSK operations
void HERMES_PHY_SaveRadioState(void);
void HERMES_PHY_RestoreRadioState(void);

#endif // ENABLE_MESH_NETWORK
#endif // HERMES_PHY_H
