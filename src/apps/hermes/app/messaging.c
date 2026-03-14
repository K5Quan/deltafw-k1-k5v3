/* Hermes App — GSM-7 Encoding (RFC §12.3)
 *
 * Packs 7-bit ASCII characters into a bitstream, fitting ~64 chars
 * into a 56-byte payload. Standard GSM 03.38 packing.
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_MESSENGER

#include "apps/hermes/app/messaging.h"
#include <stdbool.h>
#include <string.h>

// RFC §12.3: GSM-7 Basic Charset (128 entries, index = GSM-7 code)
// Row 0x10-0x19: Greek chars (Δ_ΦΓΛΩΠΨΣΘ) — use unique non-ASCII
// placeholders since these can't be typed on the radio anyway.
static const char gsm7_basic[] = 
    "@\xa3$\xa5\xe8\xe9\xf9\xec\xf2\xc7\n\xd8\xf8\r\xc5\xe5"
    "\x80\x5f\x81\x82\x83\x84\x85\x86\x87\x88\x89\x1b\xc6\xe6\xdf\xc9"
    " !\"#\xa4%&'()*+,-./"
    "0123456789:;<=>?"
    "\xa1\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
    "PQRSTUVWXYZ\xc4\xd6\xd1\xdc\xa7"
    "\xbf\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
    "pqrstuvwxyz\xe4\xf6\xf1\xfc\xe0";

// RFC §12.3: GSM-7 Extended Charset (Preceded by 0x1B)
// NUL (0x00) entries mean "no mapping" — FindGSM7 skips them.
static const char gsm7_ext[] = 
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0c\x00\x00\x00\x00\x00" // 0x0
    "\x00\x00\x00\x00\x5e\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 0x1
    "\x00\x00\x00\x00\x00\x00\x00\x00\x7b\x7d\x00\x00\x5b\x00\x00\x5c" // 0x2
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x7e\x5d\x00" // 0x3
    "\x7c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 0x4
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 0x5
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 0x6
    "\x00\x00\x00\x00\x00\x20\xa4\x00\x00\x00\x00\x00\x00\x00\x00\x00"; // 0x7

static uint8_t FindGSM7(char c, bool *is_ext) {
    // Don't encode NUL — it maps to '@' which causes corruption
    if (c == '\0') { *is_ext = false; return 0x20; } // map to space

    for (uint8_t i = 0; i < 128; i++) {
        if (gsm7_basic[i] == c) { *is_ext = false; return i; }
        // Skip NUL entries in ext table (they mean "no mapping")
        if (gsm7_ext[i] != '\0' && gsm7_ext[i] == c) { *is_ext = true; return i; }
    }
    return 0x20; // Default to space
}

uint8_t HERMES_MSG_PackGSM7(const char *text, uint8_t text_len, uint8_t *out) {
    if (!text || !out || text_len == 0) return 0;
    uint8_t bit_pos = 0;
    memset(out, 0, HM_PAYLOAD_SIZE);
    for (uint8_t i = 0; i < text_len && text[i] != '\0'; i++) {
        bool is_ext = false;
        uint8_t code = FindGSM7(text[i], &is_ext);
        if (is_ext) {
            // Pack ESC first
            uint8_t esc = 0x1B;
            for (uint8_t b = 0; b < 7; b++) {
                if (esc & (1 << b)) out[bit_pos / 8] |= (1 << (bit_pos % 8));
                bit_pos++;
            }
        }
        for (uint8_t b = 0; b < 7; b++) {
            if (code & (1 << b)) out[bit_pos / 8] |= (1 << (bit_pos % 8));
            bit_pos++;
        }
        if (bit_pos >= (HM_PAYLOAD_SIZE * 8)) break;
    }
    return (bit_pos + 7) / 8;
}

// char_count: number of GSM-7 characters to unpack (from HermesMessage_t.len).
// If 0, unpacks until end of packed data (legacy behavior, may produce trailing '@').
uint8_t HERMES_MSG_UnpackGSM7(const uint8_t *packed, uint8_t packed_len, char *out, uint8_t max_out) {
    if (!packed || !out || packed_len == 0 || max_out < 2) return 0;
    uint8_t chars = 0, bit_pos = 0;
    bool next_is_ext = false;
    while (chars < (max_out - 1) && bit_pos <= (packed_len * 8 - 7)) {
        uint8_t code = 0;
        for (uint8_t b = 0; b < 7; b++) {
            if (packed[bit_pos / 8] & (1 << (bit_pos % 8))) code |= (1 << b);
            bit_pos++;
        }
        if (code == 0x1B) { next_is_ext = true; continue; }
        if (code == 0x00 && !next_is_ext) {
            // GSM-7 code 0 = '@', but trailing zeros likely mean end-of-data.
            // Only emit '@' if it appears before any all-zero tail.
            // Check if remaining packed bytes are all zero — if so, stop.
            bool all_zero = true;
            for (uint8_t j = bit_pos / 8; j < packed_len; j++) {
                if (packed[j] != 0) { all_zero = false; break; }
            }
            if (all_zero) break;
        }
        out[chars++] = next_is_ext ? gsm7_ext[code] : gsm7_basic[code];
        next_is_ext = false;
    }
    out[chars] = '\0';
    return chars;
}

uint8_t HERMES_MSG_OctetCount(const uint8_t *packed, uint8_t packed_len) {
    for (int i = packed_len - 1; i >= 0; i--) if (packed[i] != 0) return i + 1;
    return 0;
}

#endif // ENABLE_HERMES_MESSENGER
#endif // ENABLE_MESH_NETWORK
