/* Hermes App — GSM-7 Encoding (RFC §12.3)
 *
 * Packs 7-bit ASCII characters into a bitstream, fitting ~64 chars
 * into a 56-byte payload. Standard GSM 03.38 packing.
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_MESSENGER

#include "apps/hermes/app/messaging.h"
#include <string.h>

uint8_t HERMES_MSG_PackGSM7(const char *text, uint8_t text_len, uint8_t *out) {
    if (!text || !out || text_len == 0) return 0;

    uint8_t packed_len = (text_len * 7 + 7) / 8;
    memset(out, 0, packed_len);

    for (uint8_t i = 0; i < text_len; i++) {
        uint8_t c = (uint8_t)text[i] & 0x7F;
        uint16_t bit_pos = (uint16_t)i * 7;
        uint8_t byte_idx = bit_pos / 8;
        uint8_t bit_off  = bit_pos % 8;

        out[byte_idx] |= (c << bit_off);
        if (bit_off > 1) {
            out[byte_idx + 1] |= (c >> (8 - bit_off));
        }
    }

    return packed_len;
}

uint8_t HERMES_MSG_UnpackGSM7(const uint8_t *packed, uint8_t packed_len, char *out, uint8_t max_out) {
    if (!packed || !out || packed_len == 0 || max_out < 2) return 0;

    uint8_t chars = 0;
    uint8_t limit = max_out - 1;

    while (chars < limit) {
        uint16_t bit_pos = (uint16_t)chars * 7;
        uint8_t byte_idx = bit_pos / 8;
        uint8_t bit_off  = bit_pos % 8;

        if (byte_idx >= packed_len) break;

        uint16_t window = packed[byte_idx];
        if (byte_idx + 1 < packed_len) {
            window |= ((uint16_t)packed[byte_idx + 1] << 8);
        }

        uint8_t c = (window >> bit_off) & 0x7F;
        if (c == 0) break; // NUL terminator
        out[chars++] = (char)c;
    }

    out[chars] = '\0';
    return chars;
}

#endif // ENABLE_HERMES_MESSENGER
#endif // ENABLE_MESH_NETWORK
