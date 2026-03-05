#include "apps/hermes/ui/hermes_ui.h"
#include "ui/ag_menu.h"
#include "ui/ui.h"
#include "drivers/bsp/st7565.h"
#include <stddef.h>

static bool HM_Broadcast(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    // Placeholder for Broadcast UI
    return true;
}

static bool HM_Groups(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    // Placeholder for Groups UI
    return true;
}

static bool HM_Nodes(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    // Placeholder for Nodes UI
    return true;
}

static bool HM_Settings(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key != KEY_MENU && key != KEY_PTT) return false;
    if (!key_pressed || key_held) return true;
    // Placeholder for Settings UI
    return true;
}

static const MenuItem hermesMenuItems[] = {
    {"Broadcast", 0, NULL, NULL, NULL, HM_Broadcast},
    {"Groups",    0, NULL, NULL, NULL, HM_Groups},
    {"Nodes",     0, NULL, NULL, NULL, HM_Nodes},
    {"Settings",  0, NULL, NULL, NULL, HM_Settings},
};

static Menu hermesMenu = {
    .title = "Network",
    .items = hermesMenuItems,
    .num_items = sizeof(hermesMenuItems) / sizeof(hermesMenuItems[0]),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

void HERMES_UI_Init(void) {
    AG_MENU_Init(&hermesMenu);
}

void HERMES_UI_Render(void) {
    AG_MENU_Render();
    ST7565_BlitFullScreen();
}
