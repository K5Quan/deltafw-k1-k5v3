/* Hermes Network — Controlled Flooding & Dedup (RFC §6.2) */
#ifndef HERMES_ROUTING_H
#define HERMES_ROUTING_H

#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_ROUTER

#include <stdint.h>
#include <stdbool.h>
#include "apps/hermes/hermes_types.h"

// Initialize routing state (clear dedup cache)
void HERMES_Route_Init(void);

// Check if we have already seen this packet (ring buffer dedup)
bool HERMES_Route_IsDuplicate(const uint8_t packet_id[6]);

// Record a packet ID in the dedup cache
void HERMES_Route_Record(const uint8_t packet_id[6]);

// Decide whether to forward a received packet
// Checks: not duplicate, TTL > 0, not unicast-for-us
bool HERMES_Route_ShouldForward(const HermesHeader_t *hdr, const uint8_t our_id[6]);

// Decrement TTL, returns new value (0 = do not forward)
uint8_t HERMES_Route_DecrementTTL(HermesHeader_t *hdr);

// Calculate SNR-weighted rebroadcast delay in ms
// Better signal → shorter delay, giving better-positioned nodes priority
uint16_t HERMES_Route_CalcBackoff(int16_t rssi_dbm);

// Queue a packet for SNR-weighted delayed forwarding
bool HERMES_Route_QueueForward(const HermesDataBlock_t *block, uint32_t sync_word, uint16_t delay_ms);

// Process delayed forwarding queue
void HERMES_Route_Tick(uint32_t now_ms);

#endif // ENABLE_HERMES_ROUTER
#endif // ENABLE_MESH_NETWORK
#endif
