/* Hermes Transport — Stop-and-Wait ARQ (RFC §8.2) */
#ifndef HERMES_RELIABILITY_H
#define HERMES_RELIABILITY_H

#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_MESSENGER

#include <stdint.h>
#include <stdbool.h>
#include "apps/hermes/hermes_types.h"

// Initialize ARQ state
void HERMES_ARQ_Init(void);

// Queue a packet for reliable delivery (starts ACK timer)
bool HERMES_ARQ_Send(const HermesDataBlock_t *block, uint32_t sync_word);

// Handle incoming ACK matching a pending packet
bool HERMES_ARQ_HandleAck(const uint8_t packet_id[6]);

// Tick function — call periodically to check timeouts and trigger retries
// Returns true if a retry was sent
bool HERMES_ARQ_Tick(uint32_t now_ms);

// Check if we have a TX pending ACK
bool HERMES_ARQ_IsPending(void);

#endif // ENABLE_HERMES_MESSENGER
#endif
#endif
