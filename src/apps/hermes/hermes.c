#include "apps/hermes/hermes.h"
#include "apps/hermes/ui/hermes_ui.h"
#include "ui/ag_menu.h"
#include "features/app/app.h"
#include "core/misc.h"
#include "ui/ui.h"

void HERMES_Init(void) {
    HERMES_UI_Init();
}

void HERMES_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    if (!AG_MENU_IsActive()) {
        HERMES_Init();
    }

    if (AG_MENU_HandleInput(Key, bKeyPressed, bKeyHeld)) {
        gUpdateDisplay = true;
        return;
    }

    if (!AG_MENU_IsActive()) { // Back was pressed
        gRequestDisplayScreen = DISPLAY_LAUNCHER;
    }
}
