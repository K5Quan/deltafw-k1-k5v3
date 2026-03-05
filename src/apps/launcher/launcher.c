
#include <string.h>
#include "apps/launcher/launcher.h"
#include "apps/spectrum/spectrum.h"
#include "apps/fm/fm.h"
#include "apps/scanner/scanner.h"
#include "ui/ui.h"
#include "ui/helper.h"
#include "ui/menu.h"
#include "drivers/bsp/st7565.h"
#include "features/action/action.h"
#include "features/app/app.h"
#include "features/audio/audio.h"
#include "features/radio/radio.h"
#include "apps/settings/settings.h"
#include "apps/settings/settings_ui.h"
#include "apps/aircopy/aircopy.h"
#include "apps/boot/boot.h"

#ifdef ENABLE_MESH_NETWORK
#include "apps/hermes/hermes.h"
#endif

#ifdef ENABLE_APP_BREAKOUT_GAME
#include "features/breakout/breakout.h"
#endif

#include "apps/scanner/scanner_ui.h"
#include "apps/fm/fm_ui.h"
#include "apps/aircopy/aircopy_ui.h"
#include "apps/boot/welcome.h"
#include "apps/sysinfo/sysinfo.h"
#include "apps/memories/memories.h"

#include "../ui/ag_menu.h"

static bool LA_Memories(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    MEMORIES_Init();
    gRequestDisplayScreen = DISPLAY_MEMORIES;
    return true;
}

// Actions Wrappers
static bool LA_Settings(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    SETTINGS_UI_Init();
    gRequestDisplayScreen = DISPLAY_MENU;
    return true;
}


static bool LA_Spectrum(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    #if defined(ENABLE_SPECTRUM_EXTENSIONS) && defined(ENABLE_SPECTRUM)
    APP_RunSpectrum();
    #endif
    gRequestDisplayScreen = DISPLAY_MAIN;
    return true;
}

static bool LA_FM(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    #ifdef ENABLE_FMRADIO
    if (!gFmRadioMode) {
        ACTION_FM();
    } else {
        gRequestDisplayScreen = DISPLAY_FM;
    }
    #endif
    return true;
}

static bool LA_Scanner(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
     if (key != KEY_MENU && key != KEY_PTT) return false;
     if (!key_pressed || key_held) return true;
     gBackup_CROSS_BAND_RX_TX = gEeprom.CROSS_BAND_RX_TX;
     gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
     gUpdateStatus = true;
     SCANNER_Start(false);
     gRequestDisplayScreen = DISPLAY_SCANNER;
     return true;
}

#ifdef ENABLE_AIRCOPY
static bool LA_AirCopy(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    gRequestDisplayScreen = DISPLAY_AIRCOPY;
    return true;
}
#endif

#ifdef ENABLE_CW_KEYER
static bool LA_Keyer(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    
    // Start the new CW Keyer Menu UI
    CW_UI_Init();
    gRequestDisplayScreen = DISPLAY_CW_KEYER;
    return true;
}
#endif

static bool LA_Info(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    SYSINFO_Init();
    gRequestDisplayScreen = DISPLAY_SYSINFO;
    return true;
}

static bool LA_Network(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    HERMES_Init();
    gRequestDisplayScreen = DISPLAY_NETWORK;
    return true;
}

// Menu Items (Normal)
static const MenuItem normalLauncherItems[] = {
#ifdef ENABLE_MESH_NETWORK
    {"Network", 0, NULL, NULL, NULL, LA_Network},
#endif
    {"Memories", 0, NULL, NULL, NULL, LA_Memories},
    {"Settings", 0, NULL, NULL, NULL, LA_Settings},
#if defined(ENABLE_SPECTRUM_EXTENSIONS) && defined(ENABLE_SPECTRUM)
    {"Spectrum", 0, NULL, NULL, NULL, LA_Spectrum},
#endif
#ifdef ENABLE_FMRADIO
    {"FM Radio", 0, NULL, NULL, NULL, LA_FM},
#endif
    {"Scanner", 0, NULL, NULL, NULL, LA_Scanner},
#ifdef ENABLE_AIRCOPY
    {"Air Copy", 0, NULL, NULL, NULL, LA_AirCopy},
#endif
    {"Info", 0, NULL, NULL, NULL, LA_Info}
};

#ifdef ENABLE_CW_KEYER
static const MenuItem cwLauncherItems[] = {
#ifdef ENABLE_MESH_NETWORK
    {"Network", 0, NULL, NULL, NULL, LA_Network},
#endif
    {"Keyer", 0, NULL, NULL, NULL, LA_Keyer},
    {"Memories", 0, NULL, NULL, NULL, LA_Memories},
    {"Settings", 0, NULL, NULL, NULL, LA_Settings},
#if defined(ENABLE_SPECTRUM_EXTENSIONS) && defined(ENABLE_SPECTRUM)
    {"Spectrum", 0, NULL, NULL, NULL, LA_Spectrum},
#endif
#ifdef ENABLE_FMRADIO
    {"FM Radio", 0, NULL, NULL, NULL, LA_FM},
#endif
    {"Scanner", 0, NULL, NULL, NULL, LA_Scanner},
#ifdef ENABLE_AIRCOPY
    {"Air Copy", 0, NULL, NULL, NULL, LA_AirCopy},
#endif
    {"Info", 0, NULL, NULL, NULL, LA_Info}
};
#endif

static Menu launcherMenu = {
    .title = "Menu", 
    .items = normalLauncherItems,
    .num_items = sizeof(normalLauncherItems) / sizeof(normalLauncherItems[0]),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};


void LAUNCHER_Init() {
#ifdef ENABLE_CW_KEYER
    if (gTxVfo->Modulation == MODULATION_CW) {
        launcherMenu.items = cwLauncherItems;
        launcherMenu.num_items = sizeof(cwLauncherItems) / sizeof(cwLauncherItems[0]);
    } else {
        launcherMenu.items = normalLauncherItems;
        launcherMenu.num_items = sizeof(normalLauncherItems) / sizeof(normalLauncherItems[0]);
    }
#else
    launcherMenu.items = normalLauncherItems;
    launcherMenu.num_items = sizeof(normalLauncherItems) / sizeof(normalLauncherItems[0]);
#endif
    AG_MENU_Init(&launcherMenu);
}

void UI_DisplayLauncher(void) {
    // Check if menu needs init? LAUNCHER_Init should be called before switching here ideally.
    // But existing flow might rely on lazy init.
    // AG_MENU_Init tracks active menu. If we are here, we want launcher active.
    if (!AG_MENU_IsActive()) { 
        LAUNCHER_Init();
    }
    
    // Safety check if active menu is NOT launcher (e.g. settings left active)
    // We might need to enforce launcher if we are in DisplayLauncher mode.
    // allow logic to persist.

    AG_MENU_Render();
    ST7565_BlitFullScreen();
}


void LAUNCHER_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    if (!AG_MENU_IsActive()) LAUNCHER_Init();

    if (AG_MENU_HandleInput(Key, bKeyPressed, bKeyHeld)) {
        gUpdateDisplay = true;
        return;
    }
    
    // If handle input returns false (e.g. exit/back), what to do?
    // Go to Main
    if (!AG_MENU_IsActive()) { // Back was pressed
         gRequestDisplayScreen = DISPLAY_MAIN;
    }
}
