/* Hermes App — ACK Packets (RFC §12.7)
 *
 * Layout:
 * [0..5]   acked_packet_id (6 bytes)
 * [6..13]  acked_inner_mac (8 bytes)
 * [14]     status
 * [15]     flags (HM_ACK_HAS_*)
 * [16..]   optional health blob (battery, lqi, rssi, temperature)
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_ACK_HEALTH

#include "apps/hermes/app/ack.h"
#include <string.h>

uint8_t HERMES_ACK_Build(const HermesAck_t *ack, uint8_t out[56]) {
    if (!ack || !out) return 0;
    memset(out, 0, 56);

    memcpy(out, ack->acked_id, 6);
    memcpy(out + 6, ack->acked_mac, 8);
    out[14] = ack->status;
    out[15] = ack->flags;

    uint8_t pos = 16;
    if (ack->flags & HM_ACK_HAS_HEALTH) {
        out[pos++] = ack->battery;
        out[pos++] = ack->lqi;
        out[pos++] = (uint8_t)ack->rssi;
        out[pos++] = ack->temperature;
    }

    return pos;
}

bool HERMES_ACK_Parse(const uint8_t in[56], HermesAck_t *ack) {
    if (!in || !ack) return false;
    memset(ack, 0, sizeof(HermesAck_t));

    memcpy(ack->acked_id, in, 6);
    memcpy(ack->acked_mac, in + 6, 8);
    ack->status = in[14];
    ack->flags = in[15];

    uint8_t pos = 16;
    if (ack->flags & HM_ACK_HAS_HEALTH) {
        ack->battery = in[pos++];
        ack->lqi = in[pos++];
        ack->rssi = (int8_t)in[pos++];
        ack->temperature = in[pos++];
    }

    return true;
}

#endif // ENABLE_HERMES_ACK_HEALTH
#endif // ENABLE_MESH_NETWORK
