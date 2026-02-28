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
#include <stdlib.h>

#include "features/dtmf/dtmf.h"
#include "ui/menu.h"
#include "features/menu/menu.h"
#include "ui/bitmaps.h"
#include "core/board.h"
#include "features/dcs/dcs.h"
#include "../drivers/bsp/backlight.h"
#include "../drivers/bsp/bk4819.h"
#include "../drivers/bsp/eeprom.h"
#include "../drivers/bsp/st7565.h"
#include "features/radio/frequencies.h"
#include "../apps/battery/battery.h"
#include "core/misc.h"
#include "apps/settings/settings.h"

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    #include "core/version.h"
#endif

#include "helper.h"
#include "inputbox.h"
#include "ag_menu.h"
#include "ui.h"
#include "drivers/bsp/adc.h"
#include "helper/crypto.h"


const t_menu_item MenuList[] =
{
//   text,          menu ID
    {"Step",        MENU_STEP          },
    {"Power",       MENU_TXP           }, // was "TXP"
    {"RxDCS",       MENU_R_DCS         }, // was "R_DCS"
    {"RxCTCS",      MENU_R_CTCS        }, // was "R_CTCS"
    {"TxDCS",       MENU_T_DCS         }, // was "T_DCS"
    {"TxCTCS",      MENU_T_CTCS        }, // was "T_CTCS"
    {"Mode",        MENU_AM            }, // was "AM"
#ifdef ENABLE_TX_OFFSET
    {"TxODir",      MENU_SFT_D         }, // was "SFT_D"
    {"TxOffs",      MENU_OFFSET        }, // was "OFFSET"
#endif
    {"W/N",         MENU_W_N           },
#ifdef ENABLE_SCRAMBLER
    {"Scramb",      MENU_SCR           }, // was "SCR"
#endif
    {"BusyCL",      MENU_BCL           }, // was "BCL"
    {"Compnd",      MENU_COMPAND       },

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"TXLock",      MENU_TX_LOCK       }, 
#endif
#ifdef ENABLE_LIVESEEK
    {"LiveSk",      MENU_LIVESEEK      },
#endif
#ifdef ENABLE_SCAN_LIST_EDITING
    {"ScAdd1",      MENU_S_ADD1        },
    {"ScAdd2",      MENU_S_ADD2        },
    {"ScAdd3",      MENU_S_ADD3        },
#endif
#ifdef ENABLE_ON_DEVICE_PROGRAMMING
    {"ChSave",      MENU_MEM_CH        }, // was "MEM-CH"
    {"ChDele",      MENU_DEL_CH        }, // was "DEL-CH"
    {"ChName",      MENU_MEM_NAME      },
#endif
#ifdef ENABLE_SCAN_LIST_EDITING
    {"SList",       MENU_S_LIST        },
    {"SList1",      MENU_SLIST1        },
    {"SList2",      MENU_SLIST2        },
    {"SList3",      MENU_SLIST3        },
#endif
    {"ScnRev",      MENU_SC_REV        },
#ifndef ENABLE_CUSTOM_FIRMWARE_MODS
    #ifdef ENABLE_NOAA
        {"NOAA-S",      MENU_NOAA_S    },
    #endif
#endif


    {"KeyLck",      MENU_AUTOLK        }, // was "AUTOLk"
    {"TxTOut",      MENU_TOT           }, // was "TOT"
    {"BatSav",      MENU_SAVE          }, // was "SAVE"
    {"BatTxt",      MENU_BAT_TXT       },
    {"Mic",         MENU_MIC           },
    {"MicBar",      MENU_MIC_BAR       },
    {"MicAGC",      MENU_MIC_AGC       },
    {"VolGai",      MENU_VOL_GAIN      },
#ifdef ENABLE_TX_SOFT_START
    {"TxSftS",      MENU_TX_SOFT_START   },
#endif
#ifdef ENABLE_TX_AUDIO_COMPRESSOR
    {"TxComp",      MENU_TX_COMPRESSOR   },
#endif
#ifdef ENABLE_CTCSS_LEAD_IN
    {"CTLead",      MENU_CTCSS_LEAD      },
#endif
    {"ChDisp",      MENU_MDF           }, // was "MDF"
    {"POnMsg",      MENU_PONMSG        },
    {"BLTime",      MENU_ABR           }, // was "ABR"
    {"BLMin",       MENU_ABR_MIN       },
    {"BLMax",       MENU_ABR_MAX       },
    {"BLTxRx",      MENU_ABR_ON_TX_RX  },
    {"Beep",        MENU_BEEP          },
#ifdef ENABLE_VOICE
    {"Voice",       MENU_VOICE         },
#endif
    {"Roger",       MENU_ROGER         },
    {"F1Shrt",      MENU_F1SHRT        },
    {"F1Long",      MENU_F1LONG        },
    {"F2Shrt",      MENU_F2SHRT        },
    {"F2Long",      MENU_F2LONG        },
    {"M Long",      MENU_MLONG         },
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"SetNav",      MENU_SET_NAV       },
#endif
    {"STE",         MENU_STE           },
    {"RP STE",      MENU_RP_STE        },
    {"1 Call",      MENU_1_CALL        },
#ifdef ENABLE_ALARM
    {"AlarmT",      MENU_AL_MOD        },
#endif
#ifdef ENABLE_DTMF_CALLING
    {"ANI ID",      MENU_ANI_ID        },
#endif
#ifdef ENABLE_DTMF_CALLING
    {"UPCode",      MENU_UPCODE        },
    {"DWCode",      MENU_DWCODE        },
    {"PTT ID",      MENU_PTT_ID        },
    {"D ST",        MENU_D_ST          },
#endif
#ifdef ENABLE_DTMF_CALLING
    {"D Resp",      MENU_D_RSP         },
    {"D Hold",      MENU_D_HOLD        },
#endif
#ifdef ENABLE_DTMF_CALLING
    {"D Prel",      MENU_D_PRE         },
#endif
#ifdef ENABLE_DTMF_CALLING
    {"D Decd",      MENU_D_DCD         },
    {"D List",      MENU_D_LIST        },
#endif
#ifdef ENABLE_DTMF_CALLING
    {"D Live",      MENU_D_LIVE_DEC    }, // live DTMF decoder
#endif
#ifndef ENABLE_CUSTOM_FIRMWARE_MODS
    #ifdef ENABLE_AM_FIX
        {"AM Fix",      MENU_AM_FIX        },
    #endif
#endif
    {"VOX",         MENU_VOX           },
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"SysInf",      MENU_VOL           }, // was "VOL"
#else
    {"BatVol",      MENU_VOL           }, // was "VOL"
#endif
    {"RxMode",      MENU_TDR           },
    {"Sql",         MENU_SQL           },
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"SetPwr",      MENU_SET_PWR       },
    {"SetPTT",      MENU_SET_PTT       },
    {"SetTOT",      MENU_SET_TOT       },
    {"SetEOT",      MENU_SET_EOT       },
    {"SetCtr",      MENU_SET_CTR       },
    {"SetInv",      MENU_SET_INV       },
    {"SetLck",      MENU_SET_LCK       },
    {"SetTmr",      MENU_SET_TMR       },
#ifdef ENABLE_DEEP_SLEEP_MODE
    {"SetOff",       MENU_SET_OFF      },
#endif
#ifdef ENABLE_NARROWER_BW_FILTER
    {"SetNFM",      MENU_SET_NFM       },
#endif
#ifdef ENABLE_SYSTEM_INFO_MENU
    {"SetVol",      MENU_SET_VOL       },
#endif
#ifdef ENABLE_RESCUE_OPERATIONS
    {"SetKey",      MENU_SET_KEY       },
#endif
#ifdef ENABLE_NOAA
    {"SetNWR",      MENU_NOAA_S    },
#endif
#endif
    // hidden menu items from here on
    // enabled if pressing both the PTT and upper side button at power-on
    {"F Lock",      MENU_F_LOCK        },
#ifndef ENABLE_CUSTOM_FIRMWARE_MODS
    {"Tx 200",      MENU_200TX         }, // was "200TX"
    {"Tx 350",      MENU_350TX         }, // was "350TX"
    {"Tx 500",      MENU_500TX         }, // was "500TX"
#endif
    {"350 En",      MENU_350EN         }, // was "350EN"
#ifdef ENABLE_SCRAMBLER
    {"ScraEn",      MENU_SCREN         }, // was "SCREN"
#endif
#ifdef ENABLE_F_CAL_MENU
    {"FrCali",      MENU_F_CALI        }, // reference xtal calibration
#endif
    {"BatCal",      MENU_BATCAL        }, // battery voltage calibration
    {"BatTyp",      MENU_BATTYP        }, // battery type 1600/2200mAh
    {"Reset",       MENU_RESET         }, // might be better to move this to the hidden menu items ?

    {"",                              0xff               }  // end of list - DO NOT delete or move this this
};

const uint8_t FIRST_HIDDEN_MENU_ITEM = MENU_F_LOCK;

const char gSubMenu_TXP[][6] =
{
    "USER",
    "LOW 1",
    "LOW 2",
    "LOW 3",
    "LOW 4",
    "LOW 5",
    "MID",
    "HIGH"
};

#ifdef ENABLE_TX_OFFSET
const char gSubMenu_SFT_D[][4] =
{
    "OFF",
    "+",
    "-"
};
#endif

const char gSubMenu_W_N[][7] =
{
    "WIDE",
    "NARROW"
};

const char gSubMenu_LiveSeek[][9] =
{
    "OFF",
    "MONITOR",
    "SPECTRUM"
};

const char gSubMenu_OFF_ON[][4] =
{
    "OFF",
    "ON"
};

const char gSubMenu_NA[4] =
{
    "N/A"
};

const char* const gSubMenu_RXMode[] =
{
    "MAIN ONLY",       // TX and RX on main only
    "DUAL RX RESPOND", // Watch both and respond
    "CROSS BAND",      // TX on main, RX on secondary
    "MAIN TX DUAL RX"  // always TX on main, but RX on both
};

#ifdef ENABLE_VOICE
    const char gSubMenu_VOICE[][4] =
    {
        "OFF",
        "CHI",
        "ENG"
    };
#endif

const char* const gSubMenu_MDF[] =
{
    "FREQ",
    "CH NUM",
    "NAME",
    "NAME+FREQ"
};

#ifdef ENABLE_ALARM
    const char gSubMenu_AL_MOD[][5] =
    {
        "SITE",
        "TONE"
    };
#endif

#ifdef ENABLE_DTMF_CALLING
const char gSubMenu_D_RSP[][11] =
{
    "DO NOTHING",
    "RING",
    "REPLY",
    "BOTH"
};
#endif

const char* const gSubMenu_PTT_ID[] =
{
    "OFF",
    "UP CODE",
    "DOWN CODE",
    "UP+DOWN CODE",
    "APOLLO QUINDAR"
};

const char gSubMenu_PONMSG[][8] =
{
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    "ALL",
    "SOUND",
#else
    "FULL",
#endif
    "MESSAGE",
    "VOLTAGE",
    "NONE"
};

const char* const gSubMenu_ROGER[ROGER_SUBMENU_SIZE] =
{
    "OFF",
    "ROGER",
    "MDC1200",
#ifdef ENABLE_EXTRA_ROGER
    "MOTOTRBO",
    "APX6000 TPT",
    "T40",
    "TLKR T80",
    "COBRA AM845",
    "POLICE",
    "UV-5RC",
#endif
#ifdef ENABLE_CUSTOM_ROGER
    "CUSTOM 1",
    "CUSTOM 2",
    "CUSTOM 3",
#endif
};

const char gSubMenu_RESET[][4] =
{
    "VFO",
    "ALL"
};

const char * const gSubMenu_F_LOCK[] =
{
    "DEFAULT+ 137-174 400-470",
    "FCC HAM 144-148 420-450",
#ifdef ENABLE_FREQUENCY_LOCK_REGION_CA
    "CA HAM 144-148 430-450",
#endif
    "CE HAM 144-146 430-440",
    "GB HAM 144-148 430-440",
    "137-174 400-430",
    "137-174 400-438",
#ifdef ENABLE_PMR446_FREQUENCY_BAND
    "PMR 446",
#endif
#ifdef ENABLE_GMRS_FRS_MURS_BANDS
    "GMRS FRS MURS",
#endif
    "DISABLE ALL",
    "UNLOCK ALL",
};

const char gSubMenu_RX_TX[][6] =
{
    "OFF",
    "TX",
    "RX",
    "TX/RX"
};

const char gSubMenu_BAT_TXT[][8] =
{
    "NONE",
    "VOLTAGE",
    "PERCENT",
    "ICON",
    "I+VOLT",
    "I+PERC",
    "VOLT",
    "PERC"
};

const char gSubMenu_BATTYP[][12] =
{
    "1600mAh K5",
    "2200mAh K5",
    "3500mAh K5",
    "1400mAh K1",
    "2500mAh K1"
};

#ifdef ENABLE_SCRAMBLER
const char gSubMenu_SCRAMBLER[][7] =
{
    "OFF",
    "2600Hz",
    "2700Hz",
    "2800Hz",
    "2900Hz",
    "3000Hz",
    "3100Hz",
    "3200Hz",
    "3300Hz",
    "3400Hz",
    "3500Hz"
};
#endif

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    const char gSubMenu_SET_PWR[][6] =
    {
        "< 20m",
        "125m",
        "250m",
        "500m",
        "1",
        "2",
        "5"
    };

    const char gSubMenu_SET_PTT[][7] =
    {
        "HOLD",
        "LATCH",
        "BOTH"
    };

    const char gSubMenu_SET_TOT[][7] =  // Use by SET_EOT too
    {
        "OFF",
        "SOUND",
        "VISUAL",
        "ALL"
    };

    const char gSubMenu_SET_LCK[][9] =
    {
        "KEYS",
        "KEYS+PTT"
    };


    #ifdef ENABLE_NARROWER_BW_FILTER
        const char gSubMenu_SET_NFM[][9] =
        {
            "NARROW",
            "NARROWER"
        };
    #endif

    #ifdef ENABLE_RESCUE_OPERATIONS
        const char gSubMenu_SET_KEY[][9] =
        {
            "KEY_MENU",
            "KEY_UP",
            "KEY_DOWN",
            "KEY_EXIT",
            "KEY_STAR"
        };
    #endif

    const char gSubMenu_SET_AUD[][6] =
    {
        "FLAT",
        "CLEAN",
        "MID",
        "BOOST",
        "MAX"
    };
#endif

const t_sidefunction gSubMenu_SIDEFUNCTIONS[] =
{
    {"NONE",            ACTION_OPT_NONE},
#ifdef ENABLE_FLASHLIGHT
    {"FLASHLIGHT",    ACTION_OPT_FLASHLIGHT},
#endif
    {"POWER",           ACTION_OPT_POWER},
    {"MONITOR",         ACTION_OPT_MONITOR},
    {"SCAN",            ACTION_OPT_SCAN},
#ifdef ENABLE_VOX
    {"VOX",             ACTION_OPT_VOX},
#endif
#ifdef ENABLE_ALARM
    {"ALARM",           ACTION_OPT_ALARM},
#endif
#ifdef ENABLE_FMRADIO
    {"FM RADIO",        ACTION_OPT_FM},
#endif
#ifdef ENABLE_TX1750
    {"1750Hz",          ACTION_OPT_1750},
#endif
#ifdef ENABLE_REGA
    {"REGA ALARM",     ACTION_OPT_REGA_ALARM},
    {"REGA TEST",      ACTION_OPT_REGA_TEST},
#endif
    {"LOCK KEYPAD",    ACTION_OPT_KEYLOCK},
    {"VFO A VFO B",    ACTION_OPT_A_B},
    {"VFO MEM",        ACTION_OPT_VFO_MR},
    {"MODE",            ACTION_OPT_SWITCH_DEMODUL},
#ifdef ENABLE_BLMIN_TMP_OFF
    {"BLMIN TMP OFF",  ACTION_OPT_BLMIN_TMP_OFF},      //BackLight Minimum Temporay OFF
#endif
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"RX MODE",         ACTION_OPT_RXMODE},
    {"TX ON MAIN",       ACTION_OPT_MAINONLY},
    {"PTT",             ACTION_OPT_PTT},
    {"PTT A",           ACTION_OPT_PTT_A},
    {"PTT B",           ACTION_OPT_PTT_B},
    {"WIDE NARROW",    ACTION_OPT_WN},
    //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
    {"MUTE",            ACTION_OPT_MUTE},
    //#endif
    #ifdef ENABLE_RESCUE_OPERATIONS
        {"POWER HIGH",    ACTION_OPT_POWER_HIGH},
        {"REMOVE OFFSET",  ACTION_OPT_REMOVE_OFFSET},
    #endif
#endif
};

const uint8_t gSubMenu_SIDEFUNCTIONS_size = ARRAY_SIZE(gSubMenu_SIDEFUNCTIONS);

bool    gIsInSubMenu;
uint8_t gMenuCursor;
int UI_MENU_GetCurrentMenuId() {
    if(gMenuCursor < ARRAY_SIZE(MenuList))
        return MenuList[gMenuCursor].menu_id;

    return MenuList[ARRAY_SIZE(MenuList)-1].menu_id;
}

uint8_t UI_MENU_GetMenuIdx(uint8_t id)
{
    for(uint8_t i = 0; i < ARRAY_SIZE(MenuList); i++)
        if(MenuList[i].menu_id == id)
            return i;
    return 0;
}

int32_t gSubMenuSelection;

// edit box
char    edit_original[17]; // a copy of the text before editing so that we can easily test for changes/difference
char    edit[17];
int     edit_index;

void UI_DisplayMenu(void)
{
    const unsigned int menu_list_width = 6; // max no. of characters on the menu list (left side)
    const unsigned int menu_item_x1    = (8 * menu_list_width) + 2;
    const unsigned int menu_item_x2    = LCD_WIDTH - 1;
    unsigned int       i;
    char               String[64];  // bigger cuz we can now do multi-line in one string (use '\n' char)

#ifdef ENABLE_DTMF_CALLING
    char               Contact[16];
#endif

    UI_DisplayClear();

    if (AG_MENU_IsActive()) {
        AG_MENU_Render();
        ST7565_BlitFullScreen();
        return;
    }

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    UI_DrawLineBuffer(gFrameBuffer, 48, 0, 48, 55, 1); // Be ware, status zone = 8 lines, the rest = 56 ->total 64
    //UI_DrawLineDottedBuffer(gFrameBuffer, 0, 46, 50, 46, 1);

    for (uint8_t i = 0; i < 48; i += 2)
    {
        gFrameBuffer[5][i] = 0x40;
    }
#endif

#ifndef ENABLE_CUSTOM_MENU_LAYOUT
        // original menu layout
    for (i = 0; i < 3; i++)
        if (gMenuCursor > 0 || i > 0)
            if ((gMenuListCount - 1) != gMenuCursor || i != 2)
                UI_PrintString(MenuList[gMenuCursor + i - 1].name, 0, 0, i * 2, 8);

    // invert the current menu list item pixels
    for (i = 0; i < (8 * menu_list_width); i++)
    {
        gFrameBuffer[2][i] ^= 0xFF;
        gFrameBuffer[3][i] ^= 0xFF;
    }

    // draw vertical separating dotted line
    for (i = 0; i < 7; i++)
        gFrameBuffer[i][(8 * menu_list_width) + 1] = 0xAA;

    // draw the little sub-menu triangle marker
    if (gIsInSubMenu)
        memcpy(gFrameBuffer[0] + (8 * menu_list_width) + 1, BITMAP_CurrentIndicator, sizeof(BITMAP_CurrentIndicator));

    // draw the menu index number/count
    // sprintf(String, "%2u.%u", 1 + gMenuCursor, gMenuListCount);
    NUMBER_ToDecimal(String, 1 + gMenuCursor, 2, false);
    String[2] = '.';
    NUMBER_ToDecimal(String + 3, gMenuListCount, 3, false);

    UI_PrintStringSmallNormal(String, 2, 0, 6);

#else
    {   // new menu layout .. experimental & unfinished
        const int menu_index = gMenuCursor;  // current selected menu item
        i = 1;

        if (!gIsInSubMenu) {
            while (i < 2)
            {   // leading menu items - small text
                const int k = menu_index + i - 2;
                if (k < 0)
                    UI_PrintStringSmallNormal(MenuList[gMenuListCount + k].name, 0, 0, i);  // wrap-a-round
                else if (k >= 0 && k < (int)gMenuListCount)
                    UI_PrintStringSmallNormal(MenuList[k].name, 0, 0, i);
                i++;
            }

            // current menu item - keep big n fat
            if (menu_index >= 0 && menu_index < (int)gMenuListCount)
                UI_PrintString(MenuList[menu_index].name, 0, 0, 2, 8);
            i++;

            while (i < 4)
            {   // trailing menu item - small text
                const int k = menu_index + i - 2;
                if (k >= 0 && k < (int)gMenuListCount)
                    UI_PrintStringSmallNormal(MenuList[k].name, 0, 0, 1 + i);
                else if (k >= (int)gMenuListCount)
                    UI_PrintStringSmallNormal(MenuList[gMenuListCount - k].name, 0, 0, 1 + i);  // wrap-a-round
                i++;
            }

            // draw the menu index number/count
#ifndef ENABLE_CUSTOM_FIRMWARE_MODS
            // sprintf(String, "%2u.%u", 1 + gMenuCursor, gMenuListCount);
            NUMBER_ToDecimal(String, 1 + gMenuCursor, 2, false);
            String[2] = '.';
            NUMBER_ToDecimal(String + 3, gMenuListCount, 3, false);
            UI_PrintStringSmallNormal(String, 2, 0, 6);
#endif
        }
        else if (menu_index >= 0 && menu_index < (int)gMenuListCount)
        {   // current menu item
//          strcat(String, ":");
            UI_PrintString(MenuList[menu_index].name, 0, 0, 0, 8);
//          UI_PrintStringSmallNormal(String, 0, 0, 0);
        }

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
        // sprintf(String, "%02u/%u", 1 + gMenuCursor, gMenuListCount);
        NUMBER_ToDecimal(String, 1 + gMenuCursor, 2, true);
        String[2] = '/';
        NUMBER_ToDecimal(String + 3, gMenuListCount, 3, false);
        UI_PrintStringSmallNormal(String, 6, 0, 6);
#endif
    }
#endif

    // **************

    memset(String, 0, sizeof(String));

    bool already_printed = false;

    /* Brightness is set to max in some entries of this menu. Return it to the configured brightness
       level the "next" time we enter here.I.e., when we move from one menu to another.
       It also has to be set back to max when pressing the Exit key. */

    BACKLIGHT_TurnOn();

    //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
        uint8_t gaugeLine = 0;
        uint8_t gaugeMin = 0;
        uint8_t gaugeMax = 0;
    //#endif

    switch (UI_MENU_GetCurrentMenuId())
    {
        case MENU_SQL:
             // Moved to grouped block
            break;

        case MENU_LIVESEEK:
            strcpy(String, gSubMenu_LiveSeek[gSubMenuSelection]);
            break;

        case MENU_MIC:
            {   // display the mic gain in actual dB rather than just an index number
                const uint8_t mic = gMicGain_dB2[gSubMenuSelection];
                // sprintf(String, "+%u.%01udB", mic / 2, mic % 2);
                strcpy(String, "+  . dB");
                NUMBER_ToDecimal(String + 1, mic / 2, 2, false);
                String[4] = (mic % 2) + '0';
            }
            break;



        case MENU_VOL_GAIN:
            NUMBER_ToDecimal(String, gSubMenuSelection, 2, true);
            gaugeLine = 6;
            gaugeMin = 0;
            gaugeMax = 63;
            break;



        case MENU_STEP: {
            uint16_t step = gStepFrequencyTable[FREQUENCY_GetStepIdxFromSortedIdx(gSubMenuSelection)];
            // sprintf(String, "%d.%02ukHz", step / 100, step % 100);
            NUMBER_ToDecimal(String, step / 100, 2, false);
            strcat(String, ".");
            NUMBER_ToDecimal(String + strlen(String), step % 100, 2, true);
            strcat(String, "kHz");
            break;
        }

        case MENU_TXP:
            if(gSubMenuSelection == 0)
            {
                strcpy(String, gSubMenu_TXP[gSubMenuSelection]);
            }
            else
            {
                // sprintf(String, "%s %sW", gSubMenu_TXP[gSubMenuSelection], gSubMenu_SET_PWR[gSubMenuSelection - 1]);
                strcpy(String, gSubMenu_TXP[gSubMenuSelection]);
                strcat(String, " ");
                strcat(String, gSubMenu_SET_PWR[gSubMenuSelection - 1]);
                strcat(String, "W");
            }
            break;

        case MENU_R_DCS:
        case MENU_T_DCS:
            if (gSubMenuSelection == 0)
                strcpy(String, gSubMenu_OFF_ON[0]);
            else if (gSubMenuSelection < 105) {
                strcpy(String, "D   N");
                uint16_t val = DCS_Options[gSubMenuSelection - 1];
                String[1] = ((val >> 6) & 7) + '0';
                String[2] = ((val >> 3) & 7) + '0';
                String[3] = (val & 7) + '0';
            } else {
                strcpy(String, "D   I");
                uint16_t val = DCS_Options[gSubMenuSelection - 105];
                String[1] = ((val >> 6) & 7) + '0';
                String[2] = ((val >> 3) & 7) + '0';
                String[3] = (val & 7) + '0';
            }
            break;

        case MENU_R_CTCS:
        case MENU_T_CTCS:
        {
            if (gSubMenuSelection == 0)
                strcpy(String, gSubMenu_OFF_ON[0]);
            else {
                // sprintf(String, "%u.%uHz", CTCSS_Options[gSubMenuSelection - 1] / 10, CTCSS_Options[gSubMenuSelection - 1] % 10);
                NUMBER_ToDecimal(String, CTCSS_Options[gSubMenuSelection - 1] / 10, 3, false);
                strcat(String, ".");
                uint8_t len = strlen(String);
                String[len] = (CTCSS_Options[gSubMenuSelection - 1] % 10) + '0';
                String[len+1] = '\0';
                strcat(String, "Hz");
            }
            break;
        }

        case MENU_SFT_D:
            strcpy(String, gSubMenu_SFT_D[gSubMenuSelection]);
            break;

        case MENU_OFFSET:
            if (!gIsInSubMenu || gInputBoxIndex == 0)
            {
                UI_PrintFrequencyEx(String, gSubMenuSelection, true);
                UI_PrintString(String, menu_item_x1, menu_item_x2, 1, 8);
            }
            else
            {
                const char * ascii = INPUTBOX_GetAscii();
                // sprintf(String, "%.3s.%.3s  ",ascii, ascii + 3);
                strncpy(String, ascii, 3);
                String[3] = '.';
                strncpy(String + 4, ascii + 3, 3);
                String[7] = '\0';
                UI_PrintString(String, menu_item_x1, menu_item_x2, 1, 8);
            }

            UI_PrintString("MHz",  menu_item_x1, menu_item_x2, 3, 8);

            already_printed = true;
            break;

        case MENU_W_N:
            strcpy(String, gSubMenu_W_N[gSubMenuSelection]);
            break;

        case MENU_SCR:
            strcpy(String, gSubMenu_SCRAMBLER[gSubMenuSelection]);
            if (gRxVfo->Modulation == MODULATION_FM) {
                if (gSubMenuSelection > 0 && gSetting_ScrambleEnable)
                    BK4819_EnableScramble(gSubMenuSelection - 1);
                else
                    BK4819_DisableScramble();
            }
            break;

        case MENU_VOX:
            #ifdef ENABLE_VOX
                if (gSubMenuSelection == 0)
                    strcpy(String, gSubMenu_OFF_ON[0]);
                else
                    NUMBER_ToDecimal(String, gSubMenuSelection, 2, false);
                
                gaugeLine = 6;
                gaugeMin = 0;
                gaugeMax = 10;
            #else
                strcpy(String, gSubMenu_NA);
            #endif
            break;

        case MENU_ABR:
            if(gSubMenuSelection == 0)
            {
                strcpy(String, gSubMenu_OFF_ON[0]);
            }
            else if(gSubMenuSelection < 61)
            {

                strcpy(String, "  m:  s");
                NUMBER_ToDecimal(String, ((gSubMenuSelection) * 5) / 60, 2, true);
                NUMBER_ToDecimal(String + 4, ((gSubMenuSelection) * 5) % 60, 2, true);
                gaugeLine = 4;
                gaugeMin = 1;
                gaugeMax = 60;
            }
            else
            {
                strcpy(String, "ON");
            }

            // Obsolete ???
            //if(BACKLIGHT_GetBrightness() < 4)
            //    BACKLIGHT_SetBrightness(4);
            break;

        case MENU_ABR_MIN:
        case MENU_ABR_MAX:
             // Moved to grouped block
            break;

        case MENU_AM:
            strcpy(String, gModulationStr[gSubMenuSelection]);
            break;

        case MENU_AUTOLK:
            if (gSubMenuSelection == 0)
            {
                strcpy(String, "OFF");
            }
            else
            {
                strcpy(String, "  m:  s");
                NUMBER_ToDecimal(String, (gSubMenuSelection * 15) / 60, 2, true);
                NUMBER_ToDecimal(String + 4, (gSubMenuSelection * 15) % 60, 2, true);
                gaugeLine = 4;
                gaugeMin = 1;
                gaugeMax = 40;
            }
            break;

        case MENU_COMPAND:
        case MENU_ABR_ON_TX_RX:
            strcpy(String, gSubMenu_RX_TX[gSubMenuSelection]);
            break;

        #ifndef ENABLE_CUSTOM_FIRMWARE_MODS
            #ifdef ENABLE_AM_FIX
                case MENU_AM_FIX:
            #endif
        #endif
        case MENU_BCL:
        case MENU_BEEP:
        case MENU_S_ADD1:
        case MENU_S_ADD2:
        case MENU_S_ADD3:
        case MENU_STE:
        case MENU_D_ST:
#ifdef ENABLE_DTMF_CALLING
        case MENU_D_DCD:
#endif
        case MENU_D_LIVE_DEC:
        #ifdef ENABLE_NOAA
            case MENU_NOAA_S:
        #endif
#ifndef ENABLE_CUSTOM_FIRMWARE_MODS
        case MENU_350TX:
        case MENU_200TX:
        case MENU_500TX:
#endif
        case MENU_350EN:
        case MENU_SCREN:
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
        case MENU_SET_TMR:
        case MENU_SET_INV:
        case MENU_SET_NAV:
        case MENU_TX_LOCK:
#endif
        case MENU_MIC_AGC:
        case MENU_MIC_BAR:
            #ifdef ENABLE_MIC_BAR
                strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);
            #else
                if (UI_MENU_GetCurrentMenuId() == MENU_MIC_BAR)
                    strcpy(String, gSubMenu_NA);
                else
                    strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);
            #endif
            break;

        case MENU_MEM_CH:
        case MENU_1_CALL:
        case MENU_DEL_CH:
        {
            const bool valid = RADIO_CheckValidChannel(gSubMenuSelection, false, 0);

            UI_GenerateChannelStringEx(String, valid, gSubMenuSelection);
            UI_PrintString(String, menu_item_x1, menu_item_x2, 0, 8);

            if (valid && !gAskForConfirmation)
            {   // show the frequency so that the user knows the channels frequency
                const uint32_t frequency = SETTINGS_FetchChannelFrequency(gSubMenuSelection);
                // sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
                UI_PrintFrequencyEx(String, frequency, true);
                UI_PrintString(String, menu_item_x1, menu_item_x2, 4, 8);
            }

            SETTINGS_FetchChannelName(String, gSubMenuSelection);
            UI_PrintString(String[0] ? String : "--", menu_item_x1, menu_item_x2, 2, 8);
            already_printed = true;
            break;
        }

        case MENU_MEM_NAME:
        {
            const bool valid = RADIO_CheckValidChannel(gSubMenuSelection, false, 0);

            UI_GenerateChannelStringEx(String, valid, gSubMenuSelection);
            UI_PrintString(String, menu_item_x1, menu_item_x2, 0, 8);

            if (valid)
            {
                const uint32_t frequency = SETTINGS_FetchChannelFrequency(gSubMenuSelection);

                //if (!gIsInSubMenu || edit_index < 0)
                if (!gIsInSubMenu)
                    edit_index = -1;
                if (edit_index < 0)
                {   // show the channel name
                    SETTINGS_FetchChannelName(String, gSubMenuSelection);
                    char *pPrintStr = String[0] ? String : "--";
                    UI_PrintString(pPrintStr, menu_item_x1, menu_item_x2, 2, 8);
                }
                else
                {   // show the channel name being edited
                    //UI_PrintString(edit, menu_item_x1, 0, 2, 8);
                    UI_PrintString(edit, menu_item_x1, menu_item_x2, 2, 8);
                    if (edit_index < 10)
                        //UI_PrintString("^", menu_item_x1 + (8 * edit_index), 0, 4, 8);  // show the cursor
                        UI_PrintString("^", menu_item_x1 - 1 + (8 * edit_index),0, 4, 8); // show the cursor
                }

                if (!gAskForConfirmation)
                {   // show the frequency so that the user knows the channels frequency
                    // sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
                    UI_PrintFrequencyEx(String, frequency, true);
                    UI_PrintString(String, menu_item_x1, menu_item_x2, 4 + (gIsInSubMenu && edit_index >= 0), 8);
                }
            }

            already_printed = true;
            break;
        }

        case MENU_SAVE:
            if (gSubMenuSelection == 0)
                strcpy(String, gSubMenu_OFF_ON[0]);
            else {
                strcpy(String, "1:");
                NUMBER_ToDecimal(String + 2, gSubMenuSelection, 1, false);
            }
            break;

        case MENU_TDR:
            strcpy(String, gSubMenu_RXMode[gSubMenuSelection]);
            break;

        case MENU_TOT:
            strcpy(String, "  m:  s");
            NUMBER_ToDecimal(String, ((gSubMenuSelection + 1) * 5) / 60, 2, true);
            NUMBER_ToDecimal(String + 4, ((gSubMenuSelection + 1) * 5) % 60, 2, true);
            gaugeLine = 4;
            gaugeMin = 5;
            gaugeMax = 179;
            break;

        #ifdef ENABLE_VOICE
            case MENU_VOICE:
                strcpy(String, gSubMenu_VOICE[gSubMenuSelection]);
                break;
        #endif

        case MENU_SC_REV:
            if(gSubMenuSelection == 0)
            {
                strcpy(String, "STOP");
            }
            else if(gSubMenuSelection < 81)
            {
                // sprintf(String, "CARRIER %02ds:%03dms", ((gSubMenuSelection * 250) / 1000), ((gSubMenuSelection * 250) % 1000));
                strcpy(String, "CARRIER   s:   ms");
                NUMBER_ToDecimal(String + 8, (gSubMenuSelection * 250) / 1000, 2, true);
                NUMBER_ToDecimal(String + 13, (gSubMenuSelection * 250) % 1000, 3, true);
                //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
                //ST7565_Gauge(5, 1, 80, gSubMenuSelection);
                gaugeLine = 5;
                gaugeMin = 1;
                gaugeMax = 80;
                //#endif
            }
            else
            {
                // sprintf(String, "TIMEOUT %02dm:%02ds", (((gSubMenuSelection - 80) * 5) / 60), (((gSubMenuSelection - 80) * 5) % 60));
                strcpy(String, "TIMEOUT   m:   s");
                NUMBER_ToDecimal(String + 8, ((gSubMenuSelection - 80) * 5) / 60, 2, true);
                NUMBER_ToDecimal(String + 13, ((gSubMenuSelection - 80) * 5) % 60, 2, true);
                String[15] = '\0';
                //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
                //ST7565_Gauge(5, 80, 104, gSubMenuSelection);
                gaugeLine = 5;
                gaugeMin = 80;
                gaugeMax = 104;
                //#endif
            }
            break;

        case MENU_MDF:
            strcpy(String, gSubMenu_MDF[gSubMenuSelection]);
            break;

        case MENU_RP_STE:
            if (gSubMenuSelection == 0)
                strcpy(String, gSubMenu_OFF_ON[0]);
            else {
                NUMBER_ToDecimal(String, gSubMenuSelection, 1, false);
                strcat(String, "*100ms");
            }
            break;

        case MENU_S_LIST:
            if (gSubMenuSelection == 0)
                strcpy(String, "LIST [0] NO LIST");
            else if (gSubMenuSelection < 4)
            {
                // sprintf(String, "LIST [%u]", gSubMenuSelection);
                strcpy(String, "LIST [ ]");
                String[6] = gSubMenuSelection + '0';
            }
            else if (gSubMenuSelection == 4)
                strcpy(String, "LISTS [1, 2, 3]");
            else if (gSubMenuSelection == 5)
                strcpy(String, "ALL");
            break;

        #ifdef ENABLE_ALARM
            case MENU_AL_MOD:
                strcpy(String, gSubMenu_AL_MOD[gSubMenuSelection]);
                break;
        #endif

#ifdef ENABLE_DTMF_CALLING
        case MENU_ANI_ID:
            strcpy(String, gEeprom.ANI_DTMF_ID);
            break;
#endif
        case MENU_UPCODE:
            strncpy(String, gEeprom.DTMF_UP_CODE, 8);
            String[8] = ' ';
            strncpy(String + 9, gEeprom.DTMF_UP_CODE + 8, 8);
            String[17] = '\0';
            break;

        case MENU_DWCODE:
            strncpy(String, gEeprom.DTMF_DOWN_CODE, 8);
            String[8] = ' ';
            strncpy(String + 9, gEeprom.DTMF_DOWN_CODE + 8, 8);
            String[17] = '\0';
            break;

#ifdef ENABLE_DTMF_CALLING
        case MENU_D_RSP:
            strcpy(String, gSubMenu_D_RSP[gSubMenuSelection]);
            break;

        case MENU_D_HOLD:
        case MENU_ABR_MIN:
        case MENU_ABR_MAX:
        case MENU_SQL:
            NUMBER_ToDecimal(String, gSubMenuSelection, 2, false);
            if(UI_MENU_GetCurrentMenuId() == MENU_D_HOLD)
            {
                strcat(String, "s");
            }
            else if((UI_MENU_GetCurrentMenuId() == MENU_ABR_MIN || UI_MENU_GetCurrentMenuId() == MENU_ABR_MAX) && gIsInSubMenu)
            {
                BACKLIGHT_SetBrightness(gSubMenuSelection);
            }
            break;
#endif
        case MENU_D_PRE:
            NUMBER_ToDecimal(String, gSubMenuSelection, 3, false);
            strcat(String, "*10ms");
            break;

        case MENU_PTT_ID:
            strcpy(String, gSubMenu_PTT_ID[gSubMenuSelection]);
            break;

        case MENU_BAT_TXT:
            strcpy(String, gSubMenu_BAT_TXT[gSubMenuSelection]);
            break;

#ifdef ENABLE_DTMF_CALLING
        case MENU_D_LIST:
            gIsDtmfContactValid = DTMF_GetContact((int)gSubMenuSelection - 1, Contact);
            if (!gIsDtmfContactValid)
                strcpy(String, "NULL");
            else
                memcpy(String, Contact, 8);
            break;
#endif

        case MENU_PONMSG:
            strcpy(String, gSubMenu_PONMSG[gSubMenuSelection]);
            break;

        case MENU_ROGER:
            strcpy(String, gSubMenu_ROGER[gSubMenuSelection]);
            break;

        case MENU_VOL:
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
            // sprintf(String, "%s %s", AUTHOR_STRING, VERSION_STRING_2);
            strcpy(String, AUTHOR_STRING);
            strcat(String, " ");
            strcat(String, VERSION_STRING_2);
#else
            // sprintf(String, "%u.%02uV %u%%", gBatteryVoltageAverage / 100, gBatteryVoltageAverage % 100, BATTERY_VoltsToPercent(gBatteryVoltageAverage));
            NUMBER_ToDecimal(String, gBatteryVoltageAverage / 100, 2, false);
            strcat(String, ".");
            NUMBER_ToDecimal(String + strlen(String), gBatteryVoltageAverage % 100, 2, true);
            strcat(String, "V ");
            NUMBER_ToDecimal(String + strlen(String), BATTERY_VoltsToPercent(gBatteryVoltageAverage), 3, false);
            strcat(String, "%");
#endif
            break;

        case MENU_RESET:
            strcpy(String, gSubMenu_RESET[gSubMenuSelection]);
            break;

        case MENU_F_LOCK:
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
            if(!gIsInSubMenu && gUnlockAllTxConfCnt>0 && gUnlockAllTxConfCnt<3)
#else
            if(!gIsInSubMenu && gUnlockAllTxConfCnt>0 && gUnlockAllTxConfCnt<10)
#endif
                strcpy(String, "READ MANUAL");
            else
                strcpy(String, gSubMenu_F_LOCK[gSubMenuSelection]);
            break;

        #ifdef ENABLE_F_CAL_MENU
            case MENU_F_CALI:
                {
                    const uint32_t value   = 22656 + gSubMenuSelection;
                    const uint32_t xtal_Hz = (0x4f0000u + value) * 5;

                    writeXtalFreqCal(gSubMenuSelection, false);

                    // sprintf(String, "%d %u.%06u MHz", gSubMenuSelection, xtal_Hz / 1000000, xtal_Hz % 1000000);
                    NUMBER_ToDecimal(String, gSubMenuSelection, 3, false);
                    strcat(String, " ");
                    NUMBER_ToDecimal(String + strlen(String), xtal_Hz / 1000000, 2, false);
                    strcat(String, ".");
                    NUMBER_ToDecimal(String + strlen(String), xtal_Hz % 1000000, 6, true);
                    strcat(String, " MHz");
                }
                break;
        #endif

        {
            const uint16_t vol = (uint32_t)gBatteryVoltageAverage * gBatteryCalibration[3] / gSubMenuSelection;
            // sprintf(String, "%u.%02uV %u", vol / 100, vol % 100, gSubMenuSelection);
            NUMBER_ToDecimal(String, vol / 100, 2, false);
            strcat(String, ".");
            NUMBER_ToDecimal(String + strlen(String), vol % 100, 2, true);
            strcat(String, "V ");
            NUMBER_ToDecimal(String + strlen(String), gSubMenuSelection, 4, false);
            break;
        }

        case MENU_BATTYP:
            strcpy(String, gSubMenu_BATTYP[gSubMenuSelection]);
            break;

        case MENU_F1SHRT:
        case MENU_F1LONG:
        case MENU_F2SHRT:
        case MENU_F2LONG:
        case MENU_MLONG:
            strcpy(String, gSubMenu_SIDEFUNCTIONS[gSubMenuSelection].name);
            break;

#ifdef ENABLE_DEEP_SLEEP_MODE
        case MENU_SET_OFF:
            if(gSubMenuSelection == 0)
            {
                strcpy(String, gSubMenu_OFF_ON[0]);
            }
            else if(gSubMenuSelection < 121)
            {
                strcpy(String, "  h:  m");
                NUMBER_ToDecimal(String, gSubMenuSelection / 60, 2, false);
                NUMBER_ToDecimal(String + 4, gSubMenuSelection % 60, 2, true);
                gaugeLine = 4;
                gaugeMin = 1;
                gaugeMax = 120;
            }
            break;
#endif

#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
        case MENU_SET_PWR:
            // sprintf(String, "%s %sW", gSubMenu_TXP[gSubMenuSelection + 1], gSubMenu_SET_PWR[gSubMenuSelection]);
            strcpy(String, gSubMenu_TXP[gSubMenuSelection + 1]);
            strcat(String, " ");
            strcat(String, gSubMenu_SET_PWR[gSubMenuSelection]);
            strcat(String, "W");
            break;
    
        case MENU_SET_PTT:
            strcpy(String, gSubMenu_SET_PTT[gSubMenuSelection]);
            break;

        case MENU_SET_TOT:
        case MENU_SET_EOT:
            strcpy(String, gSubMenu_SET_TOT[gSubMenuSelection]); // Same as SET_TOT
            break;

        case MENU_SET_CTR:
            #ifdef ENABLE_LCD_CONTRAST_OPTION
                NUMBER_ToDecimal(String, gSubMenuSelection, 2, false);
                gSetting_set_ctr = gSubMenuSelection;
                ST7565_ContrastAndInv();
            #else
                strcpy(String, gSubMenu_NA);
            #endif
            break;

        // Set inv case handled above

        // Tx lock handled above

        case MENU_SET_LCK:
            strcpy(String, gSubMenu_SET_LCK[gSubMenuSelection]);
            break;

        // Set nav handled above


        #ifdef ENABLE_NARROWER_BW_FILTER
            case MENU_SET_NFM:
                strcpy(String, gSubMenu_SET_NFM[gSubMenuSelection]);
                break;
        #endif

        #ifdef ENABLE_SYSTEM_INFO_MENU
            case MENU_SET_VOL:
                if(gSubMenuSelection == 0)
                {
                    strcpy(String, gSubMenu_OFF_ON[0]);
                }
                else if(gSubMenuSelection < 64)
                {
                    // sprintf(String, "%02u", gSubMenuSelection);
                    NUMBER_ToDecimal(String, gSubMenuSelection, 2, true);
                    //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
                    //ST7565_Gauge(4, 1, 63, gSubMenuSelection);
                    gaugeLine = 6;
                    gaugeMin = 1;
                    gaugeMax = 63;
                    //#endif
                }
                gEeprom.VOLUME_GAIN = gSubMenuSelection;
                BK4819_WriteRegister(BK4819_REG_48,
                    (11u << 12)                |     // ??? .. 0 ~ 15, doesn't seem to make any difference
                    ( 0u << 10)                |     // AF Rx Gain-1
                    (gEeprom.VOLUME_GAIN << 4) |     // AF Rx Gain-2
                    (gEeprom.DAC_GAIN    << 0));     // AF DAC Gain (after Gain-1 and Gain-2)
                break;
        #endif

        #ifdef ENABLE_RESCUE_OPERATIONS
            case MENU_SET_KEY:
                strcpy(String, gSubMenu_SET_KEY[gSubMenuSelection]);
                break;                
        #endif
#endif

    }

    //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
    if(gaugeLine != 0)
    {
        ST7565_Gauge(gaugeLine, gaugeMin, gaugeMax, gSubMenuSelection);
    }
    //#endif

    if (!already_printed)
    {   // we now do multi-line text in a single string

        unsigned int y;
        unsigned int lines = 1;
        unsigned int len   = strlen(String);
        bool         small = false;

        if (len > 0)
        {
            // count number of lines
            for (i = 0; i < len; i++)
            {
                if (String[i] == ' ' && i < (len - 1))
                {   // found new line char
                    lines++;
                    String[i] = 0;  // null terminate the line
                }
            }

            if (lines > 3)
            {   // use small text
                small = true;
                if (lines > 7)
                    lines = 7;
            }

            // center vertically'ish
            /*
            if (small)
                y = 3 - ((lines + 0) / 2);  // untested
            else
                y = 2 - ((lines + 0) / 2);
            */

            y = (small ? 3 : 2) - (lines / 2); 

            // only for SysInf
            if(UI_MENU_GetCurrentMenuId() == MENU_VOL)
            {
                // Battery Info
                UI_FormatVoltage(edit, gBatteryVoltageAverage * 10);
                strcat(edit, " ");
                NUMBER_ToDecimal(edit + strlen(edit), BATTERY_VoltsToPercent(gBatteryVoltageAverage), 3, false);
                strcat(edit, "%");
                UI_PrintStringSmallNormal(edit, 54, 127, 1);

                // Internal Sensors (Entropy Source Debug)
                // Temperature and Vref from ADC driver
                strcpy(String, "TS:");
                UI_FormatTemp(String + 3, ADC_GetTemp());
                UI_PrintStringSmallNormal(String, 54, 127, 3);
                
                // System VCC (from VREFINT)
                strcpy(String, "VCC:");
                UI_FormatVoltage(String + 4, ADC_GetVref());
                UI_PrintStringSmallNormal(String, 54, 127, 4);

                #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
                    // UI_PrintStringSmallNormal(Edition, 54, 127, 6);
                #endif

                y = 2;
            }

            // draw the text lines
            for (i = 0; i < len && lines > 0; lines--)
            {
                if (small)
                    UI_PrintStringSmallNormal(String + i, menu_item_x1, menu_item_x2, y);
                else
                    UI_PrintString(String + i, menu_item_x1, menu_item_x2, y, 8);

                // look for start of next line
                while (i < len && String[i] >= 32)
                    i++;

                // hop over the null term char(s)
                while (i < len && String[i] < 32)
                    i++;

                y += small ? 1 : 2;
            }
        }
    }

    if (UI_MENU_GetCurrentMenuId() == MENU_SLIST1 || UI_MENU_GetCurrentMenuId() == MENU_SLIST2 || UI_MENU_GetCurrentMenuId() == MENU_SLIST3)
    {
        i = UI_MENU_GetCurrentMenuId() - MENU_SLIST1;

        char *pPrintStr = String;

        if (gSubMenuSelection < 0) {
            pPrintStr = "NULL";
        } else {
            UI_GenerateChannelStringEx(String, true, gSubMenuSelection);
            pPrintStr = String;
        }

        // channel number
        UI_PrintString(pPrintStr, menu_item_x1, menu_item_x2, 0, 8);

        SETTINGS_FetchChannelName(String, gSubMenuSelection);
        pPrintStr = String[0] ? String : "--";

        // channel name and scan-list
        if (gSubMenuSelection < 0 || !gEeprom.SCAN_LIST_ENABLED[i]) {
            UI_PrintString(pPrintStr, menu_item_x1, menu_item_x2, 2, 8);
        } else {
            /*
            UI_PrintStringSmallNormal(pPrintStr, menu_item_x1, menu_item_x2, 2);

            if (IS_MR_CHANNEL(gEeprom.SCANLIST_PRIORITY_CH1[i])) {
                // sprintf(String, "PRI%d:%u", 1, gEeprom.SCANLIST_PRIORITY_CH1[i] + 1);
                strcpy(String, "PRI1:");
                NUMBER_ToDecimal(String + 5, gEeprom.SCANLIST_PRIORITY_CH1[i] + 1, 3, false);
                UI_PrintString(String, menu_item_x1, menu_item_x2, 3, 8);
            }

            if (IS_MR_CHANNEL(gEeprom.SCANLIST_PRIORITY_CH2[i])) {
                // sprintf(String, "PRI%d:%u", 2, gEeprom.SCANLIST_PRIORITY_CH2[i] + 1);
                strcpy(String, "PRI2:");
                NUMBER_ToDecimal(String + 5, gEeprom.SCANLIST_PRIORITY_CH2[i] + 1, 3, false);
                UI_PrintString(String, menu_item_x1, menu_item_x2, 5, 8);
            }
            */

            UI_PrintStringSmallNormal(pPrintStr, menu_item_x1, menu_item_x2, 2);

            for (uint8_t pri = 1; pri <= 2; pri++) {
                uint8_t channel = (pri == 1) ? gEeprom.SCANLIST_PRIORITY_CH1[i] : gEeprom.SCANLIST_PRIORITY_CH2[i];

                if (IS_MR_CHANNEL(channel)) {
                    // sprintf(String, "PRI%d:%u", pri, channel + 1);
                    strcpy(String, "PRI :   ");
                    String[3] = pri + '0';
                    NUMBER_ToDecimal(String + 5, channel + 1, 3, false);
                    UI_PrintString(String, menu_item_x1, menu_item_x2, pri * 2 + 1, 8);
                }
            }

        }
    }

    if ((UI_MENU_GetCurrentMenuId() == MENU_R_CTCS || UI_MENU_GetCurrentMenuId() == MENU_R_DCS) && gCssBackgroundScan)
        UI_PrintString("SCAN", menu_item_x1, menu_item_x2, 4, 8);

#ifdef ENABLE_DTMF_CALLING
    if (UI_MENU_GetCurrentMenuId() == MENU_D_LIST && gIsDtmfContactValid) {
        Contact[11] = 0;
        memcpy(&gDTMF_ID, Contact + 8, 4);
        // sprintf(String, "ID:%4s", gDTMF_ID);
        strcpy(String, "ID:");
        strncat(String, gDTMF_ID, 4);
        UI_PrintString(String, menu_item_x1, menu_item_x2, 4, 8);
    }
#endif

    if (UI_MENU_GetCurrentMenuId() == MENU_R_CTCS ||
        UI_MENU_GetCurrentMenuId() == MENU_T_CTCS ||
        UI_MENU_GetCurrentMenuId() == MENU_R_DCS  ||
        UI_MENU_GetCurrentMenuId() == MENU_T_DCS
#ifdef ENABLE_DTMF_CALLING
        || UI_MENU_GetCurrentMenuId() == MENU_D_LIST
#endif
    ) {
        // sprintf(String, "%2d", gSubMenuSelection);
        NUMBER_ToDecimal(String, gSubMenuSelection, 2, false);
        UI_PrintStringSmallNormal(String, 105, 0, 0);
    }

    if ((UI_MENU_GetCurrentMenuId() == MENU_RESET    ||
         UI_MENU_GetCurrentMenuId() == MENU_MEM_CH   ||
         UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME ||
         UI_MENU_GetCurrentMenuId() == MENU_DEL_CH) && gAskForConfirmation)
    {   // display confirmation
        char *pPrintStr = (gAskForConfirmation == 1) ? "SURE?" : "WAIT!";
        UI_PrintString(pPrintStr, menu_item_x1, menu_item_x2, 5, 8);
    }

    ST7565_BlitFullScreen();
}