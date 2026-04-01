/* Hermes — Core app logic
 *
 * Dispatched by the display system:
 *   DISPLAY_NETWORK → HERMES_ProcessKeys (keys) / HERMES_UI_Render (display)
 *
 * Handles: Init, key routing (menu/chat/compose), FSK ISR, tick,
 *          and the RX processing pipeline.
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/hermes.h"
#include "features/audio/audio.h"
#include "apps/hermes/ui/hermes_ui.h"
#include "apps/hermes/physical/phy.h"
#include "apps/hermes/datalink/framing.h"
#include "apps/hermes/network/addressing.h"
#include "drivers/bsp/bk4819.h"
#include "drivers/bsp/systick.h"
#include "ui/ag_menu.h"
#include "ui/textinput.h"
#include "core/misc.h"
#include "ui/ui.h"
#include "features/storage/storage.h"
#include "features/radio/radio.h"
#include "features/radio/functions.h"
#include <string.h>
#include <stdio.h>
#include "ui/helper.h"

#ifdef ENABLE_HERMES_MESSENGER
#include "apps/hermes/app/messaging.h"
#include "apps/hermes/transport/reliability.h"
#endif

#ifdef ENABLE_HERMES_ACK_HEALTH
#include "apps/hermes/app/ack.h"
#endif

#include "apps/hermes/transport/packet.h"
#include "apps/hermes/transport/csma.h"
#include "apps/hermes/security/kdf.h"
#include "apps/hermes/security/seal.h"

#ifdef ENABLE_HERMES_DISCOVERY
#include "apps/hermes/app/discovery.h"
#include "core/scheduler.h"
#endif

#ifdef ENABLE_HERMES_PING
#include "apps/hermes/app/ping.h"
#endif

#ifdef ENABLE_HERMES_TELEMETRY
#include "apps/hermes/app/telemetry.h"
#endif

#ifdef ENABLE_HERMES_ROUTER
#include "apps/hermes/network/routing.h"
#endif
#ifdef ENABLE_HERMES_MESSENGER
#include "apps/hermes/app/messaging.h"
#include "apps/hermes/transport/reliability.h"
#endif

// ──── Global State ────
HermesConfig_t  gHermesConfig;
bool            gHermesEnabled       = false;
HermesMessage_t gHermesMessages[HM_MSG_SLOTS];
uint8_t         gHermesMsgCount      = 0;
bool            gHermesHasNewMessage = false;

// ──── RX buffer ────
static uint8_t  rx_buffer[HM_FRAME_SIZE];
static uint16_t rx_write_idx = 0;

// ═══════════════════════════════════════════════════════════
// Init
// ═══════════════════════════════════════════════════════════

void HERMES_Init(void) {
    memset(&gHermesConfig, 0, sizeof(gHermesConfig));

    HermesSettings_t settings;
    bool loaded = Storage_ReadRecord(REC_HERMES_SETTINGS, &settings, 0, sizeof(HermesSettings_t));
    
    // Initialize EEPROM record if empty/invalid
    if (!loaded || settings.fields.Magic != 0x484D) {
        memset(&settings, 0, sizeof(settings));
        settings.fields.Magic = 0x484D;
        settings.fields.MacPolicy = 0; // Hardware MAC
        strcpy(settings.fields.Alias, "NOCALL");
        memset(settings.fields.Passcode, 0, 32); 
        settings.fields.HasPasscode = 0;
        settings.fields.FreqMode = 0; // LPD66
        settings.fields.FreqCh = 0;
        settings.fields.Enabled = 1;
        settings.fields.RelayEnabled = 1; // Default ON
        settings.fields.AckMode = 2; // Auto
        settings.fields.TTL = 5;
        settings.fields.TxPower = 1; // Mid
        settings.fields.FskMute = 1; // Default: mute FSK audio
        memset(settings.fields.Salt, 0, 16);
        memcpy(settings.fields.Salt, "HERMES", 6);
        Storage_WriteRecord(REC_HERMES_SETTINGS, &settings, 0, sizeof(HermesSettings_t));
    }

    // Explicit field-by-field mapping to avoid struct padding surprises
    gHermesConfig.mac_policy    = settings.fields.MacPolicy <= 2 ? settings.fields.MacPolicy : 2;
    memcpy(gHermesConfig.node_id, settings.fields.CustomMac, 6);
    strncpy(gHermesConfig.alias, settings.fields.Alias, 12);
    gHermesConfig.alias[12] = '\0';
    
    // Passcode Handling
    strncpy(gHermesConfig.passcode, settings.fields.Passcode, 32);
    gHermesConfig.passcode[32] = '\0';
    memcpy(gHermesConfig.salt, settings.fields.Salt, 16);

    gHermesConfig.enabled       = (settings.fields.Enabled != 0);
    gHermesConfig.relay_enabled = (settings.fields.RelayEnabled != 0);
    gHermesConfig.ack_mode      = settings.fields.AckMode <= 2 ? settings.fields.AckMode : 2;
    gHermesConfig.freq_mode     = settings.fields.FreqMode <= 2 ? settings.fields.FreqMode : 1;
    gHermesConfig.freq_ch       = settings.fields.FreqCh;
    gHermesConfig.ttl           = (settings.fields.TTL >= 1 && settings.fields.TTL <= 15) ? settings.fields.TTL : 3;
    gHermesConfig.tx_power      = settings.fields.TxPower <= 2 ? settings.fields.TxPower : 1;
    gHermesConfig.debug         = (settings.fields.Debug != 0);
    gHermesConfig.fsk_mute      = (settings.fields.FskMute != 0);

    gHermesEnabled = gHermesConfig.enabled;
    
    HERMES_UpdateFrequency();
    
    // 3. Network Keys (KDF RFC §2) — Initial Setup
    static bool gHermesInitialized = false;
    if (!gHermesInitialized) {
        memset(gHermesMessages, 0, sizeof(gHermesMessages));
        gHermesMsgCount      = 0;
        gHermesHasNewMessage = false;

        // Passcode and Salt from settings
        strncpy(gHermesConfig.passcode, settings.fields.Passcode, 32);
        gHermesConfig.passcode[32] = '\0';
        memcpy(gHermesConfig.salt, settings.fields.Salt, 16);
        
        // Always derive the actual Network Key (K_net) on boot
        // This performs the 10,000 iteration hardening loop (RFC §2.1.2)
        HERMES_KDF_DeriveNetworkKey(gHermesConfig.passcode, gHermesConfig.salt, gHermesConfig.net_key);

        // Apply MAC Policy
        if (settings.fields.MacPolicy == 1) {       // Custom Hex MAC
            memcpy(gHermesConfig.node_id, settings.fields.CustomMac, 6);
        } else if (settings.fields.MacPolicy == 2) { // Alias base40
            HERMES_Addr_Encode(settings.fields.Alias, gHermesConfig.node_id);
        } else {                                    // Hardware MAC
            uint8_t *uid = (uint8_t *)0x1FFF0E00;
            gHermesConfig.node_id[0] = uid[0] ^ uid[6];
            gHermesConfig.node_id[1] = uid[1] ^ uid[7];
            gHermesConfig.node_id[2] = uid[2] ^ uid[8];
            gHermesConfig.node_id[3] = uid[3] ^ uid[9];
            gHermesConfig.node_id[4] = uid[4] ^ uid[10];
            gHermesConfig.node_id[5] = uid[5] ^ uid[11];
        }

        gHermesInitialized   = true;
    }

    HERMES_PHY_Init();

#ifdef ENABLE_HERMES_DISCOVERY
    HERMES_DISC_Init();
#endif
#ifdef ENABLE_HERMES_ROUTER
    HERMES_Route_Init();
#endif
#ifdef ENABLE_HERMES_MESSENGER
    HERMES_ARQ_Init();
#endif

    HERMES_UI_Init();
}

void HERMES_UpdateFrequency(void) {
    if (gHermesConfig.freq_mode == 0) {
        gHermesConfig.frequency = 43335000; // LPD66 433.350 MHz
    } else if (gHermesConfig.freq_mode == 1) {
        gHermesConfig.frequency = gTxVfo ? gTxVfo->pTX->Frequency : 43335000;
    } else {
        struct {
            uint32_t Frequency;
            uint32_t Offset;
        } __attribute__((packed)) info;
        if (Storage_ReadRecordIndexed(REC_CHANNEL_DATA, gHermesConfig.freq_ch, &info, 0, sizeof(info))) {
            gHermesConfig.frequency = (info.Frequency != 0xFFFFFFFF && info.Frequency != 0) ? info.Frequency : 43335000;
        } else {
            gHermesConfig.frequency = 43335000;
        }
    }
    
    // Apply immediately to BK4819
    BK4819_SetFrequency(gHermesConfig.frequency);
    BK4819_PickRXFilterPathBasedOnFrequency(gHermesConfig.frequency);
}

#ifdef ENABLE_HERMES_MESSENGER
// ──── Contact Lookup ────
static bool HERMES_GetContactSecret(const uint8_t id[6], uint8_t secret[32]) {
    HermesContactRecord_t rec;
    for (uint8_t i = 0; i < 16; i++) {
        if (Storage_ReadRecordIndexed(REC_HERMES_CONTACTS, i, &rec, 0, sizeof(rec))) {
            if (rec.fields.NodeID[0] != 0xFF && memcmp(rec.fields.NodeID, id, 6) == 0) {
                // Return passcode as secret (padded/truncated to 32)
                strncpy((char*)secret, rec.fields.Passcode, 32);
                return true;
            }
        }
    }
    return false;
}
#endif

// ═══════════════════════════════════════════════════════════

void HERMES_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    // ── Text input views: delegate everything to TextInput ──
    if (gHermesView == HM_VIEW_COMPOSE ||
        gHermesView == HM_VIEW_INPUT_ALIAS ||
        gHermesView == HM_VIEW_INPUT_PASSCODE ||
        gHermesView == HM_VIEW_INPUT_SALT ||
        gHermesView == HM_VIEW_INPUT_CONTACT) {
        if (TextInput_IsActive()) {
            if (TextInput_HandleInput(Key, bKeyPressed, bKeyHeld))
                gUpdateDisplay = true;
        } else {
            // Text input was deinited (e.g. by EXIT long-press) — return to parent view
            if (gHermesView == HM_VIEW_COMPOSE) {
                gHermesView = HM_VIEW_CHAT;
            } else if (gHermesView == HM_VIEW_INPUT_CONTACT) {
                gHermesView = HM_VIEW_CONTACTS;
                HERMES_UI_Init();
            } else {
                gHermesView = HM_VIEW_SETTINGS;
                HERMES_UI_Init();
            }
            gUpdateDisplay = true;
        }
        return;
    }

    // ── Chat view: custom key handling ──
    if (gHermesView == HM_VIEW_CHAT) {
        if (!bKeyPressed) return;
        if (bKeyHeld) return;

        switch (Key) {
            case KEY_UP:
                if (gHermesChatScroll > 0) {
                    gHermesChatScroll--;
                }
                gUpdateDisplay = true;
                return;
            case KEY_DOWN:
                if (gHermesChatScroll < gHermesMsgCount - 1) {
                    gHermesChatScroll++;
                }
                gUpdateDisplay = true;
                return;
            case KEY_MENU:
                HERMES_UI_StartCompose();
                return;
            case KEY_EXIT:
                gHermesView = HM_VIEW_MENU;
                HERMES_UI_Init();
                gUpdateDisplay = true;
                return;
            default:
                return;
        }
    }

    // ── Menu/Settings/Contacts views: AG_MENU handles everything ──
    if (gHermesView == HM_VIEW_CONTACTS) {
        extern bool MA_DeleteContact(const struct MenuItem *i, KEY_Code_t k, bool kp, bool kh);
        if (MA_DeleteContact(NULL, Key, bKeyPressed, bKeyHeld)) {
            gUpdateDisplay = true;
            return;
        }
    }

    if (!AG_MENU_IsActive())
        HERMES_UI_Init();

    if (AG_MENU_HandleInput(Key, bKeyPressed, bKeyHeld)) {
        gUpdateDisplay = true;
        return;
    }

    // If menu exited (back from settings → main, or back from main → launcher)
    if (!AG_MENU_IsActive()) {
        if (gHermesView == HM_VIEW_SETTINGS || gHermesView == HM_VIEW_CONTACTS) {
            // Back from settings/contacts → reinit main menu
            gHermesView = HM_VIEW_MENU;
            HERMES_UI_Init();
            gUpdateDisplay = true;
        } else {
            // Back from main menu → return to launcher
            gRequestDisplayScreen = DISPLAY_LAUNCHER;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// RX Pipeline
// ═══════════════════════════════════════════════════════════

static uint32_t sync_word_u32(void) {
    return ((uint32_t)gHermesConfig.sync_word[0] << 24) |
           ((uint32_t)gHermesConfig.sync_word[1] << 16) |
           ((uint32_t)gHermesConfig.sync_word[2] << 8)  |
           ((uint32_t)gHermesConfig.sync_word[3]);
}

static void ProcessReceivedFrame(void) {
    HermesDataBlock_t block;
    if (!HERMES_Frame_Unpack((HermesFrame_t *)rx_buffer, &block, sync_word_u32()))
        return;

    // Security - Outer Layer Deobfuscation (RFC §3)
    uint32_t sw = sync_word_u32();
    uint32_t freq = gHermesConfig.frequency;

    // RFC §5.3: Outer seal (Deduplication & Router visibility) — Always active
    if (!HERMES_Seal_VerifyOuterMAC((HermesDataBlock_t *)rx_buffer, gHermesConfig.net_key, sw, freq)) {
        return; // Failed outer HMAC or repeat nonce
    }
    // 2. Deobfuscate Outer Layer (Hop)
    HERMES_Seal_DeobfuscateOuter(&block, gHermesConfig.net_key, sw, freq);

    uint8_t type = HM_HDR_Type(&block.header);

#ifdef ENABLE_HERMES_ROUTER
    if (HERMES_Route_IsDuplicate(block.header.packet_id)) {
        // Implicit ACK: If we hear our own packet being rebroadcasted by a neighbor
        if (memcmp(block.header.src, gHermesConfig.node_id, HM_NODE_ID_SIZE) == 0) {
            for (uint8_t i = 0; i < gHermesMsgCount; i++) {
                if (gHermesMessages[i].is_outgoing && 
                    gHermesMessages[i].is_pending &&
                    memcmp(gHermesMessages[i].packet_id, block.header.packet_id, HM_PACKET_ID_SIZE) == 0) {
                    
                    gHermesMessages[i].is_pending = false;
                    gHermesMessages[i].is_acked = true;
                    gUpdateDisplay = true;
                    break;
                }
            }
        }
        return;
    }
    HERMES_Route_Record(block.header.packet_id);

    if (gHermesConfig.relay_enabled &&
        HERMES_Route_ShouldForward(&block.header, gHermesConfig.node_id)) {
        HERMES_Route_DecrementTTL(&block.header);
        
        // SNR-weighted delayed retransmit
        int16_t rssi = HERMES_PHY_GetRSSI();
        uint16_t delay_ms = HERMES_Route_CalcBackoff(rssi);
        HERMES_Route_QueueForward(&block, sync_word_u32(), delay_ms);
    }
#endif

#ifdef ENABLE_HERMES_MESSENGER
    if (type == HM_TYPE_MESSAGE) {
        // Security - Inner Layer Decryption (RFC §4, §5)
        uint8_t key[32];
        uint8_t addr_mode = HM_HDR_AddrMode(&block.header);
        uint8_t label;
        uint8_t secret[32];
        bool    has_secret = false;
 
        switch (addr_mode) {
            case HM_ADDR_UNICAST:   
                label = HM_LABEL_UNICAST;   
                has_secret = HERMES_GetContactSecret(block.header.src, secret);
                break;
            case HM_ADDR_MULTICAST: 
                label = HM_LABEL_MULTICAST; 
                has_secret = HERMES_GetContactSecret(block.header.dest, secret);
                break;
            case HM_ADDR_BROADCAST: label = HM_LABEL_BROADCAST; break;
            case HM_ADDR_DISCOVER:  label = HM_LABEL_DISCOVERY; break;
            default:                label = HM_LABEL_BROADCAST; break;
        }
 
        // RFC §5: Use OUR node_id as destination (receiver derives using own address)
        HERMES_KDF_DeriveTrafficKey(gHermesConfig.net_key, label,
                                     gHermesConfig.node_id, has_secret ? secret : NULL, key);

        if (!HERMES_Seal_DecryptInner(&block, key))
            return; // Dropped: invalid inner MAC or decryption failed

        if (HERMES_MSG_OctetCount(block.payload, HM_PAYLOAD_SIZE) > 0) {
            // Shift messages if full
            if (gHermesMsgCount >= HM_MSG_SLOTS) {
                for (int i = 0; i < HM_MSG_SLOTS - 1; i++) {
                    gHermesMessages[i] = gHermesMessages[i + 1];
                }
                gHermesMsgCount = HM_MSG_SLOTS - 1;
            }

            HermesMessage_t *m = &gHermesMessages[gHermesMsgCount++];
            memset(m, 0, sizeof(*m));
            memcpy(m->addr, block.header.src, HM_NODE_ID_SIZE);
            memcpy(m->payload, block.payload, HM_PAYLOAD_SIZE);
            m->len         = HM_MSG_MAX_TEXT; // Unpack logic handles this
            m->is_outgoing = false;
            m->addressing  = HM_HDR_AddrMode(&block.header);
            gHermesHasNewMessage = true;
            gUpdateDisplay       = true;
        }

        // Send ACK if requested
        if (HM_HDR_WantAck(&block.header) && HM_IsOurAddress(block.header.dest, gHermesConfig.node_id)) {
            HermesAck_t ack;
            memset(&ack, 0, sizeof(ack));
            memcpy(ack.acked_id, block.header.packet_id, 6);
            memcpy(ack.acked_mac, block.inner_mac, 8);
            ack.status = 0; // OK
            
            HermesDataBlock_t ack_block;
            memset(&ack_block, 0, sizeof(ack_block));
            HERMES_Pkt_Build(&ack_block.header, HM_TYPE_ACK, HM_ADDR_UNICAST, gHermesConfig.ttl, false, block.header.src, gHermesConfig.node_id);
            HERMES_ACK_Build(&ack, ack_block.payload);
            
            // Note: ACKs are always sent direct via CSMA, never queued in ARQ (otherwise infinite loop)
            HermesFrame_t ack_frame;
            if (HERMES_Frame_Pack(&ack_block, &ack_frame, sync_word_u32())) {
                HERMES_CSMA_Transmit(ack_frame.raw, sizeof(ack_frame.raw), HM_PRIO_CRITICAL);
            }
        }
    } else if (type == HM_TYPE_ACK) {
        HermesAck_t ack;
        if (HERMES_ACK_Parse(block.payload, &ack)) {
            // Unqueue if waiting
            if (HERMES_ARQ_HandleAck(ack.acked_id)) {
                // Mark matching message as acked in UI
                for (uint8_t i = 0; i < gHermesMsgCount; i++) {
                    if (gHermesMessages[i].is_outgoing && 
                        gHermesMessages[i].is_pending &&
                        memcmp(gHermesMessages[i].packet_id, ack.acked_id, HM_PACKET_ID_SIZE) == 0) {
                        
                        gHermesMessages[i].is_pending = false;
                        gHermesMessages[i].is_acked = true;
                        break;
                    }
                }
    }
}
#endif

    // Auto-scroll to new incoming message if we were at the end
    if (gHermesView == HM_VIEW_CHAT && gHermesChatScroll == (int16_t)gHermesMsgCount - 2) {
        gHermesChatScroll = (int16_t)gHermesMsgCount - 1;
    }
    gUpdateDisplay = true;
    }

#ifdef ENABLE_HERMES_TELEMETRY
    else if (type == HM_TYPE_TELEMETRY) {
        HermesTelemetry_t tel;
        if (HERMES_TEL_Unpack(block.payload, &tel)) {
#ifdef ENABLE_HERMES_DISCOVERY
            // Check if from a known neighbor
            uint8_t count = HERMES_DISC_GetNeighborCount();
            for (uint8_t i = 0; i < count; i++) {
                const HermesNeighbor_t *n = HERMES_DISC_GetNeighbor(i);
                if (n && n->active && memcmp(n->node_id, block.header.src, HM_NODE_ID_SIZE) == 0) {
                    // (We would update existing neighbor, but the API returns const)
                    // Just triggering display update for now
                    gUpdateDisplay = true;
                    break;
                }
            }
#endif
        }
    }
#endif

#ifdef ENABLE_HERMES_DISCOVERY
    else if (type == HM_TYPE_DISCOVERY) {
        int16_t rssi = HERMES_PHY_GetRSSI();
        HERMES_DISC_ProcessBeacon(block.payload, rssi);
        gUpdateDisplay = true;
    }
#endif

#ifdef ENABLE_HERMES_PING
    else if (type == HM_TYPE_PING) {
        if (HM_IsOurAddress(block.header.dest, gHermesConfig.node_id)) {
            // Ping reached us
            gUpdateDisplay = true;
        } else if (gHermesConfig.relay_enabled) {
            // Relay ping, recording our hop
            if (HERMES_PING_InsertHop(block.payload, gHermesConfig.node_id)) {
                HermesFrame_t frame;
                if (HERMES_Frame_Pack(&block, &frame, sync_word_u32())) {
                     HERMES_CSMA_Transmit(frame.raw, sizeof(frame.raw), HM_PRIO_NORMAL);
                }
            }
        }
    }
#endif
}

// ═══════════════════════════════════════════════════════════
// TX Pipeline
// ═══════════════════════════════════════════════════════════

void HERMES_SendMessage(HermesMessage_t *m_ptr) {
#ifdef ENABLE_HERMES_MESSENGER
    // Store message data locally to avoid issues if gHermesMessages array shifts due to logs
    HermesMessage_t m;
    memcpy(&m, m_ptr, sizeof(HermesMessage_t));

    HermesDataBlock_t block;
    memset(&block, 0, sizeof(block));

    // 1. Build Header
    HERMES_Pkt_Build(&block.header,
                     HM_TYPE_MESSAGE,
                     m.addressing,
                     gHermesConfig.ttl,
                     (gHermesConfig.ack_mode > 0 && m.addressing == HM_ADDR_UNICAST),
                     m.addr,
                     gHermesConfig.node_id);

    memcpy(m.packet_id, block.header.packet_id, HM_PACKET_ID_SIZE);
    memcpy(m_ptr->packet_id, block.header.packet_id, HM_PACKET_ID_SIZE);

    // Use already packed payload from the message struct
    memcpy(block.payload, m.payload, HM_PAYLOAD_SIZE);

    if (gHermesConfig.debug) {
        char buf[32];
        strcpy(buf, "TX ");
        uint8_t mode = (uint8_t)m.addressing;
        NUMBER_ToHex(buf + 3, mode, 2);
        strcat(buf, " to ");
        char target[12];
        HERMES_Addr_Decode(m.addr, target, sizeof(target));
        strcat(buf, target);
        HERMES_DebugLog(buf);
    }

    // 2. Security Layer (RFC §4, §5) - Always Encrypted
    uint8_t k_scope[32];
    uint8_t label;
    uint8_t secret[32];
    bool    has_secret = false;

    switch (m.addressing) {
        case HM_ADDR_UNICAST:   
            label = HM_LABEL_UNICAST;   
            has_secret = HERMES_GetContactSecret(m.addr, secret);
            break;
        case HM_ADDR_MULTICAST: 
            label = HM_LABEL_MULTICAST; 
            has_secret = HERMES_GetContactSecret(m.addr, secret);
            break;
        case HM_ADDR_BROADCAST: label = HM_LABEL_BROADCAST; break;
        case HM_ADDR_DISCOVER:  label = HM_LABEL_DISCOVERY; break;
        default:                label = HM_LABEL_BROADCAST; break;
    }

    HERMES_KDF_DeriveTrafficKey(gHermesConfig.net_key, label,
                                 m.addr, has_secret ? secret : NULL, k_scope);

    HermesFrame_t frame;
    // Inner: encrypt payload+source using AEAD (E2E)
    HERMES_Seal_EncryptInner(&block, k_scope);

    uint32_t sw = sync_word_u32();
    uint32_t freq = gHermesConfig.frequency;

    // Outer: obfuscate packet (Hop)
    HERMES_Seal_ObfuscateOuter(&block, gHermesConfig.net_key, sw, freq);
    // Bind Outer MAC to obfuscated block
    HERMES_Seal_CalculateOuterMAC(&block, gHermesConfig.net_key, sw, freq);

    // Pack into OTA frame
    if (!HERMES_Frame_Pack(&block, &frame, sw)) {
        if (gHermesConfig.debug) HERMES_DebugLog("Pack Err");
        return;
    }

    // 4. Transport (ARQ or direct Transmit)
    if (gHermesConfig.ack_mode > 0 && HM_HDR_WantAck(&block.header)) {
        HERMES_ARQ_Send(&block, sw);
        if (gHermesConfig.debug) HERMES_DebugLog("ARQ Queue");
    } else {
        if (gHermesConfig.debug) HERMES_DebugLog("CSMA Start");
        HERMES_CSMA_Transmit(frame.raw, sizeof(frame.raw), HM_PRIO_LOW);
    // For non-ARQ (broadcast): always clear pending after attempt. 
    // Need to find the message in the array again because m_ptr might have moved!
    for (int i = 0; i < gHermesMsgCount; i++) {
        if (memcmp(gHermesMessages[i].packet_id, m.packet_id, HM_PACKET_ID_SIZE) == 0) {
            gHermesMessages[i].is_pending = false;
            break;
        }
    }
    gUpdateDisplay = true;
}
#endif
}

void HERMES_DebugLog(const char *text) {
    if (!gHermesConfig.debug) return;

    // Shift messages if full to act as a circular buffer
    if (gHermesMsgCount >= HM_MSG_SLOTS) {
        for (int i = 0; i < HM_MSG_SLOTS - 1; i++) {
            gHermesMessages[i] = gHermesMessages[i + 1];
        }
        gHermesMsgCount = HM_MSG_SLOTS - 1;
    }

    HermesMessage_t *m = &gHermesMessages[gHermesMsgCount++];
    memset(m, 0, sizeof(*m));
    m->is_debug = true;
    m->is_read = true;
    m->is_outgoing = false;
    m->len = HERMES_MSG_PackGSM7(text, strlen(text), m->payload);
    
    gHermesHasNewMessage = true;
    gUpdateDisplay = true;
}

// ═══════════════════════════════════════════════════════════
// FSK Interrupt Handler
// ═══════════════════════════════════════════════════════════

// ──── RX LED State ────
static uint16_t rx_led_timeout_ms = 0;

void HERMES_HandleFSKInterrupt(uint16_t interrupt_bits) {
    if (interrupt_bits & BK4819_REG_02_FSK_RX_SYNC) {
        rx_write_idx = 0;
        memset(rx_buffer, 0, sizeof(rx_buffer));
        // Mute speaker during FSK data reception (kamil: AUDIO_AudioPathOff on rx_sync)
        if (gHermesConfig.fsk_mute)
            AUDIO_AudioPathOff();
    }

    if (interrupt_bits & BK4819_REG_02_FSK_FIFO_ALMOST_FULL) {
        // Toggle green LED only if unsquelched (receiving actual data)
        if (gCurrentFunction == FUNCTION_RECEIVE) {
            BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
            rx_led_timeout_ms = 100; // blink duration
        }
        uint16_t rem = HM_FRAME_SIZE - rx_write_idx;
        if (rem > 0)
            rx_write_idx += HERMES_PHY_ReadFIFO(rx_buffer + rx_write_idx, HM_FRAME_SIZE, rx_write_idx);
    }

    if (interrupt_bits & BK4819_REG_02_FSK_RX_FINISHED) {
        if (rx_write_idx >= HM_FRAME_SIZE) {
            ProcessReceivedFrame();
        }
        rx_write_idx = 0;
        HERMES_PHY_StartRx();
        // Restore speaker after FSK reception complete
        if (gHermesConfig.fsk_mute)
            AUDIO_AudioPathOn();
    }
}

// ═══════════════════════════════════════════════════════════
// Periodic Tick
// ═══════════════════════════════════════════════════════════

bool gHermesSendTrigger = false;

void HERMES_Tick(void) {
    if (!gHermesEnabled) return;

    if (gHermesSendTrigger) {
        gHermesSendTrigger = false;
        // Find the newest pending message and send it
        if (gHermesMsgCount > 0) {
            HERMES_SendMessage(&gHermesMessages[gHermesMsgCount - 1]);
        }
    }

#ifdef ENABLE_HERMES_ROUTER
    // Process forward queue
    HERMES_Route_Tick(SYSTICK_GetTick());
#endif

#ifdef ENABLE_HERMES_MESSENGER
    // Process ARQ timeouts and retries
    HERMES_ARQ_Tick(SYSTICK_GetTick());
#endif

    if (rx_led_timeout_ms > 0) {
        // approximate tick depending on app scheduler speed (usually 10ms)
        if (rx_led_timeout_ms <= 10) {
            rx_led_timeout_ms = 0;
            BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
        } else {
            rx_led_timeout_ms -= 10;
        }
    }

    if ((gHermesView == HM_VIEW_COMPOSE ||
         gHermesView == HM_VIEW_INPUT_ALIAS ||
         gHermesView == HM_VIEW_INPUT_PASSCODE ||
         gHermesView == HM_VIEW_INPUT_SALT ||
         gHermesView == HM_VIEW_INPUT_CONTACT) && TextInput_IsActive()) {
        if (TextInput_Tick()) gUpdateDisplay = true;
    }
}

#endif // ENABLE_MESH_NETWORK
