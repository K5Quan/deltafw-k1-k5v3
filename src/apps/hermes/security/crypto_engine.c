/* Hermes Security — AEAD Crypto Engine (RFC §11.1)
 *
 * Wraps the existing helper/chacha20 + helper/poly1305 libraries
 * for Hermes AEAD operations.
 *
 * Uses truncated 8-byte MAC (from full 16-byte Poly1305 tag)
 * to fit within the frame budget.
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/security/crypto_engine.h"
#include <string.h>

#ifdef ENABLE_CRYPTO
#include "helper/chacha20.h"
#include "helper/poly1305.h"

void HERMES_Crypto_Encrypt(const uint8_t key[32], const uint8_t nonce[12],
                           uint8_t *data, uint16_t len,
                           uint8_t mac_out[8]) {
    if (!key || !nonce || !data || !mac_out) return;

    // ChaCha20 encrypt in-place
    chacha20_ctx ctx;
    chacha20_init(&ctx, key, nonce, 1);
    chacha20_encrypt(&ctx, data, data, len);

    // Poly1305 MAC over ciphertext
    // Build Poly1305 key from ChaCha20 block 0
    uint8_t poly_key[32];
    uint8_t zeros[32] = {0};
    chacha20_ctx key_ctx;
    chacha20_init(&key_ctx, key, nonce, 0);
    chacha20_encrypt(&key_ctx, zeros, poly_key, 32);

    uint8_t full_mac[16];
    poly1305_context pctx;
    poly1305_init(&pctx, poly_key);
    poly1305_update(&pctx, data, len);
    poly1305_finish(&pctx, full_mac);

    // Truncate to 8 bytes
    memcpy(mac_out, full_mac, 8);
}

bool HERMES_Crypto_Decrypt(const uint8_t key[32], const uint8_t nonce[12],
                           uint8_t *data, uint16_t len,
                           const uint8_t mac[8]) {
    if (!key || !nonce || !data || !mac) return false;

    // Compute MAC over ciphertext first
    uint8_t poly_key[32];
    uint8_t zeros[32] = {0};
    chacha20_ctx key_ctx;
    chacha20_init(&key_ctx, key, nonce, 0);
    chacha20_encrypt(&key_ctx, zeros, poly_key, 32);

    uint8_t full_mac[16];
    poly1305_context pctx;
    poly1305_init(&pctx, poly_key);
    poly1305_update(&pctx, data, len);
    poly1305_finish(&pctx, full_mac);

    // Verify truncated MAC
    if (memcmp(full_mac, mac, 8) != 0) return false;

    // Decrypt in-place
    chacha20_ctx ctx;
    chacha20_init(&ctx, key, nonce, 1);
    chacha20_encrypt(&ctx, data, data, len);
    return true;
}

#else
// Stub when CRYPTO is disabled — pass-through, no encryption
void HERMES_Crypto_Encrypt(const uint8_t key[32], const uint8_t nonce[12],
                           uint8_t *data, uint16_t len,
                           uint8_t mac_out[8]) {
    (void)key; (void)nonce; (void)data; (void)len;
    if (mac_out) memset(mac_out, 0, 8);
}

bool HERMES_Crypto_Decrypt(const uint8_t key[32], const uint8_t nonce[12],
                           uint8_t *data, uint16_t len,
                           const uint8_t mac[8]) {
    (void)key; (void)nonce; (void)data; (void)len; (void)mac;
    return true;
}
#endif // ENABLE_CRYPTO

#endif // ENABLE_MESH_NETWORK
