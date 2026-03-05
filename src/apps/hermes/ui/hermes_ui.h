#ifndef HERMES_UI_H
#define HERMES_UI_H

#ifdef ENABLE_MESH_NETWORK

#include <stdint.h>

typedef enum {
    HM_VIEW_MENU,
    HM_VIEW_CHAT,
    HM_VIEW_COMPOSE,
    HM_VIEW_SETTINGS,
} HermesView_t;

extern HermesView_t gHermesView;
extern int16_t      gHermesChatScroll;

void HERMES_UI_Init(void);
void HERMES_UI_Render(void);
void HERMES_UI_StartCompose(void);

#endif
#endif
