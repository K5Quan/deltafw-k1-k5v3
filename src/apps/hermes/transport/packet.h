/* Hermes Transport — Packet Header Manipulation (RFC §5) */
#ifndef HERMES_PACKET_H
#define HERMES_PACKET_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>
#include "apps/hermes/hermes_types.h"

// Build a new header with given parameters
void HERMES_Pkt_Build(HermesHeader_t *hdr,
                      HermesPacketType_t type,
                      HermesAddrMode_t addr_mode,
                      uint8_t ttl,
                      bool want_ack,
                      const uint8_t dest[6],
                      const uint8_t src[6]);

// Generate a random 48-bit packet ID
void HERMES_Pkt_NewID(uint8_t out[6]);

// Generate a random 32-bit hop nonce
void HERMES_Pkt_NewHopNonce(uint8_t out[4]);

#endif
#endif
