/* Hermes Transport — CSMA/CA (RFC §9) */
#ifndef HERMES_CSMA_H
#define HERMES_CSMA_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>

// Check if the channel is clear (RSSI below threshold)
bool HERMES_CSMA_IsChannelClear(void);

// Full CSMA/CA transmit: CCA + exponential backoff + transmit
// Returns true if transmitted successfully
bool HERMES_CSMA_Transmit(const uint8_t *frame, uint16_t len);

#endif
#endif
