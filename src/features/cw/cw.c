/* Copyright 2025 deltafw
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

#include "features/cw/cw.h"

#ifdef ENABLE_CW_KEYER

#include <string.h>
#include "features/app/app.h"
#include "drivers/bsp/bk4819.h"
#include "drivers/bsp/gpio.h"
#include "drivers/bsp/system.h"
#include "features/audio/audio.h"
#include "features/radio/radio.h"
#include "features/radio/functions.h"
#include "core/misc.h"
#include "features/storage/storage.h"
#include "apps/settings/settings.h"
#include "drivers/bsp/st7565.h"
#include "ui/helper.h"
#include "ui/main.h"

extern bool gUpdateDisplay;
extern volatile uint16_t gFlashLightBlinkCounter;

// Global CW context
CW_Context_t gCW;
CWSettings_t gCWSettings;

static uint16_t gCW_HangTimer_10ms = 0;
#define CW_HANG_TIME_MS 250

void CW_LoadSettings(void) {
    Storage_ReadRecord(REC_CW_SETTINGS, &gCWSettings, 0, sizeof(CWSettings_t));
    if (gCWSettings.fields.Magic != 0x4357) {
        memset(&gCWSettings, 0, sizeof(CWSettings_t));
        gCWSettings.fields.Magic = 0x4357;
        gCWSettings.fields.WPM = 20;
        gCWSettings.fields.ToneFreqHz = 650;
        gCWSettings.fields.DecodeMode = 3; // TX/RX
        gCWSettings.fields.AutoSpacing = 1;
        gCWSettings.fields.KeyerMode = 2; // Iambic B
        gCWSettings.fields.Weighting = 30; // 3.0 ratio
        gCWSettings.fields.TxDelayMs = 0;
        gCWSettings.fields.FarnsworthWPM = 0;
        strcpy(gCWSettings.fields.Callsign, "N0CALL");
        CW_SaveSettings();
    }
}

void CW_SaveSettings(void) {
    Storage_WriteRecord(REC_CW_SETTINGS, &gCWSettings, 0, sizeof(CWSettings_t));
    Storage_Commit(REC_CW_SETTINGS);
}

void CW_SetupTXHardware(void) {
    RADIO_SetModulation(MODULATION_FM);
    RADIO_SetTxParameters();
    
    BK4819_SetAF(BK4819_AF_MUTE);
    BK4819_WriteRegister(BK4819_REG_40, 0x0000);
    
    BK4819_WriteRegister(BK4819_REG_30, 0);
    BK4819_WriteRegister(BK4819_REG_30, 
        BK4819_REG_30_ENABLE_AF_DAC | 
        BK4819_REG_30_ENABLE_DISC_MODE | 
        BK4819_REG_30_ENABLE_TX_DSP); 

    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
}

void CW_StopTXHardware(void) {
    BK4819_WriteRegister(BK4819_REG_70, 0x0000); // Stop tone
    
    AUDIO_AudioPathOff();
    gEnableSpeaker = false;

    RADIO_SetupRegisters(true);
    RADIO_SetModulation(gCurrentVfo->Modulation); 
}

void CW_KeyHardware(bool down) {
    if (down) {
        BK4819_TransmitTone(true, gCWSettings.fields.ToneFreqHz);
        SYSTEM_DelayMs(2); // Wait for tone to stabilize
        uint16_t reg30 = BK4819_ReadRegister(BK4819_REG_30);
        BK4819_WriteRegister(BK4819_REG_30, reg30 | BK4819_REG_30_ENABLE_PA_GAIN);
    } else {
        uint16_t reg30 = BK4819_ReadRegister(BK4819_REG_30);
        BK4819_WriteRegister(BK4819_REG_30, reg30 & ~BK4819_REG_30_ENABLE_PA_GAIN);
        SYSTEM_DelayMs(2); // Wait for PA to drop
        BK4819_WriteRegister(BK4819_REG_70, 0x0000); // Stop sidetone
    }
}

// Morse lookup table
static const struct {
    char character;
    const char *pattern;
} MorseTable[] = {
    {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."},
    {'F', "..-."}, {'G', "--."}, {'H', "...."}, {'I', ".."}, {'J', ".---"},
    {'K', "-.-"}, {'L', ".-.."}, {'M', "--"}, {'N', "-."}, {'O', "---"},
    {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
    {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, {'Y', "-.--"},
    {'Z', "--.."}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
    {'8', "---.."}, {'9', "----."}, {'0', "-----"},
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'/', "-..-."},
    {'-', "-....-"}, {'(', "-.--."}, {')', "-.--.-"}, {':', "---..."},
    {'=', "-...-"}, {'+', ".-.-."}, {'@', ".--.-."},
    {0, NULL}
};

// Internal: Decode elements to character
static char CW_DecodeElements(void)
{
    if (gCW.decodeCount == 0 || gCW.decodeCount > CW_ELEMENT_BUF_SIZE) return '?';
    
    char pattern[8];
    for (uint8_t i = 0; i < gCW.decodeCount; i++) {
        pattern[i] = gCW.decodeBuf[i] ? '-' : '.';
    }
    pattern[gCW.decodeCount] = '\0';
    
    for (int i = 0; MorseTable[i].character != 0; i++) {
        if (strcmp(pattern, MorseTable[i].pattern) == 0) {
            return MorseTable[i].character;
        }
    }
    return '?';
}

// Internal: Add decoded character to text buffer
static void CW_AddDecodedChar(char c)
{
    // Clear symbol buffer when character is decoded
    gCW.symbolLen = 0;
    gCW.symbolBuf[0] = '\0';
    
    if (c == 0) return;
    
    if (gCW.textLen >= CW_DECODE_BUF_SIZE) {
        memmove(gCW.textBuf, gCW.textBuf + 1, CW_DECODE_BUF_SIZE - 1);
        gCW.textLen--;
    }
    gCW.textBuf[gCW.textLen++] = c;
    gCW.textBuf[gCW.textLen] = '\0';
    gUpdateDisplay = true;
}

// Internal: Add symbol to live buffer (e.g. '.' or '-')
static void CW_AddSymbol(char s)
{
    if (gCW.symbolLen >= 8) return; 
    gCW.symbolBuf[gCW.symbolLen++] = s;
    gCW.symbolBuf[gCW.symbolLen] = '\0';
    gUpdateDisplay = true;
}

#include "drivers/bsp/system.h"

static void CW_ToneOn(void) { 
    // QSK TX keying
    CW_KeyHardware(true);
    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    
    // Software audio shaping (Attack)
    // Register 48 is AF Volume
    for (int vol = 0; vol <= 15; vol++) {
        BK4819_WriteRegister(BK4819_REG_48, (vol << 4) | vol);
        SYSTEM_DelayMs(1);
    }
}

static void CW_ToneOff(void) { 
    // Software audio shaping (Decay)
    for (int vol = 15; vol >= 0; vol--) {
        BK4819_WriteRegister(BK4819_REG_48, (vol << 4) | vol);
        SYSTEM_DelayMs(1);
    }
    
    // Restore normal volume level
    BK4819_WriteRegister(BK4819_REG_48, (gEeprom.VOLUME_GAIN << 4) | gEeprom.VOLUME_GAIN);
    
    // QSK RX keying
    CW_KeyHardware(false);
    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
}

static void CW_StartTX(void)
{
    gCW.state = CW_STATE_TX_STARTING;
    gCW.timer_10ms = 2;  
    gCW_HangTimer_10ms = 0;
    FUNCTION_Select(FUNCTION_TRANSMIT);
    CW_SetupTXHardware();
}

static void CW_StopTX(void)
{
    CW_StopTXHardware();
    gCW.state = CW_STATE_IDLE;
    FUNCTION_Select(FUNCTION_FOREGROUND);
}



static bool CW_QueuePush(CW_Element_t elem)
{
    if (gCW.queueCount >= CW_QUEUE_SIZE) return false;
    gCW.queue[gCW.queueTail] = elem;
    gCW.queueTail = (gCW.queueTail + 1) % CW_QUEUE_SIZE;
    gCW.queueCount++;
    return true;
}

static bool CW_QueuePop(CW_Element_t *elem)
{
    if (gCW.queueCount == 0) return false;
    *elem = gCW.queue[gCW.queueHead];
    gCW.queueHead = (gCW.queueHead + 1) % CW_QUEUE_SIZE;
    gCW.queueCount--;
    return true;
}

void CW_Init(void)
{
    CW_LoadSettings();
    memset(&gCW, 0, sizeof(gCW));
    gCW.state = CW_STATE_IDLE;
    
    // Convert WPM to ms
    uint16_t wpmToDotMs = 1200 / gCWSettings.fields.WPM;
    gCW.avgDotMs = wpmToDotMs;  
    gCW.avgDashMs = wpmToDotMs * 3;
    
    gCW.ema_noise_floor = 0;
    gCW.ema_signal_peak = 0;
    
    gCW.debug = true;
}

void CW_StraightKeyDown(void) { gCW.straightKeyDown = true; CW_QueuePush(CW_ELEM_STRAIGHT_START); }
void CW_StraightKeyUp(void) { gCW.straightKeyDown = false; CW_QueuePush(CW_ELEM_STRAIGHT_STOP); }

void CW_KeyString(const char *str)
{
    if (str == NULL) return;
    
    // UI progress indication: we count characters, not elements
    gCW.macroLen = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] != ' ') gCW.macroLen++;
    }
    gCW.macroPos = 0;
    
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c >= 'a' && c <= 'z') c -= 32; // Uppercase
        
        if (c == ' ') {
            // Word gap (7 dots total). 
            // Previous char ended with 1 dot gap. 
            // Pushing 6 more dots.
            for (int k = 0; k < 6; k++) CW_QueuePush(CW_ELEM_GAP);
            continue;
        }
        
        // Find in MorseTable
        for (int j = 0; MorseTable[j].character != 0; j++) {
            if (MorseTable[j].character == c) {
                const char *p = MorseTable[j].pattern;
                while (*p) {
                    CW_QueuePush((*p == '-') ? CW_ELEM_DAH : CW_ELEM_DIT);
                    p++;
                }
                // Char gap (3 dots total). 
                // Last bit had 1 dot gap. 
                // Pushing 2 more dots.
                CW_QueuePush(CW_ELEM_GAP);
                CW_QueuePush(CW_ELEM_GAP);
                break;
            }
        }
        
        // We push a special marker to increment the macro pos for UI drawing
        CW_QueuePush(CW_ELEM_CHAR_DONE);
    }
}

// Calculates lengths based on Weighting (default 30 = 3.0 ratio)
static void CW_UpdateTiming(void) {
    uint8_t wpm = gCWSettings.fields.WPM;
    if (wpm < 5) wpm = 5;
    if (wpm > 60) wpm = 60;
    
    if (gCWSettings.fields.Weighting < 20 || gCWSettings.fields.Weighting > 50) {
        gCWSettings.fields.Weighting = 30; // Enforce default if corrupted
    }
    
    uint16_t dotMs = 1200 / wpm; // Standard formula
    gCW.avgDotMs = dotMs;
    // We only use the calculated dash in TX, RX adapts
}

static void CW_ProcessPaddles(void)
{
    // Poll hardware pins directly to bypass keyboard matrix limitations
    uint32_t rows = LL_GPIO_ReadInputPort(GPIOB);
    gCW.paddle.dit_pressed = !(rows & LL_GPIO_PIN_15); 
    gCW.paddle.dah_pressed = !(rows & LL_GPIO_PIN_14);

    // Read raw inputs and account for Reverse Paddles
    bool dit_in = gCWSettings.fields.ReversePaddles ? gCW.paddle.dah_pressed : gCW.paddle.dit_pressed;
    bool dah_in = gCWSettings.fields.ReversePaddles ? gCW.paddle.dit_pressed : gCW.paddle.dah_pressed;
    
    // Store in memory if pressed (cleared by state machine once executed)
    if (dit_in) gCW.paddle.dit_memory = true;
    if (dah_in) gCW.paddle.dah_memory = true;
    
    // Squeeze detection (both pressed)
    gCW.paddle.squeezed = (dit_in && dah_in);
}

bool gCW_SquelchOpen = false;
static uint16_t gCW_SquelchOpenTimer_10ms = 0;

void CW_Tick10ms(void)
{
    // AGC Management: Disable for CW to get sharp R/A drops, restore otherwise.
    bool inCwMode = (gRxVfo->Modulation == MODULATION_CW);
    bool isRxOrForeground = (FUNCTION_IsRx() || gCurrentFunction == FUNCTION_FOREGROUND);
    
    if (inCwMode && isRxOrForeground) {
        if (!gCW.wasAgcEnabled) { // Borrowing this as "is AGC currently disabled by us"
            BK4819_SetAGC(false);
            gCW.wasAgcEnabled = true;
        }
    } else if (gCW.wasAgcEnabled) {
        BK4819_SetAGC(true);
        gCW.wasAgcEnabled = false;
    }

    if (gCW.state == CW_STATE_IDLE && isRxOrForeground && inCwMode) {
        uint16_t rssi = BK4819_GetRSSI();
        uint8_t glitch = BK4819_GetGlitchIndicator();
        uint8_t noise = BK4819_GetExNoiseIndicator();
        
        gCW.lastRSSI = rssi;
        gCW.lastGlitch = glitch;
        gCW.lastNoise = noise;
        gUpdateDisplay = true; 

        // Synthetic Signal Level: Higher RSSI is good, higher Noise/Glitch is bad.
        // Weighting: Noise drops significantly when carrier is present.
        int32_t sig = (int32_t)rssi - ((int32_t)noise * 2) - ((int32_t)glitch * 1);
        if (sig < 0) sig = 0;
        
        gCW.signal_level = (uint16_t)sig;

        // Initialization / Fast-tracker
        bool startup = (gCW.rxSignalTimer_10ms == 0 && gCW.rxGapTimer_10ms < 200);
        uint16_t alpha = startup ? 4 : 32; // EMA smoothing factor (1/alpha)

        // EMA Envelope Tracking
        if (!gCW.rxSignalOn) {
            // Track noise floor when signal is OFF
            if (gCW.ema_noise_floor == 0) gCW.ema_noise_floor = gCW.signal_level;
            gCW.ema_noise_floor = gCW.ema_noise_floor + ((int32_t)gCW.signal_level - gCW.ema_noise_floor) / alpha;
            
            // Slowly leak signal peak down to noise floor when no signal
            if (gCW.ema_signal_peak > gCW.ema_noise_floor) {
                gCW.ema_signal_peak--;
            }
        } else {
            // Track signal peak when signal is ON
            if (gCW.signal_level > gCW.ema_signal_peak) {
                // Fast attack
                gCW.ema_signal_peak = gCW.signal_level;
            } else {
                // Slow decay to follow fading
                gCW.ema_signal_peak = gCW.ema_signal_peak + ((int32_t)gCW.signal_level - gCW.ema_signal_peak) / 128;
            }
        }

        // Safety margins
        if (gCW.ema_signal_peak < gCW.ema_noise_floor + 20) {
            gCW.ema_signal_peak = gCW.ema_noise_floor + 20;
        }

        // Dynamic Thresholding with Hysteresis
        gCW.dynamic_threshold = gCW.ema_noise_floor + (gCW.ema_signal_peak - gCW.ema_noise_floor) / 2;
        uint16_t turn_on_thresh  = gCW.dynamic_threshold + 5;
        uint16_t turn_off_thresh = gCW.dynamic_threshold - 5;

        bool signalDetected = false;
        if (!gCW.rxSignalOn && gCW.signal_level > turn_on_thresh) {
            signalDetected = true;
        } else if (gCW.rxSignalOn && gCW.signal_level > turn_off_thresh) {
            signalDetected = true;
        }

        // Glitch Filter / Debouncer
        if (signalDetected != gCW.rxSignalOn) {
            gCW.rxGlitchTimer_10ms++;
            // Make filter far more aggressive for live tracking
            uint16_t glitchLimit = signalDetected ? 1 : 2; 
            
            if (gCW.rxGlitchTimer_10ms >= glitchLimit || startup) { 
                gCW.rxSignalOn = signalDetected;
                gCW.rxGlitchTimer_10ms = 0;
                
                if (gCW.rxSignalOn) {
                    gCW.rxSignalTimer_10ms = 0;
                    gCW.rxGapTimer_10ms = 0;
                    // Inject a 'dot' on the rising edge
                    CW_AddSymbol('.');
                } else {
                    // FALLING EDGE: Classify element
                    // Subtract the glitch release time
                    uint16_t duration_10ms = (gCW.rxSignalTimer_10ms > glitchLimit) ? (gCW.rxSignalTimer_10ms - glitchLimit) : 0;
                    uint16_t ms = duration_10ms * 10;
                    
                    if (ms >= 10 && ms < 1500) {
                        uint16_t rxDashMs = (gCW.avgDotMs * gCWSettings.fields.Weighting) / 10;
                        uint16_t distDot = (ms > gCW.avgDotMs) ? (ms - gCW.avgDotMs) : (gCW.avgDotMs - ms);
                        uint16_t distDash = (ms > rxDashMs) ? (ms - rxDashMs) : (rxDashMs - ms);
                        
                        // Hysteresis towards dash if it's borderline
                        bool isDah = (distDash < (distDot * 2)); 

                        if (gCW.decodeCount < CW_ELEMENT_BUF_SIZE) 
                            gCW.decodeBuf[gCW.decodeCount++] = isDah ? 1 : 0;
                    }
                    gCW.rxGapTimer_10ms = 0;
                }
            }
        } else {
            gCW.rxGlitchTimer_10ms = 0;
        }

        // Squelch Gating removed as per user request to use standard squelch
        
        if (gCW.rxSignalOn) {
            gCW.rxSignalTimer_10ms++;
            
            // Live symbol injection: if a tone holds longer than a dit length, change the live character to a dash
            if (gCW.rxSignalTimer_10ms * 10 > gCW.avgDotMs) {
                if (gCW.symbolLen > 0 && gCW.symbolBuf[gCW.symbolLen - 1] == '.') {
                    gCW.symbolBuf[gCW.symbolLen - 1] = '-';
                    gUpdateDisplay = true;
                }
            }
        } else {
            // Gap Processing
            gCW.rxGapTimer_10ms++;
            uint16_t gapMs = gCW.rxGapTimer_10ms * 10;
            uint16_t dotLen = gCW.avgDotMs;
            
            // Standard spacing: 3 dots between chars, 7 dots between words.
            // Pushing the thresholds slightly wider to prevent breaking chars too early on sloppy manual CW
            if (gapMs >= (dotLen * 4 + 20)) { 
                if (gCW.decodeCount > 0) {
                    CW_AddDecodedChar(CW_DecodeElements());
                    gCW.decodeCount = 0;
                }
            }
            if (gapMs >= (dotLen * 9)) {
                if (gCW.textLen > 0 && gCW.textBuf[gCW.textLen-1] != ' ') {
                    CW_AddDecodedChar(' ');
                }
                gCW.rxGapTimer_10ms = 0; 
            }
        }
    }

    // -------------------------------------------------------------------------
    // TX Processing
    // -------------------------------------------------------------------------
    // TX Processing
    // -------------------------------------------------------------------------
    CW_UpdateTiming();
    CW_ProcessPaddles();

    bool dit_in = gCWSettings.fields.ReversePaddles ? gCW.paddle.dah_pressed : gCW.paddle.dit_pressed;
    bool dah_in = gCWSettings.fields.ReversePaddles ? gCW.paddle.dit_pressed : gCW.paddle.dah_pressed;

    if (gCW.state == CW_STATE_IDLE && (gCW.paddle.dit_memory || gCW.paddle.dah_memory || gCW.queueCount > 0)) {
        CW_StartTX();
        return;
    }
    
    switch (gCW.state) {
        case CW_STATE_TX_STARTING:
            if (gCW.timer_10ms > 0) gCW.timer_10ms--;
            else {
                gCW.state = CW_STATE_INTER_ELEMENT_GAP; 
                gCW.timer_10ms = 0;
                gCW.duration_10ms = 0;
            }
            break;
            
        case CW_STATE_PLAYING_DIT:
        case CW_STATE_PLAYING_DAH:
            gCW.timer_10ms++;
            if (gCW.timer_10ms >= gCW.duration_10ms) {
                CW_ToneOff();
                gCW.state = CW_STATE_INTER_ELEMENT_GAP;
                gCW.timer_10ms = 0;
                
                // Gap is generally 1 dit length
                uint16_t gap_ms = gCW.avgDotMs;
                // Apply Farnsworth spacing if enabled
                if (gCWSettings.fields.FarnsworthWPM > 0 && gCWSettings.fields.FarnsworthWPM < gCWSettings.fields.WPM) {
                    gap_ms = 1200 / gCWSettings.fields.FarnsworthWPM;
                }
                gCW.duration_10ms = gap_ms / 10;
                
                // Mode B Logic: Read memory during the element. If squeezed, inject the opposite element memory.
                uint8_t mode = gCWSettings.fields.KeyerMode;
                if (mode == 2 && gCW.paddle.squeezed) { // Iambic B
                    if (gCW.state == CW_STATE_PLAYING_DIT) gCW.paddle.dah_memory = true;
                    if (gCW.state == CW_STATE_PLAYING_DAH) gCW.paddle.dit_memory = true;
                }
            }
            break;
            
        case CW_STATE_INTER_ELEMENT_GAP:
            gCW.timer_10ms++;
            gCW.gapTimer_10ms++;
            if (gCW.timer_10ms >= gCW.duration_10ms) {
                bool playDit = false;
                bool playDah = false;
                uint8_t mode = gCWSettings.fields.KeyerMode;

                if (gCW.paddle.dit_memory && gCW.paddle.dah_memory) {
                    if (mode == 3) { // Ultimatic: Whichever was pressed last takes priority
                         // In a simple loop, we just alternate for now if both are in memory
                         if (gCW.paddle.last_was_dit) playDah = true; else playDit = true;
                    } else if (mode == 4) { // Bug: Dahs are straight, Dits are auto
                        // In Bug mode, Dah is actually a straight key. We handle this below.
                        playDah = true;
                    } else { // Iambic A & B
                        if (gCW.paddle.last_was_dit) playDah = true; 
                        else playDit = true;
                    }
                }
                else if (gCW.paddle.dit_memory) playDit = true;
                else if (gCW.paddle.dah_memory) playDah = true;
                else {
                    // Check software queue if paddles are empty
                    CW_Element_t elem;
                    while (CW_QueuePop(&elem)) {
                        if (elem == CW_ELEM_DIT) { playDit = true; break; }
                        else if (elem == CW_ELEM_DAH) { playDah = true; break; }
                        else if (elem == CW_ELEM_GAP) {
                            gCW.duration_10ms = gCW.avgDotMs / 10; 
                            gCW.timer_10ms = 0;
                            return;
                        }
                        else if (elem == CW_ELEM_STRAIGHT_START) {
                            CW_ToneOn();
                            gCW.state = CW_STATE_STRAIGHT_TONE;
                            gCW.straightTimer_10ms = 0;
                            return;
                        }
                        else if (elem == CW_ELEM_CHAR_DONE) {
                            gCW.macroPos++;
                            gUpdateDisplay = true;
                            // Loop continues to pop the next actual element or empty
                        }
                    }
                    if (!playDit && !playDah) {
                        // Queue empty, active macro finished
                        gCW.activeMacroIndex = -1;
                        gUpdateDisplay = true;
                    }
                }

                if (playDit) {
                    gCW.paddle.dit_memory = false; 
                    gCW.paddle.last_was_dit = true;
                    CW_ToneOn(); 
                    gCW.state = CW_STATE_PLAYING_DIT;
                    gCW.duration_10ms = gCW.avgDotMs / 10; 
                    gCW.timer_10ms = 0; 
                    gCW.gapTimer_10ms = 0;
                    if (gCW.decodeCount < CW_ELEMENT_BUF_SIZE) gCW.decodeBuf[gCW.decodeCount++] = 0;
                    CW_AddSymbol('.');
                } else if (playDah) {
                    gCW.paddle.dah_memory = false; 
                    gCW.paddle.last_was_dit = false;
                    CW_ToneOn(); 
                    gCW.state = CW_STATE_PLAYING_DAH;
                    uint16_t dashMs = (gCW.avgDotMs * gCWSettings.fields.Weighting) / 10;
                    
                    // Bug logic: If it's a Dah, and they are holding the Dah paddle, tone plays continuously
                    if (mode == 4 && dah_in) {
                        gCW.state = CW_STATE_STRAIGHT_TONE;
                        gCW.straightTimer_10ms = 0;
                    } else {
                        gCW.duration_10ms = dashMs / 10;
                    }
                    gCW.timer_10ms = 0; 
                    gCW.gapTimer_10ms = 0;
                    if (gCW.decodeCount < CW_ELEMENT_BUF_SIZE) gCW.decodeBuf[gCW.decodeCount++] = 1;
                    CW_AddSymbol('-');
                } else {
                    gCW_HangTimer_10ms = 0;
                    gCW.state = CW_STATE_IDLE;
                }
            }
            break;
            
        case CW_STATE_IDLE:
            if (gCW.decodeCount > 0 || gCW.textLen > 0) { // Keep gap timer running if we have elements to flush
                gCW.gapTimer_10ms++;
                uint16_t gapMs = gCW.gapTimer_10ms * 10;
                uint16_t dotLen = gCW.avgDotMs;
                
                // Use 4 dots for strict TX char pause, 7 dots for word space
                if (gapMs >= (dotLen * 4)) { 
                    if (gCW.decodeCount > 0) { CW_AddDecodedChar(CW_DecodeElements()); gCW.decodeCount = 0; }
                }
                if (gapMs >= (dotLen * 7)) {
                    if (gCW.textLen > 0 && gCW.textBuf[gCW.textLen-1] != ' ') CW_AddDecodedChar(' '); // Append space
                    gCW.gapTimer_10ms = 0;
                }
            } else {
                gCW.gapTimer_10ms = 0;
            }
            
            gCW_HangTimer_10ms++;
            if (gCW_HangTimer_10ms * 10 >= CW_HANG_TIME_MS && gCW.queueCount == 0 && !dit_in && !dah_in && !gCW.straightKeyDown) {
                CW_StopTX();
            }
            break;
            
        case CW_STATE_STRAIGHT_TONE:
            gCW.straightTimer_10ms++;
            
            // Software Debounce: Enforce a minimum 20ms hold (timer >= 2)
            if (gCW.straightTimer_10ms < 2) {
                break;
            }
            
            // Check if queue forced a stop OR if the hardware straight key (Bug Dah or PTT) was released
            bool hwStop = (gCWSettings.fields.KeyerMode == 4 && !dah_in) || (!gCW.straightKeyDown && gCW.queueCount == 0);
            bool queueStop = (gCW.queueCount > 0 && gCW.queue[gCW.queueHead] == CW_ELEM_STRAIGHT_STOP);
            
            if (queueStop || hwStop) {
                if (queueStop) {
                    CW_Element_t dummy; CW_QueuePop(&dummy);
                }
                CW_ToneOff();
                uint16_t ms = gCW.straightTimer_10ms * 10;
                bool isDah = ms >= gCW.avgDotMs * 2;
                
                // Decode straight key elements (Bug, PTT, or Macros)
                if (gCW.decodeCount < CW_ELEMENT_BUF_SIZE) gCW.decodeBuf[gCW.decodeCount++] = isDah ? 1 : 0;
                CW_AddSymbol(isDah ? '-' : '.');
                
                gCW.state = CW_STATE_INTER_ELEMENT_GAP; 
                gCW.timer_10ms = 0; 
                gCW.duration_10ms = gCW.avgDotMs / 10; 
                gCW.gapTimer_10ms = 0;
            }
            break;
            
        default: break;
    }
}

bool CW_IsBusy(void) { return gCW.state != CW_STATE_IDLE || gCW.queueCount > 0; }
const char* CW_GetDecodedText(void) { return gCW.textBuf; }
const char* CW_GetSymbolBuffer(void) { return gCW.symbolBuf; }
void CW_ClearDecoded(void) { gCW.textLen = 0; gCW.textBuf[0] = '\0'; gCW.symbolLen = 0; gCW.symbolBuf[0] = '\0'; }

void UI_DisplayCW_Center(uint8_t line)
{
    const char *decoded = CW_GetDecodedText();
    bool showCursor = (gFlashLightBlinkCounter % 40) < 20;
    
    // Clear only the part of the line where we draw decoded text
    memset(gFrameBuffer[line], 0, LCD_WIDTH);
    
    int maxDecChars = (120 - 8) / 6;
    if (maxDecChars < 0) maxDecChars = 0;
    int decLen = decoded ? strlen(decoded) : 0;
    const char *decStart = (decLen > maxDecChars) ? (decoded + decLen - maxDecChars) : decoded;
    UI_PrintStringSmallNormal(decStart, 4, 0, line);
    if (showCursor) UI_PrintStringSmallNormal("\x7F", 4 + (strlen(decStart) * 6), 0, line);
    
    ST7565_BlitLine(line);
}

void UI_DisplayCW_VFO(uint8_t vfo_num, uint8_t line)
{
    const char *symbols = CW_GetSymbolBuffer();
    int symLen = symbols[0] ? strlen(symbols) : 0;
    
    // Draw Live Symbols on the Attribute Line on the far right
    if (symLen) {
        int symWidth = (symLen + 1) * 4; // > is 4px, each sym is 4px
        char prompt[16]; prompt[0] = '>'; strcpy(prompt + 1, symbols);
        // Ensure no overlap with modulation text (drawn on a different line anyway)
        for (int i = 114 - symWidth; i < 115; i++) gFrameBuffer[line][i] = 0; 
        UI_PrintStringSmallest(prompt, 114 - symWidth, line * 8 + 1, false, true);
    }

    char cw_stat[32];
    const char *mode_str = "STR8";
    switch (gCWSettings.fields.KeyerMode) {
        case 1: mode_str = "IAM-A"; break;
        case 2: mode_str = "IAM-B"; break;
        case 3: mode_str = "ULTI";  break;
        case 4: mode_str = "BUG";   break;
    }
    
    int16_t dbm = (gCW.lastRSSI / 2) - 160;
    strcpy(cw_stat, mode_str);
    strcat(cw_stat, " ");
    
    char num[8];
    if (dbm < 0) {
        strcat(cw_stat, "-");
        NUMBER_ToDecimal(num, -dbm, 1, false);
    } else {
        NUMBER_ToDecimal(num, dbm, 1, false);
    }
    char *p = num; while (*p == ' ') p++;
    strcat(cw_stat, p);
    strcat(cw_stat, "dBm ");
    
    NUMBER_ToDecimal(num, gCWSettings.fields.WPM, 2, false);
    p = num; while (*p == ' ') p++;
    strcat(cw_stat, p);
    strcat(cw_stat, "WPM");
    
    UI_PrintStringSmallest(cw_stat, 6, line * 8 + 1, false, true);
}

void CW_GetCQMessage(char *buf) {
    strcpy(buf, "CQ CQ CQ DE ");
    strcat(buf, gCWSettings.fields.Callsign);
    strcat(buf, " K");
}

void CW_KeyMacro(int8_t index) {
    gCW.activeMacroIndex = index;
    gUpdateDisplay = true;

    if (index == 0) {
        char buf[32];
        CW_GetCQMessage(buf);
        CW_KeyString(buf);
    } else if (index >= 1 && index <= 8) {
        CW_KeyString(gCWSettings.fields.Macros[index - 1]);
    }
}

#endif // ENABLE_CW_KEYER
