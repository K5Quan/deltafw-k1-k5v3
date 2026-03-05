/* Hermes App — GSM-7 Messaging (RFC §12.3) */
#ifndef HERMES_MESSAGING_H
#define HERMES_MESSAGING_H

#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_MESSENGER

#include <stdint.h>

// Pack ASCII text into GSM-7 septets (7 bits per char)
// Returns number of packed bytes written
uint8_t HERMES_MSG_PackGSM7(const char *text, uint8_t text_len, uint8_t *out);

// Unpack GSM-7 septets back to ASCII
// Returns number of characters decoded
uint8_t HERMES_MSG_UnpackGSM7(const uint8_t *packed, uint8_t packed_len, char *out, uint8_t max_out);

#endif // ENABLE_HERMES_MESSENGER
#endif
#endif
