/* Hermes UI — From scratch
 *
 * Handles: Main menu, chat with bubbles, compose (TextInput), settings.
 * Uses AG_MENU for menu views, custom rendering for chat bubbles.
 * Follows the same dispatch pattern as LAUNCHER/CW_KEYER apps.
 */
#ifdef ENABLE_MESH_NETWORK

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
    if (gHermesMessages[a].is_outgoing) return true;
    return memcmp(gHermesMessages[a].src, gHermesMessages[b].src, HM_NODE_ID_SIZE) == 0;
}

static bool IsTiny(const HermesMessage_t *m) { return (m->len > 22); }

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

static uint8_t CountLines(const HermesMessage_t *m) {
    bool tiny = IsTiny(m);
    uint8_t maxc = tiny ? 26 : 14;
    uint16_t pos = 0;
    uint8_t n = 0;
    while (pos < m->len && n < 8) {
        uint16_t ll = 0, sp = 0xFFFF, ls = pos;
        while (pos < m->len && ll < maxc) { if (m->text[pos] == ' ') sp = pos; pos++; ll++; }
        if (pos < m->len && m->text[pos] != ' ' && sp != 0xFFFF && sp >= ls) pos = sp + 1;
        n++;
        while (pos < m->len && m->text[pos] == ' ') pos++;
        if (ll == 0) break;
    }
    return n == 0 ? 1 : n;
}

static uint8_t BubbleWidth(int16_t i) {
    const HermesMessage_t *m = &gHermesMessages[i];
    bool tiny = IsTiny(m);
    uint8_t lines = CountLines(m);
    uint8_t w = 112;
    if (lines == 1) {
        uint16_t s, l;
        GetLine(m->text, m->len, tiny ? 26 : 14, 0, &s, &l);
        w = l * (tiny ? 4 : 8) + 8;
    }
    return (w > 112) ? 112 : w;
}

static uint8_t BubbleHeight(int16_t i) {
    const HermesMessage_t *m = &gHermesMessages[i];
    bool tiny = IsTiny(m);
    uint8_t lines = CountLines(m);
    uint8_t h = tiny ? (lines * 6 + 3) : (lines * 12 - 1);
    if (!IsSameSender(i, i + 1)) h += 2; else h -= 1;
    if (!m->is_outgoing && !IsSameSender(i, i - 1)) h += 8;
    return h < 1 ? 1 : h;
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
    while (start >= 0) {
        uint8_t h = BubbleHeight(start);
        if (total + h > area && start != gHermesChatScroll) break;
        total += h; start--;
    }
    start++;

    uint8_t y = 8;
    for (int16_t i = start; i < gHermesMsgCount && y < 54; i++) {
        HermesMessage_t *m = &gHermesMessages[i];
        bool sel = (i == gHermesChatScroll);
        bool tiny = IsTiny(m);
        uint8_t lines = CountLines(m);
        uint8_t bh = tiny ? (lines * 6 + 3) : (lines * 12 - 1);
        uint8_t bw = BubbleWidth(i);
        uint8_t bx = m->is_outgoing ? (124 - bw) : 2;
        uint8_t by = y;

        // Sender label for incoming
        if (!m->is_outgoing && !IsSameSender(i, i - 1)) {
            char sender[12];
            HERMES_Addr_Decode(m->src, sender, sizeof(sender));
            UI_PrintStringSmallest(sender, 2, y - 7, false, true);
            by += 8;
        }

        // Unread dot
        if (!m->is_outgoing && !m->is_read)
            AG_FillRect(0, by + 3, 2, 2, C_FILL);

        // ACK tick
        if (m->is_outgoing && m->is_acked) {
            AG_PutPixel(bx - 4, by + bh - 3, 1);
            AG_PutPixel(bx - 3, by + bh - 2, 1);
            AG_PutPixel(bx - 2, by + bh - 3, 1);
        }

        // Bubble outline/fill
        if (sel && !m->is_pending)
            AG_FillRect(bx, by, bw, bh, C_FILL);
        else {
            AG_DrawVLine(bx, by, bh, C_FILL);
            AG_DrawVLine(bx + bw - 1, by, bh, C_FILL);
            if (!IsSameSender(i, i - 1)) AG_DrawHLine(bx, by, bw, C_FILL);
            if (!IsSameSender(i, i + 1)) AG_DrawHLine(bx, by + bh - 1, bw, C_FILL);
        }

        // Text
        uint8_t maxc = tiny ? 26 : 14;
        Color tc = (sel && !m->is_pending) ? C_CLEAR : C_FILL;

        for (uint8_t l = 0; l < lines; l++) {
            uint16_t ls, ll;
            GetLine(m->text, m->len, maxc, l, &ls, &ll);
            if (ll == 0 && l > 0) continue;
            char buf[32];
            uint8_t n = (ll > 31) ? 31 : (uint8_t)ll;
            memcpy(buf, m->text + ls, n); buf[n] = '\0';
            if (tiny) UI_PrintStringSmallest(buf, bx + 4, by + l * 6 - 6, false, tc == C_FILL);
            else      AG_PrintMediumEx(bx + 4, by + 8 + l * 12, POS_L, tc, buf);
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
        // Store outgoing message
        uint8_t slot = gHermesMsgCount % HM_MSG_SLOTS;
        HermesMessage_t *m = &gHermesMessages[slot];
        memset(m, 0, sizeof(*m));
        memcpy(m->src, gHermesConfig.node_id, HM_NODE_ID_SIZE);
        memset(m->dest, 0xFF, HM_NODE_ID_SIZE); // broadcast
        strncpy(m->text, composeBuffer, HM_MSG_MAX_TEXT);
        m->len = len;
        m->is_outgoing = true;
        m->is_pending = true;
        if (gHermesMsgCount < HM_MSG_SLOTS) gHermesMsgCount++;
        // Don't auto-select the new one yet because it's pending
        // gHermesChatScroll = gHermesMsgCount - 1; 
        
        // Find the last non-pending message to select
        int8_t last_valid = -1;
        for (int16_t i = gHermesMsgCount - 1; i >= 0; i--) {
            if (!gHermesMessages[i].is_pending) {
                last_valid = (int8_t)i;
                break;
            }
        }
        if (last_valid >= 0) gHermesChatScroll = last_valid;

        // Trigger TX pipeline
        HERMES_SendMessage(m);
    }
    memset(composeBuffer, 0, sizeof(composeBuffer));
    gUpdateDisplay = true;
}

void HERMES_UI_StartCompose(void) {
    memset(composeBuffer, 0, sizeof(composeBuffer));
    gHermesView = HM_VIEW_COMPOSE;
    TextInput_InitEx(composeBuffer, HM_MSG_MAX_TEXT, true, true, false, true, OnComposeDone);
    gUpdateDisplay = true;
}

// ═══════════════════════════════════════════════════════════
// Settings Menu
// ═══════════════════════════════════════════════════════════
#include "features/storage/storage.h"

static void SaveSettings(void) {
    HermesSettings_t settings;
    if (Storage_ReadRecord(REC_HERMES_SETTINGS, &settings, 0, sizeof(HermesSettings_t))) {
        settings.fields.Enabled = gHermesConfig.enabled;
        settings.fields.FreqMode = gHermesConfig.freq_mode;
        settings.fields.RoutingMode = gHermesConfig.routing_mode;
        settings.fields.AckMode = gHermesConfig.ack_mode;
        settings.fields.MacPolicy = gHermesConfig.mac_policy;
        settings.fields.TxPower = gHermesConfig.tx_power;
        settings.fields.TTL = gHermesConfig.ttl;
        Storage_WriteRecord(REC_HERMES_SETTINGS, &settings, 0, sizeof(HermesSettings_t));
    }
}

static void GetEnabled(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; strcpy(b, gHermesConfig.enabled ? "ON":"OFF"); }
static void SetEnabled(const MenuItem *i, bool u) { (void)i; (void)u; gHermesConfig.enabled ^= 1; gHermesEnabled = gHermesConfig.enabled; SaveSettings(); }

static void GetNodeID(const MenuItem *i, char *b, uint8_t s) { 
    (void)i; (void)s;
    const uint8_t *id = gHermesConfig.node_id;
    sprintf(b, "%02X%02X%02X%02X", id[0], id[1], id[2], id[3]); // Display first 4 bytes
}

static void GetStatus(const MenuItem *i, char *b, uint8_t s) {
    (void)i; (void)s;
    sprintf(b, "R:%s A:%s", 
        gHermesConfig.routing_mode == 0 ? "OFF" : (gHermesConfig.routing_mode == 1 ? "PAS" : "ACT"),
        gHermesConfig.ack_mode == 0 ? "OFF" : (gHermesConfig.ack_mode == 1 ? "MAN" : "AUT")
    );
}

static void GetFreqMode(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; const char* m[]={"LPD66", "VFO", "MEM"}; strcpy(b, m[gHermesConfig.freq_mode]); }
static void SetFreqMode(const MenuItem *i, bool u) { (void)i; gHermesConfig.freq_mode = u ? (gHermesConfig.freq_mode==2?0:gHermesConfig.freq_mode+1) : (gHermesConfig.freq_mode==0?2:gHermesConfig.freq_mode-1); SaveSettings(); }

static void GetRouting(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; const char* m[]={"OFF", "PASSIVE", "ACTIVE"}; strcpy(b, m[gHermesConfig.routing_mode]); }
static void SetRouting(const MenuItem *i, bool u) { (void)i; gHermesConfig.routing_mode = u ? (gHermesConfig.routing_mode==2?0:gHermesConfig.routing_mode+1) : (gHermesConfig.routing_mode==0?2:gHermesConfig.routing_mode-1); SaveSettings(); }

static void GetAck(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; const char* m[]={"OFF", "MANUAL", "AUTO"}; strcpy(b, m[gHermesConfig.ack_mode]); }
static void SetAck(const MenuItem *i, bool u) { (void)i; gHermesConfig.ack_mode = u ? (gHermesConfig.ack_mode==2?0:gHermesConfig.ack_mode+1) : (gHermesConfig.ack_mode==0?2:gHermesConfig.ack_mode-1); SaveSettings(); }

static void GetMacPolicy(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; const char* m[]={"HW", "CUSTOM", "ALIAS"}; strcpy(b, m[gHermesConfig.mac_policy]); }
static void SetMacPolicy(const MenuItem *i, bool u) { (void)i; gHermesConfig.mac_policy = u ? (gHermesConfig.mac_policy==2?0:gHermesConfig.mac_policy+1) : (gHermesConfig.mac_policy==0?2:gHermesConfig.mac_policy-1); SaveSettings(); }

static void GetTxPwr(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; const char* m[]={"LOW", "MID", "HIGH"}; strcpy(b, m[gHermesConfig.tx_power]); }
static void SetTxPwr(const MenuItem *i, bool u) { (void)i; gHermesConfig.tx_power = u ? (gHermesConfig.tx_power==2?0:gHermesConfig.tx_power+1) : (gHermesConfig.tx_power==0?2:gHermesConfig.tx_power-1); SaveSettings(); }

static void GetTTL(const MenuItem *i, char *b, uint8_t s) { (void)i; (void)s; b[0] = '0' + (gHermesConfig.ttl / 10); b[1] = '0' + (gHermesConfig.ttl % 10); b[2] = 0; if(b[0]=='0') { b[0]=b[1]; b[1]=0; } }
static void SetTTL(const MenuItem *i, bool u) { (void)i; if (u) { if (gHermesConfig.ttl < 15) gHermesConfig.ttl++; } else { if (gHermesConfig.ttl > 1) gHermesConfig.ttl--; } SaveSettings(); }

static const MenuItem settingsItems[] = {
    { .name = "Hermes",      .get_value_text = GetEnabled,   .change_value = SetEnabled,   .type = M_ITEM_SELECT },
    { .name = "Node ID",     .get_value_text = GetNodeID,    .type = M_ITEM_SELECT },
    { .name = "Net Status",  .get_value_text = GetStatus,    .type = M_ITEM_SELECT },
    { .name = "Frequency",   .get_value_text = GetFreqMode,  .change_value = SetFreqMode,  .type = M_ITEM_SELECT },
    { .name = "Mesh Relay",  .get_value_text = GetRouting,   .change_value = SetRouting,   .type = M_ITEM_SELECT },
    { .name = "ACK Mode",    .get_value_text = GetAck,       .change_value = SetAck,       .type = M_ITEM_SELECT },
    { .name = "ID Source",   .get_value_text = GetMacPolicy, .change_value = SetMacPolicy, .type = M_ITEM_SELECT },
    { .name = "TX Power",    .get_value_text = GetTxPwr,     .change_value = SetTxPwr,     .type = M_ITEM_SELECT },
    { .name = "TTL",         .get_value_text = GetTTL,       .change_value = SetTTL,       .type = M_ITEM_SELECT },
};
static Menu settingsMenu = {
    .title = "Settings", .items = settingsItems, .num_items = sizeof(settingsItems) / sizeof(settingsItems[0]),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

// ═══════════════════════════════════════════════════════════
// Main Menu
// ═══════════════════════════════════════════════════════════

static bool MA_Broadcast(const MenuItem *item, KEY_Code_t key, bool kp, bool kh) {
    (void)item;
    if (key != KEY_MENU) return false;
    if (!kp || kh) return true;
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
    gHermesView = HM_VIEW_MENU;
    AG_MENU_Init(&mainMenu);
}

void HERMES_UI_Render(void) {
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

    switch (gHermesView) {
        case HM_VIEW_MENU:
        case HM_VIEW_SETTINGS:
            AG_MENU_Render();
            break;
        case HM_VIEW_CHAT:
            RenderChat();
            break;
        case HM_VIEW_COMPOSE:
            if (TextInput_IsActive())
                TextInput_Render();
            break;
    }

    ST7565_BlitFullScreen();
}

#endif // ENABLE_MESH_NETWORK
