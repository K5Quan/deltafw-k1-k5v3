/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>
#include "drivers/bsp/st7565.h"
#include "drivers/bsp/bk4819.h"
#include "core/misc.h"
#include "features/radio/frequencies.h"
#include "apps/settings/settings.h"
#include "ui/helper.h"
#include "ui/main.h"
#include "drivers/bsp/systick.h"
#include "features/radio/functions.h"
#include <stdlib.h>

#if defined(ENABLE_RSSI_BAR) || defined(ENABLE_MIC_BAR)

static uint8_t s_rssi_peak = 0;
static uint16_t s_rssi_peak_timer = 0;

static uint8_t s_af_peak = 0;
static uint16_t s_af_peak_timer = 0;

static FUNCTION_Type_t s_last_func = FUNCTION_N_ELEM;

static const uint8_t U8RssiMap[] = { 121, 115, 109, 103, 97, 91, 85, 79, 73, 63 };

static uint8_t DBm2S(int dbm) {
    uint8_t i = 0;
    dbm *= -1;
    for (i = 0; i < ARRAY_SIZE(U8RssiMap); i++) {
        if (dbm >= U8RssiMap[i]) return i;
    }
    return i;
}

static int Rssi2DBm(uint16_t rssi) {
    return (rssi / 2) - 160 + dBmCorrTable[gRxVfo->Band];
}

#ifdef ENABLE_RSSI_BAR
void UI_DisplayRSSIBar(const bool now) {
  char String[16];

  if (gCurrentFunction != s_last_func) {
    s_rssi_peak = 0;
    s_af_peak = 0;
    s_last_func = gCurrentFunction;
  }

  const uint8_t LINE = 3;
  const uint8_t BAR_LEFT_MARGIN = 24;

  int16_t rssi = BK4819_GetRSSI();
  int dBm = Rssi2DBm(rssi);
  uint8_t s = DBm2S(dBm);
  uint8_t *line = gFrameBuffer[LINE];

  memset(line, 0, 128);

  // Peak tracking (s is current strength 0-12)
  if (s >= s_rssi_peak) {
    s_rssi_peak = s;
    s_rssi_peak_timer = 60; // Hold peak for a while
  } else {
    if (s_rssi_peak_timer > 0) {
      s_rssi_peak_timer--;
    } else if (s_rssi_peak > 0) {
      s_rssi_peak--; // Slowly recede
      s_rssi_peak_timer = 10; // Slow down recession
    }
  }

  // Draw bars (4px wide, 5px step)
  for (int i = 0; i < s && i < 12; i++) {
    uint8_t x = BAR_LEFT_MARGIN + i * 5;
    if (x + 3 < 128) {
      bool hollow = (i >= 9);
      line[x] = line[x + 3] = 0b00111110;
      line[x + 1] = line[x + 2] = hollow ? 0b00100010 : 0b00111110;
    }
  }

  // Draw 2px peak line (only if not overlapping hollow block)
  // Hollow starts at level 10 (index 9)
  if (s_rssi_peak > 0 && s_rssi_peak <= 12 && s_rssi_peak < 10) {
    uint8_t px = BAR_LEFT_MARGIN + (s_rssi_peak - 1) * 5 + 2;
    if (px + 1 < 128) {
      line[px] |= 0x3E;
      line[px + 1] |= 0x3E;
    }
  }

  // dBm Label
  NUMBER_ToDecimal(String, abs(dBm), 3, false);
  char *p = String;
  while (*p == ' ') p++;
  
  char label[16];
  if (dBm < 0) {
    label[0] = '-';
    strcpy(label + 1, p);
  } else {
    strcpy(label, p);
  }
  strcat(label, " dBm");
  
  uint8_t labelX = 128 - (strlen(label) * 4);
  UI_PrintStringSmallest(label, labelX, LINE * 8 + 1, false, true);
  
  if (s <= 9) {
    String[0] = 'S';
    String[1] = s + '0';
    String[2] = '\0';
  } else {
    strcpy(String, "S9+");
    String[3] = (s - 9) + '0';
    String[4] = '0';
    String[5] = '\0';
  }
  UI_PrintStringSmallest(String, 2, LINE * 8 + 1, false, true);
  
  if (now) ST7565_BlitLine(LINE);
}
#endif

#ifdef ENABLE_MIC_BAR
void UI_DisplayAudioBar(void) {
  char String[16];

  if (gCurrentFunction != s_last_func) {
    s_rssi_peak = 0;
    s_af_peak = 0;
    s_last_func = gCurrentFunction;
  }

  const uint8_t LINE = 3;
  uint8_t *line = gFrameBuffer[LINE];
  memset(line, 0, 128);

  const uint8_t BAR_LEFT_MARGIN = 0;

  bool is_mic = false;
#ifdef ENABLE_MIC_BAR
  if (gSetting_mic_bar) is_mic = true;
#endif

  uint8_t s = 0;

  if (is_mic) {
      uint8_t afDB = BK4819_ReadRegister(0x6F) & 0b1111111;
      
      // Logarithmic-style scaling for audio: sensitive at bottom, compressed at top
      int val = afDB - 26;
      if (val < 0) val = 0;
      
      // Custom non-linear mapping for 12 segments
      if (val >= 1)  s = 1;
      if (val >= 2)  s = 2;
      if (val >= 4)  s = 3;
      if (val >= 7)  s = 4;
      if (val >= 11) s = 5;
      if (val >= 16) s = 6;
      if (val >= 23) s = 7;
      if (val >= 32) s = 8;
      if (val >= 43) s = 9;
      if (val >= 57) s = 10;
      if (val >= 75) s = 11;
      if (val >= 95) s = 12;

      // Audio dB Label
      NUMBER_ToDecimal(String, afDB, 3, false);
      char *ap = String;
      while (*ap == ' ') ap++;
      strcpy(String, ap);
      strcat(String, " dB");
      
      uint8_t labelX = 128 - (strlen(String) * 4);
      UI_PrintStringSmallest(String, labelX, LINE * 8 + 1, false, true);

  } else {
      // TX Power Bar mapping
      // Get the TX power level reading (from TX envelope or configured power)
      // Since there is no direct envelope register in the open datasheet easily accessible,
      // we'll map the current selected power to a dynamic display.
      // E.g., User/Low1..Low5/Mid/High
      
      #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
          uint8_t pwr = gTxVfo->OUTPUT_POWER;
          // map 0..7 to segments
          // High(7) = 12, Mid(6) = 10, Low5(5) = 8, Low4(4) = 6, Low3(3) = 4, Low2(2) = 3, Low1(1) = 2, User(0) = 1
          uint8_t pwr2seg[] = {2, 2, 3, 4, 6, 8, 10, 12};
          if (pwr < 8) s = pwr2seg[pwr];
      #else
          uint8_t pwr = gTxVfo->OUTPUT_POWER;
          if (pwr == 0) s = 3;       // Low
          else if (pwr == 1) s = 8;  // Mid 
          else s = 12;               // High
      #endif
      
      // Some dynamic variation (flicker) to make it feel alive, based on tick
      uint8_t flicker = (SYSTICK_GetTick() % 3);
      if (s > 2 && flicker == 0) s -= 1; 

      strcpy(String, "TX PWR");
      uint8_t labelX = 128 - (strlen(String) * 4);
      UI_PrintStringSmallest(String, labelX, LINE * 8 + 1, false, true);
  }



  // Peak tracking
  if (s >= s_af_peak) {
    s_af_peak = s;
    s_af_peak_timer = 20;
  } else {
    if (s_af_peak_timer > 0) {
      s_af_peak_timer--;
    } else if (s_af_peak > 0) {
      s_af_peak--;
      s_af_peak_timer = 5;
    }
  }

  // Draw bars (4px wide, 5px step)
  for (int i = 0; i < s && i < 12; i++) {
    uint8_t x = BAR_LEFT_MARGIN + i * 5;
    if (x + 3 < 128) {
      bool clipping = (i >= 10);
      line[x] = line[x + 3] = 0b00111110;
      line[x + 1] = line[x + 2] = clipping ? 0b00100010 : 0b00111110;
    }
  }

  // Draw 2px peak line (only if not overlapping clipping block)
  // Clipping starts at level 11 (index 10)
  if (s_af_peak > 0 && s_af_peak <= 12 && s_af_peak < 11) {
    uint8_t px = BAR_LEFT_MARGIN + (s_af_peak - 1) * 5 + 2;
    if (px + 1 < 128) {
      line[px] |= 0x3E;
      line[px + 1] |= 0x3E;
    }
  }


  
  if (gCurrentFunction == FUNCTION_TRANSMIT)
      ST7565_BlitFullScreen();
}
#endif

#endif
