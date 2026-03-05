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

// Check if a 6-byte address looks like valid Base40 (all decoded chars are printable Base40 chars)
bool HERMES_Addr_IsBase40(const uint8_t addr[6]);

// Format a 6-byte address as MAC "XX:XX:XX:XX:XX:XX" (needs 18-byte buffer)
void HERMES_Addr_FormatMAC(const uint8_t addr[6], char *out, uint8_t max_len);

// Smart format: Base40 callsign if valid, else MAC address
void HERMES_Addr_Format(const uint8_t addr[6], uint8_t mac_policy, char *out, uint8_t max_len);

#endif
#endif
