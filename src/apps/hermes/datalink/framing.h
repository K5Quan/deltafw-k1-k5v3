/* Hermes Data-Link — Frame Packing (RFC §3.1) */
#ifndef HERMES_FRAMING_H
#define HERMES_FRAMING_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>
#include "apps/hermes/hermes_types.h"

// Pack a 96-byte data block into a 128-byte frame (adds RS parity)
bool HERMES_Frame_Pack(const HermesDataBlock_t *block, HermesFrame_t *frame, uint32_t sync_word);

// Unpack a 128-byte frame into a data block (RS decode + dewhiten)
// Returns true if FEC succeeded
bool HERMES_Frame_Unpack(const HermesFrame_t *frame, HermesDataBlock_t *block, uint32_t sync_word);

#endif
#endif
