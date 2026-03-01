#include <string.h>
#include "apps/cw_compose/cw_compose.h"
#include "ui/ui.h"
#include "ui/helper.h"
#include "ui/textinput.h"
#include "ui/main.h"
#include "drivers/bsp/st7565.h"
#include "features/cw/cw.h"
#include "features/action/action.h"

extern bool gUpdateDisplay;
extern GUI_DisplayType_t gRequestDisplayScreen;

// Note text_input.h/c is usually found in features/text or similar, or I can use native char builder
static char szMsg[22] = {0};

static void CW_COMPOSE_Callback(void) {
    if (szMsg[0] != '\0') {
        CW_KeyString(szMsg);
        // Maybe clear after sending? User preference. User said "Top compose should use textinput".
        // We'll return to main screen or stay? Usually one wants to see the decoded output.
    }
    gRequestDisplayScreen = DISPLAY_MAIN;
}

void CW_COMPOSE_Init(void) {
    memset(szMsg, 0, sizeof(szMsg));
    TextInput_Init(szMsg, sizeof(szMsg) - 1, true, CW_COMPOSE_Callback);
}

void CW_COMPOSE_Render(void) {
    UI_DisplayClear();
    UI_PrintString("Compose", 0, 127, 0, 8);
    TextInput_Render();
    ST7565_BlitFullScreen();
}

void CW_COMPOSE_ProcessKey(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    if (TextInput_HandleInput(Key, bKeyPressed, bKeyHeld)) {
        gUpdateDisplay = true;
        return;
    }
    
    if (bKeyHeld || !bKeyPressed) return;
    
    if (Key == KEY_EXIT) {
        gRequestDisplayScreen = DISPLAY_MAIN;
        return;
    }
}
