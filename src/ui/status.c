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

#include "apps/scanner/chFrScanner.h"
#ifdef ENABLE_FMRADIO
    #include "apps/fm/fm.h"
#endif
#include "apps/scanner/scanner.h"
#include "ui/bitmaps.h"
#include "drivers/bsp/keyboard.h"
#include "drivers/bsp/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "apps/battery/battery.h"
#include "core/misc.h"
#include "apps/settings/settings.h"
#include "apps/battery/battery_ui.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"
#include "ui/ag_menu.h"

#ifdef ENABLE_RX_TX_TIMER_DISPLAY
#ifndef ENABLE_FIRMWARE_DEBUG_LOGGING
static void convertTime(uint8_t *line, uint8_t type) 
{
    uint16_t t = (type == 0) ? (gTxTimerCountdown_500ms / 2) : (3600 - gRxTimerCountdown_500ms / 2);

    uint8_t m = t / 60;
    uint8_t s = t - (m * 60); // Replace modulo with subtraction for efficiency

    gStatusLine[0] = gStatusLine[7] = gStatusLine[14] = 0x00; // Quick fix on display (on scanning I, II, etc.)

    char str[6];
    sprintf(str, "%02u:%02u", m, s);
    UI_PrintStringSmallBufferNormal(str, line);

    gUpdateStatus = true;
}
#endif
#endif


void UI_DisplayStatus() {
    gUpdateStatus = false;
    memset(gStatusLine, 0, sizeof(gStatusLine));

    char str[42] = "";
    char *p = str;

    if (AG_MENU_IsActive()) {
        AG_MENU_GetPath(str, sizeof(str));
    } else {
#ifdef ENABLE_NOAA
    if (!(gScanStateDir != SCAN_OFF || SCANNER_IsScanning()) && gIsNoaaMode) { // NOAA SCAN indicator
        *p++ = 'N';
        *p++ = 'O';
        *p++ = 'A';
        *p++ = 'A';
        *p++ = ' ';
    }
    else if (gCurrentFunction == FUNCTION_POWER_SAVE) {
        *p++ = 'S';
        *p++ = ' ';
    }
#else
    if (gCurrentFunction == FUNCTION_POWER_SAVE) {
        *p++ = 'S';
        *p++ = ' ';
    }
#endif

    if (gEeprom.KEY_LOCK) {
        *p++ = 'L';
        *p++ = ' ';
    }
    else if (gWasFKeyPressed) {
        *p++ = 'F';
        *p++ = ' ';
    }

#ifdef ENABLE_VOX
    if (gEeprom.VOX_SWITCH) {
        *p++ = 'V';
        *p++ = 'O';
        *p++ = 'X';
        *p++ = ' ';
    }
#endif

    if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) {
         *p++ = 'D';
         *p++ = 'W';
         *p++ = ' ';
    } else if (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF) {
         *p++ = 'X';
         *p++ = 'B';
         *p++ = ' ';
    }
    
    // USB-C
#ifdef ENABLE_USBC_CHARGING_INDICATOR
    if (gChargingWithTypeC) {
        *p++ = 'U';
        *p++ = 'S';
        *p++ = 'B';
        *p++ = ' ';
    }
#endif
    *p = '\0';
    }

    if (gEeprom.RX_VFO == 0 && gEeprom.TX_VFO == 0) {
        // Main VFO A
    }
    UI_PrintStringSmallest(str, 0, 0, true, true);

    // Battery Display
    // Modes:
    // 0: NONE
    // 1: VOLTAGE
    // 2: PERCENT
    // 3: ICON
    // 4: I+VOLT
    // 5: I+PERC
    // 6: VOLT (Duplicate?)
    // 7: PERC (Duplicate?)

    bool show_icon = (gSetting_battery_text >= 3 && gSetting_battery_text <= 5);
    bool show_volt = (gSetting_battery_text == 1 || gSetting_battery_text == 4 || gSetting_battery_text == 6);
    bool show_perc = (gSetting_battery_text == 2 || gSetting_battery_text == 5 || gSetting_battery_text == 7);

    // If icon is shown, it occupies 115-127 approx (13 px)
    // text should end at 113.
    // If no icon, text should end at 127.

    if (show_icon) {
        UI_DisplayBattery(gBatteryDisplayLevel, gLowBatteryBlink);
    }

    if (show_volt || show_perc) {
        char bat_str[10];
        if (show_volt) {
             const uint16_t voltage = (gBatteryVoltageAverage <= 999) ? gBatteryVoltageAverage : 999;
             sprintf(bat_str, "%u.%02uV", voltage / 100, voltage % 100);
        } else {
             sprintf(bat_str, "%u%%", BATTERY_VoltsToPercent(gBatteryVoltageAverage));
        }

        uint8_t str_len = strlen(bat_str);
        uint8_t str_width = str_len * 4;
        uint8_t x_pos = (show_icon) ? (115 - str_width - 2) : (128 - str_width - 1);
        
        UI_PrintStringSmallest(bat_str, x_pos, 0, true, true);
    }
    
    
    // Draw separator line at y=6 (Row 7)
    for (int i = 0; i < 128; i++) {
        gStatusLine[i] |= (1 << 6);
    }

    ST7565_BlitStatusLine();
}

