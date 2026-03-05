#ifndef HERMES_H
#define HERMES_H

#include <stdbool.h>
#include "drivers/bsp/keyboard.h"

void HERMES_Init(void);
void HERMES_UI_Init(void);
void HERMES_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);

#endif
