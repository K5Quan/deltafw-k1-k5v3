/* Hermes App — Discovery & Neighbor Table (RFC §12.6)
 *
 * Discovery beacons are broadcast with TTL=1 (single-hop).
 * Payload: node_id(6) + alias(12) + battery(1) = 19 bytes.
 * Neighbor table: 8 slots with LRU eviction after 3 missed beacons.
 */
#ifdef ENABLE_MESH_NETWORK
#ifdef ENABLE_HERMES_DISCOVERY

#include "apps/hermes/app/discovery.h"
#include <string.h>

static HermesNeighbor_t neighbor_table[HM_NEIGHBOR_SLOTS];
static uint8_t neighbor_count = 0;

void HERMES_DISC_Init(void) {
    memset(neighbor_table, 0, sizeof(neighbor_table));
    neighbor_count = 0;
}

void HERMES_DISC_BuildBeacon(uint8_t payload[56], const uint8_t node_id[6],
                             const char *alias, uint8_t battery) {
    memset(payload, 0, 56);
    memcpy(payload, node_id, 6);

    if (alias) {
        uint8_t len = 0;
        while (alias[len] && len < 12) len++;
        memcpy(payload + 6, alias, len);
    }

    payload[18] = battery;
}

void HERMES_DISC_ProcessBeacon(const uint8_t payload[56], int16_t rssi) {
    const uint8_t *id = payload;

    // Find existing entry
    int8_t slot = -1;
    for (uint8_t i = 0; i < neighbor_count; i++) {
        if (memcmp(neighbor_table[i].node_id, id, HM_NODE_ID_SIZE) == 0) {
            slot = (int8_t)i;
            break;
        }
    }

    // No existing entry, find free slot or evict oldest
    if (slot < 0) {
        if (neighbor_count < HM_NEIGHBOR_SLOTS) {
            slot = (int8_t)neighbor_count;
            neighbor_count++;
        } else {
            // Evict entry with most missed beacons
            uint8_t worst = 0;
            for (uint8_t i = 1; i < HM_NEIGHBOR_SLOTS; i++) {
                if (neighbor_table[i].missed > neighbor_table[worst].missed)
                    worst = i;
            }
            slot = (int8_t)worst;
        }
    }

    // Update entry
    HermesNeighbor_t *n = &neighbor_table[slot];
    memcpy(n->node_id, id, HM_NODE_ID_SIZE);
    n->rssi = (int8_t)rssi;
    n->battery = payload[18];
    n->missed = 0;
    n->active = true;
    // last_seen would be set by caller with system tick
}

uint8_t HERMES_DISC_GetNeighborCount(void) {
    return neighbor_count;
}

const HermesNeighbor_t* HERMES_DISC_GetNeighbor(uint8_t index) {
    if (index < neighbor_count) return &neighbor_table[index];
    return 0;
}

void HERMES_DISC_EvictStale(void) {
    for (uint8_t i = 0; i < neighbor_count; i++) {
        neighbor_table[i].missed++;
        if (neighbor_table[i].missed >= 3) {
            // Shift remaining entries down
            neighbor_table[i].active = false;
            if (i < neighbor_count - 1) {
                memcpy(&neighbor_table[i], &neighbor_table[i + 1],
                       (neighbor_count - 1 - i) * sizeof(HermesNeighbor_t));
            }
            neighbor_count--;
            i--; // Re-check this slot
        }
    }
}

#endif // ENABLE_HERMES_DISCOVERY
#endif // ENABLE_MESH_NETWORK
