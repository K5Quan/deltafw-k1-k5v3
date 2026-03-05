/* Hermes App — Discovery (RFC §12.6) */
#ifndef HERMES_DISCOVERY_H
#define HERMES_DISCOVERY_H

#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_DISCOVERY

#include <stdint.h>
#include <stdbool.h>
#include "apps/hermes/hermes_types.h"

// Build a discovery beacon payload
void HERMES_DISC_BuildBeacon(uint8_t payload[56], const uint8_t node_id[6],
                             const char *alias, uint8_t battery);

// Process incoming discovery beacon, update neighbor table
void HERMES_DISC_ProcessBeacon(const uint8_t payload[56], int16_t rssi);

// Get neighbor table
uint8_t HERMES_DISC_GetNeighborCount(void);
const HermesNeighbor_t* HERMES_DISC_GetNeighbor(uint8_t index);

// Evict stale neighbors (call periodically)
void HERMES_DISC_EvictStale(void);

// Initialize neighbor table
void HERMES_DISC_Init(void);

#endif // ENABLE_HERMES_DISCOVERY
#endif
#endif
