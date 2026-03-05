#ifndef HERMES_UI_H
#define HERMES_UI_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>

typedef enum {
    HM_VIEW_MENU,
    HM_VIEW_CHAT,
    HM_VIEW_COMPOSE,
    HM_VIEW_SETTINGS,
    HM_VIEW_INPUT_ALIAS,
    HM_VIEW_INPUT_PASSCODE,
    HM_VIEW_INPUT_SALT,
    HM_VIEW_CONTACTS,
    HM_VIEW_INPUT_CONTACT,
} HermesView_t;

extern HermesView_t gHermesView;
extern int16_t      gHermesChatScroll;

void HERMES_UI_Init(void);
void HERMES_UI_Render(void);
void HERMES_UI_StartCompose(void);
extern uint8_t     composeDestID[6];
extern uint8_t     composeAddrMode;

#endif
#endif
