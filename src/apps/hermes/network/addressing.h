/* Hermes Network — Base40 Addressing (RFC §6.1) */
#ifndef HERMES_ADDRESSING_H
#define HERMES_ADDRESSING_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>

// Encode a callsign string (up to 9 chars) into a 6-byte (48-bit) Base40 address
void HERMES_Addr_Encode(const char *callsign, uint8_t out[6]);

// Decode a 6-byte Base40 address back to a string (max 10 chars including NUL)
void HERMES_Addr_Decode(const uint8_t addr[6], char *out, uint8_t max_len);

#endif
#endif
