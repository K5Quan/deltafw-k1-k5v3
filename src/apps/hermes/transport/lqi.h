/* Hermes Transport — Link Quality Indicator (RFC §10) */
#ifndef HERMES_LQI_H
#define HERMES_LQI_H

#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_ACK_HEALTH

#include <stdint.h>

// Compute LQI from RSSI triad (packet, ack, idle) → 0–255
uint8_t HERMES_LQI_Compute(int16_t rssi_pkt, int16_t rssi_idle);

// Fast Integer Sigmoid (Elliot approximation)
// Maps SNR [-30..+30] dB to [0..255]
uint8_t HERMES_LQI_Sigmoid(int16_t snr_db);

#endif // ENABLE_HERMES_ACK_HEALTH
#endif
#endif
