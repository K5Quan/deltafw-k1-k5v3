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
#include <stdlib.h>  // abs()

#include "apps/scanner/chFrScanner.h"
#include "core/scheduler.h"
#include "features/dtmf/dtmf.h"
#ifdef ENABLE_AM_FIX
    #include "features/am_fix/am_fix.h"
#endif
#include "ui/bitmaps.h"
#include "core/board.h"
#include "drivers/bsp/bk4819.h"
#include "drivers/bsp/st7565.h"
#ifdef ENABLE_LIVESEEK
    #include "apps/liveseek/liveseek.h"
#endif
#include "features/radio/functions.h"
#include "apps/battery/battery.h"
#include "core/misc.h"
#include "features/radio/radio.h"
#include "apps/settings/settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/ui.h"
#include "features/audio/audio.h"

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    #include "drivers/bsp/system.h"
#endif

#ifdef ENABLE_CW_KEYER
    #include "features/cw/cw.h"
#endif

#ifdef ENABLE_SIGNAL_CLASSIFIER
    #include "features/rx/signal_classifier.h"
#endif

center_line_t center_line = CENTER_LINE_NONE;

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    static int8_t RxBlink;
    static int8_t RxBlinkLed = 0;
    static int8_t RxBlinkLedCounter;
    static int8_t RxLine;
    static uint32_t RxOnVfofrequency;

    bool isMainOnlyInputDTMF = false;

    static bool isMainOnly()
    {
        return (gEeprom.DUAL_WATCH == DUAL_WATCH_OFF) && (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF);
    }
#endif

const int8_t dBmCorrTable[7] = {
            -15, // band 1
            -25, // band 2
            -20, // band 3
            -4, // band 4
            -7, // band 5
            -6, // band 6
             -1  // band 7
        };

void UI_DisplayRSSIBar(const bool now);

const char *VfoStateStr[] = {
       [VFO_STATE_NORMAL]="",
       [VFO_STATE_BUSY]="BUSY",
       [VFO_STATE_BAT_LOW]="BAT LOW",
       [VFO_STATE_TX_DISABLE]="TX DISABLE",
       [VFO_STATE_TIMEOUT]="TIMEOUT",
       [VFO_STATE_ALARM]="ALARM",
       [VFO_STATE_VOLTAGE_HIGH]="VOLT HIGH"
};

// ***************************************************************************

#if defined(ENABLE_AM_FIX) && defined(ENABLE_AM_FIX_SHOW_DATA)
#include "features/am_fix/am_fix.h"
#endif


#ifdef ENABLE_AGC_SHOW_DATA
void UI_MAIN_PrintAGC(bool now)
{
    char buf[20];
    memset(gFrameBuffer[3], 0, 128);
    union {
        struct {
            uint16_t _ : 5;
            uint16_t agcSigStrength : 7;
            int16_t gainIdx : 3;
            uint16_t agcEnab : 1;
        };
        uint16_t __raw;
    } reg7e;
    reg7e.__raw = BK4819_ReadRegister(0x7E);
    uint8_t gainAddr = reg7e.gainIdx < 0 ? 0x14 : 0x10 + reg7e.gainIdx;
    union {
        struct {
            uint16_t pga:3;
            uint16_t mixer:2;
            uint16_t lna:3;
            uint16_t lnaS:2;
        };
        uint16_t __raw;
    } agcGainReg;
    agcGainReg.__raw = BK4819_ReadRegister(gainAddr);
    int8_t lnaShortTab[] = {-28, -24, -19, 0};
    int8_t lnaTab[] = {-24, -19, -14, -9, -6, -4, -2, 0};
    int8_t mixerTab[] = {-8, -6, -3, 0};
    int8_t pgaTab[] = {-33, -27, -21, -15, -9, -6, -3, 0};
    int16_t agcGain = lnaShortTab[agcGainReg.lnaS] + lnaTab[agcGainReg.lna] + mixerTab[agcGainReg.mixer] + pgaTab[agcGainReg.pga];

    // sprintf(buf, "%d%2d %2d %2d %3d", reg7e.agcEnab, reg7e.gainIdx, -agcGain, reg7e.agcSigStrength, BK4819_GetRSSI());
    memset(buf, ' ', 14);
    buf[14] = '\0';
    buf[0] = reg7e.agcEnab + '0';
    NUMBER_ToDecimal(buf + 1, reg7e.gainIdx, 2, false);
    NUMBER_ToDecimal(buf + 4, -agcGain, 2, false);
    NUMBER_ToDecimal(buf + 7, reg7e.agcSigStrength, 2, false);
    NUMBER_ToDecimal(buf + 10, BK4819_GetRSSI(), 3, false);
    
    UI_PrintStringSmallNormal(buf, 2, 0, 3);
    if(now)
        ST7565_BlitLine(3);
}
#endif

void UI_MAIN_TimeSlice500ms(void)
{
    if(gScreenToDisplay==DISPLAY_MAIN) {
#ifdef ENABLE_AGC_SHOW_DATA
        UI_MAIN_PrintAGC(true);
        return;
#endif

        if(FUNCTION_IsRx()) {
#ifdef ENABLE_RSSI_BAR
#ifdef ENABLE_CW_KEYER
        if (gRxVfo->Modulation != MODULATION_CW)
#endif
            UI_DisplayRSSIBar(true);
#endif
        }
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS // Blink Green Led for white...
        else if(gSetting_set_eot > 0 && RxBlinkLed == 2)
        {
            if(RxBlinkLedCounter <= 8)
            {
                if(RxBlinkLedCounter % 2 == 0)
                {
                    if(gSetting_set_eot > 1 )
                    {
                        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
                    }
                }
                else
                {
                    if(gSetting_set_eot > 1 )
                    {
                        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
                    }

                    if(gSetting_set_eot == 1 || gSetting_set_eot == 3)
                    {
                        switch(RxBlinkLedCounter)
                        {
                            case 1:
                            AUDIO_PlayBeep(BEEP_400HZ_30MS);
                            break;

                            case 3:
                            AUDIO_PlayBeep(BEEP_400HZ_30MS);
                            break;

                            case 5:
                            AUDIO_PlayBeep(BEEP_500HZ_30MS);
                            break;

                            case 7:
                            AUDIO_PlayBeep(BEEP_600HZ_30MS);
                            break;
                        }
                    }
                }
                RxBlinkLedCounter += 1;
            }
            else
            {
                RxBlinkLed = 0;
            }
        }
#endif
    }
}

// ***************************************************************************

#ifdef ENABLE_CW_KEYER
// (Function moved to features/cw.c)
#endif

// --- Flash Optimization Wrappers ---
static void UI_PrintMainString(const char *str, uint8_t x, uint8_t y) {
    UI_PrintString(str, x, 0, y, 8);
}
static void UI_PrintMainStringSmall(const char *str, uint8_t x, uint8_t y) {
    UI_PrintStringSmallNormal(str, x, 0, y);
}

void UI_DisplayMain(void)
{
    char               String[22];

    center_line = CENTER_LINE_NONE;

    // clear the screen
    UI_DisplayClear();

    if(gLowBattery && !gLowBatteryConfirmed) {
        UI_DisplayPopup("LOW BATTERY");
        ST7565_BlitFullScreen();
        return;
    }

#ifndef ENABLE_CUSTOM_FIRMWARE_MODS
    if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
    {   // tell user how to unlock the keyboard
        UI_PrintMainString("Long press #", 0, 1);
        UI_PrintMainString("to unlock",    0, 3);
        ST7565_BlitFullScreen();
        return;
    }
#else
    if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
    {   // tell user how to unlock the keyboard
        uint8_t shift = 3;

        /*
        BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
        SYSTEM_DelayMs(50);
        BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
        SYSTEM_DelayMs(50);
        */

        if(isMainOnly())
        {
            shift = 5;
        }
        //memcpy(gFrameBuffer[shift] + 2, gFontKeyLock, sizeof(gFontKeyLock));
        UI_PrintStringSmallBold("UNLOCK KEYBOARD", 12, 0, shift);
        //memcpy(gFrameBuffer[shift] + 120, gFontKeyLock, sizeof(gFontKeyLock));

        /*
        for (uint8_t i = 12; i < 116; i++)
        {
            gFrameBuffer[shift][i] ^= 0xFF;
        }
        */
    }
#endif


    unsigned int activeTxVFO = gRxVfoIsActive ? gEeprom.RX_VFO : gEeprom.TX_VFO;

    // Check if the current scan list has any participants (MR mode only)
    bool hasParticipants = true;
    if (gScanStateDir != SCAN_OFF && IS_MR_CHANNEL(gEeprom.ScreenChannel[gEeprom.RX_VFO])) {
        hasParticipants = (RADIO_FindNextChannel(0, 1, true, gEeprom.SCAN_LIST_DEFAULT) != 0xFF);
    }

    for (unsigned int vfo_num = 0; vfo_num < 2; vfo_num++)
    {

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
        const unsigned int line0 = 0;  // text screen line
        const unsigned int line1 = 4;
        unsigned int line;
        if (isMainOnly())
        {
            line       = 0;
        }
        else
        {
            line       = (vfo_num == 0) ? line0 : line1;
        }
        const bool         isMainVFO  = (vfo_num == gEeprom.TX_VFO);
        uint8_t           *p_line0    = gFrameBuffer[line + 0];
#if !defined(ENABLE_RSSI_BAR) && !defined(ENABLE_MIC_BAR)
        uint8_t           *p_line1    = gFrameBuffer[line + 1];
#endif

        enum Vfo_txtr_mode mode       = VFO_MODE_NONE;      
        char              attrStr[32]; // Buffer for concatenated attributes
        attrStr[0] = 0;                // Initialize empty
#else
        const unsigned int line0 = 0;  // text screen line
        const unsigned int line1 = 4;
        const unsigned int line       = (vfo_num == 0) ? line0 : line1;
        const bool         isMainVFO  = (vfo_num == gEeprom.TX_VFO);
        uint8_t           *p_line0    = gFrameBuffer[line + 0];
        enum Vfo_txtr_mode mode       = VFO_MODE_NONE;
#endif

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    if (isMainOnly())
    {
        if (activeTxVFO != vfo_num)
        {
            continue;
        }
    }
#endif

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
        if (activeTxVFO != vfo_num || isMainOnly())
#else
        if (activeTxVFO != vfo_num) // this is not active TX VFO
#endif
        {
#ifdef ENABLE_SCAN_RANGES
            if(gScanRangeStart) {

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
                //if(IS_FREQ_CHANNEL(gEeprom.ScreenChannel[0]) && IS_FREQ_CHANNEL(gEeprom.ScreenChannel[1])) {
                if(IS_FREQ_CHANNEL(gEeprom.ScreenChannel[activeTxVFO])) {

                    uint8_t shift = 0;

                    if (isMainOnly())
                    {
                        shift = 3;
                    }

                    UI_PrintString("ScnRng", 5, 0, line + shift, 8);
                    UI_PrintFrequencyEx(String, gScanRangeStart, true);
                    UI_PrintStringSmallNormal(String, 56, 0, line + shift);
                    UI_PrintFrequencyEx(String, gScanRangeStop, true);
                    UI_PrintStringSmallNormal(String, 56, 0, line + shift + 1);

                    if (!isMainOnly())
                        continue;
                }
                else
                {
                    gScanRangeStart = 0;
                }
#else
                UI_PrintMainString("ScnRng", 5, line);
                UI_PrintFrequencyEx(String, gScanRangeStart, true);
                UI_PrintMainStringSmall(String, 56, line);
                UI_PrintFrequencyEx(String, gScanRangeStop, true);
                UI_PrintMainStringSmall(String, 56, line + 1);
                continue;
#endif
            }
#endif


#ifdef ENABLE_DTMF_CALLING
            if (gDTMF_InputMode
                || gDTMF_CallState != DTMF_CALL_STATE_NONE || gDTMF_IsTx
            ) {
#else
            if (false) {
#endif
                char *pPrintStr = "";
                // show DTMF stuff
#ifdef ENABLE_DTMF_CALLING
                char Contact[16];
                if (!gDTMF_InputMode) {
                    if (gDTMF_CallState == DTMF_CALL_STATE_CALL_OUT) {
                        pPrintStr = DTMF_FindContact(gDTMF_String, Contact) ? Contact : gDTMF_String;
                    } else if (gDTMF_CallState == DTMF_CALL_STATE_RECEIVED || gDTMF_CallState == DTMF_CALL_STATE_RECEIVED_STAY){
                        pPrintStr = DTMF_FindContact(gDTMF_Callee, Contact) ? Contact : gDTMF_Callee;
                    }else if (gDTMF_IsTx) {
                        pPrintStr = gDTMF_String;
                    }
                }

                UI_PrintMainString(pPrintStr, 2, 2 + (vfo_num * 3));

                pPrintStr = "";
                if (!gDTMF_InputMode) {
                    if (gDTMF_CallState == DTMF_CALL_STATE_CALL_OUT) {
                        pPrintStr = (gDTMF_State == DTMF_STATE_CALL_OUT_RSP) ? "CALL OUT(RSP)" : "CALL OUT";
                    } else if (gDTMF_CallState == DTMF_CALL_STATE_RECEIVED || gDTMF_CallState == DTMF_CALL_STATE_RECEIVED_STAY){
                        char Contact[16];
                        // sprintf(String, "CALL FRM:%s", (DTMF_FindContact(gDTMF_Caller, Contact)) ? Contact : gDTMF_Caller);
                        strcpy(String, "CALL FRM:");
                        strcat(String, (DTMF_FindContact(gDTMF_Caller, Contact)) ? Contact : gDTMF_Caller);
                        UI_PrintString(String, 0, 127, 2, 8);
                    }else if (gDTMF_IsTx) {
                        pPrintStr = (gDTMF_State == DTMF_STATE_TX_SUCC) ? "DTMF TX(SUCC)" : "DTMF TX";
                    }
                }
                else
                {
                    // sprintf(String, ">%s", gDTMF_InputBox);
                    strcpy(String, ">");
                    strcat(String, gDTMF_InputBox);
                    UI_PrintString(String, 0, 127, 0, 8);
                }
#endif

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
                if (isMainOnly())
                {
                    UI_PrintString(pPrintStr, 2, 0, 5, 8);
                    isMainOnlyInputDTMF = true;
                    center_line = CENTER_LINE_IN_USE;
                }
                else
                {
                    UI_PrintString(pPrintStr, 2, 0, 0 + (vfo_num * 3), 8);
                    isMainOnlyInputDTMF = false;
                    center_line = CENTER_LINE_IN_USE;
                    continue;
                }
#else
                UI_PrintMainString(pPrintStr, 2, 0 + (vfo_num * 3));
                center_line = CENTER_LINE_IN_USE;
                continue;
#endif
            }
        }

        // Matoz-style VFO indicator box (19px wide) with Scanning Blink logic
        bool isScVfo = (gScanStateDir != SCAN_OFF && vfo_num == gEeprom.RX_VFO);
        bool blink = (SYSTICK_GetTick() / 80) % 2;
        bool fillBox = isMainVFO;
        
        if (isScVfo && hasParticipants && blink) {
            fillBox = !fillBox;
        }

        if (fillBox) {
            memset(p_line0, 127, 19);
        }

        uint32_t frequency = gEeprom.VfoInfo[vfo_num].pRX->Frequency;

        if(TX_freq_check(frequency) != 0 && gEeprom.VfoInfo[vfo_num].TX_LOCK == true)
        {
            if(isMainOnly())
                memcpy(p_line0 + 14, BITMAP_VFO_Lock, sizeof(BITMAP_VFO_Lock));
            else
                memcpy(p_line0 + 24, BITMAP_VFO_Lock, sizeof(BITMAP_VFO_Lock));
        }

        if (gCurrentFunction == FUNCTION_TRANSMIT)
        {   // transmitting

#ifdef ENABLE_ALARM
            if (gAlarmState == ALARM_STATE_SITE_ALARM)
                mode = VFO_MODE_RX;
            else
#endif
            {
                if (activeTxVFO == vfo_num)
                {   // show the TX symbol
                    mode = VFO_MODE_TX;
                    UI_PrintStringSmallBold("TX", 0, 0, line + 1);
                }
            }
        }
        else
        {   // receiving .. show the RX symbol
            mode = VFO_MODE_RX;
            //if (FUNCTION_IsRx() && gEeprom.RX_VFO == vfo_num) {
            if (FUNCTION_IsRx() && gEeprom.RX_VFO == vfo_num && VfoState[vfo_num] == VFO_STATE_NORMAL) {
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
                RxBlinkLed = 1;
                RxBlinkLedCounter = 0;
                RxLine = line;
                RxOnVfofrequency = frequency;
                if(!isMainVFO)
                {
                    RxBlink = 1;
                }
                else
                {
                    RxBlink = 0;
                }
                // Matoz-style RX indicator at line+1, X=0 (removed old RSSI-based RX display)
                UI_PrintStringSmallBold("RX", 0, 0, line + 1);
#else
                UI_PrintStringSmallBold("RX", 0, 0, line + 1);
#endif
            }
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
            else
            {
                if(RxOnVfofrequency == frequency && !isMainOnly())
                {
                    //UI_PrintStringSmallNormal(">>", 8, 0, line);
                    //memcpy(p_line0 + 14, BITMAP_VFO_Default, sizeof(BITMAP_VFO_Default));
                }

                if(RxBlinkLed == 1)
                    RxBlinkLed = 2;
            }
#endif
        }

        // Matoz-style channel label inside indicator box
        bool filled = fillBox;  // solid box means inverted text
        if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo_num]))
        {   // channel mode - show M001, M002, etc.
            const bool inputting = gInputBoxIndex != 0 && gEeprom.TX_VFO == vfo_num;
            if (!inputting) {
                strcpy(String, "M   ");
                NUMBER_ToDecimal(String + 1, gEeprom.ScreenChannel[vfo_num] + 1, 3, true);
            } else {
                strcpy(String, "M   ");
                strncpy(String + 1, INPUTBOX_GetAscii(), 3);
            }
            UI_PrintStringSmallest(String, 2, line * 8 + 1, false, !filled);
        }
        else if (IS_FREQ_CHANNEL(gEeprom.ScreenChannel[vfo_num]))
        {   // frequency mode - show VFO1, VFO2, etc.
            strcpy(String, "VFO ");
            String[3] = (1 + gEeprom.ScreenChannel[vfo_num] - FREQ_CHANNEL_FIRST) + '0';
            UI_PrintStringSmallest(String, 2, line * 8 + 1, false, !filled);
        }
#ifdef ENABLE_NOAA
        else
        {   // NOAA channel
            if (gInputBoxIndex == 0 || gEeprom.TX_VFO != vfo_num) {
                strcpy(String, "N ");
                String[1] = (1 + gEeprom.ScreenChannel[vfo_num] - NOAA_CHANNEL_FIRST) + '0';
            } else {
                strcpy(String, "N  ");
                String[1] = '0' + gInputBox[0];
                String[2] = '0' + gInputBox[1];
            }
            UI_PrintStringSmallest(String, 4, line * 8 + 1, false, !filled);
        }
#endif

        // ************

        enum VfoState_t state = VfoState[vfo_num];

#ifdef ENABLE_ALARM
        if (gCurrentFunction == FUNCTION_TRANSMIT && gAlarmState == ALARM_STATE_SITE_ALARM) {
            if (activeTxVFO == vfo_num)
                state = VFO_STATE_ALARM;
        }
#endif
        if (state != VFO_STATE_NORMAL)
        {
            if (state < ARRAY_SIZE(VfoStateStr))
                UI_PrintMainString(VfoStateStr[state], 31, line);
        }
        else if (gInputBoxIndex > 0 && IS_FREQ_CHANNEL(gEeprom.ScreenChannel[vfo_num]) && gEeprom.TX_VFO == vfo_num)
        {   // user entering a frequency
            const char * ascii = INPUTBOX_GetAscii();
            bool isGigaF = (frequency >= _1GHz_in_KHz);
            uint8_t mDigits = isGigaF ? 4 : 3;

            char mhz[5];
            char khzBig[4];
            char khzSmall[3];

            // Slice the 10-char ASCII buffer into components
            memcpy(mhz, ascii, mDigits); mhz[mDigits] = 0;
            memcpy(khzBig, ascii + mDigits, 3); khzBig[3] = 0;
            memcpy(khzSmall, ascii + mDigits + 3, 2); khzSmall[2] = 0;

            #ifdef ENABLE_BIG_FREQ
            if(!isGigaF) {
                // Large digits for MHz.kHz
                // sprintf(String, "%s.%s", mhz, khzBig);
                strcpy(String, mhz);
                strcat(String, ".");
                strcat(String, khzBig);
                
                // Dynamic X to keep it snug against small digits at 113
                // We use spaces in the input box to push it right if needed
                uint8_t startX = 32;
                uint8_t len = strlen(String);
                if (len < 7) startX += (7 - len) * 13;
                
                UI_DisplayFrequencyStr(String, startX, line, false);
                // Small digits for Hz (high precision)
                UI_PrintMainStringSmall(khzSmall, 113, line + 1);
            }
            else
            {
                // Non-big freq or GHz range
                // sprintf(String, "%s.%s%s", mhz, khzBig, khzSmall);
                strcpy(String, mhz);
                strcat(String, ".");
                strcat(String, khzBig);
                strcat(String, khzSmall);
                UI_PrintMainString(String, 32, line);
            }
            #endif

            continue;
        }
        else
        {
            if (gCurrentFunction == FUNCTION_TRANSMIT)
            {   // transmitting
                if (activeTxVFO == vfo_num)
                    frequency = gEeprom.VfoInfo[vfo_num].pTX->Frequency;
            }

            if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo_num]))
            {   // it's a channel

                #ifdef ENABLE_RESCUE_OPERATIONS
                    if(gEeprom.MENU_LOCK == false) {
                #endif

                if(gMR_ChannelExclude[gEeprom.ScreenChannel[vfo_num]] == false)
                {
                    // Scanlist text indicators
                    const ChannelAttributes_t att = gMR_ChannelAttributes[gEeprom.ScreenChannel[vfo_num]];
                    
                    if (att.scanlist1) strcat(attrStr, "1");
                    if (att.scanlist2) strcat(attrStr, "2");
                    if (att.scanlist3) strcat(attrStr, "3");
                }
                else
                {
                    strcat(attrStr, "X");
                }

                #ifdef ENABLE_RESCUE_OPERATIONS
                {
                    }
                }
                #endif

                // compander symbol
#ifndef ENABLE_BIG_FREQ
                if (att.compander)
                    memcpy(p_line0 + 120 + LCD_WIDTH, BITMAP_compand, sizeof(BITMAP_compand));
#else
                // TODO:  // find somewhere else to put the symbol
#endif

                switch (gEeprom.CHANNEL_DISPLAY_MODE)
                {
                    case MDF_FREQUENCY: // show the channel frequency
                        UI_PrintFrequencyEx(String, frequency, true);
#ifdef ENABLE_BIG_FREQ
                        if(frequency < _1GHz_in_KHz) {
                            // show the remaining 2 small frequency digits
                            UI_PrintMainStringSmall(String + 7, 113, line + 1);
                            String[7] = 0;

                            // show the main large frequency digits
                            // Dynamic start to keep it snug with small digits at 113
                            uint8_t startX = 32;
                            const char *p = String;
                            while(*p == ' ') p++; // skip leading spaces for alignment
                            uint8_t len = strlen(p);
                            if (len < 7) startX += (7 - len) * 13;

                            UI_DisplayFrequencyStr(String, startX, line, false);
                        }
                        else
#endif
                        {
                            // show the frequency in the main font
                            UI_PrintMainString(String, 32, line);
                        }

                        break;

                    case MDF_CHANNEL:   // show the channel number
                        strcpy(String, "CH-   ");
                        NUMBER_ToDecimal(String + 3, gEeprom.ScreenChannel[vfo_num] + 1, 3, true);
                        UI_PrintMainString(String, 32, line);
                        break;

                    case MDF_NAME:      // show the channel name
                    case MDF_NAME_FREQ: // show the channel name and frequency

                        SETTINGS_FetchChannelName(String, gEeprom.ScreenChannel[vfo_num]);
                        if (String[0] == 0)
                        {   // no channel name, show the channel number instead
                            strcpy(String, "CH-   ");
                            NUMBER_ToDecimal(String + 3, gEeprom.ScreenChannel[vfo_num] + 1, 3, true);
                        }

                        if (gEeprom.CHANNEL_DISPLAY_MODE == MDF_NAME) {
                            UI_PrintMainString(String, 32, line);
                        }
                        else {
                            // MDF_NAME_FREQ
                            // Matoz Style: Name (SmallBold) + Freq (SmallNormal) both at X=39
                            
                            // Name - Small Bold Font
                             UI_PrintStringSmallBold(String, 39, 0, line);
                         
                            // Frequency - Small Normal Font
                            UI_PrintFrequencyEx(String, frequency, true);
                            UI_PrintMainStringSmall(String, 39, line + 1);
                        }
                        break;
                }
            }
            else
            {   // frequency mode
                UI_PrintFrequencyEx(String, frequency, true);

#ifdef ENABLE_BIG_FREQ
                if(frequency < _1GHz_in_KHz) {
                    // show the remaining 2 small frequency digits
                    UI_PrintMainStringSmall(String + 7, 113, line + 1);
                    String[7] = 0;
                    // show the main large frequency digits
                UI_DisplayFrequencyStr(String, 32, line, false);
                }
                else
#endif
                {
                    // show the frequency in the main font
                    UI_PrintMainString(String, 32, line);
                }

                // show the channel symbols
                const ChannelAttributes_t att = gMR_ChannelAttributes[gEeprom.ScreenChannel[vfo_num]];
                if (att.compander)
#ifdef ENABLE_BIG_FREQ
                    memcpy(p_line0 + 120, BITMAP_compand, sizeof(BITMAP_compand));
#else
                    memcpy(p_line0 + 120 + LCD_WIDTH, BITMAP_compand, sizeof(BITMAP_compand));
#endif
            }
        }

        // ************

        {   // show the TX/RX level
            if (mode == VFO_MODE_RX)
            {   // RX signal level
#if !defined(ENABLE_RSSI_BAR) && !defined(ENABLE_MIC_BAR)
                if (gVFO_RSSI_bar_level[vfo_num] > 0) {
                    UI_DrawAntenna(p_line1 + 110, gVFO_RSSI_bar_level[vfo_num]);
                }
#endif
            }
        }

        // ************

        // Consolidated Attribute Line (Enhanced Matoz Style)
        // Order: ScanList, SCR, BCL, CMP, DTMF, R, Offset, BW, Power, DCS/CTCSS
        
        const VFO_Info_t *vfoInfo = &gEeprom.VfoInfo[vfo_num];

        // Attribute labels collection
        const char *labels[12];
        uint8_t nLabels = 0;

        // Scan Indicator
        bool isScanning = (gScanStateDir != SCAN_OFF && vfo_num == gEeprom.RX_VFO);
        static char scanStr[16];
        if (isScanning && !FUNCTION_IsRx()) {
            if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo_num])) {
                uint8_t list = gEeprom.SCAN_LIST_DEFAULT;
                if (list == 0) strcpy(scanStr, "SCAN NONE");
                else if (list >= 1 && list <= 3) {
                    strcpy(scanStr, "SCAN S ");
                    scanStr[6] = '0' + list;
                    scanStr[7] = 0;
                } else if (list == 4) strcpy(scanStr, "SCAN LST");
                else strcpy(scanStr, "SCAN ALL");
            } else {
                strcpy(scanStr, "SCANNING");
            }

            if (hasParticipants) {
                const char *dotArr[] = {"", ".", "..", "..."};
                uint8_t dots = (SYSTICK_GetTick() / 33) % 4;
                strcat(scanStr, dotArr[dots]);
            } else {
                strcat(scanStr, "?");
            }
            
            // Pad to 11 to keep attribute spacing stable
            uint8_t len = strlen(scanStr);
            while(len < 11) scanStr[len++] = ' ';
            scanStr[11] = 0;
            
            labels[nLabels++] = scanStr;
        }

        // 1. Scan List Participation (Supports 3 lists)
        if (!isScanning && IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo_num])) {
            const ChannelAttributes_t att = gMR_ChannelAttributes[gEeprom.ScreenChannel[vfo_num]];
            if (att.scanlist1 && att.scanlist2 && att.scanlist3) labels[nLabels++] = "S123";
            else if (att.scanlist1 && att.scanlist2) labels[nLabels++] = "S12";
            else if (att.scanlist2 && att.scanlist3) labels[nLabels++] = "S23";
            else if (att.scanlist1 && att.scanlist3) labels[nLabels++] = "S13";
            else if (att.scanlist1) labels[nLabels++] = "S1";
            else if (att.scanlist2) labels[nLabels++] = "S2";
            else if (att.scanlist3) labels[nLabels++] = "S3";
        }

        // 2. Scramble
        if (vfoInfo->SCRAMBLING_TYPE > 0 && gSetting_ScrambleEnable) labels[nLabels++] = "SCR";

        // 3. Busy Channel Lockout
        if (vfoInfo->BUSY_CHANNEL_LOCK) labels[nLabels++] = "BCL";

        // 4. Compander
        if (vfoInfo->Compander) labels[nLabels++] = "CMP";

        // 5. VOX
        if (gEeprom.VOX_SWITCH) labels[nLabels++] = "VOX";

        // 6. DTMF
        #ifdef ENABLE_DTMF_CALLING
        if (vfoInfo->DTMF_DECODING_ENABLE || gSetting_KILLED) labels[nLabels++] = "DTM";
        #endif

        // 7. Reverse
        if (vfoInfo->FrequencyReverse) labels[nLabels++] = "R";

        // 8. Offset Direction
#ifdef ENABLE_TX_OFFSET
        const char *dir_list[] = {"", "+", "-"};
        if (vfoInfo->freq_config_RX.Frequency != vfoInfo->freq_config_TX.Frequency) {
            if (vfoInfo->TX_OFFSET_FREQUENCY_DIRECTION < 3 && vfoInfo->TX_OFFSET_FREQUENCY_DIRECTION > 0)
                labels[nLabels++] = dir_list[vfoInfo->TX_OFFSET_FREQUENCY_DIRECTION];
        }
#endif


        // 9. Bandwidth
        const char *bwNames[] = {"25k", "12.5k", "8.33k", "6.25k"};
        if (vfoInfo->CHANNEL_BANDWIDTH < 4) labels[nLabels++] = bwNames[vfoInfo->CHANNEL_BANDWIDTH];

        // 10. Power (Granular 8 levels)
        const char *powerNames[] = {"USER", "LOW1", "LOW2", "LOW3", "LOW4", "LOW5", "MID", "HIGH"};
        uint8_t pwrIndex = vfoInfo->OUTPUT_POWER;
        if (pwrIndex > 7) pwrIndex = 7;
        labels[nLabels++] = powerNames[pwrIndex];

        // 11. DCS/CTCSS
        const char *dcsNames[] = {"", "CT", "DCS", "DCS"};
        const FREQ_Config_t *pConfig = (gCurrentFunction == FUNCTION_TRANSMIT) ? vfoInfo->pTX : vfoInfo->pRX;
        if (pConfig->CodeType != CODE_TYPE_OFF) {
            labels[nLabels++] = dcsNames[pConfig->CodeType];
        } else if (vfoInfo->pTX->CodeType != CODE_TYPE_OFF) {
            labels[nLabels++] = dcsNames[vfoInfo->pTX->CodeType];
        }

#ifdef ENABLE_SIGNAL_CLASSIFIER
        // 12. Signal Classifier Hint
        if (FUNCTION_IsRx() && vfo_num == gEeprom.RX_VFO) {
            SignalClass_t sClass = SIGNAL_CLASSIFIER_GetClass(vfo_num);
            if (sClass == SIGNAL_CLASS_FAST && vfoInfo->Modulation != MODULATION_FM) {
                labels[nLabels++] = "FM?";
            } else if (sClass == SIGNAL_CLASS_NORMAL && vfoInfo->Modulation != MODULATION_AM && vfoInfo->Modulation != MODULATION_USB && vfoInfo->Modulation != 5) {
                labels[nLabels++] = "AM?";
            } else if (sClass == SIGNAL_CLASS_SLOW && vfoInfo->Modulation != MODULATION_USB && vfoInfo->Modulation != 6) {
                labels[nLabels++] = "SSB?";
            }
        }
#endif

        // Rendering with dynamic spacing
        if (nLabels > 0) {
#ifdef ENABLE_CW_KEYER
            if (vfoInfo->Modulation == MODULATION_CW) {
                UI_DisplayCW_VFO(vfo_num, line + 2);
            } else
#endif
            {
                uint16_t charW = 0;
                for (uint8_t i = 0; i < nLabels; i++) {
                    charW += strlen(labels[i]) * 4;
                }

                // Available range: X=6 to X=118 (internal area)
                // Preferred gap: 10px (wide/loose)
                int16_t gap = 10;
                uint16_t totalW;
                
                // Shrink gap if necessary to fit in 112px
                while (gap > 2) {
                    totalW = charW + (gap * (nLabels - 1));
                    if (totalW <= 112) break;
                    gap--;
                }
                totalW = charW + (gap * (nLabels - 1));

                // Target center X=58 (balanced slightly left)
                int16_t x = 58 - (totalW / 2);

                // Bounds check
                if (x < 6) x = 6;
                if (x + totalW > 118) x = 118 - totalW;

                for (uint8_t i = 0; i < nLabels; i++) {
                    UI_PrintStringSmallest(labels[i], (uint8_t)x, (line + 2) * 8 + 1, false, true);
                    x += strlen(labels[i]) * 4 + gap;
                }
            }
        }

        // Modulation (Top Right - Keep separate as per Matoz style)
        const char *modNames[] = {" FM", " AM", "USB", "BYP", "RAW", "DSB", " CW"};
        if (vfoInfo->Modulation < 7) {
             UI_PrintStringSmallest(modNames[vfoInfo->Modulation], 116, 2 + vfo_num * 32, false, true);
        }
    }
#ifdef ENABLE_AGC_SHOW_DATA
    center_line = CENTER_LINE_IN_USE;
    UI_MAIN_PrintAGC(false);
#endif

    if (center_line == CENTER_LINE_NONE)
    {   // we're free to use the middle line
#ifdef ENABLE_CW_KEYER
        if (gTxVfo->Modulation == MODULATION_CW || gRxVfo->Modulation == MODULATION_CW) {
            UI_DisplayCW_Center(3);
            center_line = CENTER_LINE_IN_USE;
        }
#endif

        const bool rx = FUNCTION_IsRx();

#ifdef ENABLE_MIC_BAR
        if (gSetting_mic_bar && gCurrentFunction == FUNCTION_TRANSMIT) {
#ifdef ENABLE_CW_KEYER
            if (gRxVfo->Modulation == MODULATION_CW || gTxVfo->Modulation == MODULATION_CW) {
                // Mic bar hidden during CW TX.
            } else
#endif
            {
                center_line = CENTER_LINE_MIC_BAR;
                UI_DisplayAudioBar();
            }
        }
        else
#endif

#if defined(ENABLE_AM_FIX) && defined(ENABLE_AM_FIX_SHOW_DATA)
        if (rx && gEeprom.VfoInfo[gEeprom.RX_VFO].Modulation == MODULATION_AM && gSetting_AM_fix)
        {
            if (gScreenToDisplay != DISPLAY_MAIN
#ifdef ENABLE_DTMF_CALLING
                || gDTMF_CallState != DTMF_CALL_STATE_NONE
#endif
                )
                return;

            center_line = CENTER_LINE_AM_FIX_DATA;
            AM_fix_print_data(gEeprom.RX_VFO, String);
            UI_PrintStringSmallNormal(String, 2, 0, 3);
        }
        else
#endif

#ifdef ENABLE_RSSI_BAR
        if (rx) {
#ifdef ENABLE_CW_KEYER
            if (gRxVfo->Modulation == MODULATION_CW || gTxVfo->Modulation == MODULATION_CW) {
                center_line = CENTER_LINE_IN_USE;
                UI_DisplayCW_Center(3);
            } else
#endif
            {
                center_line = CENTER_LINE_RSSI;
                UI_DisplayRSSIBar(false);
            }
        }
        else
#endif
        if (rx || gCurrentFunction == FUNCTION_FOREGROUND || gCurrentFunction == FUNCTION_POWER_SAVE)
        {
#ifdef ENABLE_DTMF_CALLING
            #if 1
                if (gSetting_live_DTMF_decoder && gDTMF_RX_live[0] != 0)
                {   // show live DTMF decode
                    const unsigned int len = strlen(gDTMF_RX_live);
                    const unsigned int idx = (len > (17 - 5)) ? len - (17 - 5) : 0;  // limit to last 'n' chars

                    if (gScreenToDisplay != DISPLAY_MAIN
                        || gDTMF_CallState != DTMF_CALL_STATE_NONE
                        )
                        return;

                    center_line = CENTER_LINE_DTMF_DEC;

                    strcpy(String, "DTMF ");
                    strcat(String, gDTMF_RX_live + idx);
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
                    if (isMainOnly())
                    {
                        UI_PrintStringSmallNormal(String, 2, 0, 5);
                    }
                    else
                    {
                        UI_PrintStringSmallNormal(String, 2, 0, 3);
                    }
#else
                    UI_PrintMainStringSmall(String, 2, 3);
#endif
                }
            #else
                if (gSetting_live_DTMF_decoder && gDTMF_RX_index > 0)
                {   // show live DTMF decode
                    const unsigned int len = gDTMF_RX_index;
                    const unsigned int idx = (len > (17 - 5)) ? len - (17 - 5) : 0;  // limit to last 'n' chars

                    if (gScreenToDisplay != DISPLAY_MAIN ||
                        gDTMF_CallState != DTMF_CALL_STATE_NONE)
                        return;

                    center_line = CENTER_LINE_DTMF_DEC;

                    strcpy(String, "DTMF ");
                    strcat(String, gDTMF_RX_live + idx);
                    UI_PrintMainStringSmall(String, 2, 3);
                }
            #endif
#endif

#ifdef ENABLE_SHOW_CHARGE_LEVEL
            else if (gChargingWithTypeC)
            {   // charging .. show the battery state
                if (gScreenToDisplay != DISPLAY_MAIN
#ifdef ENABLE_DTMF_CALLING
                    || gDTMF_CallState != DTMF_CALL_STATE_NONE
#endif
                    )
                    return;

                center_line = CENTER_LINE_CHARGE_DATA;

                strcpy(String, "Charge  .  V    %");
                NUMBER_ToDecimal(String + 7, gBatteryVoltageAverage / 100, 2, false);
                NUMBER_ToDecimal(String + 10, gBatteryVoltageAverage % 100, 2, true);
                NUMBER_ToDecimal(String + 13, BATTERY_VoltsToPercent(gBatteryVoltageAverage), 3, false);
                
                UI_PrintMainStringSmall(String, 2, 3);
            }
#endif
        }
    }
#ifdef ENABLE_CW_KEYER
    else if (center_line == CENTER_LINE_CW)
    {
        UI_DisplayCW_Center(3);
    }
#endif

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    //#ifdef ENABLE_RESCUE_OPERATIONS
    //if(gEeprom.MENU_LOCK == false)
    //{
    //#endif
    if (isMainOnly() && !gDTMF_InputMode)
    {
        strcpy(String, "VFO  ");
        String[4] = activeTxVFO ? 'B' : 'A';
        UI_PrintStringSmallBold(String, 92, 0, 6);
        for (uint8_t i = 92; i < 128; i++)
        {
            gFrameBuffer[6][i] ^= 0x7F;
        }
    }
    //#ifdef ENABLE_RESCUE_OPERATIONS
    //}
    //#endif
#endif

#ifdef ENABLE_LIVESEEK
    LiveSeek_DrawSpectrum();
#endif
    ST7565_BlitFullScreen();
}


// ***************************************************************************
