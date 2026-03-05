/* Hermes Network — Fragmentation (RFC §6.3) */
#ifndef HERMES_FRAGMENTATION_H
#define HERMES_FRAGMENTATION_H

#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_ROUTER

#include <stdint.h>
#include <stdbool.h>
#include "apps/hermes/hermes_types.h"

// Fragment a large payload (up to 896B) into multiple 56-byte chunks.
// Sets frag_index and last_frag in each header.
// Returns number of fragments.
uint8_t HERMES_Frag_Split(const uint8_t *data, uint16_t len,
                          uint8_t out_payloads[][HM_PAYLOAD_SIZE],
                          uint8_t max_frags);

// Reassembly state for a single multi-fragment session
typedef struct {
    uint8_t  packet_id[HM_PACKET_ID_SIZE];
    uint8_t  received_mask;   // bitmask of received fragments (up to 8)
    uint8_t  total_frags;
    uint8_t  data[HM_PAYLOAD_SIZE * 8]; // up to 448B reassembly buffer (trimmed for RAM)
    bool     active;
} HermesReassembly_t;

// Process arrival of a fragment. Returns true when reassembly complete.
bool HERMES_Frag_Receive(HermesReassembly_t *ctx,
                         uint8_t frag_idx, bool last_frag,
                         const uint8_t *packet_id,
                         const uint8_t *payload);

// Get reassembled data (valid after Receive returns true)
uint16_t HERMES_Frag_GetData(const HermesReassembly_t *ctx, uint8_t *out, uint16_t max_len);

// Reset reassembly context
void HERMES_Frag_Reset(HermesReassembly_t *ctx);

#endif // ENABLE_HERMES_ROUTER
#endif // ENABLE_MESH_NETWORK
#endif
