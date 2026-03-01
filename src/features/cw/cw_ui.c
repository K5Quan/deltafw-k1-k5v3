/* cw_ui.c */
#include "features/cw/cw_ui.h"
#ifdef ENABLE_CW_KEYER

#include <string.h>
#include "ui/helper.h"
#include "drivers/bsp/keyboard.h"
#include "features/cw/cw.h"
#include "ui/ag_menu.h"
#include "ui/textinput.h"
#include "drivers/bsp/st7565.h"
#include "ui/ui.h"

extern bool gUpdateDisplay;

static Menu cwRootMenu;
static Menu cwMacrosMenu;
static Menu cwSettingsMenu;

static char composeBuffer[64];
static char macroBuffer[17];
static int editingMacroIndex = -1;

enum {
    MENU_CW_WPM,
    MENU_CW_TONE,
    MENU_CW_DECODE,
    MENU_CW_AUTOSPACE,
    MENU_CW_KEYER_MODE,
    MENU_CW_WEIGHTING,
    MENU_CW_TX_DELAY,
    MENU_CW_FARNSWORTH,
    MENU_CW_REVERSE,
    MENU_CW_OUTPUT_MODE,
    MENU_CW_SIDETONE
};

static void get_setting_text(const MenuItem *item, char *buf, uint8_t buf_size) {
    switch (item->setting) {
        case MENU_CW_WPM: NUMBER_ToDecimal(buf, gCWSettings.fields.WPM, 2, false); break;
        case MENU_CW_TONE: NUMBER_ToDecimal(buf, gCWSettings.fields.ToneFreqHz, 4, false); break;
        case MENU_CW_DECODE: 
            if (gCWSettings.fields.DecodeMode == 0) strcpy(buf, "Off");
            else if (gCWSettings.fields.DecodeMode == 1) strcpy(buf, "TX");
            else if (gCWSettings.fields.DecodeMode == 2) strcpy(buf, "RX");
            else strcpy(buf, "TX/RX");
            break;
        case MENU_CW_AUTOSPACE: strcpy(buf, gCWSettings.fields.AutoSpacing ? "On" : "Off"); break;
        case MENU_CW_KEYER_MODE:
            if (gCWSettings.fields.KeyerMode == 0) strcpy(buf, "Straight");
            else if (gCWSettings.fields.KeyerMode == 1) strcpy(buf, "Iambic A");
            else if (gCWSettings.fields.KeyerMode == 2) strcpy(buf, "Iambic B");
            else if (gCWSettings.fields.KeyerMode == 3) strcpy(buf, "Ultimatic");
            else strcpy(buf, "Bug");
            break;
        case MENU_CW_WEIGHTING:
            NUMBER_ToDecimal(buf, gCWSettings.fields.Weighting, 2, false);
            break;
        case MENU_CW_TX_DELAY:
            NUMBER_ToDecimal(buf, gCWSettings.fields.TxDelayMs, 3, false);
            strcat(buf, "ms");
            break;
        case MENU_CW_FARNSWORTH:
            if (gCWSettings.fields.FarnsworthWPM == 0) strcpy(buf, "Off");
            else NUMBER_ToDecimal(buf, gCWSettings.fields.FarnsworthWPM, 2, false);
            break;
        case MENU_CW_REVERSE:
            strcpy(buf, gCWSettings.fields.ReversePaddles ? "Yes" : "No");
            break;
        case MENU_CW_OUTPUT_MODE:
            if (gCWSettings.fields.OutputMode == 0) strcpy(buf, "RF");
            else if (gCWSettings.fields.OutputMode == 1) strcpy(buf, "F-Light");
            else strcpy(buf, "Trainer");
            break;
        case MENU_CW_SIDETONE:
            strcpy(buf, gCWSettings.fields.SidetoneEnabled ? "On" : "Off");
            break;
    }
}

static void change_setting(const MenuItem *item, bool up) {
    switch (item->setting) {
        case MENU_CW_WPM:
            if (up && gCWSettings.fields.WPM < 50) gCWSettings.fields.WPM++;
            else if (!up && gCWSettings.fields.WPM > 5) gCWSettings.fields.WPM--;
            // Convert WPM to ms for keyer
            gCW.avgDotMs = 1200 / gCWSettings.fields.WPM;
            gCW.avgDashMs = gCW.avgDotMs * 3;
            break;
        case MENU_CW_TONE:
            if (up && gCWSettings.fields.ToneFreqHz < 2000) gCWSettings.fields.ToneFreqHz += 10;
            else if (!up && gCWSettings.fields.ToneFreqHz > 300) gCWSettings.fields.ToneFreqHz -= 10;
            break;
        case MENU_CW_DECODE:
            if (up && gCWSettings.fields.DecodeMode < 3) gCWSettings.fields.DecodeMode++;
            else if (!up && gCWSettings.fields.DecodeMode > 0) gCWSettings.fields.DecodeMode--;
            break;
        case MENU_CW_AUTOSPACE:
            gCWSettings.fields.AutoSpacing = !gCWSettings.fields.AutoSpacing;
            break;
        case MENU_CW_KEYER_MODE:
            if (up && gCWSettings.fields.KeyerMode < 4) gCWSettings.fields.KeyerMode++;
            else if (!up && gCWSettings.fields.KeyerMode > 0) gCWSettings.fields.KeyerMode--;
            break;
        case MENU_CW_WEIGHTING:
            if (up && gCWSettings.fields.Weighting < 50) gCWSettings.fields.Weighting++;
            else if (!up && gCWSettings.fields.Weighting > 20) gCWSettings.fields.Weighting--;
            break;
        case MENU_CW_TX_DELAY:
            if (up && gCWSettings.fields.TxDelayMs < 250) gCWSettings.fields.TxDelayMs += 5;
            else if (!up && gCWSettings.fields.TxDelayMs > 0) gCWSettings.fields.TxDelayMs -= 5;
            break;
        case MENU_CW_FARNSWORTH:
            if (up && gCWSettings.fields.FarnsworthWPM < 60) gCWSettings.fields.FarnsworthWPM++;
            else if (!up && gCWSettings.fields.FarnsworthWPM > 0) gCWSettings.fields.FarnsworthWPM--;
            break;
        case MENU_CW_REVERSE:
            gCWSettings.fields.ReversePaddles = !gCWSettings.fields.ReversePaddles;
            break;
        case MENU_CW_OUTPUT_MODE:
            if (up && gCWSettings.fields.OutputMode < 2) gCWSettings.fields.OutputMode++;
            else if (!up && gCWSettings.fields.OutputMode > 0) gCWSettings.fields.OutputMode--;
            break;
        case MENU_CW_SIDETONE:
            gCWSettings.fields.SidetoneEnabled = !gCWSettings.fields.SidetoneEnabled;
            break;
    }
    CW_SaveSettings();
}

static void onComposeDone(void) {
    if (strlen(composeBuffer) > 0) {
        gCW.activeMacroIndex = 99; // 99 implies compose buffer
        gUpdateDisplay = true;
        CW_KeyString(composeBuffer);
    }
}

static bool action_compose(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (!key_pressed || key_held) return true;
    if (key == KEY_MENU || key == KEY_PTT) {
        memset(composeBuffer, 0, sizeof(composeBuffer));
        TextInput_InitEx(composeBuffer, sizeof(composeBuffer) - 1, true, false, true, true, onComposeDone);
    }
    return true;
}

static void onMacroEditDone(void) {
    if (editingMacroIndex >= 0 && editingMacroIndex < 8) {
        strncpy(gCWSettings.fields.Macros[editingMacroIndex], macroBuffer, 16);
        gCWSettings.fields.Macros[editingMacroIndex][16] = '\0';
        CW_SaveSettings();
    }
    editingMacroIndex = -1;
}

static bool action_macro_slot(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    int idx = item->setting;
    if (key_pressed && key_held) {
        // Edit macro
        editingMacroIndex = idx;
        strncpy(macroBuffer, gCWSettings.fields.Macros[idx], 16);
        macroBuffer[16] = '\0';
        TextInput_InitEx(macroBuffer, 16, true, true, true, false, onMacroEditDone);
        return true;
    } else if (!key_pressed && !key_held) {
        // Trigger on release if it wasn't held
        CW_KeyMacro(idx + 1);
    }
    return true;
}

static void onCallsignEditDone(void) {
    strncpy(gCWSettings.fields.Callsign, macroBuffer, 8);
    gCWSettings.fields.Callsign[8] = '\0';
    CW_SaveSettings();
}

static bool action_callsign(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (!key_pressed || key_held) return true;
    if (key == KEY_MENU || key == KEY_PTT) {
        strncpy(macroBuffer, gCWSettings.fields.Callsign, 8);
        macroBuffer[8] = '\0';
        TextInput_InitEx(macroBuffer, 8, true, true, true, false, onCallsignEditDone);
    }
    return true;
}
static void get_callsign_text(const MenuItem *item, char *buf, uint8_t buf_size) {
    strcpy(buf, gCWSettings.fields.Callsign);
}

static void get_macro_item_name(const MenuItem *item, char *buf, uint8_t buf_size) {
    int idx = item->setting;
    const char *text = (idx == 0) ? "CQ Call" : (gCWSettings.fields.Macros[idx-1]);
    
    if (idx == 0) {
        char cq[32];
        CW_GetCQMessage(cq);
        strncpy(buf, cq, buf_size-1);
        buf[buf_size-1] = '\0';
    } else if (strlen(text) == 0) {
        strcpy(buf, "(empty)");
    } else {
        strncpy(buf, text, buf_size-1);
        buf[buf_size-1] = '\0';
    }
}

static void get_macro_value_text(const MenuItem *item, char *buf, uint8_t buf_size) {
    int idx = item->setting; // 0=CQ, 1-8=Custom
    
    // Check if THIS specific macro is being transmitted
    if (gCW.activeMacroIndex == idx) {
        // Show progress X/Y
        char temp[16];
        temp[0] = ' '; temp[1] = '\0';
        NUMBER_ToDecimal(temp + 1, gCW.macroPos, 1, false);
        strcat(temp, "/");
        char total[8];
        NUMBER_ToDecimal(total, gCW.macroLen, 1, false);
        char *p = total; while (*p == ' ') p++;
        strcat(temp, p);
        strcat(temp, " "); // End with a space
        
        // Let's pad it so it draws a solid background over the macro text
        strcpy(buf, temp);
    } else {
        // Show keybind label 1..9
        int key = (idx == 0) ? 9 : idx;
        buf[0] = key + '0';
        buf[1] = '\0';
    }
}

static void cw_macros_on_tick(Menu *menu) {
    menu->is_held = (gCW.activeMacroIndex != -1);
}


static void get_compose_value_text(const MenuItem *item, char *buf, uint8_t buf_size) {
    if (gCW.activeMacroIndex == 99) { // 99 indicates active compose string
        char temp[16];
        temp[0] = ' '; temp[1] = '\0';
        NUMBER_ToDecimal(temp + 1, gCW.macroPos, 1, false);
        strcat(temp, "/");
        char total[8];
        NUMBER_ToDecimal(total, gCW.macroLen, 1, false);
        char *p = total; while (*p == ' ') p++;
        strcat(temp, p);
        strcat(temp, " ");
        
        strcpy(buf, temp);
    } else {
        buf[0] = '\0';
    }
}

static void cw_root_on_tick(Menu *menu) {
    menu->is_held = (gCW.activeMacroIndex == 99);
}

static const MenuItem rootItems[] = {
    {"Compose", 0, get_compose_value_text, NULL, NULL, action_compose, M_ITEM_ACTION},
    {"Macros", 0, NULL, NULL, &cwMacrosMenu, NULL, M_ITEM_ACTION},
    {"Settings", 0, NULL, NULL, &cwSettingsMenu, NULL, M_ITEM_ACTION},
};
static Menu cwRootMenu = {
    .title = "CW Keyer", .items = rootItems, .num_items = 3,
    .on_tick = cw_root_on_tick,
    .x = 0, .y = MENU_Y, .width = 128, .height = 64 - MENU_Y, .itemHeight = MENU_ITEM_H
};

static const MenuItem settingsItems[] = {
    {"Callsign", 0, get_callsign_text, NULL, NULL, action_callsign, M_ITEM_ACTION},
    {"WPM", MENU_CW_WPM, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"Tone Freq", MENU_CW_TONE, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"Decode Mode", MENU_CW_DECODE, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"Auto Space", MENU_CW_AUTOSPACE, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"Keyer Mode", MENU_CW_KEYER_MODE, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"Weighting", MENU_CW_WEIGHTING, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"TX Delay", MENU_CW_TX_DELAY, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"Farnsworth", MENU_CW_FARNSWORTH, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"Reverse Paddles", MENU_CW_REVERSE, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"Output Mode", MENU_CW_OUTPUT_MODE, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT},
    {"Sidetone", MENU_CW_SIDETONE, get_setting_text, change_setting, NULL, NULL, M_ITEM_SELECT}
};
static Menu cwSettingsMenu = {
    .title = "Settings", .items = settingsItems, .num_items = 10,
    .x = 0, .y = MENU_Y, .width = 128, .height = 64 - MENU_Y, .itemHeight = MENU_ITEM_H
};

static bool action_cq(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (!key_pressed && !key_held) {
        CW_KeyMacro(0);
    }
    return true;
}

static const MenuItem macroItems[] = {
    {NULL, 0, get_macro_value_text, NULL, NULL, action_cq, M_ITEM_ACTION, .get_name = get_macro_item_name},
    {NULL, 1, get_macro_value_text, NULL, NULL, action_macro_slot, M_ITEM_ACTION, .get_name = get_macro_item_name},
    {NULL, 2, get_macro_value_text, NULL, NULL, action_macro_slot, M_ITEM_ACTION, .get_name = get_macro_item_name},
    {NULL, 3, get_macro_value_text, NULL, NULL, action_macro_slot, M_ITEM_ACTION, .get_name = get_macro_item_name},
    {NULL, 4, get_macro_value_text, NULL, NULL, action_macro_slot, M_ITEM_ACTION, .get_name = get_macro_item_name},
    {NULL, 5, get_macro_value_text, NULL, NULL, action_macro_slot, M_ITEM_ACTION, .get_name = get_macro_item_name},
    {NULL, 6, get_macro_value_text, NULL, NULL, action_macro_slot, M_ITEM_ACTION, .get_name = get_macro_item_name},
    {NULL, 7, get_macro_value_text, NULL, NULL, action_macro_slot, M_ITEM_ACTION, .get_name = get_macro_item_name},
    {NULL, 8, get_macro_value_text, NULL, NULL, action_macro_slot, M_ITEM_ACTION, .get_name = get_macro_item_name},
};
static Menu cwMacrosMenu = {
    .title = "Macros", .items = macroItems, .num_items = 9,
    .on_tick = cw_macros_on_tick,
    .x = 0, .y = MENU_Y, .width = 128, .height = 64 - MENU_Y, .itemHeight = MENU_ITEM_H
};

void CW_UI_Init(void) {
    AG_MENU_Init(&cwRootMenu);
}

void CW_UI_Render(void) {
    if (TextInput_IsActive()) {
        TextInput_Render();
    } else {
        AG_MENU_Render();
    }
    ST7565_BlitFullScreen();
}

void CW_UI_HandleInput(KEY_Code_t key, bool key_pressed, bool key_held) {
    if (TextInput_IsActive()) {
        if (!TextInput_HandleInput(key, key_pressed, key_held)) {
            // Unhandled input? maybe ignore?
        }
        if (TextInput_Tick()) {
            gUpdateDisplay = true;
        }
        return;
    }
    
    if (AG_MENU_HandleInput(key, key_pressed, key_held)) {
        gUpdateDisplay = true;
        return;
    }

    // Intercept F + 1..9 for macros
    if (gWasFKeyPressed && key_pressed && !key_held) {
        if (key >= KEY_1 && key <= KEY_8) {
            CW_KeyMacro(key - KEY_0);
            gWasFKeyPressed = false;
            gUpdateDisplay = true;
            return;
        } else if (key == KEY_9) {
            CW_KeyMacro(0); // CQ Call
            gWasFKeyPressed = false;
            gUpdateDisplay = true;
            return;
        }
    }
    
    if (!AG_MENU_IsActive()) {
        gRequestDisplayScreen = DISPLAY_MAIN;
    }
}

#endif // ENABLE_CW_KEYER
