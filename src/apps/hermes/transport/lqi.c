/* Hermes Transport — LQI (RFC §10)
 *
 * Fast Integer Sigmoid for SNR → LQI mapping.
 * Elliot approximation: f(x) = x / (1 + |x|)
 * All integer math, no floating point.
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_ACK_HEALTH

#include "apps/hermes/transport/lqi.h"

uint8_t HERMES_LQI_Sigmoid(int16_t snr_db) {
    // Elliot sigmoid: f(x) = 128 + (x * 128) / (64 + abs(x))
    // Maps [-inf..+inf] to [0..255], centered at 128
    int16_t abs_snr = (snr_db < 0) ? -snr_db : snr_db;
    int32_t num = (int32_t)snr_db * 128;
    int32_t den = 64 + abs_snr;
    int16_t result = 128 + (int16_t)(num / den);

    if (result < 0) result = 0;
    if (result > 255) result = 255;
    return (uint8_t)result;
}

uint8_t HERMES_LQI_Compute(int16_t rssi_pkt, int16_t rssi_idle) {
    // SNR = RSSI_packet - RSSI_idle (noise floor)
    int16_t snr = rssi_pkt - rssi_idle;
    return HERMES_LQI_Sigmoid(snr);
}

#endif // ENABLE_HERMES_ACK_HEALTH
#endif // ENABLE_MESH_NETWORK
