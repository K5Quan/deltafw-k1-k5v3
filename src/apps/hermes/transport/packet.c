/* Hermes Transport — Packet Header Builder (RFC §5) */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/transport/packet.h"
#include "helper/trng.h"
#include <string.h>

void HERMES_Pkt_Build(HermesHeader_t *hdr,
                      HermesPacketType_t type,
                      HermesAddrMode_t addr_mode,
                      uint8_t ttl,
                      bool want_ack,
                      const uint8_t dest[6],
                      const uint8_t src[6]) {
    if (!hdr) return;
    memset(hdr, 0, sizeof(HermesHeader_t));

    HM_HDR_SetType(hdr, (uint8_t)type);
    HM_HDR_SetTTL(hdr, ttl & 0x0F);
    HM_HDR_SetAddrMode(hdr, (uint8_t)addr_mode);
    HM_HDR_SetWantAck(hdr, want_ack);
    HM_HDR_SetFragIdx(hdr, 0);
    HM_HDR_SetLastFrag(hdr, true); // Single-fragment by default

    // Generate random packet ID using hardware TRNG
    HERMES_Pkt_NewID(hdr->packet_id);

    // Copy addresses
    if (dest) memcpy(hdr->dest, dest, HM_NODE_ID_SIZE);
    if (src)  memcpy(hdr->src, src, HM_NODE_ID_SIZE);

    // Generate hop nonce
    HERMES_Pkt_NewHopNonce(hdr->hop_nonce);
}

void HERMES_Pkt_NewID(uint8_t out[6]) {
    // Generate 48 bits (6 bytes) of TRNG
    TRNG_Fill(out, 6);
}

void HERMES_Pkt_NewHopNonce(uint8_t out[4]) {
    uint32_t n = TRNG_GetU32();
    out[0] = (uint8_t)(n >> 0);
    out[1] = (uint8_t)(n >> 8);
    out[2] = (uint8_t)(n >> 16);
    out[3] = (uint8_t)(n >> 24);
}

#endif // ENABLE_MESH_NETWORK
