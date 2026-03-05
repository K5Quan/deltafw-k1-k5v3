/* Hermes Security — AEAD Crypto Engine (RFC §11.1) */
#ifndef HERMES_CRYPTO_ENGINE_H
#define HERMES_CRYPTO_ENGINE_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>

// AEAD encrypt: ChaCha20-Poly1305
// Encrypts data in-place, writes 8-byte MAC to mac_out
void HERMES_Crypto_Encrypt(const uint8_t key[32], const uint8_t nonce[12],
                           uint8_t *data, uint16_t len,
                           uint8_t mac_out[8]);

// AEAD decrypt: ChaCha20-Poly1305
// Decrypts data in-place, verifies MAC. Returns true if authentic.
bool HERMES_Crypto_Decrypt(const uint8_t key[32], const uint8_t nonce[12],
                           uint8_t *data, uint16_t len,
                           const uint8_t mac[8]);

#endif
#endif
