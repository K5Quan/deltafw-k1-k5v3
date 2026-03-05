/* Hermes Data-Link — Reed-Solomon FEC (RFC §3.3) */
#ifndef HERMES_FEC_H
#define HERMES_FEC_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>

// RS(128,96) over GF(2^8)
// Encode: block[0..95] → block[96..127] filled with 32 parity bytes
void HERMES_FEC_Encode(uint8_t block[128]);

// Decode in-place. Returns erasure count (0 = clean, >0 = corrected, <0 = uncorrectable)
int8_t HERMES_FEC_Decode(uint8_t block[128]);

#endif
#endif
