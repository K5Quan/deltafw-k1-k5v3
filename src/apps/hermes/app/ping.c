/* Hermes App — Ping Hop Tracing (RFC §12.5)
 *
 * PING payload: 9 × 6-byte hop slots = 54 bytes.
 * Each relay node inserts its address in the next empty slot.
 * Empty slots are all-zeros.
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_PING

#include "apps/hermes/app/ping.h"
#include <string.h>

static const uint8_t EMPTY_HOP[6] = {0,0,0,0,0,0};

void HERMES_PING_Init(uint8_t payload[56], const uint8_t our_id[6]) {
    memset(payload, 0, 56);
    // First hop = originator
    memcpy(payload, our_id, 6);
}

bool HERMES_PING_InsertHop(uint8_t payload[56], const uint8_t our_id[6]) {
    for (uint8_t i = 0; i < HM_PING_MAX_HOPS; i++) {
        uint8_t *slot = payload + (i * 6);
        if (memcmp(slot, EMPTY_HOP, 6) == 0) {
            memcpy(slot, our_id, 6);
            return true;
        }
    }
    return false; // All slots full
}

uint8_t HERMES_PING_GetHopCount(const uint8_t payload[56]) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < HM_PING_MAX_HOPS; i++) {
        if (memcmp(payload + (i * 6), EMPTY_HOP, 6) != 0) count++;
        else break;
    }
    return count;
}

void HERMES_PING_GetHop(const uint8_t payload[56], uint8_t index, uint8_t out[6]) {
    if (index < HM_PING_MAX_HOPS) {
        memcpy(out, payload + (index * 6), 6);
    } else {
        memset(out, 0, 6);
    }
}

#endif // ENABLE_HERMES_PING
#endif // ENABLE_MESH_NETWORK
