/* Hermes Data-Link — PN15 Whitening (RFC §3.2) */
#ifndef HERMES_WHITENING_H
#define HERMES_WHITENING_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>

// PN15 LFSR whitening/descrambling (symmetric operation)
// Polynomial: x^15 + x^14 + 1
// Seed derived from sync word
void HERMES_Whiten(uint8_t *data, uint16_t len, uint32_t sync_word);

#endif
#endif
