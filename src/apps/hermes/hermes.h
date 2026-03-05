#ifndef HERMES_H
#define HERMES_H

#ifdef ENABLE_MESH_NETWORK

#include <stdbool.h>
#include "drivers/bsp/keyboard.h"
#include "apps/hermes/hermes_types.h"

// ──── Global State ────
extern HermesConfig_t  gHermesConfig;
extern bool            gHermesEnabled;
extern HermesMessage_t gHermesMessages[HM_MSG_SLOTS];
extern uint8_t         gHermesMsgCount;
extern bool            gHermesHasNewMessage;

// ──── Core API ────
void HERMES_Init(void);
void HERMES_UI_Init(void);
void HERMES_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);

// ──── Protocol Processing ────
void HERMES_HandleFSKInterrupt(uint16_t interrupt_bits);
void HERMES_Tick(void);  // Called from scheduler for ARQ/routing timers
void HERMES_SendMessage(const HermesMessage_t *m);

#endif // ENABLE_MESH_NETWORK
#endif
