#include "ag_menu.h"
#include "ag_graphics.h"
#include "../drivers/bsp/st7565.h"
#include <string.h>
#include <stdlib.h>
#include "features/audio/audio.h"
#include "menu.h"

#define MENU_STACK_DEPTH 4

static Menu *menu_stack[MENU_STACK_DEPTH];
static uint8_t menu_stack_top = 0;

static Menu *active_menu = NULL;
static bool is_editing = false;
static bool is_pressed = false;

static void (*renderFn)(uint8_t x, uint8_t y, const char *str);

extern bool gUpdateStatus;

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Helper to get right edge of menu
static inline uint8_t getMenuRightEdge(void) {
  return active_menu->x + active_menu->width;
}

static uint32_t IncDecU(uint32_t val, uint32_t min, uint32_t max, bool inc) {
    if (inc) {
        if (val >= max - 1) return min;
        return val + 1;
    } else {
        if (val <= min) return max - 1;
        return val - 1;
    }
}

static void renderItem(uint16_t index, uint8_t i) {
  const MenuItem *item = &active_menu->items[index];
  const uint8_t ex = getMenuRightEdge();
  const uint8_t y = active_menu->y + i * active_menu->itemHeight;
  
  // Use baseline calculation matching fagci logic
  const uint8_t baseline_y = y + active_menu->itemHeight - (active_menu->itemHeight >= MENU_ITEM_H ? 3 : 2);

  char label[32]; // Buffer to hold the formatted string
  if (item->get_name) {
    item->get_name(item, label, sizeof(label));
  } else {
    strcpy(label, item->name);
  }
  strcat(label, item->submenu ? " >" : "  ");
  renderFn(3, baseline_y, label);

  if (item->get_value_text) {
    char value_buf[32];
    item->get_value_text(item, value_buf, sizeof(value_buf));
    AG_PrintSmallEx(ex - 7, baseline_y, POS_R, C_FILL, value_buf);
  }
}

static void init() {
  if (active_menu->y < MENU_Y)
    active_menu->y = MENU_Y;

  if (!active_menu->width)
    active_menu->width = LCD_WIDTH;

  if (!active_menu->height)
    active_menu->height = LCD_HEIGHT - active_menu->y;

  if (!active_menu->itemHeight)
    active_menu->itemHeight = MENU_ITEM_H;

  if (active_menu->on_enter)
    active_menu->on_enter();

  is_pressed = false;
  is_editing = false;

  if (!active_menu->render_item) {
    active_menu->render_item = renderItem;
  }

  renderFn = active_menu->itemHeight >= MENU_ITEM_H ? AG_PrintMedium : AG_PrintSmall;
}

void AG_MENU_Init(Menu *main_menu) {
  active_menu = main_menu;
  menu_stack_top = 0;

  if (active_menu->i >= active_menu->num_items) {
    active_menu->i = 0;
  }

  init();
}

void AG_MENU_Deinit(void) {
  active_menu = NULL;
  is_pressed = false;
}

void AG_MENU_Reset(void) {
  active_menu = NULL;
  menu_stack_top = 0;
  is_pressed = false;
  is_editing = false;
}

// Map domain function helper
static int32_t ConvertDomain(int32_t aValue, int32_t aMin, int32_t aMax, int32_t bMin, int32_t bMax) {
   if (aMax == aMin) return bMin;
   return bMin + (aValue - aMin) * (bMax - bMin) / (aMax - aMin);
}

void AG_MENU_Render(void) {
  if (!active_menu)
    return;

  if (active_menu->on_tick)
    active_menu->on_tick(active_menu);

  // Rigorous bounds checking
  if (active_menu->num_items == 0) {
      active_menu->i = 0;
      AG_PrintSmallEx(LCD_WIDTH/2, active_menu->y + 10, POS_C, C_FILL, "(empty)");
  } else if (active_menu->i >= active_menu->num_items) {
      active_menu->i = active_menu->num_items - 1;
  }

  uint8_t itemsShow = active_menu->height / active_menu->itemHeight;
  if (itemsShow == 0) itemsShow = 1;

  // Calculate scrolling
  const uint16_t offset = (active_menu->i >= (itemsShow / 2)) ? active_menu->i - (itemsShow / 2) : 0;
  uint16_t effective_offset = offset;
  if (active_menu->num_items > itemsShow && effective_offset + itemsShow > active_menu->num_items)
     effective_offset = active_menu->num_items - itemsShow;
  if (active_menu->num_items <= itemsShow)
     effective_offset = 0;

  const uint16_t visible = MIN(active_menu->num_items, itemsShow);
  
  const uint8_t ex = getMenuRightEdge();

  AG_FillRect(active_menu->x, active_menu->y, active_menu->width,
           active_menu->height, C_CLEAR);
           
  for (uint16_t i = 0; i < visible; ++i) {
    uint16_t idx = i + effective_offset;
    if (idx >= active_menu->num_items)
      break;

    const bool isActive = idx == active_menu->i;
    const uint8_t y = active_menu->y + i * active_menu->itemHeight;

    active_menu->render_item(idx, i);

    if (isActive) {
      if (active_menu->items) {
          const MenuItem *item = &active_menu->items[idx];
          if (item->type == M_ITEM_SELECT) {
              if (is_editing) {
                  AG_FillRect(active_menu->x, y, ex - 4, active_menu->itemHeight, C_INVERT);
              } else {
                  AG_DrawRect(active_menu->x, y, ex - 4, active_menu->itemHeight, C_FILL);
              }
          } else {
              const uint8_t rw = ex - 4 - active_menu->x;
              if (is_pressed || active_menu->is_held) {
                  AG_DrawRect(active_menu->x, y, rw, active_menu->itemHeight, C_FILL);
              } else {
                  AG_FillRect(active_menu->x, y, rw, active_menu->itemHeight, C_INVERT);
              }
          }
      } else {
          const uint8_t rw = ex - 4 - active_menu->x;
          AG_FillRect(active_menu->x, y, rw, active_menu->itemHeight, C_INVERT);
      }
    }
  }

  // Scrollbar
  // Always draw scrollbar as requested (fagci style)
  const uint8_t ey = active_menu->y + active_menu->height;
  uint8_t y_pos;
  if (active_menu->num_items <= 1) {
      y_pos = active_menu->y;
  } else {
      y_pos = ConvertDomain(active_menu->i, 0, active_menu->num_items - 1,
                                   active_menu->y, ey - 3);
  }

  AG_DrawVLine(ex - 2, active_menu->y, active_menu->height, C_FILL);
  AG_FillRect(ex - 3, y_pos, 3, 3, C_FILL);
}

void AG_MENU_EnterMenu(Menu *submenu) {
    if (menu_stack_top < MENU_STACK_DEPTH) {
        menu_stack[menu_stack_top++] = active_menu;
        active_menu = submenu;
        active_menu->i = 0;
        gUpdateStatus = true;
        init();
    }
}

bool AG_MENU_HandleInput(KEY_Code_t key, bool key_pressed, bool key_held);

static bool handleUpDownNavigation(KEY_Code_t key, bool hasItems, bool key_held) {
  if (key != KEY_UP && key != KEY_DOWN && key != KEY_SIDE1 && key != KEY_SIDE2) {
    return false;
  }

  if (active_menu->num_items == 0) {
    return true;
  }

  active_menu->i = IncDecU(active_menu->i, 0, active_menu->num_items, (key == KEY_DOWN || key == KEY_SIDE2));
  
  if (!key_held) {
    AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
  }

  gUpdateStatus = true;

  if (!hasItems && active_menu->action) {
    active_menu->action(active_menu->i, key, false, false);
  }

  return true;
}

bool AG_MENU_IsActive(void) { return active_menu != NULL; }

bool AG_MENU_HandleInput(KEY_Code_t key, bool key_pressed, bool key_held) {
  if (!active_menu) {
    return false;
  }

  const bool hasItems = (active_menu->items != NULL);

  // Handle key release
  if (!key_pressed && !key_held) {
      if (is_pressed) {
          is_pressed = false;
          // Let action handle the release if needed
          if (hasItems) {
              const MenuItem *item = &active_menu->items[active_menu->i];
              if (item->action) {
                  item->action(item, key, false, false);
              }
          }
      } else {
          // Consume release if we didn't see the press, to prevent passthrough
          if (key == KEY_PTT || key == KEY_MENU || key == KEY_EXIT) return true;
          return false;
      }
  }

  if (is_pressed && key != KEY_MENU && key != KEY_PTT && (key_pressed || key_held)) {
      is_pressed = false;
  }

  if (hasItems) {
      const MenuItem *item = &active_menu->items[active_menu->i];
      
      if (is_editing) {
          if (key_pressed || key_held) {
              if (key == KEY_UP || key == KEY_DOWN || key == KEY_STAR || key == KEY_F || key == KEY_SIDE1 || key == KEY_SIDE2) {
                  if (item->change_value) {
                      bool up = (key == KEY_UP || key == KEY_F || key == KEY_SIDE1);
                      item->change_value(item, up);
                      if (!key_held && item->setting != MENU_ROGER) AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
                      return true;
                  }
              }
              if (key == KEY_MENU || key == KEY_EXIT || key == KEY_PTT) {
                  is_editing = false;
                  if (item->setting != MENU_ROGER) AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
                  return true;
              }
          }
          return true; // Consume all input while editing
      }
  }

  if (key_pressed || key_held) {
    if (handleUpDownNavigation(key, hasItems, key_held)) {
      is_pressed = false; // navigation resets press state
      return true;
    }
  }

  if (!hasItems) {
    if (active_menu->action &&
        active_menu->action(active_menu->i, key, key_pressed, key_held)) {
      return true;
    }
    return false;
  }

  const MenuItem *item = &active_menu->items[active_menu->i];

  if (key_pressed) {
    switch (key) {
    case KEY_PTT:
    case KEY_MENU: // Enter submenu or execute action
      if (!key_held) {
        if (item->type == M_ITEM_SELECT && item->change_value) {
            is_editing = true;
            if (item->setting != MENU_ROGER) AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
            return true;
        }
        
        is_pressed = true; // Visual state for buttons/folders
        
        if (item->submenu) {
          AG_MENU_EnterMenu(item->submenu);
          AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
          return true;
        } else if (item->action) {
           if (item->action(item, key, key_pressed, key_held)) {
               AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
               return true;
           }
        } else if (item->change_value) { // Toggle (M_ITEM_ACTION with change_value)
            item->change_value(item, true);
            AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
            return true;
        }
      } else if (is_pressed) {
        // Handle long press if needed (passed to action)
        if (item->action && item->action(item, key, key_pressed, key_held)) {
            return true;
        }
      }
      break;
    case KEY_EXIT:
      is_pressed = false;
      is_editing = false;
      return AG_MENU_Back();
    default:
      break;
    }
  }
  
  // Final fallthrough for other keys (UP/DOWN/F etc handled by action)
  if (key_pressed && item->action && item->action(item, key, key_pressed, key_held)) {
    return true;
  }

  // Consume navigation and confirmation keys to prevent passthrough to background apps
  if (key == KEY_UP || key == KEY_DOWN || key == KEY_MENU || key == KEY_EXIT || key == KEY_SIDE1 || key == KEY_SIDE2) {
      return true;
  }

  return false;
}

bool AG_MENU_Back(void) {
  if (menu_stack_top > 0) {
    active_menu = menu_stack[--menu_stack_top];
    gUpdateStatus = true;
    init();
    return true;
  }
  active_menu = NULL;
  return false;
}

void AG_MENU_GetPath(char *buf, size_t len) {
    if (!buf || len == 0) return;
    buf[0] = '\0';
    
    size_t offset = 0;
    
    for (uint8_t i = 0; i < menu_stack_top; ++i) {
        const char *t = menu_stack[i]->title ? menu_stack[i]->title : "";
        size_t t_len = strlen(t);
        
        if (offset + t_len + 4 > len) break; // Check for "title > "
        
        memcpy(buf + offset, t, t_len);
        offset += t_len;
        memcpy(buf + offset, " > ", 3);
        offset += 3;
    }
    
    // Active menu
    if (active_menu) {
        const char *t = active_menu->title ? active_menu->title : "";
        size_t t_len = strlen(t);
        if (offset + t_len + 1 > len) t_len = len - offset - 1;
        memcpy(buf + offset, t, t_len);
        offset += t_len;
    }
    buf[offset] = '\0';
}
