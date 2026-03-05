/* Hermes Security — Key Derivation (RFC §2, §5)
 *
 * K_net  = KDF(passcode, salt)     — 10,000 ChaCha20 iterations
 * K_scope = PRF(K_net, Label || Dest || Secret)
 *
 * K_mesh is a synonym for K_net (same key).
 * K_scope (Traffic Key) is derived per-destination using
 * addressing-mode labels: 'U'=Unicast, 'M'=Multicast,
 * 'B'=Broadcast, 'D'=Discovery.
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/security/kdf.h"
#include <string.h>

#ifdef ENABLE_CRYPTO
#include "helper/chacha20.h"

// RFC §5.2: Scope labels (ASCII)
#define HM_LABEL_UNICAST   0x55  // 'U'
#define HM_LABEL_MULTICAST 0x4D  // 'M'
#define HM_LABEL_BROADCAST 0x42  // 'B'
#define HM_LABEL_DISCOVERY 0x44  // 'D'

void HERMES_KDF_DeriveNetworkKey(const char *passcode, const uint8_t salt[16], uint8_t out[32]) {
    if (!salt || !out) return;
    // NULL / empty passcode → NULL key (all zeros)
    if (!passcode || passcode[0] == '\0') {
        memset(out, 0, 32);
        return;
    }

    // RFC §2.1.2: Absorb salt + passcode into initial state
    uint8_t work[32];
    memset(work, 0, 32);

    // Simple absorption: XOR salt and passcode bytes into work buffer
    for (int i = 0; i < 16; i++)
        work[i] ^= salt[i];
    const char *p = passcode;
    for (int i = 0; *p && i < 32; i++, p++)
        work[16 + (i % 16)] ^= (uint8_t)*p;

    // RFC §2.1.2: 10,000 ChaCha20 PRF iterations for hardening
    // Uses current W as Key, Salt[0:11] as Nonce
    uint8_t nonce[12];
    memcpy(nonce, salt, 12);  // Salt bytes 0-11 as nonce

    for (uint32_t i = 0; i < 10000; i++) {
        uint8_t zero[32] = {0};
        chacha20_ctx ctx;
        chacha20_init(&ctx, work, nonce, 0);
        chacha20_encrypt(&ctx, zero, work, 32);
    }

    memcpy(out, work, 32);
}

void HERMES_KDF_DeriveTrafficKey(const uint8_t k_net[32],
                                  uint8_t label,
                                  const uint8_t dest[6],
                                  const uint8_t *secret,
                                  uint8_t out[32]) {
    if (!k_net || !dest || !out) return;

    static const uint8_t NULL_SECRET[32] = {0};

    // RFC §5.1: PRF(K_net, Label || Destination || Secret)
    // Build 12-byte nonce: Label (1B) + Dest (6B) + Secret[0:4] (5B)
    uint8_t nonce[12];
    nonce[0] = label;
    memcpy(nonce + 1, dest, 6);
    // Use first 5 bytes of secret as remaining nonce bytes for domain separation
    const uint8_t *sec = secret ? secret : NULL_SECRET;
    memcpy(nonce + 7, sec, 5);

    // Absorb remaining secret bytes into a second pass
    // First pass: PRF with K_net as key, nonce as context
    uint8_t temp[32] = {0};
    chacha20_ctx ctx;
    chacha20_init(&ctx, k_net, nonce, 0);
    chacha20_encrypt(&ctx, temp, temp, 32);

    // Second pass: fold in remaining secret bytes (5:31)
    // XOR secret[5:31] into temp for additional domain separation
    for (int i = 0; i < 27; i++)
        temp[i] ^= sec[5 + i];

    // Final squeeze: PRF(temp, nonce) to produce K_scope
    chacha20_init(&ctx, temp, nonce, 0);
    chacha20_encrypt(&ctx, out, out, 32);
    memset(out, 0, 32);
    chacha20_encrypt(&ctx, out, out, 32);
}

#else
// Stubs when CRYPTO disabled
void HERMES_KDF_DeriveNetworkKey(const char *passcode, const uint8_t salt[16], uint8_t out[32]) {
    (void)passcode; (void)salt;
    if (out) memset(out, 0, 32);
}

void HERMES_KDF_DeriveTrafficKey(const uint8_t k_net[32], uint8_t label,
                                  const uint8_t dest[6], const uint8_t *secret,
                                  uint8_t out[32]) {
    (void)label; (void)dest; (void)secret;
    if (k_net && out) memcpy(out, k_net, 32);
}
#endif // ENABLE_CRYPTO

#endif // ENABLE_MESH_NETWORK

