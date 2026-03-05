/* Hermes App — Telemetry Packing (RFC §12.4)
 *
 * Payload layout:
 * [0..16]  tag (17 bytes)
 * [17..20] uptime (4 bytes, little-endian)
 * [21]     flags (HM_TEL_HAS_*)
 * [22]     battery (conditional)
 * [23..31] location (conditional: lat4 + lon4 + alt2 = 10 bytes)
 *
 * Total fixed: 22 bytes. Optional blobs appended per flags.
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_TELEMETRY

#include "apps/hermes/app/telemetry.h"
#include <string.h>

uint8_t HERMES_TEL_Pack(const HermesTelemetry_t *tel, uint8_t out[56]) {
    if (!tel || !out) return 0;
    memset(out, 0, 56);

    // Tag (17 bytes)
    memcpy(out, tel->tag, 17);

    // Uptime (LE)
    out[17] = (uint8_t)(tel->uptime >> 0);
    out[18] = (uint8_t)(tel->uptime >> 8);
    out[19] = (uint8_t)(tel->uptime >> 16);
    out[20] = (uint8_t)(tel->uptime >> 24);

    // Flags
    out[21] = tel->flags;

    uint8_t pos = 22;

    // Battery blob
    if (tel->flags & HM_TEL_HAS_BATTERY) {
        out[pos++] = tel->battery;
    }

    // Location blob (10 bytes)
    if (tel->flags & HM_TEL_HAS_LOCATION) {
        out[pos++] = (uint8_t)(tel->lat >> 0);
        out[pos++] = (uint8_t)(tel->lat >> 8);
        out[pos++] = (uint8_t)(tel->lat >> 16);
        out[pos++] = (uint8_t)(tel->lat >> 24);
        out[pos++] = (uint8_t)(tel->lon >> 0);
        out[pos++] = (uint8_t)(tel->lon >> 8);
        out[pos++] = (uint8_t)(tel->lon >> 16);
        out[pos++] = (uint8_t)(tel->lon >> 24);
        out[pos++] = (uint8_t)(tel->alt >> 0);
        out[pos++] = (uint8_t)(tel->alt >> 8);
    }

    return pos;
}

bool HERMES_TEL_Unpack(const uint8_t in[56], HermesTelemetry_t *tel) {
    if (!in || !tel) return false;
    memset(tel, 0, sizeof(HermesTelemetry_t));

    memcpy(tel->tag, in, 17);
    tel->uptime = (uint32_t)in[17] | ((uint32_t)in[18] << 8) |
                  ((uint32_t)in[19] << 16) | ((uint32_t)in[20] << 24);
    tel->flags = in[21];

    uint8_t pos = 22;

    if (tel->flags & HM_TEL_HAS_BATTERY) {
        tel->battery = in[pos++];
    }

    if (tel->flags & HM_TEL_HAS_LOCATION) {
        tel->lat = (int32_t)((uint32_t)in[pos] | ((uint32_t)in[pos+1] << 8) |
                   ((uint32_t)in[pos+2] << 16) | ((uint32_t)in[pos+3] << 24));
        pos += 4;
        tel->lon = (int32_t)((uint32_t)in[pos] | ((uint32_t)in[pos+1] << 8) |
                   ((uint32_t)in[pos+2] << 16) | ((uint32_t)in[pos+3] << 24));
        pos += 4;
        tel->alt = (int16_t)((uint16_t)in[pos] | ((uint16_t)in[pos+1] << 8));
    }

    return true;
}

#endif // ENABLE_HERMES_TELEMETRY
#endif // ENABLE_MESH_NETWORK
