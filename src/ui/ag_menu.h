#ifndef AG_MENU_H
#define AG_MENU_H

#include "../drivers/bsp/keyboard.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MENU_Y 8
#define MENU_ITEM_H 13
#define MENU_LINES_TO_SHOW 4

// Forward declarations
struct MenuItem;
struct Menu;

typedef void (*MenuAction)(void);
typedef void (*MenuOnEnter)(void);
typedef void (*MenuRenderItem)(uint16_t index, uint8_t visIndex);

typedef struct MenuItem {
  const char *name;
  uint8_t setting; // ID of the setting to change
  void (*get_value_text)(const struct MenuItem *item, char *buf, uint8_t buf_size);
  void (*change_value)(const struct MenuItem *item, bool up);
  struct Menu *submenu; // if not NULL, enters this submenu
  
  bool (*action)(const struct MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held);
} MenuItem;

typedef struct Menu {
  const char *title;
  const MenuItem *items;
  uint16_t num_items;
  uint16_t i; // Current selection index
  MenuRenderItem render_item;
  MenuOnEnter on_enter;
  
  // Custom action handler for the menu itself (e.g. for non-standard navigation)
  bool (*action)(uint16_t index, KEY_Code_t key, bool key_pressed, bool key_held);
  
  uint8_t itemHeight;
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
} Menu;

void AG_MENU_Init(Menu *main_menu);
void AG_MENU_Deinit(void);
void AG_MENU_Render(void);
bool AG_MENU_HandleInput(KEY_Code_t key, bool key_pressed, bool key_held);
bool AG_MENU_Back(void);
void AG_MENU_GetPath(char *buf, size_t len);
bool AG_MENU_IsActive(void);

#endif /* AG_MENU_H */
