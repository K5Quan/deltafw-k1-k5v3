/* Hermes Security — Key Derivation (RFC §2, §5) */
#ifndef HERMES_KDF_H
#define HERMES_KDF_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>

// RFC §5.2: Scope Labels
#define HM_LABEL_UNICAST   0x55  // 'U'
#define HM_LABEL_MULTICAST 0x4D  // 'M'
#define HM_LABEL_BROADCAST 0x42  // 'B'
#define HM_LABEL_DISCOVERY 0x44  // 'D'

// RFC §2: Derive K_net from passcode + salt (10,000 ChaCha20 iterations)
void HERMES_KDF_DeriveNetworkKey(const char *passcode, const uint8_t salt[16], uint8_t out[32]);

// RFC §5: Derive K_scope (Traffic Key) from K_net + label + dest + secret
void HERMES_KDF_DeriveTrafficKey(const uint8_t k_net[32],
                                  uint8_t label,
                                  const uint8_t dest[6],
                                  const uint8_t *secret,
                                  uint8_t out[32]);

#endif
#endif
