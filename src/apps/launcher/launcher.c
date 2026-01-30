
#include <string.h>
#include "apps/launcher/launcher.h"
#include "apps/spectrum/spectrum.h"
#include "apps/fm/fm.h"
#include "apps/scanner/scanner.h"
#include "ui/ui.h"
#include "ui/helper.h"
#include "ui/menu.h"
#include "drivers/bsp/st7565.h"
#include "features/action.h"
#include "features/app.h"
#include "audio.h"
#include "apps/settings/settings.h"
#include "apps/settings_new.h"
#include "apps/aircopy/aircopy.h"
#include "apps/boot/boot.h"

#ifdef ENABLE_APP_BREAKOUT_GAME
#include "features/breakout.h"
#endif

#include "apps/scanner/scanner_ui.h"
#include "apps/fm/fm_ui.h"
#include "apps/aircopy/aircopy_ui.h"
#include "apps/boot/welcome.h"

#include "../ui/ag_menu.h"

// Actions Wrappers
static bool LA_Settings(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU) return false;
    SETTINGS_NEW_Init();
    gRequestDisplayScreen = DISPLAY_MENU;
    return true;
}

static bool LA_EditScanlist(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
     if (key != KEY_MENU) return false;
     gEeprom.SCAN_LIST_DEFAULT = (gEeprom.SCAN_LIST_DEFAULT + 1) % 6;
    #ifdef ENABLE_BOOT_RESUME_STATE
        SETTINGS_WriteCurrentState();
    #endif
    gRequestDisplayScreen = DISPLAY_MAIN;
    return true;
}

static bool LA_Spectrum(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU) return false;
    #if defined(ENABLE_SPECTRUM_EXTENSIONS) && defined(ENABLE_SPECTRUM)
    APP_RunSpectrum();
    #endif
    gRequestDisplayScreen = DISPLAY_MAIN;
    return true;
}

static bool LA_FM(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU) return false;
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
     if (key != KEY_MENU) return false;
     gBackup_CROSS_BAND_RX_TX = gEeprom.CROSS_BAND_RX_TX;
     gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
     gUpdateStatus = true;
     SCANNER_Start(false);
     gRequestDisplayScreen = DISPLAY_SCANNER;
     return true;
}

static bool LA_AirCopy(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU) return false;
    #ifdef ENABLE_AIRCOPY
    gRequestDisplayScreen = DISPLAY_AIRCOPY;
    #endif
    return true;
}

static bool LA_Info(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU) return false;
    UI_DisplayWelcome();
    return true;
}

// Menu Items
static const MenuItem launcherItems[] = {
    {"Settings", 0, NULL, NULL, NULL, LA_Settings},
    {"Edit Scanlist", 0, NULL, NULL, NULL, LA_EditScanlist},
    {"Spectrum", 0, NULL, NULL, NULL, LA_Spectrum},
    #ifdef ENABLE_FMRADIO
    {"FM Radio", 0, NULL, NULL, NULL, LA_FM},
    #endif
    {"Scanner", 0, NULL, NULL, NULL, LA_Scanner},
    #ifdef ENABLE_AIRCOPY
    {"Air Copy", 0, NULL, NULL, NULL, LA_AirCopy},
    #endif
    {"Info", 0, NULL, NULL, NULL, LA_Info}
};

static Menu launcherMenu = {
    .title = "Menu", // Or Launcher
    .items = launcherItems,
    .num_items = sizeof(launcherItems) / sizeof(launcherItems[0]),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};


void LAUNCHER_Init() {
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
