/* Hermes Transport — CSMA/CA (RFC §9) */
#ifndef HERMES_CSMA_H
#define HERMES_CSMA_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    HM_PRIO_CRITICAL = 0, // ACKs (CW divisor = 4)
    HM_PRIO_NORMAL   = 1, // Unicast (CW divisor = 1)
    HM_PRIO_LOW      = 2  // Broadcast (CW multiplier = 2)
} Hermes_Priority_t;

// Check if the channel is clear (RSSI below threshold and no preamble)
bool HERMES_CSMA_IsChannelClear(void);

// Full CSMA/CA transmit: CCA + exponential backoff + transmit
// Returns true if transmitted successfully
bool HERMES_CSMA_Transmit(const uint8_t *frame, uint16_t len, Hermes_Priority_t priority);

#endif
#endif
