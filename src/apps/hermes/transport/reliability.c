/* Hermes Transport — Stop-and-Wait ARQ (RFC §8.2)
 *
 * Single outstanding packet. 1500ms ACK timeout, 3 retries.
 * After final timeout → mark as failed.
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_MESSENGER

#include "apps/hermes/transport/reliability.h"
#include "apps/hermes/transport/csma.h"
#include "apps/hermes/datalink/framing.h"
#include <string.h>

// ──── Pending TX state ────
static struct {
    HermesDataBlock_t block;
    uint32_t          sync_word;
    uint8_t           packet_id[HM_PACKET_ID_SIZE];
    uint32_t          timeout_at;
    uint8_t           retries;
    bool              active;
} arq;

void HERMES_ARQ_Init(void) {
    memset(&arq, 0, sizeof(arq));
}

bool HERMES_ARQ_Send(const HermesDataBlock_t *block, uint32_t sync_word) {
    if (!block || arq.active) return false;

    memcpy(&arq.block, block, sizeof(HermesDataBlock_t));
    arq.sync_word = sync_word;
    memcpy(arq.packet_id, block->header.packet_id, HM_PACKET_ID_SIZE);
    arq.retries = 0;
    arq.active = true;
    // Timeout will be set after actual transmission in Tick
    arq.timeout_at = 0; // Will be set on first TX

    // Perform first transmission
    HermesFrame_t frame;
    if (!HERMES_Frame_Pack(&arq.block, &frame, arq.sync_word)) {
        arq.active = false;
        return false;
    }

    return HERMES_CSMA_Transmit(frame.raw, HM_FRAME_SIZE);
}

bool HERMES_ARQ_HandleAck(const uint8_t packet_id[6]) {
    if (!arq.active) return false;
    if (memcmp(arq.packet_id, packet_id, HM_PACKET_ID_SIZE) != 0) return false;

    arq.active = false;
    return true; // ACK matched
}

bool HERMES_ARQ_Tick(uint32_t now_ms) {
    if (!arq.active) return false;

    // Set initial timeout after first TX
    if (arq.timeout_at == 0) {
        arq.timeout_at = now_ms + HM_ACK_TIMEOUT_MS;
        return false;
    }

    // Not timed out yet
    if (now_ms < arq.timeout_at) return false;

    // Timeout expired
    arq.retries++;
    if (arq.retries > HM_MAX_RETRIES) {
        arq.active = false;
        return false; // Give up
    }

    // Retry
    HermesFrame_t frame;
    if (HERMES_Frame_Pack(&arq.block, &frame, arq.sync_word)) {
        HERMES_CSMA_Transmit(frame.raw, HM_FRAME_SIZE);
    }

    arq.timeout_at = now_ms + HM_ACK_TIMEOUT_MS;
    return true;
}

bool HERMES_ARQ_IsPending(void) {
    return arq.active;
}

#endif // ENABLE_HERMES_MESSENGER
#endif // ENABLE_MESH_NETWORK
