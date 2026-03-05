/* Hermes Security — Inner/Outer Encryption (RFC §3, §4) */
#ifndef HERMES_SEAL_H
#define HERMES_SEAL_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>
#include "apps/hermes/hermes_types.h"

// Inner Layer: encrypt Source(6B) + Payload(56B) using K_scope
void HERMES_Seal_EncryptInner(HermesDataBlock_t *block, const uint8_t k_scope[32]);

// Inner Layer: decrypt and verify inner MAC
bool HERMES_Seal_DecryptInner(HermesDataBlock_t *block, const uint8_t k_scope[32]);

// Outer Layer: obfuscate header(0-19) + inner frame(24-87) using K_net
void HERMES_Seal_ObfuscateOuter(HermesDataBlock_t *block, const uint8_t k_net[32],
                                 uint32_t sync_word, uint32_t frequency);

// Outer Layer: deobfuscate (symmetric — same function as obfuscate)
#define HERMES_Seal_DeobfuscateOuter HERMES_Seal_ObfuscateOuter

// RFC §3.2: Calculate Outer MAC over obfuscated bytes 0-87
void HERMES_Seal_CalculateOuterMAC(HermesDataBlock_t *block, const uint8_t k_net[32],
                                    uint32_t sync_word, uint32_t frequency);

// Verify Outer MAC
bool HERMES_Seal_VerifyOuterMAC(HermesDataBlock_t *block, const uint8_t k_net[32],
                                 uint32_t sync_word, uint32_t frequency);

#endif
#endif
