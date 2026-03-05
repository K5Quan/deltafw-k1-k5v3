/* Hermes Network — Base40 Addressing (RFC §6.1)
 *
 * M17-compatible Base40 encoding maps 9-character callsigns
 * into 48-bit (6-byte) Node IDs.
 *
 * Alphabet (40 chars): ' ' A-Z 0-9 - / .
 * Each character is encoded as a digit in base-40, packed
 * MSB-first into a 48-bit integer.
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/network/addressing.h"
#include <string.h>

// Base40 alphabet: space, A-Z, 0-9, -, /, .
static const char B40_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";
#define B40_BASE 40
#define B40_MAX_CHARS 9

static uint8_t char_to_b40(char c) {
    if (c == ' ' || c == 0) return 0;
    if (c >= 'a' && c <= 'z') c -= 32; // uppercase
    if (c >= 'A' && c <= 'Z') return (uint8_t)(c - 'A' + 1);
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0' + 27);
    if (c == '-') return 37;
    if (c == '/') return 38;
    if (c == '.') return 39;
    return 0; // unknown → space
}

static char b40_to_char(uint8_t val) {
    if (val < 40) return B40_CHARS[val];
    return ' ';
}

void HERMES_Addr_Encode(const char *callsign, uint8_t out[6]) {
    if (!callsign || !out) return;

    // Pack callsign into 48-bit value: val = Σ char[i] * 40^(8-i)
    uint64_t val = 0;
    uint8_t len = (uint8_t)strlen(callsign);
    if (len > B40_MAX_CHARS) len = B40_MAX_CHARS;

    for (uint8_t i = 0; i < B40_MAX_CHARS; i++) {
        val *= B40_BASE;
        if (i < len) val += char_to_b40(callsign[i]);
    }

    // Store as big-endian 6 bytes
    for (int8_t i = 5; i >= 0; i--) {
        out[i] = (uint8_t)(val & 0xFF);
        val >>= 8;
    }
}

void HERMES_Addr_Decode(const uint8_t addr[6], char *out, uint8_t max_len) {
    if (!addr || !out || max_len < 2) return;

    // Reconstruct 48-bit value from big-endian bytes
    uint64_t val = 0;
    for (uint8_t i = 0; i < 6; i++) {
        val = (val << 8) | addr[i];
    }

    // Decode base-40 digits (most significant first)
    char buf[B40_MAX_CHARS + 1];
    for (int8_t i = B40_MAX_CHARS - 1; i >= 0; i--) {
        buf[i] = b40_to_char((uint8_t)(val % B40_BASE));
        val /= B40_BASE;
    }
    buf[B40_MAX_CHARS] = '\0';

    // Trim trailing spaces
    for (int8_t i = B40_MAX_CHARS - 1; i >= 0; i--) {
        if (buf[i] == ' ') buf[i] = '\0';
        else break;
    }

    uint8_t copy_len = (uint8_t)strlen(buf);
    if (copy_len >= max_len) copy_len = max_len - 1;
    memcpy(out, buf, copy_len);
    out[copy_len] = '\0';
}

bool HERMES_Addr_IsBase40(const uint8_t addr[6]) {
    // Decode and verify all chars are valid Base40 alphabet
    char buf[10];
    HERMES_Addr_Decode(addr, buf, sizeof(buf));
    // An all-zero address is not a "valid" callsign
    bool all_zero = true;
    for (uint8_t i = 0; i < 6; i++) {
        if (addr[i] != 0) { all_zero = false; break; }
    }
    if (all_zero) return false;
    // Check each decoded char is in the Base40 set
    for (uint8_t i = 0; buf[i]; i++) {
        char c = buf[i];
        if (c == ' ') continue;
        if (c >= 'A' && c <= 'Z') continue;
        if (c >= '0' && c <= '9') continue;
        if (c == '-' || c == '/' || c == '.') continue;
        return false;
    }
    return buf[0] != '\0';
}

void HERMES_Addr_FormatMAC(const uint8_t addr[6], char *out, uint8_t max_len) {
    if (!out || max_len < 18) {
        if (out && max_len > 0) out[0] = '\0';
        return;
    }
    const char *hex = "0123456789ABCDEF";
    uint8_t p = 0;
    for (uint8_t i = 0; i < 6; i++) {
        out[p++] = hex[(addr[i] >> 4) & 0x0F];
        out[p++] = hex[addr[i] & 0x0F];
        if (i < 5) out[p++] = ':';
    }
    out[p] = '\0';
}

void HERMES_Addr_Format(const uint8_t addr[6], uint8_t mac_policy, char *out, uint8_t max_len) {
    if (!out || max_len < 2) return;
    // If mac_policy is Alias (2), always show Base40 decoded callsign
    if (mac_policy == 2) {
        HERMES_Addr_Decode(addr, out, max_len);
        return;
    }
    // If mac_policy is HW (0) or Custom (1), show as MAC
    HERMES_Addr_FormatMAC(addr, out, max_len);
}

#endif // ENABLE_MESH_NETWORK
