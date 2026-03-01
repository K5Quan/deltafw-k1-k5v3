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

#include <stdint.h>
#include <string.h>
#include <stdio.h>     // NULL

#ifdef ENABLE_AM_FIX
    #include "features/am_fix/am_fix.h"
#endif

#include "features/audio/audio.h"
#include "core/board.h"
#include "core/misc.h"
#include "features/radio/radio.h"
#include "apps/settings/settings.h"
#ifdef ENABLE_LIVESEEK
    #include "apps/liveseek/liveseek.h"
#endif
#include "core/version.h"

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    #ifdef ENABLE_FMRADIO
        #include "features/action/action.h"
        #include "ui/ui.h"
    #endif
    #ifdef ENABLE_SPECTRUM
        #ifdef ENABLE_SPECTRUM_WATERFALL
            #include "apps/spectrum/spectrum_waterfall.h"
        #else
            #include "apps/spectrum/spectrum.h"
        #endif
    #endif
    #include "apps/scanner/chFrScanner.h"
#endif

#include "features/app/app.h"
#include "features/dtmf/dtmf.h"
#include "features/cw/cw.h"

#ifdef ENABLE_SIGNAL_CLASSIFIER
#include "features/rx/signal_classifier.h"
#endif
#ifdef ENABLE_SQUELCH_TAIL_ELIMINATION
#include "features/rx/squelch_tail.h"
#endif

#include "drivers/bsp/backlight.h"
#include "drivers/bsp/bk4819.h"
#include "drivers/bsp/gpio.h"
#include "drivers/bsp/system.h"
#include "drivers/bsp/systick.h"
#include "drivers/bsp/py25q16.h"
#ifdef ENABLE_UART
    #include "drivers/bsp/uart.h"
#endif
#ifdef ENABLE_USB
#include "drivers/bsp/vcp.h"
#endif
#include "apps/battery/battery.h"
#include "apps/boot/boot.h"

//#include "ui/lock.h"
#include "apps/security/passcode.h"

#include "apps/boot/welcome.h"
#include "ui/menu.h"


void _putchar(__attribute__((unused)) char c)
{

#ifdef ENABLE_UART
    UART_Send((uint8_t *)&c, 1);
#endif

}

void Main(void)
{
    SYSTICK_Init();
    BOARD_Init();

    boot_counter_10ms = 250;   // 2.5 sec

    memset(gDTMF_String, '-', sizeof(gDTMF_String));
    gDTMF_String[sizeof(gDTMF_String) - 1] = 0;

    BK4819_Init();

    BOARD_ADC_GetBatteryInfo(&gBatteryCurrentVoltage);

    SETTINGS_InitEEPROM();
#ifdef ENABLE_PASSCODE
    Passcode_Init();
#endif

#ifdef ENABLE_ANTENNA_SIGNAL_BAR
#ifdef ENABLE_SIGNAL_CLASSIFIER
    SIGNAL_CLASSIFIER_Init();
#endif
#endif



    #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
        gDW = gEeprom.DUAL_WATCH;
        gCB = gEeprom.CROSS_BAND_RX_TX;
    #endif

    SETTINGS_WriteBuildOptions();
    SETTINGS_LoadCalibration();

    RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
    RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);

    RADIO_SelectVfos();

    RADIO_SetupRegisters(true);

    for (unsigned int i = 0; i < ARRAY_SIZE(gBatteryVoltages); i++)
        BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[i]);

    BATTERY_GetReadings(false);

#ifdef ENABLE_AM_FIX
    AM_fix_init();
#endif

#ifdef ENABLE_CW_KEYER
    CW_Init();
#endif

#ifdef ENABLE_SIGNAL_CLASSIFIER
    SIGNAL_CLASSIFIER_Init();
#endif

#ifdef ENABLE_SQUELCH_TAIL_ELIMINATION
    SQUELCH_TAIL_Init();
#endif

    BOOT_Mode_t  BootMode = BOOT_GetMode();

#ifdef ENABLE_RESCUE_OPERATIONS
    if (BootMode == BOOT_MODE_RESCUE_OPS)
    {
        gEeprom.MENU_LOCK = !gEeprom.MENU_LOCK;
        SETTINGS_SaveSettings();
    }

    /*
    if(gEeprom.MENU_LOCK == true) // Force Main Only
    {
        gEeprom.DUAL_WATCH = 0;
        gEeprom.CROSS_BAND_RX_TX = 0;
        //gFlagReconfigureVfos = true;
        //gUpdateStatus        = true;
    }
    */
#endif

#ifdef ENABLE_RESCUE_OPERATIONS
    if (BootMode == BOOT_MODE_F_LOCK && gEeprom.MENU_LOCK == true)
    {
        BootMode = BOOT_MODE_NORMAL;
    }
#endif

    if (BootMode == BOOT_MODE_F_LOCK)
    {

        gF_LOCK = true;            // flag to say include the hidden menu items
        #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
            gEeprom.KEY_LOCK = 0;
            SETTINGS_SaveSettings();

            gMenuCursor = 0;
            while (MenuList[gMenuCursor].name[0] != '\0') {
                if (MenuList[gMenuCursor].menu_id == FIRST_HIDDEN_MENU_ITEM)
                    break;
                gMenuCursor++;
            }

            gSubMenuSelection = gSetting_F_LOCK;
        #endif
    }

    // count the number of menu items
    gMenuListCount = 0;
    while (MenuList[gMenuListCount].name[0] != '\0') {
        if(!gF_LOCK && MenuList[gMenuListCount].menu_id == FIRST_HIDDEN_MENU_ITEM)
            break;

        gMenuListCount++;
    }

    // wait for user to release all butts before moving on
    if (GPIO_IsPttPressed() ||
         KEYBOARD_Poll() != KEY_INVALID ||
         BootMode != BOOT_MODE_NORMAL)
    {   // keys are pressed
        UI_DisplayReleaseKeys();
        BACKLIGHT_TurnOn();

        // 500ms
        for (int i = 0; i < 50;)
        {
            i = (!GPIO_IsPttPressed() && KEYBOARD_Poll() == KEY_INVALID) ? i + 1 : 0;
            SYSTEM_DelayMs(10);
        }
        gKeyReading0 = KEY_INVALID;
        gKeyReading1 = KEY_INVALID;
        gDebounceCounter = 0;
    }

    if (!gIsCharging && gBatteryDisplayLevel == 0)
    {
        FUNCTION_Select(FUNCTION_POWER_SAVE);

        if (gEeprom.BACKLIGHT_TIME < 61) // backlight is not set to be always on
            BACKLIGHT_TurnOff();    // turn the backlight OFF
        else
            BACKLIGHT_TurnOn();     // turn the backlight ON

        gReducedService = true;
    }
    else
    {
        BACKLIGHT_TurnOn();
#ifdef ENABLE_PASSCODE
        Passcode_Prompt();
#endif
        boot_counter_10ms = 250;

#ifdef ENABLE_UART
    UART_Init();
    UART_Send(UART_Version, strlen(UART_Version));
#endif
#ifdef ENABLE_USB
    VCP_Init();
#endif

        UI_DisplayWelcome();

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
        if (gEeprom.POWER_ON_DISPLAY_MODE != POWER_ON_DISPLAY_MODE_NONE && gEeprom.POWER_ON_DISPLAY_MODE != POWER_ON_DISPLAY_MODE_SOUND)
#else
        if (gEeprom.POWER_ON_DISPLAY_MODE != POWER_ON_DISPLAY_MODE_NONE)
#endif
        {   // 2.55 second boot-up screen
            while (boot_counter_10ms > 0)
            {
                if (KEYBOARD_Poll() != KEY_INVALID)
                {   // halt boot beeps
                    boot_counter_10ms = 0;
                    break;
                }
            }
        }
        
        RADIO_SetupRegisters(true);

        BOOT_ProcessMode(BootMode);

        // GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_VOICE_0);

#ifdef ENABLE_LIVESEEK
        LiveSeek_TimeSlice();
#endif
        gUpdateStatus = true;

#ifdef ENABLE_VOICE
        {
            uint8_t Channel;

            AUDIO_SetVoiceID(0, VOICE_ID_WELCOME);

            Channel = gEeprom.ScreenChannel[gEeprom.TX_VFO];
            if (IS_MR_CHANNEL(Channel))
            {
                AUDIO_SetVoiceID(1, VOICE_ID_CHANNEL_MODE);
                AUDIO_SetDigitVoice(2, Channel + 1);
            }
            else if (IS_FREQ_CHANNEL(Channel))
                AUDIO_SetVoiceID(1, VOICE_ID_FREQUENCY_MODE);

            AUDIO_PlaySingleVoice(0);
        }
#endif

#ifdef ENABLE_NOAA
        RADIO_ConfigureNOAA();
#endif
    }

    /*
    #ifdef ENABLE_BOOT_RESUME_STATE
    if(gEeprom.CURRENT_STATE == 2 || gEeprom.CURRENT_STATE == 5)
    {
            gScanRangeStart = gScanRangeStart ? 0 : gTxVfo->pRX->Frequency;
            gScanRangeStop = gEeprom.VfoInfo[!gEeprom.TX_VFO].freq_config_RX.Frequency;
            if(gScanRangeStart > gScanRangeStop)
            {
                SWAP(gScanRangeStart, gScanRangeStop);
            }
    }
    switch (gEeprom.CURRENT_STATE) {
        case 1:
            gEeprom.SCAN_LIST_DEFAULT = gEeprom.CURRENT_LIST;
            CHFRSCANNER_Start(true, SCAN_FWD);
            break;

        case 2:
            CHFRSCANNER_Start(true, SCAN_FWD);
            break;

        #ifdef ENABLE_FMRADIO
        case 3:
            ACTION_FM();
            GUI_SelectNextDisplay(gRequestDisplayScreen);
            break;
        #endif

        #ifdef ENABLE_SPECTRUM
        case 4:
            APP_RunSpectrum();
            break;
        case 5:
            APP_RunSpectrum();
            break;
        #endif

        default:
            // No action for CURRENT_STATE == 0 or other unexpected values
            break;
    }
    #endif
    */

    #ifdef ENABLE_BOOT_RESUME_STATE
        if (gEeprom.CURRENT_STATE == 2 || gEeprom.CURRENT_STATE == 5) {
            gScanRangeStart = gScanRangeStart ? 0 : gTxVfo->pRX->Frequency;
            gScanRangeStop = gEeprom.VfoInfo[!gEeprom.TX_VFO].freq_config_RX.Frequency;
            if (gScanRangeStart > gScanRangeStop) {
                SWAP(gScanRangeStart, gScanRangeStop);
            }
        }

        if (gEeprom.CURRENT_STATE == 1) {
            gEeprom.SCAN_LIST_DEFAULT = gEeprom.CURRENT_LIST;
        }

        if (gEeprom.CURRENT_STATE == 1 || gEeprom.CURRENT_STATE == 2) {
            CHFRSCANNER_Start(true, SCAN_FWD);
        }
        #ifdef ENABLE_FMRADIO
        else if (gEeprom.CURRENT_STATE == 3) {
            ACTION_FM();
            GUI_SelectNextDisplay(gRequestDisplayScreen);
        }
        #endif
        #ifdef ENABLE_SPECTRUM
        else if (gEeprom.CURRENT_STATE == 4 || gEeprom.CURRENT_STATE == 5) {
            APP_RunSpectrum();
        }
        #endif
    #endif
        
    while (true) {
        APP_Update();

        if (gNextTimeslice) {

            APP_TimeSlice10ms();

            if (gNextTimeslice_500ms) {
                APP_TimeSlice500ms();
            }
        }
    }
}
