/* Hermes Security — Inner/Outer Encryption Pipeline (RFC §3, §4)
 *
 * Inner Layer (E2E): Encrypt Source(6B) + Payload(56B) → InnerMAC(8B)
 * Outer Layer (Hop): XOR obfuscation using K_net + HopNonce||SyncWord||Freq
 *
 * TX: EncryptInner → ObfuscateOuter → CalculateOuterMAC → [frame]
 * RX: [frame] → VerifyOuterMAC → DeobfuscateOuter → DecryptInner
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/security/seal.h"
#include "apps/hermes/security/crypto_engine.h"
#include "helper/chacha20.h"
#include "helper/poly1305.h"
#include <string.h>

// RFC §1.4: 96-bit (12-byte) Packet Nonce for Inner AEAD
// Inner Nonce = PacketID(6B) || Destination(6B) = 12B
static void build_inner_nonce(const HermesDataBlock_t *block, uint8_t nonce[12]) {
    memcpy(nonce, block->header.packet_id, HM_PACKET_ID_SIZE);
    memcpy(nonce + HM_PACKET_ID_SIZE, block->header.dest, HM_NODE_ID_SIZE);
}

// RFC §3.2.2: 12-byte Outer Nonce = HopNonce(4B) || SyncWord(4B) || Frequency(4B)
static void build_outer_nonce(const HermesDataBlock_t *block, uint32_t sync_word, uint32_t frequency, uint8_t nonce[12]) {
    memcpy(nonce, block->header.hop_nonce, 4);
    nonce[4] = (uint8_t)(sync_word >> 0);
    nonce[5] = (uint8_t)(sync_word >> 8);
    nonce[6] = (uint8_t)(sync_word >> 16);
    nonce[7] = (uint8_t)(sync_word >> 24);
    nonce[8]  = (uint8_t)(frequency >> 0);
    nonce[9]  = (uint8_t)(frequency >> 8);
    nonce[10] = (uint8_t)(frequency >> 16);
    nonce[11] = (uint8_t)(frequency >> 24);
}

void HERMES_Seal_EncryptInner(HermesDataBlock_t *block, const uint8_t k_scope[32]) {
    if (!block || !k_scope) return;

    uint8_t nonce[12];
    build_inner_nonce(block, nonce);

    // RFC §4.1: Inner AEAD encrypts Source(6B) + Payload(56B) = 62 bytes
    uint8_t temp[62];
    memcpy(temp, block->header.src, 6);
    memcpy(temp + 6, block->payload, 56);

    HERMES_Crypto_Encrypt(k_scope, nonce, temp, 62, block->inner_mac);

    // Write back encrypted Source ID and Payload
    memcpy(block->header.src, temp, 6);
    memcpy(block->payload, temp + 6, 56);
}

bool HERMES_Seal_DecryptInner(HermesDataBlock_t *block, const uint8_t k_scope[32]) {
    if (!block || !k_scope) return false;

    uint8_t nonce[12];
    build_inner_nonce(block, nonce);

    uint8_t temp[62];
    memcpy(temp, block->header.src, 6);
    memcpy(temp + 6, block->payload, 56);

    if (!HERMES_Crypto_Decrypt(k_scope, nonce, temp, 62, block->inner_mac)) {
        return false;
    }

    memcpy(block->header.src, temp, 6);
    memcpy(block->payload, temp + 6, 56);
    return true;
}

void HERMES_Seal_ObfuscateOuter(HermesDataBlock_t *block, const uint8_t k_net[32],
                                 uint32_t sync_word, uint32_t frequency) {
    if (!block || !k_net) return;

    // RFC §3.2.2: Outer nonce from three physical-layer markers
    uint8_t nonce[12];
    build_outer_nonce(block, sync_word, frequency, nonce);

    // RFC §3.2.1: Generate 84-byte keystream for obfuscation
    // Header remainder (bytes 0-19, 20B) + Inner frame (bytes 24-87, 64B)
    // Hop Nonce (bytes 20-23) stays clear
    // Outer MAC (bytes 88-95) stays clear
    chacha20_ctx ctx;
    chacha20_init(&ctx, k_net, nonce, 0);

    uint8_t keystream[96] = {0};
    chacha20_encrypt(&ctx, keystream, keystream, 96);

    uint8_t *raw = (uint8_t *)block;
    for (int i = 0; i < 88; i++) {
        // Skip Hop Nonce (bytes 20-23) — must remain clear for de-obfuscation bootstrap
        if (i >= 20 && i <= 23) continue;
        raw[i] ^= keystream[i];
    }
    // Note: bytes 88-95 (Outer MAC) are NOT obfuscated per RFC §4.4
}

void HERMES_Seal_CalculateOuterMAC(HermesDataBlock_t *block, const uint8_t k_net[32],
                                    uint32_t sync_word, uint32_t frequency) {
    if (!block || !k_net) return;

    // RFC §3.2: Outer MAC calculated AFTER obfuscation
    uint8_t nonce[12];
    build_outer_nonce(block, sync_word, frequency, nonce);

    // Derive Poly1305 key from ChaCha20 block 0
    uint8_t poly_key[32];
    uint8_t zeros[32] = {0};
    chacha20_ctx key_ctx;
    chacha20_init(&key_ctx, k_net, nonce, 0);
    chacha20_encrypt(&key_ctx, zeros, poly_key, 32);

    // Poly1305 over obfuscated block (bytes 0-87 = 88 bytes)
    poly1305_context pctx;
    poly1305_init(&pctx, poly_key);
    poly1305_update(&pctx, (uint8_t*)block, 88);

    uint8_t full_mac[16];
    poly1305_finish(&pctx, full_mac);

    // Place truncated 8-byte MAC at bytes 88-95 (cleartext)
    memcpy(block->outer_mac, full_mac, 8);
}

bool HERMES_Seal_VerifyOuterMAC(HermesDataBlock_t *block, const uint8_t k_net[32],
                                 uint32_t sync_word, uint32_t frequency) {
    if (!block || !k_net) return false;

    // Save received MAC
    uint8_t received_mac[8];
    memcpy(received_mac, block->outer_mac, 8);

    // Calculate expected MAC
    uint8_t nonce[12];
    build_outer_nonce(block, sync_word, frequency, nonce);

    uint8_t poly_key[32];
    uint8_t zeros[32] = {0};
    chacha20_ctx key_ctx;
    chacha20_init(&key_ctx, k_net, nonce, 0);
    chacha20_encrypt(&key_ctx, zeros, poly_key, 32);

    poly1305_context pctx;
    poly1305_init(&pctx, poly_key);
    poly1305_update(&pctx, (uint8_t*)block, 88);

    uint8_t full_mac[16];
    poly1305_finish(&pctx, full_mac);

    return memcmp(full_mac, received_mac, 8) == 0;
}

#endif // ENABLE_MESH_NETWORK
