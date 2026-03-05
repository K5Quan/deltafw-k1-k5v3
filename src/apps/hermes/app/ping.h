/* Hermes App — Ping (RFC §12.5) */
#ifndef HERMES_PING_H
#define HERMES_PING_H

#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_PING

#include <stdint.h>
#include <stdbool.h>

#define HM_PING_MAX_HOPS 9  // 9 × 6 = 54 bytes (fits in 56B payload)

// Build a PING payload with our node ID in the first hop slot
void HERMES_PING_Init(uint8_t payload[56], const uint8_t our_id[6]);

// Process a relay: insert our address into the next empty hop slot
// Returns true if successfully inserted (false if hop array full)
bool HERMES_PING_InsertHop(uint8_t payload[56], const uint8_t our_id[6]);

// Get the number of hops recorded in a ping payload
uint8_t HERMES_PING_GetHopCount(const uint8_t payload[56]);

// Get a specific hop address from the payload
void HERMES_PING_GetHop(const uint8_t payload[56], uint8_t index, uint8_t out[6]);

#endif // ENABLE_HERMES_PING
#endif
#endif
