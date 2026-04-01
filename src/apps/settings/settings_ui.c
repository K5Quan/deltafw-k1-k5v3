#include <string.h>
#include "features/storage/storage.h"
#include "settings_ui.h"
#include "../../ui/ag_menu.h"
#include "../../drivers/bsp/st7565.h"
#include "../../apps/settings/settings.h"
#include "../../apps/security/passcode.h"
#include "ui/menu.h"

#include "../../ui/ui.h"
#include "../../ui/textinput.h"
#include "../../ui/ag_graphics.h"
#include "../../drivers/bsp/keyboard.h"
#include "../../drivers/bsp/system.h"
#include "../../drivers/bsp/bk4819.h"
#include "../../core/misc.h"
#include "features/radio/frequencies.h"
#include "features/audio/audio.h"
#include "features/dcs/dcs.h" // For CTCSS/DCS tables
#include "../../ui/helper.h" // For frequency helpers
#ifdef ENABLE_EEPROM_HEXDUMP
#include "../../ui/hexdump.h"
#include "../../drivers/bsp/i2c.h" // For EEPROM_ReadBuffer if available or define local
// Usually standard EEPROM read is I2C_ReadBuffer or similar.
// Let's assume EEPROM_ReadBuffer exists as in user example or define a wrapper.
extern void EEPROM_ReadBuffer(uint32_t Address, uint8_t *pBuffer, uint32_t Size);
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define INC_DEC(val, min, max, inc) do { \
    if (inc) { if ((val) < (max)) (val)++; else (val) = (min); } \
    else     { if ((val) > (min)) (val)--; else (val) = (max); } \
} while(0)

// Reference to DCS/CTCSS tables
// They are extern in dcs.h

// --- Table-Driven Settings metadata ---

typedef enum {
    SET_TYPE_BOOL,
    SET_TYPE_LIST,
    SET_TYPE_INT8,
    SET_TYPE_UNIT,
    SET_TYPE_SPECIAL
} SetType;

typedef struct {
    uint8_t id;
    SetType type;
    void *ptr;
    uint8_t min;
    uint8_t max;
    const void *list;
    uint8_t width; // 0 for char**, >0 for char[N][W]
} SettingConfig;

static const SettingConfig settingConfigs[] = {
    {MENU_SQL,      SET_TYPE_INT8,  &gEeprom.SQUELCH_LEVEL, 0, 9, NULL, 0},
    {MENU_BEEP,     SET_TYPE_BOOL,  &gEeprom.BEEP_CONTROL, 0, 1, gSubMenu_OFF_ON, 4},
    {MENU_ROGER,    SET_TYPE_LIST,  &gEeprom.ROGER, 0,
#ifdef ENABLE_CUSTOM_ROGER
        ROGER_MODE_CUSTOM3,
#elif defined(ENABLE_EXTRA_ROGER)
        ROGER_MODE_UV5RC,
#else
        ROGER_MODE_MDC,
#endif
        gSubMenu_ROGER, 0},
    {MENU_STE,      SET_TYPE_BOOL,  &gEeprom.TAIL_TONE_ELIMINATION, 0, 1, gSubMenu_OFF_ON, 4},
    {MENU_RP_STE,   SET_TYPE_INT8,  &gEeprom.REPEATER_TAIL_TONE_ELIMINATION, 0, 10, NULL, 0},
    {MENU_MDF,      SET_TYPE_LIST,  &gEeprom.CHANNEL_DISPLAY_MODE, 0, 3, gSubMenu_MDF, 0},
    {MENU_ABR_MAX,  SET_TYPE_INT8,  &gEeprom.BACKLIGHT_MAX, 1, 10, NULL, 0},
    {MENU_ABR_MIN,  SET_TYPE_INT8,  &gEeprom.BACKLIGHT_MIN, 0, 9, NULL, 0},
    {MENU_BAT_TXT,  SET_TYPE_LIST,  &gSetting_battery_text, 0, 7, gSubMenu_BAT_TXT, 8},
    {MENU_PONMSG,   SET_TYPE_LIST,  &gEeprom.POWER_ON_DISPLAY_MODE, 0, 4, gSubMenu_PONMSG, 8},
    {MENU_ABR_ON_TX_RX, SET_TYPE_LIST, &gSetting_backlight_on_tx_rx, 0, 3, gSubMenu_RX_TX, 6},
    {MENU_SET_LCK,  SET_TYPE_BOOL,  &gSetting_set_lck, 0, 1, gSubMenu_SET_LCK, 9},
    {MENU_SET_TMR,  SET_TYPE_BOOL,  &gSetting_set_tmr, 0, 1, gSubMenu_OFF_ON, 4},
    {MENU_SET_AUD,  SET_TYPE_LIST,  &gSetting_set_audio, 0, 4, gSubMenu_SET_AUD, 6},
    {MENU_BATTYP,   SET_TYPE_LIST,  &gEeprom.BATTERY_TYPE, 0, 4, gSubMenu_BATTYP, 12},
    {MENU_D_ST,     SET_TYPE_BOOL,  &gEeprom.DTMF_SIDE_TONE, 0, 1, gSubMenu_OFF_ON, 4},
    {MENU_D_LIVE_DEC, SET_TYPE_BOOL, &gSetting_live_DTMF_decoder, 0, 1, gSubMenu_OFF_ON, 4},
#ifdef ENABLE_MIC_BAR
    {MENU_MIC_BAR,  SET_TYPE_BOOL,  &gSetting_mic_bar, 0, 1, gSubMenu_OFF_ON, 4},
#endif
    {MENU_MIC_AGC,  SET_TYPE_BOOL,  &gEeprom.MIC_AGC, 0, 1, gSubMenu_OFF_ON, 4},
    {MENU_VOL_GAIN, SET_TYPE_INT8,  &gEeprom.VOLUME_GAIN, 0, 63, NULL, 0},
#ifdef ENABLE_VOICE
    {MENU_VOICE,    SET_TYPE_LIST,  &gEeprom.VOICE_PROMPT, 0, 2, gSubMenu_VOICE, 4},
#endif
#ifdef ENABLE_ALARM
    {MENU_AL_MOD,   SET_TYPE_BOOL,  &gEeprom.ALARM_MODE, 0, 1, gSubMenu_AL_MOD, 5},
#endif
#ifdef ENABLE_NARROWER_BW_FILTER
    {MENU_SET_NFM,  SET_TYPE_BOOL,  &gSetting_set_nfm, 0, 1, gSubMenu_SET_NFM, 9},
#endif
    {MENU_SET_CTR,  SET_TYPE_INT8,  &gSetting_set_ctr, 0, 15, NULL, 0},
    {MENU_SET_INV,  SET_TYPE_BOOL,  &gSetting_set_inv, 0, 1, gSubMenu_OFF_ON, 4},

#ifdef ENABLE_TX_SOFT_START
    {MENU_TX_SOFT_START, SET_TYPE_BOOL, &gEeprom.TX_SOFT_START, 0, 1, gSubMenu_OFF_ON, 4},
#endif
#ifdef ENABLE_TX_AUDIO_COMPRESSOR
    {MENU_TX_COMPRESSOR, SET_TYPE_BOOL, &gEeprom.TX_AUDIO_COMPRESSOR, 0, 1, gSubMenu_OFF_ON, 4},
#endif
#ifdef ENABLE_CTCSS_LEAD_IN
    {MENU_CTCSS_LEAD, SET_TYPE_BOOL, &gEeprom.CTCSS_LEAD_IN, 0, 1, gSubMenu_OFF_ON, 4},
#endif
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {MENU_SET_PTT,  SET_TYPE_LIST,  &gSetting_set_ptt, 0, 2, gSubMenu_SET_PTT, 7},
    {MENU_SET_TOT,  SET_TYPE_LIST,  &gSetting_set_tot, 0, 3, gSubMenu_SET_TOT, 7},
    {MENU_SET_EOT,  SET_TYPE_LIST,  &gSetting_set_eot, 0, 3, gSubMenu_SET_TOT, 7},
    {MENU_SET_PWR,  SET_TYPE_LIST,  &gSetting_set_pwr, 0, 6, gSubMenu_SET_PWR, 6},
    #ifdef ENABLE_DEEP_SLEEP_MODE
    {MENU_SET_OFF,  SET_TYPE_INT8,  &gSetting_set_off, 0, 60, NULL, 0},
    #endif
    {MENU_TX_LOCK,  SET_TYPE_BOOL,  &gSetting_set_lck, 0, 1, gSubMenu_OFF_ON, 4},
    {MENU_350EN,    SET_TYPE_BOOL,  &gSetting_350EN, 0, 1, gSubMenu_OFF_ON, 4},
#endif
    {MENU_1_CALL,   SET_TYPE_BOOL,  &gEeprom.CHAN_1_CALL, 0, 1, gSubMenu_OFF_ON, 4}, // Probably not bool but channel?

};

static const SettingConfig *GetSettingConfig(uint8_t id) {
    for (uint16_t i = 0; i < ARRAY_SIZE(settingConfigs); i++) {
        if (settingConfigs[i].id == id) return &settingConfigs[i];
    }
    return NULL;
}

// --- Helper Functions ---



static void Settings_GetValueStr(uint8_t settingId, char *buf, uint8_t bufLen) {
    if (!buf) return;
    buf[0] = '\0';

    const SettingConfig *conf = GetSettingConfig(settingId);
    if (conf) {
        uint8_t val = *(uint8_t *)conf->ptr;
        switch (conf->type) {
            case SET_TYPE_BOOL:
            case SET_TYPE_LIST:
                if (conf->list) {
                    if (conf->width == 0) strcpy(buf, ((const char *const *)conf->list)[val]);
                    else strcpy(buf, (const char *)conf->list + (val * conf->width));
                }
                return;
            case SET_TYPE_INT8:
                NUMBER_ToDecimal(buf, val, (conf->max > 9) ? 2 : 1, false);
                return;
            default: break;
        }
    }

    switch (settingId) {
        // --- Special Formatters ---
        case MENU_VOX:
            if (!gEeprom.VOX_SWITCH) strcpy(buf, "OFF");
            else NUMBER_ToDecimal(buf, gEeprom.VOX_LEVEL, 1, false);
            break;
        case MENU_MIC: {
            const uint8_t gain = gMicGain_dB2[gEeprom.MIC_SENSITIVITY];
            strcpy(buf, "+  . dB");
            NUMBER_ToDecimal(buf + 1, gain / 2, 2, false);
            buf[4] = (gain % 2) + '0';
            break;
        }
        case MENU_ABR:
            if (gEeprom.BACKLIGHT_TIME == 0) strcpy(buf, "OFF");
            else if (gEeprom.BACKLIGHT_TIME >= 61) strcpy(buf, "ON");
            else { NUMBER_ToDecimal(buf, gEeprom.BACKLIGHT_TIME * 5, 3, false); strcat(buf, "s"); }
            break;
        case MENU_F1SHRT: case MENU_F1LONG: case MENU_F2SHRT: case MENU_F2LONG: case MENU_MLONG: {
            uint8_t val = (settingId == MENU_F1SHRT) ? gEeprom.KEY_1_SHORT_PRESS_ACTION :
                          (settingId == MENU_F1LONG) ? gEeprom.KEY_1_LONG_PRESS_ACTION :
                          (settingId == MENU_F2SHRT) ? gEeprom.KEY_2_SHORT_PRESS_ACTION :
                          (settingId == MENU_F2LONG) ? gEeprom.KEY_2_LONG_PRESS_ACTION : gEeprom.KEY_M_LONG_PRESS_ACTION;
            const char* name = "NONE";
            for(int i=0; i<gSubMenu_SIDEFUNCTIONS_size; i++) if(gSubMenu_SIDEFUNCTIONS[i].id == val) { name = gSubMenu_SIDEFUNCTIONS[i].name; break; }
            strcpy(buf, name);
            break;
        }
        case MENU_STEP: {
            uint16_t step = gStepFrequencyTable[gTxVfo->STEP_SETTING];
            NUMBER_ToDecimal(buf, step / 100, 2, false); strcat(buf, ".");
            NUMBER_ToDecimal(buf + strlen(buf), step % 100, 2, true);
            break;
        }
#ifdef ENABLE_TX_OFFSET
        case MENU_OFFSET: UI_PrintFrequencyEx(buf, gTxVfo->TX_OFFSET_FREQUENCY, true); break;
#endif

        case MENU_SC_REV: {
            const char* modes[] = {"TO", "CO", "SE", "TIME"};
            strcpy(buf, modes[gEeprom.SCAN_RESUME_MODE < 4 ? gEeprom.SCAN_RESUME_MODE : 3]);
            break;
        }
        case MENU_R_DCS: case MENU_T_DCS: {
            uint8_t code = (settingId == MENU_R_DCS) ? gTxVfo->pRX->Code : gTxVfo->pTX->Code;
            uint8_t type = (settingId == MENU_R_DCS) ? gTxVfo->pRX->CodeType : gTxVfo->pTX->CodeType;
            if (type != CODE_TYPE_DIGITAL && type != CODE_TYPE_REVERSE_DIGITAL) strcpy(buf, "OFF");
            else {
                strcpy(buf, "D   N"); uint16_t v = DCS_Options[code];
                buf[1] = ((v >> 6) & 7) + '0'; buf[2] = ((v >> 3) & 7) + '0'; buf[3] = (v & 7) + '0';
                if (type == CODE_TYPE_REVERSE_DIGITAL) buf[4] = 'I';
            }
            break;
        }
        case MENU_R_CTCS: case MENU_T_CTCS: {
            uint8_t code = (settingId == MENU_R_CTCS) ? gTxVfo->pRX->Code : gTxVfo->pTX->Code;
            uint8_t type = (settingId == MENU_R_CTCS) ? gTxVfo->pRX->CodeType : gTxVfo->pTX->CodeType;
            if (type != CODE_TYPE_CONTINUOUS_TONE) strcpy(buf, "OFF");
            else {
                NUMBER_ToDecimal(buf, CTCSS_Options[code] / 10, 3, false); strcat(buf, ".");
                uint8_t l = strlen(buf); buf[l] = (CTCSS_Options[code] % 10) + '0'; buf[l+1] = '\0'; strcat(buf, "Hz");
            }
            break;
        }
        case MENU_SCR:
            if (gSetting_ScrambleEnable && gTxVfo->SCRAMBLING_TYPE > 0 && gTxVfo->SCRAMBLING_TYPE <= 10) 
                 strcpy(buf, gSubMenu_SCRAMBLER[gTxVfo->SCRAMBLING_TYPE]);
            else strcpy(buf, "OFF");
            break;
        case MENU_COMPAND: strcpy(buf, gSubMenu_RX_TX[gTxVfo->Compander]); break;
        case MENU_TOT: NUMBER_ToDecimal(buf, (gEeprom.TX_TIMEOUT_TIMER + 1) * 15, 3, false); strcat(buf, "s"); break;
        case MENU_AUTOLK:
            if (gEeprom.AUTO_KEYPAD_LOCK) { NUMBER_ToDecimal(buf, gEeprom.AUTO_KEYPAD_LOCK * 15, 3, false); strcat(buf, "s"); }
            else strcpy(buf, "OFF");
            break;
        case MENU_TDR:
            if (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF && gEeprom.DUAL_WATCH == DUAL_WATCH_OFF) strcpy(buf, "OFF");
            else if (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF) strcpy(buf, "CROSS");
            else { strcpy(buf, "CHAN  "); buf[5] = (gEeprom.DUAL_WATCH == DUAL_WATCH_CHAN_A ? 'A' : 'B'); }
            break;
        case MENU_SAVE:
            if (gEeprom.BATTERY_SAVE == 0) strcpy(buf, "OFF");
            else { strcpy(buf, "1:"); NUMBER_ToDecimal(buf + 2, gEeprom.BATTERY_SAVE, 1, false); }
            break;
         case MENU_UPCODE: strncpy(buf, gEeprom.DTMF_UP_CODE, 8); buf[8] = '\0'; break;
         case MENU_DWCODE: strncpy(buf, gEeprom.DTMF_DOWN_CODE, 8); buf[8] = '\0'; break;
#ifdef ENABLE_DTMF_CALLING
         case MENU_ANI_ID: strncpy(buf, gEeprom.ANI_DTMF_ID, 8); buf[8] = '\0'; break;
         case MENU_D_RSP: strcpy(buf, gSubMenu_D_RSP[gEeprom.DTMF_DECODE_RESPONSE]); break;
         case MENU_D_HOLD: NUMBER_ToDecimal(buf, gEeprom.DTMF_auto_reset_time, 2, false); strcat(buf, "s"); break;
         case MENU_D_PRE: NUMBER_ToDecimal(buf, gEeprom.DTMF_PRELOAD_TIME, 4, false); strcat(buf, "ms"); break;
         case MENU_D_DCD: strcpy(buf, gSubMenu_OFF_ON[gTxVfo->DTMF_DECODING_ENABLE]); break;
#endif
#ifdef ENABLE_PASSCODE
        case MENU_PASSCODE: {
            uint8_t len = Passcode_GetLength();
            if (len == 0) strcpy(buf, "OFF");
            else { if (len > 12) len = 12; memset(buf, '*', len); buf[len] = '\0'; }
            break;
        }
        case MENU_PASSCODE_MAX_TRIES: NUMBER_ToDecimal(buf, Passcode_GetMaxTries(), 2, false); break;
        case MENU_PASSCODE_EXPOSE: strcpy(buf, Passcode_GetExposeLength() ? "ON" : "OFF"); break;
        case MENU_PASSCODE_STEALTH: strcpy(buf, Passcode_GetStealthMode() ? "ON" : "OFF"); break;
#endif
        case MENU_BCL: strcpy(buf, gSubMenu_OFF_ON[gTxVfo->BUSY_CHANNEL_LOCK]); break;
        case MENU_TXP: strcpy(buf, gSubMenu_TXP[gTxVfo->OUTPUT_POWER]); break;
#ifdef ENABLE_TX_OFFSET
        case MENU_SFT_D: strcpy(buf, gSubMenu_SFT_D[gTxVfo->TX_OFFSET_FREQUENCY_DIRECTION]); break;
#endif

        case MENU_W_N: strcpy(buf, gSubMenu_W_N[gTxVfo->CHANNEL_BANDWIDTH]); break;
        case MENU_AM:  strcpy(buf, gModulationStr[gTxVfo->Modulation]); break;
#ifdef ENABLE_LIVESEEK
        case MENU_LIVESEEK: strcpy(buf, gSubMenu_LiveSeek[gEeprom.LIVESEEK_MODE]); break;
#endif
        case MENU_SET_NAV: strcpy(buf, gEeprom.SET_NAV ? "K5 (U/D)" : "K1 (L/R)"); break;
        case MENU_PTT_ID: strcpy(buf, gSubMenu_PTT_ID[gTxVfo->DTMF_PTT_ID_TX_MODE]); break;
        case MENU_SET_INV: strcpy(buf, gSetting_set_inv ? "ON" : "OFF"); break;
        case MENU_SET_CTR: NUMBER_ToDecimal(buf, gSetting_set_ctr, 2, false); break;
    }
}

static void Settings_UpdateValue(uint8_t settingId, bool up) {
    const SettingConfig *conf = GetSettingConfig(settingId);
    if (conf) {
        uint8_t *val = (uint8_t *)conf->ptr;
        INC_DEC(*val, conf->min, conf->max, up);
        // Post-update side effects
        if (settingId == MENU_ABR_MAX && gEeprom.BACKLIGHT_MIN >= gEeprom.BACKLIGHT_MAX) gEeprom.BACKLIGHT_MIN = gEeprom.BACKLIGHT_MAX - 1;
        if (settingId == MENU_ABR_MIN && gEeprom.BACKLIGHT_MAX <= gEeprom.BACKLIGHT_MIN) gEeprom.BACKLIGHT_MAX = gEeprom.BACKLIGHT_MIN + 1;
        if (settingId == MENU_SET_CTR || settingId == MENU_SET_INV) ST7565_ContrastAndInv();
        if (settingId == MENU_ROGER) {
            AG_MENU_Render();
            ST7565_BlitFullScreen();
            BK4819_PlayRogerPreview();
        }
        if (settingId == MENU_VOL_GAIN) {
            BK4819_WriteRegister(BK4819_REG_48, 
                (11u << 12)                |     // ??? .. 0 ~ 15, doesn't seem to make any difference
                ( 0u << 10)                |     // AF Rx Gain-1
                (gEeprom.VOLUME_GAIN << 4) |     // AF Rx Gain-2
                (gEeprom.DAC_GAIN    << 0));     // AF DAC Gain (after Gain-1 and Gain-2)
        }
        if (settingId == MENU_MIC_AGC) {
            BK4819_SetMicAGC(gEeprom.MIC_AGC);
        }
    } else {
        switch (settingId) {
            case MENU_VOX:
                if (!gEeprom.VOX_SWITCH) { gEeprom.VOX_SWITCH = true; gEeprom.VOX_LEVEL = up ? 1 : 9; }
                else {
                    if (up) { if (gEeprom.VOX_LEVEL < 9) gEeprom.VOX_LEVEL++; else gEeprom.VOX_SWITCH = false; }
                    else { if (gEeprom.VOX_LEVEL > 1) gEeprom.VOX_LEVEL--; else gEeprom.VOX_SWITCH = false; }
                }
                break;
            case MENU_MIC: INC_DEC(gEeprom.MIC_SENSITIVITY, 0, 4, up); break;
            case MENU_ABR: INC_DEC(gEeprom.BACKLIGHT_TIME, 0, 61, up); if (gEeprom.BACKLIGHT_TIME < 61) BACKLIGHT_TurnOn(); break;
            case MENU_F1SHRT: case MENU_F1LONG: case MENU_F2SHRT: case MENU_F2LONG: case MENU_MLONG: {
                uint8_t *v = (settingId == MENU_F1SHRT) ? &gEeprom.KEY_1_SHORT_PRESS_ACTION :
                             (settingId == MENU_F1LONG) ? &gEeprom.KEY_1_LONG_PRESS_ACTION :
                             (settingId == MENU_F2SHRT) ? &gEeprom.KEY_2_SHORT_PRESS_ACTION :
                             (settingId == MENU_F2LONG) ? &gEeprom.KEY_2_LONG_PRESS_ACTION : &gEeprom.KEY_M_LONG_PRESS_ACTION;
                int idx = 0;
                for(int i=0; i<gSubMenu_SIDEFUNCTIONS_size; i++) if(gSubMenu_SIDEFUNCTIONS[i].id == *v) { idx = i; break; }
                INC_DEC(idx, 0, gSubMenu_SIDEFUNCTIONS_size - 1, up);
                *v = gSubMenu_SIDEFUNCTIONS[idx].id;
                break;
            }
            case MENU_STEP: INC_DEC(gTxVfo->STEP_SETTING, 0, STEP_N_ELEM - 1, up); break;
#ifdef ENABLE_TX_OFFSET
            case MENU_OFFSET: if (up) gTxVfo->TX_OFFSET_FREQUENCY += 10000; else if (gTxVfo->TX_OFFSET_FREQUENCY >= 10000) gTxVfo->TX_OFFSET_FREQUENCY -= 10000; break;
#endif

            case MENU_SCR: INC_DEC(gTxVfo->SCRAMBLING_TYPE, 0, 10, up); gSetting_ScrambleEnable = (gTxVfo->SCRAMBLING_TYPE > 0); break;
            case MENU_R_DCS: case MENU_T_DCS: {
                uint8_t *t = (settingId == MENU_R_DCS) ? &gTxVfo->pRX->CodeType : &gTxVfo->pTX->CodeType;
                uint8_t *c = (settingId == MENU_R_DCS) ? &gTxVfo->pRX->Code : &gTxVfo->pTX->Code;
                if (*t == CODE_TYPE_OFF) { if(up) { *t = CODE_TYPE_DIGITAL; *c = 0; } else { *t = CODE_TYPE_REVERSE_DIGITAL; *c = 103; } }
                else if (*t == CODE_TYPE_DIGITAL) { if (up) { if (*c < 103) (*c)++; else *t = CODE_TYPE_REVERSE_DIGITAL, *c = 0; } else { if (*c > 0) (*c)--; else *t = CODE_TYPE_OFF; } }
                else if (*t == CODE_TYPE_REVERSE_DIGITAL) { if (up) { if (*c < 103) (*c)++; else *t = CODE_TYPE_OFF; } else { if (*c > 0) (*c)--; else *t = CODE_TYPE_DIGITAL, *c = 103; } }
                else { *t = CODE_TYPE_DIGITAL; *c = 0; }
                break;
            }
            case MENU_R_CTCS: case MENU_T_CTCS: {
                uint8_t *t = (settingId == MENU_R_CTCS) ? &gTxVfo->pRX->CodeType : &gTxVfo->pTX->CodeType;
                uint8_t *c = (settingId == MENU_R_CTCS) ? &gTxVfo->pRX->Code : &gTxVfo->pTX->Code;
                if (*t != CODE_TYPE_CONTINUOUS_TONE) { if(up) { *t = CODE_TYPE_CONTINUOUS_TONE; *c = 0; } else { *t = CODE_TYPE_CONTINUOUS_TONE; *c = 49; } }
                else { if (up) { if (*c < 49) (*c)++; else *t = CODE_TYPE_OFF; } else { if (*c > 0) (*c)--; else *t = CODE_TYPE_OFF; } }
                break;
            }
            case MENU_TDR: {
                uint8_t s = (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF) ? 3 : (gEeprom.DUAL_WATCH == DUAL_WATCH_CHAN_B) ? 2 : (gEeprom.DUAL_WATCH == DUAL_WATCH_CHAN_A) ? 1 : 0;
                INC_DEC(s, 0, 3, up);
                gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF; gEeprom.DUAL_WATCH = DUAL_WATCH_OFF;
                if (s == 1) gEeprom.DUAL_WATCH = DUAL_WATCH_CHAN_A; else if (s == 2) gEeprom.DUAL_WATCH = DUAL_WATCH_CHAN_B; else if (s == 3) gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_CHAN_B;
                break;
            }
#ifdef ENABLE_DTMF_CALLING
            case MENU_D_HOLD: INC_DEC(gEeprom.DTMF_auto_reset_time, 5, 60, up); break;
            case MENU_D_PRE: { uint16_t v = gEeprom.DTMF_PRELOAD_TIME / 10; INC_DEC(v, 3, 99, up); gEeprom.DTMF_PRELOAD_TIME = v * 10; break; }
#endif
#ifdef ENABLE_PASSCODE
            case MENU_PASSCODE_MAX_TRIES: { uint8_t v = Passcode_GetMaxTries(); INC_DEC(v, 3, 50, up); Passcode_SetMaxTries(v); break; }
            case MENU_PASSCODE_EXPOSE: Passcode_SetExposeLength(!Passcode_GetExposeLength()); break;
            case MENU_PASSCODE_STEALTH: Passcode_SetStealthMode(!Passcode_GetStealthMode()); break;
#endif
            case MENU_BCL: gTxVfo->BUSY_CHANNEL_LOCK = !gTxVfo->BUSY_CHANNEL_LOCK; break;
            case MENU_TXP: INC_DEC(gTxVfo->OUTPUT_POWER, 0, 2, up); break;
#ifdef ENABLE_TX_OFFSET
            case MENU_SFT_D: INC_DEC(gTxVfo->TX_OFFSET_FREQUENCY_DIRECTION, 0, 2, up); break;
#endif

            case MENU_W_N: INC_DEC(gTxVfo->CHANNEL_BANDWIDTH, 0, 1, up); break;
            case MENU_AM: INC_DEC(gTxVfo->Modulation, 0, MODULATION_UKNOWN-1, up); break;
#ifdef ENABLE_LIVESEEK
            case MENU_LIVESEEK: INC_DEC(gEeprom.LIVESEEK_MODE, 0, 2, up); break;
#endif
            case MENU_SET_NAV: gEeprom.SET_NAV = !gEeprom.SET_NAV; break;
            case MENU_PTT_ID: INC_DEC(gTxVfo->DTMF_PTT_ID_TX_MODE, 0, 3, up); break;
        }
    }
    
    // Live Save
    SETTINGS_SaveSettings();
    SETTINGS_SaveVfoIndices(); 
}


// --- Menu Item Callbacks ---
static void getVal(const MenuItem *item, char *buf, uint8_t buf_size) {
    Settings_GetValueStr(item->setting, buf, buf_size);
}
static void changeVal(const MenuItem *item, bool up) {
    Settings_UpdateValue(item->setting, up);
}

#ifdef ENABLE_EEPROM_HEXDUMP
static bool Action_MemView(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key == KEY_MENU && key_pressed) {
        GUI_SelectNextDisplay(DISPLAY_HEXDUMP);
        return true;
    }
    return false;
}
#endif

#ifdef ENABLE_PASSCODE
static bool Action_Passcode(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key == KEY_MENU && key_pressed) {
        Passcode_Change();
        return true;
    }
    return false;
}
#endif

// --- Menu Item Callbacks ---

static bool s_PasscodeDone = false;
static void s_PasscodeCallback(void) { s_PasscodeDone = true; }

static bool PromptPasscode(void) {
#ifdef ENABLE_PASSCODE
    if (!Passcode_IsSet()) return true;
    
    char buf[33] = {0};
    s_PasscodeDone = false;
    
    bool expose = Passcode_GetExposeLength();
    uint8_t len = Passcode_GetLength();
    uint8_t deb = 0;
    KEY_Code_t k0 = KEY_INVALID;
    KEY_Code_t k1 = KEY_INVALID;
    
    TextInput_InitEx(buf, expose ? len : 32, false, expose, true, false, s_PasscodeCallback);
    
    while(!s_PasscodeDone) {
         ST7565_FillScreen(0x00);
         AG_PrintMediumBoldEx(64, 10, POS_C, C_FILL, "SECURITY CHECK");
         
         TextInput_Tick();
         TextInput_Render();
         
         // Poll
         KEY_Code_t key = KEYBOARD_Poll();
         if (k0 == key) {
             if (deb++ == 2) { // 20ms debounce
                 if (key != KEY_INVALID) TextInput_HandleInput(key, true, false);
             }
         } else {
             deb = 0;
             k0 = key;
         }
         
         if (key == KEY_INVALID && k1 != KEY_INVALID) {
              TextInput_HandleInput(k1, false, false);
              k1 = KEY_INVALID;
         } else if (key != KEY_INVALID) {
              k1 = key;
         }
         
         if (key == KEY_EXIT) {
             TextInput_Deinit();
             return false;
         }
         
         SYSTEM_DelayMs(10);
    }
    
    TextInput_Deinit();
    return Passcode_Validate(buf);
#else
    return true;
#endif
}

static bool Action_FactoryReset(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    if (key == KEY_MENU && key_pressed) {
        // 1. Passcode
        if (!PromptPasscode()) {
             // Failed or Cancelled
             return true; 
        }

        // 2. Selection Loop
        bool resetAll = false;
        KEY_Code_t k0 = KEY_INVALID;
        uint8_t deb = 0;
        
        while(1) {
            ST7565_FillScreen(0x00);
            AG_PrintMediumBoldEx(64, 10, POS_C, C_FILL, "FACTORY RESET");
            
            AG_PrintMediumEx(64, 30, POS_C, C_FILL, resetAll ? "< ALL >" : "< VFO >");
            AG_PrintSmallEx(64, 45, POS_C, C_FILL, resetAll ? "Wipes EVERYTHING" : "Reset Settings Only");
            
            AG_PrintSmallEx(64, 58, POS_C, C_FILL, "MENU: Confirm  EXIT: Cancel");
            
            ST7565_BlitFullScreen();
            
            KEY_Code_t k = KEYBOARD_Poll();
            // Simple debounce/repeat for nav
            if (k0 == k) {
                if (deb++ > 2) {
                    // Holding
                }
            } else {
                deb = 0;
                k0 = k;
                if (k == KEY_UP || k == KEY_DOWN) {
                    resetAll = !resetAll;
                }
                if (k == KEY_MENU) {
                     SETTINGS_FactoryReset(resetAll);
                     NVIC_SystemReset();
                     return true;
                }
                if (k == KEY_EXIT) {
                     return true; 
                }
            }
            SYSTEM_DelayMs(20);
        }
        return true; 
    }
    return false;
}

// --- Menu Definitions ---

// Sound
static const MenuItem soundItems[] = {
    {"Squelch", MENU_SQL, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Key Beep", MENU_BEEP, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    {"Roger", MENU_ROGER, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"VOX", MENU_VOX, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Mic Sens", MENU_MIC, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #ifdef ENABLE_MIC_BAR
    {"Mic Bar", MENU_MIC_BAR, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #endif
    {"Mic AGC", MENU_MIC_AGC, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
#ifdef ENABLE_TX_AUDIO_COMPRESSOR
    {"Mic Compress", MENU_TX_COMPRESSOR, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
#endif
    {"Vol Gain", MENU_VOL_GAIN, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #ifdef ENABLE_VOICE
    {"Voice", MENU_VOICE, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #endif
    {"Tail Tone", MENU_STE, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    {"Repeater Tone", MENU_RP_STE, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    {"1 Call", MENU_1_CALL, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
#ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"Audio Profile", MENU_SET_AUD, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
#endif
    #ifdef ENABLE_ALARM
    {"Alarm", MENU_AL_MOD, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #endif
    #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"Tail Alert",  MENU_SET_EOT, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Beep Timer",    MENU_SET_TMR, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #endif
};
static Menu soundMenu = {
    .title = "Audio", .items = soundItems, .num_items = ARRAY_SIZE(soundItems),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

// Display
static const MenuItem displayItems[] = {
    {"Backlight Time", MENU_ABR, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Backlight Max", MENU_ABR_MAX, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Backlight Min", MENU_ABR_MIN, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Backlight Tx/Rx", MENU_ABR_ON_TX_RX, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    {"Channel Label", MENU_MDF, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Battery Text", MENU_BAT_TXT, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Power On Text", MENU_PONMSG, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"Contrast", MENU_SET_CTR, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Invert", MENU_SET_INV, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #endif
};
static Menu displayMenu = {
    .title = "Display", .items = displayItems, .num_items = ARRAY_SIZE(displayItems),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

// Radio
static const MenuItem radioItems[] = {
    {"Step", MENU_STEP, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Bandwidth", MENU_W_N, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    {"Power", MENU_TXP, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Rx DCS", MENU_R_DCS, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Rx CTCS", MENU_R_CTCS, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Tx DCS", MENU_T_DCS, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Tx CTCS", MENU_T_CTCS, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
#ifdef ENABLE_TX_OFFSET
    {"Offset Dir", MENU_SFT_D, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Offset Freq", MENU_OFFSET, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
#endif
    {"Busy Channel Lock", MENU_BCL, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Modulation", MENU_AM, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Scan Resume", MENU_SC_REV, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Compander", MENU_COMPAND, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
#ifdef ENABLE_SCRAMBLER
    {"Scrambler", MENU_SCR, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
#endif
#ifdef ENABLE_LIVESEEK
    {"LiveSeek", MENU_LIVESEEK, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
#endif
    #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"Tx Lock", MENU_TX_LOCK, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"350 En", MENU_350EN, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    {"Power Logic", MENU_SET_PWR, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #ifdef ENABLE_TX_SOFT_START
    {"Tx Soft Start", MENU_TX_SOFT_START, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #endif
    #ifdef ENABLE_CTCSS_LEAD_IN
    {"CTCSS Lead In", MENU_CTCSS_LEAD, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #endif
    #ifdef ENABLE_NARROWER_BW_FILTER
    {"NFM Filter",  MENU_SET_NFM, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #endif
    #endif
};
static Menu radioMenu = {
    .title = "Radio", .items = radioItems, .num_items = ARRAY_SIZE(radioItems),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

// DTMF
static const MenuItem dtmfItems[] = {
    {"PTT ID", MENU_PTT_ID, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Side Tone", MENU_D_ST, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    {"Live Dec", MENU_D_LIVE_DEC, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #ifdef ENABLE_DTMF_CALLING
    {"ANI ID", MENU_ANI_ID, getVal, NULL, NULL, NULL, M_ITEM_ACTION},
    {"Response", MENU_D_RSP, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Reset Time", MENU_D_HOLD, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Preload", MENU_D_PRE, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Decode", MENU_D_DCD, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #endif
    {"Up Code", MENU_UPCODE, getVal, NULL, NULL, NULL, M_ITEM_ACTION},
    {"Dw Code", MENU_DWCODE, getVal, NULL, NULL, NULL, M_ITEM_ACTION},
};
static Menu dtmfMenu = {
    .title = "DTMF", .items = dtmfItems, .num_items = ARRAY_SIZE(dtmfItems),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

// System
static const MenuItem systemItems[] = {
    {"Tx Timeout", MENU_TOT, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"Tx TO Alert", MENU_SET_TOT, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #endif
    {"Auto Lock", MENU_AUTOLK, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"Lock Mode",    MENU_SET_LCK, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #endif
    {"Dual Watch", MENU_TDR, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Bat Save", MENU_SAVE, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Bat Type", MENU_BATTYP, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #ifdef ENABLE_DEEP_SLEEP_MODE
    {"Deep Sleep",  MENU_SET_OFF, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    #endif
#ifdef ENABLE_PASSCODE
    {"Passcode", MENU_PASSCODE, getVal, NULL, NULL, Action_Passcode, M_ITEM_ACTION},
    {"Max Tries", MENU_PASSCODE_MAX_TRIES, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"Show Length", MENU_PASSCODE_EXPOSE, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    {"Stealth Unlock", MENU_PASSCODE_STEALTH, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
#endif
    #ifdef ENABLE_EEPROM_HEXDUMP
    {"Mem Hex Dump", MENU_MEMVIEW, NULL, NULL, NULL, Action_MemView, M_ITEM_ACTION},
    #endif
    {"Factory Reset", MENU_RESET, NULL, NULL, NULL, Action_FactoryReset, M_ITEM_ACTION},
};
static Menu systemMenu = {
    .title = "System", .items = systemItems, .num_items = ARRAY_SIZE(systemItems),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

// Buttons
static const MenuItem buttonItems[] = {
    #ifdef ENABLE_CUSTOM_FIRMWARE_MODS
    {"Push to Talk", MENU_SET_PTT, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    {"Nav Layout", MENU_SET_NAV, getVal, changeVal, NULL, NULL, M_ITEM_ACTION},
    #endif
    {"F1 Short",    MENU_F1SHRT, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"F1 Long",     MENU_F1LONG, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"F2 Short",    MENU_F2SHRT, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"F2 Long",     MENU_F2LONG, getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
    {"M Long",      MENU_MLONG,  getVal, changeVal, NULL, NULL, M_ITEM_SELECT},
};
static Menu buttonMenu = {
    .title = "Buttons", .items = buttonItems, .num_items = ARRAY_SIZE(buttonItems),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

// Main
static const MenuItem rootItems[] = {
    {"Radio", 0, NULL, NULL, &radioMenu, NULL, M_ITEM_ACTION},
    {"Display", 0, NULL, NULL, &displayMenu, NULL, M_ITEM_ACTION},
    {"Sound", 0, NULL, NULL, &soundMenu, NULL, M_ITEM_ACTION},
    {"Buttons", 0, NULL, NULL, &buttonMenu, NULL, M_ITEM_ACTION},
    {"System", 0, NULL, NULL, &systemMenu, NULL, M_ITEM_ACTION},
    {"DTMF", 0, NULL, NULL, &dtmfMenu, NULL, M_ITEM_ACTION},
};

static Menu rootMenu = {
    .title = "Settings",
    .items = rootItems,
    .num_items = ARRAY_SIZE(rootItems),
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y, .itemHeight = MENU_ITEM_H
};

void SETTINGS_UI_Init(void) {
    AG_MENU_Init(&rootMenu);
}
