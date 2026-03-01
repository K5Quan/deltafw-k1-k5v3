#ifndef CW_UI_H
#define CW_UI_H

#include <stdbool.h>
#include "drivers/bsp/keyboard.h"

#ifdef ENABLE_CW_KEYER

void CW_UI_Init(void);
void CW_UI_Render(void);
void CW_UI_HandleInput(KEY_Code_t key, bool key_pressed, bool key_held);

#endif // ENABLE_CW_KEYER

#endif // CW_UI_H
