/* Hermes App — ACK Packets (RFC §12.7) */
#ifndef HERMES_ACK_H
#define HERMES_ACK_H

#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_ACK_HEALTH

#include <stdint.h>
#include <stdbool.h>

// ACK payload flags
#define HM_ACK_HAS_HEALTH    0x01
#define HM_ACK_HAS_LOCATION  0x02

typedef struct {
    uint8_t  acked_id[6];     // Packet ID being acknowledged
    uint8_t  acked_mac[8];    // Inner MAC of acknowledged packet
    uint8_t  status;          // 0=OK, 1=Error, etc.
    uint8_t  flags;           // HM_ACK_HAS_* bitmask
    // Optional health blob
    uint8_t  battery;
    uint8_t  lqi;
    int8_t   rssi;
    uint8_t  temperature;
} HermesAck_t;

// Build an ACK payload (returns bytes written)
uint8_t HERMES_ACK_Build(const HermesAck_t *ack, uint8_t out[56]);

// Parse ACK payload
bool HERMES_ACK_Parse(const uint8_t in[56], HermesAck_t *ack);

#endif // ENABLE_HERMES_ACK_HEALTH
#endif
#endif
