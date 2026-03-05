/* Hermes Network — Controlled Flooding (RFC §6.2)
 *
 * Deduplication via a 16-slot ring buffer of seen packet IDs.
 * SNR-weighted backoff: higher RSSI → shorter delay → better-positioned
 * nodes rebroadcast first, suppressing further copies.
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_ROUTER

#include "apps/hermes/network/routing.h"
#include "apps/hermes/transport/csma.h"
#include "apps/hermes/datalink/framing.h"
#include <string.h>

// ──── Forwarding Queue ────
#define HM_FWD_QUEUE_SIZE 2
static struct {
    HermesDataBlock_t block;
    uint32_t          sync_word;
    uint32_t          transmit_at;
    bool              active;
} fwd_queue[HM_FWD_QUEUE_SIZE];


// ──── Dedup ring buffer ────
static struct {
    uint8_t id[HM_PACKET_ID_SIZE];
} dedup_cache[HM_DEDUP_SLOTS];
static uint8_t dedup_head = 0;
static uint8_t dedup_count = 0;

void HERMES_Route_Init(void) {
    memset(dedup_cache, 0, sizeof(dedup_cache));
    dedup_head = 0;
    dedup_count = 0;
    memset(fwd_queue, 0, sizeof(fwd_queue));
}

bool HERMES_Route_IsDuplicate(const uint8_t packet_id[6]) {
    uint8_t n = (dedup_count < HM_DEDUP_SLOTS) ? dedup_count : HM_DEDUP_SLOTS;
    for (uint8_t i = 0; i < n; i++) {
        if (memcmp(dedup_cache[i].id, packet_id, HM_PACKET_ID_SIZE) == 0)
            return true;
    }
    return false;
}

void HERMES_Route_Record(const uint8_t packet_id[6]) {
    memcpy(dedup_cache[dedup_head].id, packet_id, HM_PACKET_ID_SIZE);
    dedup_head = (dedup_head + 1) % HM_DEDUP_SLOTS;
    if (dedup_count < HM_DEDUP_SLOTS) dedup_count++;
}

bool HERMES_Route_ShouldForward(const HermesHeader_t *hdr, const uint8_t our_id[6]) {
    if (!hdr) return false;

    // Don't forward duplicates
    if (HERMES_Route_IsDuplicate(hdr->packet_id)) return false;

    // Don't forward if TTL exhausted
    if (HM_HDR_TTL(hdr) == 0) return false;

    // Don't forward unicast packets not addressed to us (we relay, not consume)
    uint8_t mode = HM_HDR_AddrMode(hdr);
    if (mode == HM_ADDR_UNICAST) {
        // Only forward if we're NOT the destination (someone else is)
        if (HM_IsOurAddress(hdr->dest, our_id)) return false; // we consume, not fwd
    }

    // Don't forward discovery packets (TTL=1 by design)
    if (HM_HDR_Type(hdr) == HM_TYPE_DISCOVERY) return false;

    return true;
}

uint8_t HERMES_Route_DecrementTTL(HermesHeader_t *hdr) {
    uint8_t ttl = HM_HDR_TTL(hdr);
    if (ttl > 0) {
        ttl--;
        HM_HDR_SetTTL(hdr, ttl);
    }
    return ttl;
}

uint16_t HERMES_Route_CalcBackoff(int16_t rssi_dbm) {
    // RFC §6.2: better SNR → shorter delay
    // Map RSSI from [-130, -50] to delay [200ms, 20ms]
    // Clamp RSSI
    if (rssi_dbm < -130) rssi_dbm = -130;
    if (rssi_dbm > -50)  rssi_dbm = -50;

    // Linear map: delay_ms = 200 - (rssi + 130) * 180 / 80
    int16_t range = rssi_dbm + 130; // 0..80
    uint16_t delay = (uint16_t)(200 - (range * 180) / 80);

    // Add small jitter (use RSSI lower bits as poor-man's random)
    uint8_t jitter = ((uint8_t)rssi_dbm ^ 0x5A) & 0x1F; // 0-31ms
    delay += jitter;

    if (delay < HM_BACKOFF_MIN_MS) delay = HM_BACKOFF_MIN_MS;
    if (delay > HM_BACKOFF_MAX_MS + 31) delay = HM_BACKOFF_MAX_MS + 31;

    return delay;
}

bool HERMES_Route_QueueForward(const HermesDataBlock_t *block, uint32_t sync_word, uint16_t delay_ms) {
    if (!block) return false;

    // Find empty slot
    for (uint8_t i = 0; i < HM_FWD_QUEUE_SIZE; i++) {
        if (!fwd_queue[i].active) {
            memcpy(&fwd_queue[i].block, block, sizeof(HermesDataBlock_t));
            fwd_queue[i].sync_word = sync_word;
            fwd_queue[i].transmit_at = delay_ms; // Using relative tick down strategy since uptime wrap is complex
            fwd_queue[i].active = true;
            return true;
        }
    }
    return false; // Queue full
}

void HERMES_Route_Tick(uint32_t now_ms) {
    // Note: To avoid tracking global tick times cleanly across wrapping, we decrement relative counters.
    // Call this roughly every 10ms.
    for (uint8_t i = 0; i < HM_FWD_QUEUE_SIZE; i++) {
        if (fwd_queue[i].active) {
            if (fwd_queue[i].transmit_at <= 10) {
                fwd_queue[i].active = false;
                
                HermesFrame_t frame;
                if (HERMES_Frame_Pack(&fwd_queue[i].block, &frame, fwd_queue[i].sync_word)) {
                    HERMES_CSMA_Transmit(frame.raw, HM_FRAME_SIZE);
                }
            } else {
                fwd_queue[i].transmit_at -= 10;
            }
        }
    }
}

#endif // ENABLE_HERMES_ROUTER
#endif // ENABLE_MESH_NETWORK
