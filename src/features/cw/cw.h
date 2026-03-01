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

#ifndef CW_H
#define CW_H

#include <stdint.h>
#include <stdbool.h>
#include "features/storage/storage.h"

#ifdef ENABLE_CW_KEYER

// CW Timing Parameters (~15 WPM)
#define CW_TONE_FREQ_HZ         650     // Standard CW sidetone frequency (RX offset matches this)
#define CW_DOT_MS               80      // Standard dot duration
#define CW_DASH_MS              240     // Standard dash = 3x dot
#define CW_ELEMENT_GAP_MS       80      // Gap between elements = 1 dot
#define CW_CHAR_GAP_MS          240     // Gap between characters = 3 dots
#define CW_WORD_GAP_MS          560     // Gap between words = 7 dots

#define CW_QUEUE_SIZE           256     // Element queue size
#define CW_DECODE_BUF_SIZE      22      // Decoded text buffer (fills one line)
#define CW_ELEMENT_BUF_SIZE     8       // Elements per character for decoder

// Element types in queue
typedef enum {
    CW_ELEM_DIT,
    CW_ELEM_DAH,
    CW_ELEM_GAP,                // Variable gap (uses duration_10ms)
    CW_ELEM_STRAIGHT_START,     // Start straight key tone
    CW_ELEM_STRAIGHT_STOP,      // Stop straight key tone
    CW_ELEM_CHAR_DONE           // UI Marker for macro progress
} CW_Element_t;

// Keyer state
typedef enum {
    CW_STATE_IDLE,              // Not transmitting
    CW_STATE_TX_STARTING,       // TX ramp-up delay (tx_delay_ms)
    CW_STATE_PLAYING_DIT,       // Tone currently playing a Dit
    CW_STATE_PLAYING_DAH,       // Tone currently playing a Dah
    CW_STATE_INTER_ELEMENT_GAP, // Gap between elements
    CW_STATE_STRAIGHT_TONE      // Straight key tone (variable duration)
} CW_State_t;

// CW Module Context
typedef struct {
    CW_State_t      state;
    
    // Element queue
    CW_Element_t    queue[CW_QUEUE_SIZE];
    uint16_t        queueHead;
    uint16_t        queueTail;
    uint16_t        queueCount;
    
    // Current element timing
    uint16_t        timer_10ms;
    uint16_t        duration_10ms;
    
    // Paddle state (for iambic generation)
    struct {
        bool dit_pressed;
        bool dah_pressed;
        bool dit_memory;    // Memory for fast taps
        bool dah_memory;
        bool last_was_dit;  // For iambic alternation
        bool squeezed;      // Squeeze state detection
    } paddle;
    
    // Straight key state
    bool            straightKeyDown;
    uint16_t        straightTimer_10ms;
    
    // Gap timing for decoder
    uint16_t        gapTimer_10ms;
    
    // Adaptive WPM tracking
    uint16_t        avgDotMs;
    uint16_t        avgDashMs;
    uint16_t        rxDotMs;
    
    // Decoder element buffer
    uint8_t         decodeBuf[CW_ELEMENT_BUF_SIZE];
    uint8_t         decodeCount;
    
    // Decoded text output
    char            textBuf[CW_DECODE_BUF_SIZE + 1];
    uint8_t         textLen;
    
    // Live symbol buffer (for display of current . - sequence)
    char            symbolBuf[9];
    uint8_t         symbolLen;
    
    // RX Decode State
    bool            rxSignalOn;
    uint16_t        rxSignalTimer_10ms;
    uint16_t        rxGapTimer_10ms;
    uint16_t        rxGlitchTimer_10ms;
    bool            wasAgcEnabled;

    // EMA Envelope Tracking
    uint16_t        ema_noise_floor;
    uint16_t        ema_signal_peak;
    uint16_t        signal_level;
    uint16_t        dynamic_threshold;

    // Debugging / UI Stats
    bool            debug;
    uint16_t        lastRSSI;
    uint8_t         lastGlitch;
    uint8_t         lastNoise;

    // Macro Progress Tracking
    int8_t          activeMacroIndex; // -1: None, 0: CQ, 1..8: Custom
    uint8_t         macroLen;
    uint8_t         macroPos;
} CW_Context_t;

extern CW_Context_t gCW;

// Main API
void CW_Init(void);
void CW_Tick10ms(void);         // Called every 10ms

// Key inputs (state based for iambic)
void CW_StraightKeyDown(void);
void CW_StraightKeyUp(void);
void CW_KeyString(const char *str);
void CW_KeyMacro(int8_t index);

// Query
bool CW_IsBusy(void);           // Is there work to do?
const char* CW_GetDecodedText(void);
const char* CW_GetSymbolBuffer(void);
void CW_ClearDecoded(void);
void CW_GetCQMessage(char *buf);

// Global Settings
extern CWSettings_t gCWSettings;
void CW_LoadSettings(void);
void CW_SaveSettings(void);

// UI Render
void UI_DisplayCW_Center(uint8_t line);
void UI_DisplayCW_VFO(uint8_t vfo_num, uint8_t line);

// Hardware Interface (Internal to CW module/Radio transitions)
void CW_SetupTXHardware(void);
void CW_StopTXHardware(void);
void CW_KeyHardware(bool down);

#endif // ENABLE_CW_KEYER

#endif // CW_H
