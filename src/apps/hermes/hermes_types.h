/* Hermes Protocol — Shared Types & Constants
 * Derived from the Hermes Link RFC specification.
 * All layers include this header for common definitions.
 */
#ifndef HERMES_TYPES_H
#define HERMES_TYPES_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ──── Frame & Payload Constants (RFC §3, §5) ────
#define HM_FRAME_SIZE         128   // Fixed over-the-air frame
#define HM_DATA_SIZE           96   // RS(128,96) data portion
#define HM_PARITY_SIZE         32   // RS parity bytes
#define HM_HEADER_SIZE         24   // Transport header
#define HM_PAYLOAD_SIZE        56   // Application payload
#define HM_INNER_MAC_SIZE       8   // Inner AEAD tag
#define HM_OUTER_MAC_SIZE       8   // Outer AEAD tag
#define HM_NODE_ID_SIZE         6   // 48-bit Base40 address
#define HM_PACKET_ID_SIZE       6   // Unique packet identifier

// ──── Physical Constants (RFC §1) ────
#define HM_PREAMBLE_LEN        16   // Bytes of 0x55/0xAA
#define HM_SYNC_WORD     0x2F2A11DBU // Default sync word
#define HM_BAUD_RATE         1200   // FSK baud

// ──── Network Constants (RFC §6, §7) ────
#define HM_MAX_TTL             15   // 4-bit TTL
#define HM_MAX_FRAGS           16   // 4-bit frag index
#define HM_MAX_MSG_BYTES      896   // 16 × 56
#define HM_DEDUP_SLOTS         16   // Heard-list ring buffer size (RAM-friendly)
#define HM_NEIGHBOR_SLOTS       8   // Max tracked neighbors

// ──── Transport Constants (RFC §8, §9) ────
#define HM_ACK_TIMEOUT_MS    1500   // Stop-and-Wait timeout
#define HM_MAX_RETRIES          3   // Retry limit
#define HM_CCA_WINDOW_MS        5   // Clear-channel-assessment
#define HM_BACKOFF_MIN_MS      20   // Minimum random backoff
#define HM_BACKOFF_MAX_MS     200   // Maximum random backoff
#define HM_DIFS_MS              10  // Distributed interframe space

// ──── Application Constants (RFC §12) ────
#define HM_MSG_MAX_TEXT        64   // GSM-7 chars in 56 bytes
#define HM_MSG_SLOTS            8   // Stored message ring

// ──── Packet Types (RFC §12.2) ────
typedef enum {
    HM_TYPE_ACK        = 0,
    HM_TYPE_PING       = 1,
    HM_TYPE_MESSAGE    = 2,
    HM_TYPE_TELEMETRY  = 3,
    HM_TYPE_DISCOVERY  = 4,
    HM_TYPE_KEY_RATCHET= 5,
} HermesPacketType_t;

// ──── Addressing Modes (RFC §5.1) ────
typedef enum {
    HM_ADDR_UNICAST    = 0,
    HM_ADDR_MULTICAST  = 1,
    HM_ADDR_BROADCAST  = 2,
    HM_ADDR_DISCOVER   = 3,
} HermesAddrMode_t;

// ──── TX State Machine ────
typedef enum {
    HM_TX_IDLE         = 0,
    HM_TX_CSMA_WAIT    = 1,
    HM_TX_SENDING      = 2,
    HM_TX_WAIT_ACK     = 3,
    HM_TX_DONE         = 4,
    HM_TX_FAILED       = 5,
} HermesTxState_t;

// ──── Transport Header (RFC §5: 24 bytes packed) ────
typedef struct __attribute__((packed)) {
    uint8_t  ctrl0;          // [7:4]=type, [3:0]=ttl
    uint8_t  ctrl1;          // [7:6]=addr_mode, [5]=want_ack, [4:1]=frag_idx, [0]=last_frag
    uint8_t  packet_id[HM_PACKET_ID_SIZE];
    uint8_t  dest[HM_NODE_ID_SIZE];
    uint8_t  src[HM_NODE_ID_SIZE];
    uint8_t  hop_nonce[4];
} HermesHeader_t;

// ──── Header field accessors (avoids bitfield portability issues) ────
static inline uint8_t  HM_HDR_Type(const HermesHeader_t *h)      { return (h->ctrl0 >> 4) & 0x0F; }
static inline uint8_t  HM_HDR_TTL(const HermesHeader_t *h)       { return h->ctrl0 & 0x0F; }
static inline uint8_t  HM_HDR_AddrMode(const HermesHeader_t *h)  { return (h->ctrl1 >> 6) & 0x03; }
static inline bool     HM_HDR_WantAck(const HermesHeader_t *h)   { return (h->ctrl1 >> 5) & 0x01; }
static inline uint8_t  HM_HDR_FragIdx(const HermesHeader_t *h)   { return (h->ctrl1 >> 1) & 0x0F; }
static inline bool     HM_HDR_LastFrag(const HermesHeader_t *h)  { return h->ctrl1 & 0x01; }

static inline void HM_HDR_SetType(HermesHeader_t *h, uint8_t t)     { h->ctrl0 = (h->ctrl0 & 0x0F) | ((t & 0x0F) << 4); }
static inline void HM_HDR_SetTTL(HermesHeader_t *h, uint8_t ttl)    { h->ctrl0 = (h->ctrl0 & 0xF0) | (ttl & 0x0F); }
static inline void HM_HDR_SetAddrMode(HermesHeader_t *h, uint8_t m) { h->ctrl1 = (h->ctrl1 & 0x3F) | ((m & 0x03) << 6); }
static inline void HM_HDR_SetWantAck(HermesHeader_t *h, bool a)     { h->ctrl1 = (h->ctrl1 & 0xDF) | ((a ? 1 : 0) << 5); }
static inline void HM_HDR_SetFragIdx(HermesHeader_t *h, uint8_t i)  { h->ctrl1 = (h->ctrl1 & 0xE1) | ((i & 0x0F) << 1); }
static inline void HM_HDR_SetLastFrag(HermesHeader_t *h, bool l)    { h->ctrl1 = (h->ctrl1 & 0xFE) | (l ? 1 : 0); }

// ──── Data Block: header + payload + MACs = 96 bytes ────
typedef struct __attribute__((packed)) {
    HermesHeader_t header;                      // 24
    uint8_t        payload[HM_PAYLOAD_SIZE];    // 56
    uint8_t        inner_mac[HM_INNER_MAC_SIZE];// 8
    uint8_t        outer_mac[HM_OUTER_MAC_SIZE];// 8
} HermesDataBlock_t;                            // = 96

// ──── Full OTA Frame: data + parity = 128 bytes ────
typedef struct __attribute__((packed)) {
    uint8_t raw[HM_FRAME_SIZE];
} HermesFrame_t;

// ──── Stored Message (for UI display) ────
typedef struct {
    uint8_t  src[HM_NODE_ID_SIZE];
    uint8_t  dest[HM_NODE_ID_SIZE];
    uint8_t  packet_id[HM_PACKET_ID_SIZE];
    char     text[HM_MSG_MAX_TEXT + 1];
    uint8_t  len;
    uint8_t  addressing : 2;
    bool     is_outgoing : 1;
    bool     is_pending  : 1;
    bool     is_acked    : 1;
    bool     is_read     : 1;
    uint8_t  tx_state    : 3;
    uint8_t  retry_count : 3;
} HermesMessage_t;

// ──── Contact Entry (persisted) ────
#define HM_CONTACT_FLAG_MULTICAST  0x01
typedef struct {
    uint8_t  node_id[HM_NODE_ID_SIZE];
    char     alias[13];
    char     secret[13];
    uint8_t  flags;
} HermesContact_t;

// ──── Neighbor Entry ────
typedef struct {
    uint8_t  node_id[HM_NODE_ID_SIZE];
    int8_t   rssi;
    uint8_t  lqi;
    uint8_t  battery;
    uint8_t  missed;
    uint32_t last_seen;
    bool     active;
} HermesNeighbor_t;

// ──── Runtime Configuration ────
typedef struct {
    uint8_t  node_id[HM_NODE_ID_SIZE];
    char     alias[13];
    uint32_t frequency;
    uint8_t  sync_word[4];
    uint8_t  net_key[32];    // K_net (Network Key / Mesh Secret) — RFC §2
    char     passcode[33];   // Human-readable passcode (stored in flash)
    uint8_t  salt[16];       // Network salt (stored in flash)
    uint8_t  tx_power;       // 0=LOW, 1=MID, 2=HIGH
    uint8_t  ttl;
    uint8_t  freq_mode;      // 0=LPD66, 1=VFO, 2=MEM
    uint8_t  freq_ch;        // Memory channel index (0-199) if freq_mode==2
    uint8_t  routing_mode;   // 0=Off, 1=Passive, 2=Active Relay
    uint8_t  ack_mode;       // 0=Off, 1=Manual, 2=Auto
    uint8_t  mac_policy;     // 0=HW, 1=Custom, 2=Alias
    bool     crypto_enabled;
    bool     enabled;
    bool     debug;
} HermesConfig_t;

// ──── Broadcast address ────
static const uint8_t HM_ADDR_BCAST[HM_NODE_ID_SIZE] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ──── Utility: check if address is broadcast ────
static inline bool HM_IsBroadcast(const uint8_t addr[HM_NODE_ID_SIZE]) {
    return memcmp(addr, HM_ADDR_BCAST, HM_NODE_ID_SIZE) == 0;
}

// ──── Utility: check if address is ours ────
static inline bool HM_IsOurAddress(const uint8_t addr[HM_NODE_ID_SIZE], const uint8_t our[HM_NODE_ID_SIZE]) {
    return memcmp(addr, our, HM_NODE_ID_SIZE) == 0;
}

#endif // ENABLE_MESH_NETWORK
#endif // HERMES_TYPES_H
