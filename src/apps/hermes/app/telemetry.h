/* Hermes App — Telemetry (RFC §12.4) */
#ifndef HERMES_TELEMETRY_H
#define HERMES_TELEMETRY_H

#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_TELEMETRY

#include <stdint.h>
#include <stdbool.h>

// Telemetry flags (which optional blobs are present)
#define HM_TEL_HAS_BATTERY   0x01
#define HM_TEL_HAS_LOCATION  0x02
#define HM_TEL_HAS_SENSORS   0x04

// Telemetry data structure
typedef struct {
    char     tag[17];       // 17-byte tag/callsign
    uint32_t uptime;        // Seconds since boot
    uint8_t  flags;         // HM_TEL_HAS_* bitmask
    uint8_t  battery;       // 0.1V steps, bit7 = has battery
    int32_t  lat;           // Latitude  × 1e7
    int32_t  lon;           // Longitude × 1e7
    int16_t  alt;           // Altitude in meters
} HermesTelemetry_t;

// Pack telemetry into a 56-byte payload
uint8_t HERMES_TEL_Pack(const HermesTelemetry_t *tel, uint8_t out[56]);

// Unpack a 56-byte payload into telemetry
bool HERMES_TEL_Unpack(const uint8_t in[56], HermesTelemetry_t *tel);

#endif // ENABLE_HERMES_TELEMETRY
#endif
#endif
