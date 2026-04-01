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
#include "features/rx/signal_quality.h"

#include "apps/scanner/chFrScanner.h"
#ifdef ENABLE_FMRADIO
    #include "apps/fm/fm.h"
    #include "drivers/bsp/bk1080.h"
#endif
#include "apps/scanner/scanner.h"
#include "ui/bitmaps.h"
#include "drivers/bsp/keyboard.h"
#include "drivers/bsp/st7565.h"
#include "features/radio/functions.h"
#include "apps/battery/battery.h"
#include "core/misc.h"
#include "apps/settings/settings.h"
#include "apps/battery/battery_ui.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"
#include "ui/ag_menu.h"
#include "apps/security/passcode.h"

static const char *gStatusTitleOverride = NULL;
static bool gStatusTitleCentered = false;

void UI_SetStatusTitle(const char *title) {
    gStatusTitleOverride = title;
    gStatusTitleCentered = false;
}

void UI_SetStatusTitleCentered(const char *title) {
    gStatusTitleOverride = title;
    gStatusTitleCentered = true;
}


#include "features/radio/radio.h"

static uint8_t UI_GetStatusSignalLevel(void) {
#ifdef ENABLE_ANTENNA_SIGNAL_BAR
    if (gCurrentFunction == FUNCTION_TRANSMIT) {
        // Enforce min/max for specific settings
        if (gCurrentVfo->OUTPUT_POWER == OUTPUT_POWER_HIGH) return 5;
        if (gCurrentVfo->OUTPUT_POWER == OUTPUT_POWER_LOW1) return 1;

        // Map PA bias (TXP_CalculatedSetting, 0-255) to 1-5 bars for granularity
        uint8_t pwr = gCurrentVfo->TXP_CalculatedSetting;
        if (pwr < 45)  return 1;
        if (pwr < 90)  return 2;
        if (pwr < 135) return 3;
        if (pwr < 185) return 4;
        return 5;
    }

    if (FUNCTION_IsRx()) {
        return SIGNAL_QUALITY_GetLevel();
    }
#else
    if (gCurrentFunction == FUNCTION_TRANSMIT) {
        // Map power levels (0:LOW, 1:MID, 2:HIGH) to bars (1, 3, 5)
        if (gCurrentVfo->OUTPUT_POWER == 0) return 1;
        if (gCurrentVfo->OUTPUT_POWER == 1) return 3;
        return 5;
    }

    if (FUNCTION_IsRx()) {
        uint8_t bar = gVFO_RSSI_bar_level[gEeprom.RX_VFO];
        // Radio RSSI bars are 0-6. Map to 1-5.
        if (bar > 5) return 5;
        if (bar > 0) return bar;
    }
#endif

    return 0;
}

void UI_DisplayStatus() {
    gUpdateStatus = false;
    memset(gStatusLine, 0, sizeof(gStatusLine));

    uint8_t x_off = 0;
    bool simplified = AG_MENU_IsActive();

    // 1. Draw Universal Antenna & Bars
    if (!simplified) {
        uint8_t signal = UI_GetStatusSignalLevel();
#ifdef ENABLE_ANTENNA_SIGNAL_BAR
        if (signal > 0) {
            UI_DrawAntenna(gStatusLine, signal);
            x_off = 15; // Icon(13) + Gap(2)
        }
#else
        UI_DrawAntenna(gStatusLine, signal);
        x_off = 15; // Icon(13) + Gap(2)
#endif
    }

    // 2. Battery Calculation (Pre-calculate to know available width for path/title)
    bool show_icon = (gSetting_battery_text >= 3 && gSetting_battery_text <= 5);
    bool show_volt = (gSetting_battery_text == 1 || gSetting_battery_text == 4 || gSetting_battery_text == 6);
    bool show_perc = (gSetting_battery_text == 2 || gSetting_battery_text == 5 || gSetting_battery_text == 7);
    
    char bat_str[10] = {0};
    uint8_t bat_str_width = 0;
    if (show_volt || show_perc) {
        if (show_volt) {
            const uint16_t voltage = (gBatteryVoltageAverage <= 999) ? gBatteryVoltageAverage : 999;
            NUMBER_ToDecimal(bat_str, voltage / 100, 1, false);
            bat_str[1] = '.';
            NUMBER_ToDecimal(bat_str + 2, voltage % 100, 2, true);
            strcat(bat_str, "V");
        } else {
            NUMBER_ToDecimal(bat_str, BATTERY_VoltsToPercent(gBatteryVoltageAverage), 3, false);
            strcat(bat_str, "%");
        }
        bat_str_width = strlen(bat_str) * 4;
    }

    uint8_t battery_left_x = 128;
    if (show_icon) {
        battery_left_x = (bat_str_width > 0) ? (115 - bat_str_width - 2) : 115;
    } else if (bat_str_width > 0) {
        battery_left_x = 128 - bat_str_width - 1;
    }

    // 3. Center Title / Path
    if (gStatusTitleOverride || AG_MENU_IsActive()) {
        char path[64]; // Increased buffer for safer truncation
        const char *display_text = gStatusTitleOverride;
        
        if (!display_text) {
            AG_MENU_GetPath(path, sizeof(path));
            display_text = path;
        }

        uint8_t w = strlen(display_text) * 4;
        
        if (gStatusTitleCentered && !AG_MENU_IsActive()) {
            UI_PrintStringSmallest(display_text, (128 - w) / 2, 0, true, true);
        } else {
            uint8_t max_w = (battery_left_x > x_off + 4) ? (battery_left_x - x_off - 4) : 0;
            
            if (w > max_w && max_w > 12) {
                char truncated[32];
                uint8_t chars_to_show = (max_w - 12) / 4;
                if (chars_to_show > 28) chars_to_show = 28;
                strncpy(truncated, display_text, chars_to_show);
                truncated[chars_to_show] = '.';
                truncated[chars_to_show + 1] = '.';
                truncated[chars_to_show + 2] = '.';
                truncated[chars_to_show + 3] = '\0';
                UI_PrintStringSmallest(truncated, x_off + 2, 0, true, true);
                x_off += (chars_to_show + 3) * 4 + 4;
            } else {
                UI_PrintStringSmallest(display_text, x_off + 2, 0, true, true);
                x_off += w + 4;
            }
        }
    }

    if (simplified) {
        // Simplified view still shows battery
        goto draw_battery;
    }

    // 4. Flowing Left Indicators
    // ...
    
#ifdef ENABLE_NOAA
    if (!(gScanStateDir != SCAN_OFF || SCANNER_IsScanning()) && gIsNoaaMode) {
        UI_PrintStringSmallest("NOAA", x_off, 0, true, true);
        x_off += 18;
    }
#endif

    if (gEeprom.KEY_LOCK) {
        UI_PrintStringSmallest("LOCK", x_off, 0, true, true);
        x_off += 18;
    } else if (gWasFKeyPressed) {
        // Draw inverted F-key box
        for (uint8_t i = 0; i < 5; i++) gStatusLine[x_off + i] |= 0b00111111;
        gStatusLine[x_off + 1] ^= 0x1f;
        gStatusLine[x_off + 2] ^= 0x05;
        gStatusLine[x_off + 3] ^= 0x05;
        x_off += 7;
    }

#ifdef ENABLE_VOX
    if (gEeprom.VOX_SWITCH) {
        UI_PrintStringSmallest("VOX", x_off, 0, true, true);
        x_off += 14;
    }
#endif

    if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) {
        UI_PrintStringSmallest("DW", x_off, 0, true, true);
        x_off += 10;
    }

#ifdef ENABLE_MESH_NETWORK
    extern bool gHermesHasNewMessage;
    if (gHermesHasNewMessage) {
        // Draw a tiny envelope or just the text "MSG"
        UI_PrintStringSmallest("MSG", x_off, 0, true, true);
        x_off += 14;
    }
#endif

draw_battery:
    // 5. Draw Battery
    if (show_icon) {
        UI_DisplayBattery(gBatteryDisplayLevel, gLowBatteryBlink);
    }

    if (bat_str[0]) {
        uint8_t x_pos = (show_icon) ? (115 - bat_str_width - 2) : (128 - bat_str_width - 1);
        UI_PrintStringSmallest(bat_str, x_pos, 0, true, true);
    }
    
skip_indicators:
    // Draw separator line at y=6 (Row 7)
    for (int i = 0; i < 128; i++) {
        gStatusLine[i] |= (1 << 6);
    }

    ST7565_BlitStatusLine();
}

