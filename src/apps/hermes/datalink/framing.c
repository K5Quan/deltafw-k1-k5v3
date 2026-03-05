/* Hermes Data-Link — Frame Packing (RFC §3.1)
 *
 * TX pipeline: DataBlock(96B) → RS-encode → 128B → Whiten → OTA
 * RX pipeline: OTA → 128B → Dewhiten → RS-decode → DataBlock(96B)
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/datalink/framing.h"
#include "apps/hermes/datalink/whitening.h"
#include "apps/hermes/datalink/fec.h"
#include <string.h>

bool HERMES_Frame_Pack(const HermesDataBlock_t *block, HermesFrame_t *frame, uint32_t sync_word) {
    if (!block || !frame) return false;

    // Copy 96-byte data block into frame
    memcpy(frame->raw, block, HM_DATA_SIZE);

    // RFC §2.4: Whitening must be applied BEFORE RS encoding
    // to prevent bit-slip multipliers in the FEC engine.
    HERMES_Whiten(frame->raw, HM_DATA_SIZE, sync_word);

    // Zero parity region
    memset(frame->raw + HM_DATA_SIZE, 0, HM_PARITY_SIZE);

    // RS(128,96) encode: fills parity bytes [96..127] over whitened data
    HERMES_FEC_Encode(frame->raw);

    return true;
}

bool HERMES_Frame_Unpack(const HermesFrame_t *frame, HermesDataBlock_t *block, uint32_t sync_word) {
    if (!frame || !block) return false;

    // Working copy (dewhiten + FEC are in-place)
    uint8_t work[HM_FRAME_SIZE];
    memcpy(work, frame->raw, HM_FRAME_SIZE);

    // RS(128,96) decode in-place, returns erasure count (negative = fail)
    // Must be done before dewhitening as parity covers whitened data
    int8_t result = HERMES_FEC_Decode(work);
    if (result < 0) return false;

    // Dewhiten the 96-byte data portion
    HERMES_Whiten(work, HM_DATA_SIZE, sync_word);

    // Copy decoded 96-byte data block out
    memcpy(block, work, HM_DATA_SIZE);
    return true;
}

#endif // ENABLE_MESH_NETWORK
