#ifndef APP_CW_COMPOSE_H
#define APP_CW_COMPOSE_H

#include <stdbool.h>
#include <stdint.h>
#include "features/action/action.h"

void CW_COMPOSE_Init(void);
void CW_COMPOSE_Render(void);
void CW_COMPOSE_ProcessKey(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);

#endif
