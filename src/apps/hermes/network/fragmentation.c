/* Hermes Network — Fragmentation (RFC §6.3)
 *
 * Split payloads > 56B into up to 8 fragments (448B max, trimmed from
 * RFC's 16/896B to save RAM on embedded targets).
 * Reassembly uses bitmask tracking.
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_ROUTER

#include "apps/hermes/network/fragmentation.h"
#include <string.h>

#define FRAG_MAX 8  // Trimmed from RFC's 16 to save RAM

uint8_t HERMES_Frag_Split(const uint8_t *data, uint16_t len,
                          uint8_t out_payloads[][HM_PAYLOAD_SIZE],
                          uint8_t max_frags) {
    if (!data || len == 0 || !out_payloads) return 0;
    if (max_frags > FRAG_MAX) max_frags = FRAG_MAX;

    uint8_t count = 0;
    uint16_t offset = 0;

    while (offset < len && count < max_frags) {
        uint16_t chunk = len - offset;
        if (chunk > HM_PAYLOAD_SIZE) chunk = HM_PAYLOAD_SIZE;

        memset(out_payloads[count], 0, HM_PAYLOAD_SIZE);
        memcpy(out_payloads[count], data + offset, chunk);

        offset += chunk;
        count++;
    }
    return count;
}

void HERMES_Frag_Reset(HermesReassembly_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(HermesReassembly_t));
}

bool HERMES_Frag_Receive(HermesReassembly_t *ctx,
                         uint8_t frag_idx, bool last_frag,
                         const uint8_t *packet_id,
                         const uint8_t *payload) {
    if (!ctx || !packet_id || !payload) return false;
    if (frag_idx >= FRAG_MAX) return false;

    // Start new session if needed
    if (!ctx->active || memcmp(ctx->packet_id, packet_id, HM_PACKET_ID_SIZE) != 0) {
        HERMES_Frag_Reset(ctx);
        memcpy(ctx->packet_id, packet_id, HM_PACKET_ID_SIZE);
        ctx->active = true;
    }

    // Store fragment
    memcpy(ctx->data + (frag_idx * HM_PAYLOAD_SIZE), payload, HM_PAYLOAD_SIZE);
    ctx->received_mask |= (1u << frag_idx);

    if (last_frag) {
        ctx->total_frags = frag_idx + 1;
    }

    // Check completeness
    if (ctx->total_frags > 0) {
        uint8_t expected = (1u << ctx->total_frags) - 1;
        if ((ctx->received_mask & expected) == expected) {
            return true; // All fragments received
        }
    }
    return false;
}

uint16_t HERMES_Frag_GetData(const HermesReassembly_t *ctx, uint8_t *out, uint16_t max_len) {
    if (!ctx || !out || !ctx->active || ctx->total_frags == 0) return 0;

    uint16_t total = (uint16_t)ctx->total_frags * HM_PAYLOAD_SIZE;
    if (total > max_len) total = max_len;

    memcpy(out, ctx->data, total);
    return total;
}

#endif // ENABLE_HERMES_ROUTER
#endif // ENABLE_MESH_NETWORK
