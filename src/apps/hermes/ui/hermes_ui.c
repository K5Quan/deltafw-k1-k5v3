/* Hermes UI — From scratch
 *
 * Handles: Main menu, chat with bubbles, compose (TextInput), settings.
 * Uses AG_MENU for menu views, custom rendering for chat bubbles.
 * Follows the same dispatch pattern as LAUNCHER/CW_KEYER apps.
 */
#ifdef ENABLE_MESH_NETWORK

#include "apps/hermes/app/messaging.h"
#include "apps/hermes/ui/hermes_ui.h"
#include "apps/hermes/hermes.h"
#include "apps/hermes/hermes_types.h"
#include "apps/hermes/network/addressing.h"
#include "ui/ag_menu.h"
#include "ui/ag_graphics.h"
#include "ui/textinput.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "core/misc.h"
#include "drivers/bsp/st7565.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

// ──── Global UI State ────
HermesView_t gHermesView      = HM_VIEW_MENU;
int16_t      gHermesChatScroll = -1;
uint8_t      composeDestID[HM_NODE_ID_SIZE];
uint8_t      composeAddrMode   = 2; // Default HM_ADDR_BROADCAST

// ──── Compose Buffer ────
static char composeBuffer[HM_MSG_MAX_TEXT + 1];

// ──── Forward decls ────
static void OnComposeDone(void);

// ═══════════════════════════════════════════════════════════
// Chat Bubble Rendering
// ═══════════════════════════════════════════════════════════

static bool IsSameSender(int16_t a, int16_t b) {
    if (a < 0 || a >= gHermesMsgCount || b < 0 || b >= gHermesMsgCount) return false;
    if (gHermesMessages[a].is_outgoing != gHermesMessages[b].is_outgoing) return false;
    if (gHermesMessages[a].is_outgoing) {
        return (gHermesMessages[a].addressing == gHermesMessages[b].addressing &&
                memcmp(gHermesMessages[a].addr, gHermesMessages[b].addr, HM_NODE_ID_SIZE) == 0);
    }
    return memcmp(gHermesMessages[a].addr, gHermesMessages[b].addr, HM_NODE_ID_SIZE) == 0;
}

static bool IsTiny(const HermesMessage_t *m) { 
    return (m->len > 22) || m->is_debug; 
}

static void GetLine(const char *txt, uint16_t tlen, uint8_t maxc, uint8_t idx,
                    uint16_t *out_start, uint16_t *out_len) {
    uint16_t pos = 0;
    uint8_t cur = 0;
    while (pos < tlen) {
        uint16_t ls = pos, ll = 0, sp = 0xFFFF;
        while (pos < tlen && ll < maxc) { if (txt[pos] == ' ') sp = pos; pos++; ll++; }
        if (pos < tlen && txt[pos] != ' ' && sp != 0xFFFF && sp >= ls) { pos = sp + 1; ll = pos - ls; }
        if (cur == idx) {
            while (ll > 0 && txt[ls + ll - 1] == ' ') ll--;
            *out_start = ls; *out_len = ll; return;
        }
        while (pos < tlen && txt[pos] == ' ') pos++;
        cur++;
    }
    *out_start = 0; *out_len = 0;
}

static uint8_t CountLines(const char *txt, uint8_t len, bool tiny) {
    uint8_t maxc = tiny ? 26 : 14;
    uint16_t pos = 0;
    uint8_t n = 0;
    while (pos < len && n < 8) {
        uint16_t ll = 0, sp = 0xFFFF, ls = pos;
        while (pos < len && ll < maxc) { if (txt[pos] == ' ') sp = pos; pos++; ll++; }
        if (pos < len && txt[pos] != ' ' && sp != 0xFFFF && sp >= ls) pos = sp + 1;
        n++;
        while (pos < len && txt[pos] == ' ') pos++;
        if (ll == 0) break;
    }
    return n == 0 ? 1 : n;
}

static uint8_t BubbleWidthEx(const char *txt, uint8_t len, bool tiny) {
    uint8_t lines = CountLines(txt, len, tiny);
    uint8_t w = 112;
    if (lines == 1) {
        uint16_t s, l;
        GetLine(txt, len, tiny ? 26 : 14, 0, &s, &l);
        w = l * (tiny ? 4 : 8) + 8;
    }
    return (w > 112) ? 112 : w;
}

static uint8_t BubbleHeight(int16_t i) {
    const HermesMessage_t *m = &gHermesMessages[i];
    char text[HM_MSG_MAX_TEXT + 1];
    uint8_t len = HERMES_MSG_UnpackGSM7(m->payload, HM_PAYLOAD_SIZE, text, HM_MSG_MAX_TEXT + 1);
    bool tiny = IsTiny(m);
    uint8_t lines = CountLines(text, len, tiny);
    uint8_t h = tiny ? (lines * 6 + 3) : (lines * 12 - 1);
    if (!IsSameSender(i, i + 1)) h += 2; else h -= 1;
    if (!IsSameSender(i, i - 1)) {
        if (!m->is_outgoing || m->addressing != HM_ADDR_BROADCAST) h += 8;
    }
    return h < 1 ? 1 : h;
}

static void UI_DrawSentTick(uint8_t x, uint8_t y) {
    // 5x4 single checkmark
    static const uint8_t sent_bits[] = {0x10,0x08,0x05,0x02};
    for (uint8_t r = 0; r < 4; r++) {
        for (uint8_t c = 0; c < 5; c++) {
            if (sent_bits[r] & (1 << c)) {
                AG_PutPixel(x + c, y + r, 1);
            }
        }
    }
}

static void UI_DrawClockIcon(uint8_t x, uint8_t y) {
    static const uint8_t clock_bits[] = {0x0e, 0x19, 0x15, 0x11, 0x0e};
    for (uint8_t r = 0; r < 5; r++) {
        for (uint8_t c = 0; c < 5; c++) {
            if (clock_bits[r] & (1 << c)) {
                AG_PutPixel(x + c, y + r, 1);
            }
        }
    }
}

static void UI_DrawAckTick(uint8_t x, uint8_t y) {
    // 9x4 double checkmark
    static const uint8_t double_tick[] = {0x10,0x01,0x88,0x00,0x55,0x00,0x22,0x00};
    for (uint8_t r = 0; r < 4; r++) {
        uint16_t row_bits = double_tick[r * 2] | (double_tick[r * 2 + 1] << 8);
        for (uint8_t c = 0; c < 9; c++) {
            if (row_bits & (1 << c)) {
                AG_PutPixel(x + c, y + r, 1);
            }
        }
    }
}

static void RenderChat(void) {
    if (gHermesMsgCount == 0) {
        AG_PrintSmallEx(64, 28, POS_C, C_FILL, "No messages");
        AG_PrintSmallEx(64, 58, POS_C, C_FILL, "[M] Write  [EXIT] Back");
        return;
    }
    if (gHermesChatScroll < 0) gHermesChatScroll = gHermesMsgCount - 1;

    // Find first visible message by stacking from scroll position upward
    const uint8_t area = 44;
    uint8_t total = 0;
    int16_t start = gHermesChatScroll;
    
    // Calculate how many messages fit on screen starting from scroll and going backwards
    while (start >= 0) {
        uint8_t h = BubbleHeight(start);
        if (total + h > area && start != gHermesChatScroll) {
            start++; // The previous one did not fit, so start at the next one
            break;
        }
        total += h;
        if (start == 0) break;
        start--;
    }
    
    // We now start drawing all available messages from `start` until screen is full or count reached
    // We now start drawing all available messages from `start` until screen is full or count reached
    uint8_t y = 8;
    for (int16_t i = start; i < gHermesMsgCount && y < 54; i++) {
        HermesMessage_t *m = &gHermesMessages[i];
        char mtext[HM_MSG_MAX_TEXT + 1];
        uint8_t mlen = HERMES_MSG_UnpackGSM7(m->payload, HM_PAYLOAD_SIZE, mtext, HM_MSG_MAX_TEXT + 1);

        bool sel = (i == gHermesChatScroll);
        bool tiny = IsTiny(m);
        uint8_t lines = CountLines(mtext, mlen, tiny);
        uint8_t bh = tiny ? (lines * 6 + 3) : (lines * 12 - 1);
        uint8_t bw = BubbleWidthEx(mtext, mlen, tiny);
        uint8_t bx = m->is_outgoing ? (124 - bw) : 2;
        uint8_t by = y;

        // Sender label for incoming
        if (!m->is_outgoing && !IsSameSender(i, i - 1)) {
            char name[24] = {0};
            if (m->is_debug) {
                strcpy(name, "$debug");
            } else {
                bool found = false;
                name[0] = '@';
                // Scan contacts
                for (uint8_t j = 0; j < 16; j++) {
                    HermesContactRecord_t rec;
                    if (Storage_ReadRecordIndexed(REC_HERMES_CONTACTS, j, &rec, 0, sizeof(rec))) {
                        if (rec.fields.NodeID[0] != 0xFF && memcmp(rec.fields.NodeID, m->addr, 6) == 0) {
                            strncpy(name + 1, rec.fields.Alias, 12);
                            name[13] = '\0';
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    char temp[20];
                    HERMES_Addr_Format(m->addr, 0, temp, sizeof(temp));
                    strncpy(name + 1, temp, 22);
                    name[23] = '\0';
                }
            }
            UI_PrintStringSmallest(name, 2, y - 7, false, true);
            by += 8;
        }
        
        // Destination label for outgoing (if not broadcast)
        if (m->is_outgoing && m->addressing != HM_ADDR_BROADCAST && !IsSameSender(i, i - 1)) {
            char name[16] = "To:";
            char target[12];
            HERMES_Addr_Decode(m->addr, target, sizeof(target));
            strcat(name, target);
            UI_PrintStringSmallest(name, bx, by - 7, false, true);
            by += 8;
        }

        // Unread dot
        if (!m->is_outgoing && !m->is_read)
            AG_FillRect(1, by + 4, 2, 2, C_FILL);

        // Bubble outline/fill
        if (sel && !m->is_pending) {
            AG_FillRect(bx, by, bw, bh, C_FILL);
        } else {
            // Draw Outline (for non-selected or pending outgoing)
            AG_DrawVLine(bx, by, bh, C_FILL); 
            AG_DrawVLine(bx + bw - 1, by, bh, C_FILL);
            
            if (!IsSameSender(i, i - 1)) AG_DrawHLine(bx, by, bw, C_FILL);
            else {
                char ptxt[HM_MSG_MAX_TEXT + 1];
                uint8_t plen = HERMES_MSG_UnpackGSM7(gHermesMessages[i-1].payload, HM_PAYLOAD_SIZE, ptxt, HM_MSG_MAX_TEXT + 1);
                uint8_t pbw = BubbleWidthEx(ptxt, plen, IsTiny(&gHermesMessages[i-1]));
                uint8_t pbx = gHermesMessages[i - 1].is_outgoing ? (124 - pbw) : 2;
                if (bx < pbx) AG_DrawHLine(bx, by, pbx - bx, C_FILL);
                if (bx + bw > pbx + pbw) AG_DrawHLine(pbx + pbw, by, (bx + bw) - (pbx + pbw), C_FILL);
            }
            
            if (!IsSameSender(i, i + 1)) AG_DrawHLine(bx, by + bh - 1, bw, C_FILL);
            else {
                char ntxt[HM_MSG_MAX_TEXT + 1];
                uint8_t nlen = HERMES_MSG_UnpackGSM7(gHermesMessages[i+1].payload, HM_PAYLOAD_SIZE, ntxt, HM_MSG_MAX_TEXT + 1);
                uint8_t nbw = BubbleWidthEx(ntxt, nlen, IsTiny(&gHermesMessages[i+1]));
                uint8_t nbx = gHermesMessages[i + 1].is_outgoing ? (124 - nbw) : 2;
                if (bx < nbx) AG_DrawHLine(bx, by + bh - 1, nbx - bx, C_FILL);
                if (bx + bw > nbx + nbw) AG_DrawHLine(nbx + nbw, by + bh - 1, (bx + bw) - (nbx + nbw), C_FILL);
            }
        }

        // ACK tick / pending / failed indicators
        if (m->is_outgoing) {
            uint8_t indicator_x = bx - 14;
            uint8_t indicator_y = by + bh - 6;

            if (m->is_pending) {
                // Pending: Clock icon
                UI_DrawClockIcon(bx - 8, indicator_y);
            } else if (m->is_acked) {
                // Acked: Double check
                UI_DrawAckTick(bx - 10, indicator_y);
            } else if (m->addressing != HM_ADDR_BROADCAST && gHermesConfig.ack_mode > 0) {
                // Failed (was expecting ACK but didn't get it and max retries reached)
                UI_PrintStringSmallest("X", indicator_x + 7, indicator_y, false, true);
            } else {
                // Sent
                UI_DrawSentTick(bx - 7, indicator_y);
            }
        }

        // Text
        uint8_t maxc = tiny ? 26 : 14;
        // Selection Negation: If sel && not pending, tc is C_CLEAR (white on black bubble)
        Color tc = (sel && !m->is_pending) ? C_CLEAR : C_FILL;

        for (uint8_t l = 0; l < lines; l++) {
            uint16_t ls, ll;
            GetLine(mtext, mlen, maxc, l, &ls, &ll);
            if (ll == 0 && l > 0) continue;
            char buf[32];
            uint8_t n = (ll > 31) ? 31 : (uint8_t)ll;
            memcpy(buf, mtext + ls, n); buf[n] = '\0';
            
            if (tiny) {
                // UI_PrintStringSmallest: fill=true draws white on black.
                // We want fill=true if tc is C_CLEAR (negated)
                UI_PrintStringSmallest(buf, bx + 4, by + 2 + l * 6, false, (tc == C_CLEAR));
            } else {
                AG_PrintMediumEx(bx + 4, by + 8 + l * 12, POS_L, tc, buf);
            }
        }

        y += BubbleHeight(i);
    }

    // Scrollbar
    AG_DrawVLine(126, 10, 44, C_FILL);
    if (gHermesMsgCount > 1) {
        uint8_t ty = 10 + (uint8_t)((uint16_t)gHermesChatScroll * 41 / (gHermesMsgCount - 1));
        AG_FillRect(125, ty, 3, 3, C_FILL);
    }
    UI_PrintStringSmallest("[M]Write [\\x01\\x02]Scroll [X]Back", 0, 58, false, true);
}

// ═══════════════════════════════════════════════════════════
// Compose Flow
// ═══════════════════════════════════════════════════════════

static void OnComposeDone(void) {
    uint8_t len = (uint8_t)strlen(composeBuffer);
    TextInput_Deinit();
    gHermesView = HM_VIEW_CHAT;

    if (len > 0) {
        // Shift messages if full to keep newest at the bottom
        if (gHermesMsgCount >= HM_MSG_SLOTS) {
            for (int i = 0; i < HM_MSG_SLOTS - 1; i++) {
                gHermesMessages[i] = gHermesMessages[i + 1];
            }
            gHermesMsgCount = HM_MSG_SLOTS - 1;
        }

        HermesMessage_t *m = &gHermesMessages[gHermesMsgCount++];
        memset(m, 0, sizeof(*m));
        memcpy(m->addr, composeDestID, HM_NODE_ID_SIZE); 
        m->addressing = composeAddrMode;
        m->len = HERMES_MSG_PackGSM7(composeBuffer, len, m->payload);
        m->is_outgoing = true;
        m->is_pending = true;
        m->is_debug = false;
        
        // Auto-select the newly sent message immediately 
        // to provide visual feedback that it is pending
        gHermesChatScroll = gHermesMsgCount - 1;
        
        // Trigger TX pipeline (deferred so it doesn't block UI)
        gHermesSendTrigger = true;
    }
    memset(composeBuffer, 0, sizeof(composeBuffer));
    gUpdateDisplay = true;
}

void HERMES_UI_StartCompose(void) {
    memset(composeBuffer, 0, sizeof(composeBuffer));
    // Default to broadcast if not already setup (e.g. via menu shortcut if we added one)
    if (gHermesView != HM_VIEW_CONTACTS) {
        memset(composeDestID, 0xFF, HM_NODE_ID_SIZE);
        composeAddrMode = HM_ADDR_BROADCAST;
    }
    gHermesView = HM_VIEW_COMPOSE;
    TextInput_InitEx(composeBuffer, HM_MSG_MAX_TEXT, true, true, false, true, OnComposeDone);
    gUpdateDisplay = true;
}

// ═══════════════════════════════════════════════════════════
// Settings Menu — Full Config UI
// ═══════════════════════════════════════════════════════════
#include "features/storage/storage.h"
#include "apps/hermes/security/kdf.h"

// ──── Input Buffers (Consolidated to save RAM) ────
static char HM_InputBuffer[33]; 

// ──── Contact Management State ────
static HermesContact_t tempContact;
static uint8_t         contactStep;    // 0=Alias, 1=ID, 2=Secret
static uint8_t         selContactIdx;
static MenuItem        contactMenuItems[17]; // 16 contacts + "Add New"
static Menu            contactMenu;
static void BytesToHex(const uint8_t *src, uint8_t len, char *dst) {
    const char *hex = "0123456789ABCDEF";
    for (uint8_t i = 0; i < len; i++) {
        dst[i * 2]     = hex[(src[i] >> 4) & 0x0F];
        dst[i * 2 + 1] = hex[src[i] & 0x0F];
    }
    dst[len * 2] = '\0';
}

static uint8_t HexCharVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    return 0;
}

static void HexToBytes(const char *hex, uint8_t *dst, uint8_t max_bytes) {
    uint8_t len = (uint8_t)strlen(hex);
    memset(dst, 0, max_bytes);
    uint8_t bi = 0;
    for (uint8_t i = 0; i + 1 < len && bi < max_bytes; i += 2, bi++) {
        dst[bi] = (HexCharVal(hex[i]) << 4) | HexCharVal(hex[i + 1]);
    }
}

// ──── Save All Settings to Flash ────
static void SaveSettings(void) {
    HermesSettings_t settings;
    memset(&settings, 0, sizeof(settings));
    settings.fields.Magic = 0x484D;

    // Identity
    settings.fields.MacPolicy = gHermesConfig.mac_policy;
    memcpy(settings.fields.CustomMac, gHermesConfig.node_id, 6);
    strncpy(settings.fields.Alias, gHermesConfig.alias, 12);
    settings.fields.Alias[12] = '\0';

    // Security
    strncpy(settings.fields.Passcode, gHermesConfig.passcode, 32);
    memcpy(settings.fields.Salt, gHermesConfig.salt, 16);
    settings.fields.HasPasscode = (gHermesConfig.passcode[0] != '\0');

    // Network
    settings.fields.FreqMode = gHermesConfig.freq_mode;
    settings.fields.FreqCh = gHermesConfig.freq_ch;
    settings.fields.Enabled = gHermesConfig.enabled;
    settings.fields.RelayEnabled = gHermesConfig.relay_enabled;
    settings.fields.AckMode = gHermesConfig.ack_mode;
    settings.fields.TTL = gHermesConfig.ttl;
    settings.fields.TxPower = gHermesConfig.tx_power;
    settings.fields.FskMute = gHermesConfig.fsk_mute ? 1 : 0;

    Storage_WriteRecord(REC_HERMES_SETTINGS, &settings, 0, sizeof(HermesSettings_t));
}

// ──── Re-derive K_net after passcode/salt change ────
static void RederiveNetKey(void) {
    HERMES_KDF_DeriveNetworkKey(gHermesConfig.passcode, gHermesConfig.salt, gHermesConfig.net_key);
}

// ──── Contact Input Callbacks ────
static void OnContactSecretDone(void) {
    TextInput_Deinit();
    strncpy(tempContact.passcode, HM_InputBuffer, 32);
    tempContact.passcode[32] = '\0';
    
    // Save to first empty slot or selContactIdx if editing (we'll do Add for now)
    HermesContactRecord_t rec;
    memset(&rec.raw, 0xFF, sizeof(rec.raw));
    memcpy(rec.fields.NodeID, tempContact.node_id, 6);
    strncpy(rec.fields.Alias, tempContact.alias, 12);
    strncpy(rec.fields.Passcode, tempContact.passcode, 32);
    rec.fields.Flags = tempContact.flags;
    
    Storage_WriteRecordIndexed(REC_HERMES_CONTACTS, selContactIdx, &rec, 0, sizeof(rec));
    gHermesView = HM_VIEW_CONTACTS;
    HERMES_UI_Init(); // Refresh menu
}

static void OnContactIDDone(void) {
    TextInput_Deinit();
    HexToBytes(HM_InputBuffer, tempContact.node_id, 6);
    
    contactStep = 2;
    memset(HM_InputBuffer, 0, sizeof(HM_InputBuffer));
    gHermesView = HM_VIEW_INPUT_CONTACT;
    TextInput_InitEx(HM_InputBuffer, 32, true, true, false, false, OnContactSecretDone);
}

static void OnContactAliasDone(void) {
    TextInput_Deinit();
    strncpy(tempContact.alias, HM_InputBuffer, 12);
    tempContact.alias[12] = '\0';
    
    contactStep = 1;
    memset(HM_InputBuffer, 0, sizeof(HM_InputBuffer));
    gHermesView = HM_VIEW_INPUT_CONTACT;
    TextInput_InitEx(HM_InputBuffer, 12, true, true, false, false, OnContactIDDone);
}

// ──── Text Input Callbacks ────
static void OnAliasDone(void) {
    TextInput_Deinit();
    strncpy(gHermesConfig.alias, HM_InputBuffer, 12);
    gHermesConfig.alias[12] = '\0';

    // Re-encode node ID if policy is alias-based
    if (gHermesConfig.mac_policy == 2) {
        HERMES_Addr_Encode(gHermesConfig.alias, gHermesConfig.node_id);
    }

    SaveSettings();
    gHermesView = HM_VIEW_SETTINGS;
    gUpdateDisplay = true;
}

static void OnPasscodeDone(void) {
    TextInput_Deinit();
    strncpy(gHermesConfig.passcode, HM_InputBuffer, 32);
    gHermesConfig.passcode[32] = '\0';
    RederiveNetKey();
    SaveSettings();
    gHermesView = HM_VIEW_SETTINGS;
    gUpdateDisplay = true;
}

static void OnSaltDone(void) {
    TextInput_Deinit();
    HexToBytes(HM_InputBuffer, gHermesConfig.salt, 16);
    RederiveNetKey();
    SaveSettings();
    gHermesView = HM_VIEW_SETTINGS;
    gUpdateDisplay = true;
}

// ──── Getters / Setters ────
static void GetEnabled(const MenuItem *i, char *b, uint8_t s)  { (void)i; (void)s; strcpy(b, gHermesConfig.enabled ? "ON" : "OFF"); }
static void SetEnabled(const MenuItem *i, bool u)              { (void)i; (void)u; gHermesConfig.enabled ^= 1; gHermesEnabled = gHermesConfig.enabled; SaveSettings(); }
 
static void GetDebug(const MenuItem *i, char *b, uint8_t s)    { (void)i; (void)s; strcpy(b, gHermesConfig.debug ? "ON" : "OFF"); }
static void SetDebug(const MenuItem *i, bool u)                { (void)i; (void)u; gHermesConfig.debug ^= 1; SaveSettings(); }

static void GetFskMute(const MenuItem *i, char *b, uint8_t s)  { (void)i; (void)s; strcpy(b, gHermesConfig.fsk_mute ? "MUTE" : "HEAR"); }
static void SetFskMute(const MenuItem *i, bool u)              { (void)i; (void)u; gHermesConfig.fsk_mute ^= 1; SaveSettings(); }

static void GetNodeID(const MenuItem *i, char *b, uint8_t s) {
    (void)i; (void)s;
    // Show as MAC with : separators when using HW or Custom policy
    if (gHermesConfig.mac_policy != 2) {
        // Show full MAC for menu: XX:XX:XX:XX:XX:XX
        HERMES_Addr_FormatMAC(gHermesConfig.node_id, b, s);
    } else {
        // Alias-based: show decoded callsign
        HERMES_Addr_Decode(gHermesConfig.node_id, b, s);
    }
}

static void GetAlias(const MenuItem *i, char *b, uint8_t s) {
    (void)i; (void)s;
    strncpy(b, gHermesConfig.alias, 12); b[12] = '\0';
}
static bool MA_EditAlias(const MenuItem *item, KEY_Code_t key, bool kp, bool kh) {
    (void)item;
    if (key != KEY_MENU) return false;
    if (!kp || kh) return true;
    memset(HM_InputBuffer, 0, sizeof(HM_InputBuffer));
    strncpy(HM_InputBuffer, gHermesConfig.alias, 12);
    gHermesView = HM_VIEW_INPUT_ALIAS;
    TextInput_InitEx(HM_InputBuffer, 12, true, true, false, false, OnAliasDone);
    gUpdateDisplay = true;
    return true;
}



static void GetPasscode(const MenuItem *i, char *b, uint8_t s) {
    (void)i; (void)s;
    uint8_t len = (uint8_t)strlen(gHermesConfig.passcode);
    if (len == 0) { strcpy(b, "(none)"); return; }
    uint8_t show = len > 10 ? 10 : len;
    memcpy(b, gHermesConfig.passcode, show);
    b[show] = '\0';
}
static bool MA_EditPasscode(const MenuItem *item, KEY_Code_t key, bool kp, bool kh) {
    (void)item;
    if (key != KEY_MENU) return false;
    if (!kp || kh) return true;
    memset(HM_InputBuffer, 0, sizeof(HM_InputBuffer));
    strncpy(HM_InputBuffer, gHermesConfig.passcode, 32);
    gHermesView = HM_VIEW_INPUT_PASSCODE;
    TextInput_InitEx(HM_InputBuffer, 32, true, true, false, false, OnPasscodeDone);
    gUpdateDisplay = true;
    return true;
}

static void GetSalt(const MenuItem *i, char *b, uint8_t s) {
    (void)i; (void)s;
    // Show first 8 hex chars (4 bytes) of the 16-byte salt
    BytesToHex(gHermesConfig.salt, 4, b);
}
static bool MA_EditSalt(const MenuItem *item, KEY_Code_t key, bool kp, bool kh) {
    (void)item;
    if (key != KEY_MENU) return false;
    if (!kp || kh) return true;
    memset(HM_InputBuffer, 0, sizeof(HM_InputBuffer));
    BytesToHex(gHermesConfig.salt, 16, HM_InputBuffer);
    gHermesView = HM_VIEW_INPUT_SALT;
    TextInput_InitEx(HM_InputBuffer, 32, true, true, false, false, OnSaltDone);
    gUpdateDisplay = true;
    return true;
}

static const char* FreqModes[] = {"LPD66", "VFO", "MEM"};
static void GetFreqMode(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; strcpy(b, FreqModes[gHermesConfig.freq_mode]); }
static void SetFreqMode(const MenuItem *i, bool u) { (void)i; gHermesConfig.freq_mode = u ? (gHermesConfig.freq_mode==2?0:gHermesConfig.freq_mode+1) : (gHermesConfig.freq_mode==0?2:gHermesConfig.freq_mode-1); HERMES_UpdateFrequency(); SaveSettings(); }
 
static void GetRelay(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; strcpy(b, gHermesConfig.relay_enabled ? "ON" : "OFF"); }
static void SetRelay(const MenuItem *i, bool u) { (void)i; (void)u; gHermesConfig.relay_enabled ^= 1; SaveSettings(); }
 
static const char* AckModes[] = {"OFF", "MANUAL", "AUTO"};
static void GetAck(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; strcpy(b, AckModes[gHermesConfig.ack_mode]); }
static void SetAck(const MenuItem *i, bool u) { (void)i; (void)u; gHermesConfig.ack_mode = (gHermesConfig.ack_mode + 1) % 3; SaveSettings(); }

static const char* MacPolicies[] = {"HW", "CUSTOM", "ALIAS"};
static void GetMacPolicy(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; strcpy(b, MacPolicies[gHermesConfig.mac_policy]); }
static void SetMacPolicy(const MenuItem *i, bool u) {
    (void)i;
    gHermesConfig.mac_policy = u ? (gHermesConfig.mac_policy==2?0:gHermesConfig.mac_policy+1) : (gHermesConfig.mac_policy==0?2:gHermesConfig.mac_policy-1);
    // Re-derive node_id based on new policy
    if (gHermesConfig.mac_policy == 2) {
        HERMES_Addr_Encode(gHermesConfig.alias, gHermesConfig.node_id);
    } else if (gHermesConfig.mac_policy == 0) {
        uint8_t *uid = (uint8_t *)0x1FFF0E00;
        gHermesConfig.node_id[0] = uid[0] ^ uid[6];
        gHermesConfig.node_id[1] = uid[1] ^ uid[7];
        gHermesConfig.node_id[2] = uid[2] ^ uid[8];
        gHermesConfig.node_id[3] = uid[3] ^ uid[9];
        gHermesConfig.node_id[4] = uid[4] ^ uid[10];
        gHermesConfig.node_id[5] = uid[5] ^ uid[11];
    }
    SaveSettings();
    // Rebuild settings menu to update visible items
    HERMES_UI_Init();
}

static const char* TxPowers[] = {"LOW", "MID", "HIGH"};
static void GetTxPwr(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; strcpy(b, TxPowers[gHermesConfig.tx_power]); }
static void SetTxPwr(const MenuItem *i, bool u) { (void)i; gHermesConfig.tx_power = u ? (gHermesConfig.tx_power==2?0:gHermesConfig.tx_power+1) : (gHermesConfig.tx_power==0?2:gHermesConfig.tx_power-1); SaveSettings(); }

static void GetTTL(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; NUMBER_ToDecimal(b, gHermesConfig.ttl, 2, false); }
static void SetTTL(const MenuItem *i, bool u) { (void)i; if (u) { if (gHermesConfig.ttl < 15) gHermesConfig.ttl++; } else { if (gHermesConfig.ttl > 1) gHermesConfig.ttl--; } SaveSettings(); }

// All possible settings items (superset)
static const MenuItem settingsItems[] = {
    // ── General ──
    { .name = "Hermes",      .get_value_text = GetEnabled,    .change_value = SetEnabled,    .type = M_ITEM_SELECT },
    // ── Identity ──
    { .name = "ID Source",   .get_value_text = GetMacPolicy,  .change_value = SetMacPolicy,  .type = M_ITEM_SELECT },
    { .name = "Alias",       .get_value_text = GetAlias,      .action = MA_EditAlias,        .type = M_ITEM_SELECT },
    { .name = "Node ID",     .get_value_text = GetNodeID,     .type = M_ITEM_SELECT },
    // ── Security ──
    { .name = "Passcode",    .get_value_text = GetPasscode,   .action = MA_EditPasscode,     .type = M_ITEM_SELECT },
    { .name = "Salt",        .get_value_text = GetSalt,       .action = MA_EditSalt,         .type = M_ITEM_SELECT },
    // ── Radio ──
    { .name = "Frequency",   .get_value_text = GetFreqMode,   .change_value = SetFreqMode,   .type = M_ITEM_SELECT },
    { .name = "TX Power",    .get_value_text = GetTxPwr,      .change_value = SetTxPwr,      .type = M_ITEM_SELECT },
    { .name = "TTL",         .get_value_text = GetTTL,        .change_value = SetTTL,        .type = M_ITEM_SELECT },
    // ── Mesh ──
    { .name = "Relay",       .get_value_text = GetRelay,      .change_value = SetRelay,      .type = M_ITEM_SELECT },
    { .name = "ACK Mode",    .get_value_text = GetAck,        .change_value = SetAck,        .type = M_ITEM_SELECT },
    { .name = "Debug Mode",  .get_value_text = GetDebug,      .change_value = SetDebug,      .type = M_ITEM_SELECT },
    { .name = "FSK Audio",   .get_value_text = GetFskMute,    .change_value = SetFskMute,    .type = M_ITEM_SELECT },
};
static Menu settingsMenu = {
    .title = "Settings", .items = settingsItems, .num_items = sizeof(settingsItems) / sizeof(settingsItems[0]),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};


// ──── Contact Menu Logic ────
static bool MA_ContactAction(const MenuItem *i, KEY_Code_t k, bool kp, bool kh) {
    if (k != KEY_MENU) return false;
    if (!kp || kh) return true;
    
    uint8_t idx = i - contactMenuItems;
    if (idx == contactMenu.num_items - 1) { // "Add New"
        // Find first free slot
        uint8_t free_idx = 0xFF;
        for (uint8_t j=0; j<16; j++) {
            HermesContactRecord_t rec;
            Storage_ReadRecordIndexed(REC_HERMES_CONTACTS, j, &rec, 0, sizeof(rec));
            if (rec.fields.NodeID[0] == 0xFF) { free_idx = j; break; }
        }
        if (free_idx == 0xFF) return true; // Full
        selContactIdx = free_idx;
        memset(&tempContact, 0, sizeof(tempContact));
        contactStep = 0;
        memset(HM_InputBuffer, 0, sizeof(HM_InputBuffer));
        gHermesView = HM_VIEW_INPUT_CONTACT;
        TextInput_InitEx(HM_InputBuffer, 12, true, true, false, false, OnContactAliasDone);
    } else {
        // Compose to this contact
        HermesContactRecord_t rec;
        Storage_ReadRecordIndexed(REC_HERMES_CONTACTS, idx, &rec, 0, sizeof(rec));
        memcpy(composeDestID, rec.fields.NodeID, HM_NODE_ID_SIZE);
        composeAddrMode = (rec.fields.Flags & HM_CONTACT_FLAG_MULTICAST) ? HM_ADDR_MULTICAST : HM_ADDR_UNICAST;
        HERMES_UI_StartCompose();
    }
    return true;
}

bool MA_DeleteContact(const MenuItem *i, KEY_Code_t k, bool kp, bool kh) {
    (void)i;
    if (k != KEY_SIDE1) return false; // Side 1 to delete
    if (!kp || kh) return true;
    uint8_t idx = contactMenu.i;
    if (idx < contactMenu.num_items - 1) {
        HermesContactRecord_t rec;
        memset(&rec.raw, 0xFF, sizeof(rec.raw));
        Storage_WriteRecordIndexed(REC_HERMES_CONTACTS, idx, &rec, 0, sizeof(rec));
        HERMES_UI_Init(); // Refresh
    }
    return true;
}

static void GetContactIDStr(const MenuItem *i, char *b, uint8_t s) {
    uint8_t idx = i - contactMenuItems;
    HermesContactRecord_t rec;
    Storage_ReadRecordIndexed(REC_HERMES_CONTACTS, idx, &rec, 0, sizeof(rec));
    // Format as full MAC XX:XX:XX:XX:XX:XX
    HERMES_Addr_FormatMAC(rec.fields.NodeID, b, s);
}

static void GetContactName(const MenuItem *i, char *b, uint8_t s) {
    uint8_t idx = i - contactMenuItems;
    if (idx == contactMenu.num_items - 1) {
        strncpy(b, "[Add New]", s);
        return;
    }
    HermesContactRecord_t rec;
    Storage_ReadRecordIndexed(REC_HERMES_CONTACTS, idx, &rec, 0, sizeof(rec));
    // Guard against corrupt alias data: ensure null termination
    rec.fields.Alias[12] = '\0';
    // Verify alias contains printable chars, otherwise show as ID
    bool valid = true;
    for (uint8_t j = 0; rec.fields.Alias[j] && j < 12; j++) {
        if (rec.fields.Alias[j] < 0x20 || rec.fields.Alias[j] > 0x7E) {
            valid = false;
            break;
        }
    }
    if (valid && rec.fields.Alias[0] != '\0') {
        strncpy(b, rec.fields.Alias, s);
    } else {
        // Fallback: show full MAC-style ID
        HERMES_Addr_FormatMAC(rec.fields.NodeID, b, s);
    }
}

static bool MA_Contacts(const MenuItem *item, KEY_Code_t key, bool kp, bool kh) {
    (void)item;
    if (key != KEY_MENU) return false;
    if (!kp || kh) return true;
    
    gHermesView = HM_VIEW_CONTACTS;
    
    // Build contact menu — validate each record
    uint8_t count = 0;
    for (uint8_t i=0; i<16; i++) {
        HermesContactRecord_t rec;
        if (Storage_ReadRecordIndexed(REC_HERMES_CONTACTS, i, &rec, 0, sizeof(rec))) {
            // Skip empty slots (0xFF) and fully zeroed slots
            if (rec.fields.NodeID[0] == 0xFF && rec.fields.NodeID[1] == 0xFF) continue;
            bool all_zero = true;
            for (uint8_t j = 0; j < 6; j++) {
                if (rec.fields.NodeID[j] != 0) { all_zero = false; break; }
            }
            if (all_zero) continue;
            
            memset(&contactMenuItems[count], 0, sizeof(MenuItem));
            contactMenuItems[count].name = "";
            contactMenuItems[count].get_name = GetContactName;
            contactMenuItems[count].get_value_text = GetContactIDStr;
            contactMenuItems[count].action = MA_ContactAction;
            contactMenuItems[count].type = M_ITEM_SELECT;
            count++;
        }
    }
    memset(&contactMenuItems[count], 0, sizeof(MenuItem));
    contactMenuItems[count].name = "";
    contactMenuItems[count].get_name = GetContactName;
    contactMenuItems[count].action = MA_ContactAction;
    contactMenuItems[count].type = M_ITEM_SELECT;
    count++;
    
    contactMenu.title = "Contacts";
    contactMenu.items = contactMenuItems;
    contactMenu.num_items = count;
    contactMenu.x = 0; contactMenu.y = MENU_Y;
    contactMenu.width = LCD_WIDTH; contactMenu.height = LCD_HEIGHT - MENU_Y;
    contactMenu.itemHeight = MENU_ITEM_H;
    
    AG_MENU_Init(&contactMenu);
    gUpdateDisplay = true;
    return true;
}

// ═══════════════════════════════════════════════════════════
// Main Menu
// ═══════════════════════════════════════════════════════════

static bool MA_Broadcast(const MenuItem *item, KEY_Code_t key, bool kp, bool kh) {
    (void)item;
    if (key != KEY_MENU) return false;
    if (!kp || kh) return true;
    
    memset(composeDestID, 0xFF, HM_NODE_ID_SIZE);
    composeAddrMode = HM_ADDR_BROADCAST;
    
    gHermesView = HM_VIEW_CHAT;
    gHermesChatScroll = gHermesMsgCount > 0 ? gHermesMsgCount - 1 : -1;
    gHermesHasNewMessage = false;
    gUpdateDisplay = true;
    return true;
}

static bool MA_Settings(const MenuItem *item, KEY_Code_t key, bool kp, bool kh) {
    (void)item;
    if (key != KEY_MENU) return false;
    if (!kp || kh) return true;
    gHermesView = HM_VIEW_SETTINGS;
    AG_MENU_Init(&settingsMenu);
    gUpdateDisplay = true;
    return true;
}

static const MenuItem mainItems[] = {
    { .name = "Broadcast", .action = MA_Broadcast },
    { .name = "Contacts",  .action = MA_Contacts },
    { .name = "Settings",  .action = MA_Settings },
};
static Menu mainMenu = {
    .title = "Hermes", .items = mainItems,
    .num_items = sizeof(mainItems) / sizeof(mainItems[0]),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

// ═══════════════════════════════════════════════════════════
// Public API — called by dispatch system
// ═══════════════════════════════════════════════════════════

void HERMES_UI_Init(void) {
    if (gHermesView == HM_VIEW_SETTINGS) {
        AG_MENU_Init(&settingsMenu);
    } else if (gHermesView == HM_VIEW_CONTACTS) {
        MA_Contacts(NULL, KEY_MENU, true, false);
    } else {
        gHermesView = HM_VIEW_MENU;
        AG_MENU_Init(&mainMenu);
    }
}

void HERMES_UI_Render(void) {
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

    switch (gHermesView) {
        case HM_VIEW_MENU:
        case HM_VIEW_SETTINGS:
        case HM_VIEW_CONTACTS:
            AG_MENU_Render();
            if (gHermesView == HM_VIEW_CONTACTS) {
                UI_PrintStringSmallest("(S1: DEL)", 2, LCD_HEIGHT - 8, false, true);
            }
            break;
        case HM_VIEW_CHAT:
            RenderChat();
            break;
        case HM_VIEW_COMPOSE:
        case HM_VIEW_INPUT_ALIAS:
        case HM_VIEW_INPUT_PASSCODE:
        case HM_VIEW_INPUT_SALT:
        case HM_VIEW_INPUT_CONTACT:
            if (TextInput_IsActive()) {
                const char *titles[] = {"Enter Alias", "Enter ID (Hex)", "Enter Passcode"};
                if (gHermesView == HM_VIEW_INPUT_CONTACT) {
                    UI_PrintStringSmallest(titles[contactStep], 2, 2, false, true);
                }
                TextInput_Render();
            } else {
                // Text input exited (e.g. EXIT pressed) — return to previous menu
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
            break;
    }

    ST7565_BlitFullScreen();
}

#endif // ENABLE_MESH_NETWORK
