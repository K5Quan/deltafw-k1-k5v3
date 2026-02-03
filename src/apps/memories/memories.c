#include <string.h>
#include <stdio.h>

#include "apps/memories/memories.h"
#include "ui/ag_menu.h"
#include "ui/ag_graphics.h"
#include "ui/textinput.h"
#include "ui/freqinput.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "radio.h"
#include "apps/settings/settings.h"
#include "drivers/bsp/st7565.h"
#include "core/misc.h"
#include "external/printf/printf.h"
#include "dcs.h"
#include "audio.h"



// =============================================================================
// Mode State Machine
// =============================================================================

typedef enum {
    MEM_MODE_LIST,       // Main channel list
    MEM_MODE_DETAIL,     // Channel detail menu
    MEM_MODE_RENAME,     // Text input for rename
    MEM_MODE_RX_FREQ,    // Frequency input for RX
    MEM_MODE_TX_OFFSET   // Frequency input for TX offset
} MemMode;

static MemMode currentMode = MEM_MODE_LIST;
static uint16_t detailChannelIndex = 0;
static char detailTitle[20];
static char editBuffer[17];
static bool ignoreNextMenuRelease = true;

// Numeric quick-jump state
static char searchDigits[4] = {0};  // Up to 3 digits + null
static uint8_t searchDigitCount = 0;
static char listTitle[20] = "Memories";

// Filtered channel list for search mode
static uint16_t filteredChannels[MR_CHANNEL_LAST + 1];
static uint16_t filteredCount = 0;
static bool searchActive = false;

// Working copy of channel data
static VFO_Info_t editChannel;

// =============================================================================
// Forward Declarations
// =============================================================================

static void Memories_RenderItem(uint16_t index, uint8_t visIndex);
static bool Memories_Action(uint16_t index, KEY_Code_t key, bool key_pressed, bool key_held);
static void LoadChannelData(uint16_t index);
static void SaveChannelData(void);
static void EnterDetailMenu(uint16_t index);

// =============================================================================
// String Arrays for Settings
// =============================================================================

static const char *powerNames[] = {"User", "Low1", "Low2", "Low3", "Low4", "Low5", "Mid", "High"};
static const char *bwNames[] = {"Wide", "Narrow"};
static const char *modNames[] = {"FM", "AM", "USB"};
static const char *offsetDirNames[] = {"None", "+", "-"};
static const char *compNames[] = {"Off", "TX", "RX", "TX+RX"};
static const char *yesNoNames[] = {"No", "Yes"};
static const char *stepNames[] = {"2.50", "5.00", "6.25", "10.00", "12.50", "25.00", "8.33"};
static const char *scrambleNames[] = {"Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};

// =============================================================================
// Helper Functions for CTCSS/DCS
// =============================================================================

static void GetToneText(DCS_CodeType_t type, uint8_t code, char *buf, uint8_t sz) {
    if (type == CODE_TYPE_OFF) {
        snprintf(buf, sz, "Off");
    } else if (type == CODE_TYPE_CONTINUOUS_TONE) {
        snprintf(buf, sz, "%u.%uHz", CTCSS_Options[code] / 10, CTCSS_Options[code] % 10);
    } else {
        snprintf(buf, sz, "D%03o%c", DCS_Options[code], (type == CODE_TYPE_REVERSE_DIGITAL) ? 'I' : 'N');
    }
}

static void NextTone(DCS_CodeType_t *type, uint8_t *code, bool up) {
    if (up) {
        // Increment
        if (*type == CODE_TYPE_OFF) {
            *type = CODE_TYPE_CONTINUOUS_TONE;
            *code = 0;
        } else if (*type == CODE_TYPE_CONTINUOUS_TONE) {
            (*code)++;
            if (*code >= 50) {
                *type = CODE_TYPE_DIGITAL;
                *code = 0;
            }
        } else if (*type == CODE_TYPE_DIGITAL) {
            (*code)++;
            if (*code >= 104) {
                *type = CODE_TYPE_REVERSE_DIGITAL;
                *code = 0;
            }
        } else {
            (*code)++;
            if (*code >= 104) {
                *type = CODE_TYPE_OFF;
                *code = 0;
            }
        }
    } else {
        // Decrement
        if (*type == CODE_TYPE_OFF) {
            *type = CODE_TYPE_REVERSE_DIGITAL;
            *code = 103;
        } else if (*type == CODE_TYPE_CONTINUOUS_TONE) {
            if (*code == 0) {
                *type = CODE_TYPE_OFF;
                *code = 0;
            } else {
                (*code)--;
            }
        } else if (*type == CODE_TYPE_DIGITAL) {
            if (*code == 0) {
                *type = CODE_TYPE_CONTINUOUS_TONE;
                *code = 49;
            } else {
                (*code)--;
            }
        } else {
            if (*code == 0) {
                *type = CODE_TYPE_DIGITAL;
                *code = 103;
            } else {
                (*code)--;
            }
        }
    }
}

// =============================================================================
// Main Channel List Menu
// =============================================================================

static Menu memoriesMenu = {
    .title = NULL,  // Set dynamically in MEMORIES_Init
    .items = NULL,
    .num_items = MR_CHANNEL_LAST + 1,
    .render_item = Memories_RenderItem,
    .action = Memories_Action,
    .itemHeight = MENU_ITEM_H,
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y
};

// =============================================================================
// Detail Menu Actions - Execute Directly
// =============================================================================

static void DoSelect(void) {
    // Switch VFO to this channel
    gEeprom.MrChannel[gEeprom.TX_VFO] = detailChannelIndex;
    gEeprom.ScreenChannel[gEeprom.TX_VFO] = detailChannelIndex;
    gEeprom.FreqChannel[gEeprom.TX_VFO] = detailChannelIndex;
    
    // Valid channel check logic from original firmware
    if (!RADIO_CheckValidChannel(detailChannelIndex, false, 0)) {
        // Don't select invalid channels, though menu shouldn't let you get here
        return;
    }
    
    RADIO_ConfigureChannel(gEeprom.TX_VFO, VFO_CONFIGURE_RELOAD);
    
    // Save settings and exit to main
    // Save settings and exit to main
    SETTINGS_SaveSettings();
    
    // Explicitly request exit to main app
    gRequestDisplayScreen = DISPLAY_MAIN;
    
    // Reset state but DO NOT perform generic back which might confuse the stack
    currentMode = MEM_MODE_LIST; 
}

static void DoRename(void) {
    SETTINGS_FetchChannelName(editBuffer, detailChannelIndex);
    currentMode = MEM_MODE_RENAME;
    TextInput_Init(editBuffer, 10, NULL);
}

static void DoEditFreq(void) {
    currentMode = MEM_MODE_RX_FREQ;
    FreqInput_Init(editChannel.freq_config_RX.Frequency, NULL);
}

static void DoEditOffset(void) {
    currentMode = MEM_MODE_TX_OFFSET;
    FreqInput_Init(editChannel.TX_OFFSET_FREQUENCY, NULL);
}

static void DoDelete(void) {
    // Clear channel
    VFO_Info_t empty;
    memset(&empty, 0xFF, sizeof(empty));
    SETTINGS_SaveChannel(detailChannelIndex, 0, &empty, 1);
    
    // Check if we deleted the current channel
    if (gEeprom.MrChannel[gEeprom.TX_VFO] == detailChannelIndex) {
        // Find next valid channel or go to VFO mode?
        // For now, let radio logic handle it, probably will show empty/freq mode
    }
    
    currentMode = MEM_MODE_LIST;
    AG_MENU_Back();
}

// =============================================================================
// Setting Value Handlers - Change and Save
// =============================================================================

static void GetPower(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    if (editChannel.OUTPUT_POWER <= 7) {
        snprintf(buf, sz, "%s", powerNames[editChannel.OUTPUT_POWER]);
    } else {
        snprintf(buf, sz, "Unk");
    }
}

static void ChangePower(const MenuItem *item, bool up) {
    (void)item;
    if (up) {
        if (editChannel.OUTPUT_POWER < 7) editChannel.OUTPUT_POWER++;
        else editChannel.OUTPUT_POWER = 0;
    } else {
        if (editChannel.OUTPUT_POWER > 0) editChannel.OUTPUT_POWER--;
        else editChannel.OUTPUT_POWER = 7;
    }
    SaveChannelData();
}

static void GetBandwidth(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    snprintf(buf, sz, "%s", bwNames[editChannel.CHANNEL_BANDWIDTH % 2]);
}

static void ChangeBandwidth(const MenuItem *item, bool up) {
    (void)item; (void)up;
    editChannel.CHANNEL_BANDWIDTH = !editChannel.CHANNEL_BANDWIDTH;
    SaveChannelData();
}

static void GetModulation(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    if (editChannel.Modulation < MODULATION_UKNOWN) {
        snprintf(buf, sz, "%s", modNames[editChannel.Modulation]);
    } else {
        snprintf(buf, sz, "?");
    }
}

static void ChangeModulation(const MenuItem *item, bool up) {
    (void)item;
    int mod = editChannel.Modulation;
    mod = (mod + (up ? 1 : 2)) % 3;
    editChannel.Modulation = (ModulationMode_t)mod;
    SaveChannelData();
}

static void GetOffsetDir(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    snprintf(buf, sz, "%s", offsetDirNames[editChannel.TX_OFFSET_FREQUENCY_DIRECTION % 3]);
}

static void ChangeOffsetDir(const MenuItem *item, bool up) {
    (void)item;
    editChannel.TX_OFFSET_FREQUENCY_DIRECTION = (editChannel.TX_OFFSET_FREQUENCY_DIRECTION + (up ? 1 : 2)) % 3;
    SaveChannelData();
}

static void GetOffsetVal(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    uint32_t freq = editChannel.TX_OFFSET_FREQUENCY;
    snprintf(buf, sz, "%u.%05u", freq/100000, freq%100000);
}

static void GetRxTone(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    GetToneText(editChannel.freq_config_RX.CodeType, editChannel.freq_config_RX.Code, buf, sz);
}

static void ChangeRxTone(const MenuItem *item, bool up) {
    (void)item;
    NextTone(&editChannel.freq_config_RX.CodeType, &editChannel.freq_config_RX.Code, up);
    SaveChannelData();
}

static void GetTxTone(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    GetToneText(editChannel.freq_config_TX.CodeType, editChannel.freq_config_TX.Code, buf, sz);
}

static void ChangeTxTone(const MenuItem *item, bool up) {
    (void)item;
    NextTone(&editChannel.freq_config_TX.CodeType, &editChannel.freq_config_TX.Code, up);
    SaveChannelData();
}

static void GetScramble(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    uint8_t sc = editChannel.SCRAMBLING_TYPE;
    if (sc > 10) sc = 0;
    snprintf(buf, sz, "%s", scrambleNames[sc]);
}

static void ChangeScramble(const MenuItem *item, bool up) {
    (void)item;
    if (up) {
        if (editChannel.SCRAMBLING_TYPE < 10) editChannel.SCRAMBLING_TYPE++;
        else editChannel.SCRAMBLING_TYPE = 0;
    } else {
        if (editChannel.SCRAMBLING_TYPE > 0) editChannel.SCRAMBLING_TYPE--;
        else editChannel.SCRAMBLING_TYPE = 10;
    }
    SaveChannelData();
}

static void GetStep(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    snprintf(buf, sz, "%s", stepNames[editChannel.STEP_SETTING % 7]);
}

static void ChangeStep(const MenuItem *item, bool up) {
    (void)item;
    if (up) editChannel.STEP_SETTING = (editChannel.STEP_SETTING + 1) % 7;
    else editChannel.STEP_SETTING = (editChannel.STEP_SETTING + 6) % 7;
    SaveChannelData();
}

static void GetCompander(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    snprintf(buf, sz, "%s", compNames[editChannel.Compander % 4]);
}

static void ChangeCompander(const MenuItem *item, bool up) {
    (void)item;
    editChannel.Compander = (editChannel.Compander + (up ? 1 : 3)) % 4;
    SaveChannelData();
}

static void GetBusyLock(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    snprintf(buf, sz, "%s", yesNoNames[editChannel.BUSY_CHANNEL_LOCK ? 1 : 0]);
}

static void ChangeBusyLock(const MenuItem *item, bool up) {
    (void)item; (void)up;
    editChannel.BUSY_CHANNEL_LOCK = !editChannel.BUSY_CHANNEL_LOCK;
    SaveChannelData();
}

static void GetScanlist1(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    snprintf(buf, sz, "%s", yesNoNames[editChannel.SCANLIST1_PARTICIPATION ? 1 : 0]);
}

static void ChangeScanlist1(const MenuItem *item, bool up) {
    (void)item; (void)up;
    editChannel.SCANLIST1_PARTICIPATION = !editChannel.SCANLIST1_PARTICIPATION;
    SaveChannelData();
}

static void GetScanlist2(const MenuItem *item, char *buf, uint8_t sz) {
    (void)item;
    snprintf(buf, sz, "%s", yesNoNames[editChannel.SCANLIST2_PARTICIPATION ? 1 : 0]);
}

static void ChangeScanlist2(const MenuItem *item, bool up) {
    (void)item; (void)up;
    editChannel.SCANLIST2_PARTICIPATION = !editChannel.SCANLIST2_PARTICIPATION;
    SaveChannelData();
}

// =============================================================================
// Detail Menu Actions - Listen to KEY_MENU on press
// =============================================================================

static bool ActionSelect(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    (void)item; (void)key_held;
    if (key == KEY_MENU && key_pressed) {
        DoSelect();
        return true;
    }
    return false;
}

static bool ActionRename(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    (void)item; (void)key_held;
    if (key == KEY_MENU && key_pressed) {
        DoRename();
        return true;
    }
    return false;
}

static bool ActionFreq(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    (void)item; (void)key_held;
    if (key == KEY_MENU && key_pressed) {
        DoEditFreq();
        return true;
    }
    return false;
}

static bool ActionOffsetVal(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    (void)item; (void)key_held;
    if (key == KEY_MENU && key_pressed) {
        DoEditOffset();
        return true;
    }
    return false;
}

static bool ActionDelete(const MenuItem *item, KEY_Code_t key, bool key_pressed, bool key_held) {
    (void)item; (void)key_held;
    if (key == KEY_MENU && key_pressed) {
        DoDelete();
        return true;
    }
    return false;
}

// =============================================================================
// Settings List
// =============================================================================

static const MenuItem channelDetailItems[] = {
    {"Select",       0, NULL,          NULL,            NULL, ActionSelect},
    {"Rename",       0, NULL,          NULL,            NULL, ActionRename},
    {"Frequency",    0, NULL,          NULL,            NULL, ActionFreq},
    {"RX Tone",      0, GetRxTone,     ChangeRxTone,    NULL, NULL},
    {"TX Tone",      0, GetTxTone,     ChangeTxTone,    NULL, NULL},
    {"Power",        0, GetPower,      ChangePower,     NULL, NULL},
    {"Bandwidth",    0, GetBandwidth,  ChangeBandwidth, NULL, NULL},
    {"Modulation",   0, GetModulation, ChangeModulation,NULL, NULL},
    {"Offset Dir",   0, GetOffsetDir,  ChangeOffsetDir, NULL, NULL},
    {"Offset Freq",  0, GetOffsetVal,  NULL,            NULL, ActionOffsetVal},
    {"Step",         0, GetStep,       ChangeStep,      NULL, NULL},
    {"Scrambler",    0, GetScramble,   ChangeScramble,  NULL, NULL},
    {"Compander",    0, GetCompander,  ChangeCompander, NULL, NULL},
    {"Busy Lock",    0, GetBusyLock,   ChangeBusyLock,  NULL, NULL},
    {"Scanlist 1",   0, GetScanlist1,  ChangeScanlist1, NULL, NULL},
    {"Scanlist 2",   0, GetScanlist2,  ChangeScanlist2, NULL, NULL},
    {"Delete",       0, NULL,          NULL,            NULL, ActionDelete},
};

static Menu channelDetailMenu = {
    .title = NULL,
    .items = channelDetailItems,
    .num_items = sizeof(channelDetailItems) / sizeof(channelDetailItems[0]),
    .itemHeight = MENU_ITEM_H,
    .x = 0, .y = MENU_Y, .width = LCD_WIDTH, .height = LCD_HEIGHT - MENU_Y
};

// =============================================================================
// Helper Functions
// =============================================================================

static void LoadChannelData(uint16_t index) {
    // Load target channel into global RX VFO temporarily to parse settings
    uint16_t savedChannel = gRxVfo->CHANNEL_SAVE;
    gRxVfo->CHANNEL_SAVE = index;
    RADIO_ConfigureChannel(gEeprom.RX_VFO, VFO_CONFIGURE_RELOAD);
    
    // Copy to our edit buffer
    memcpy(&editChannel, gRxVfo, sizeof(VFO_Info_t));
    
    // Restore original VFO
    gRxVfo->CHANNEL_SAVE = savedChannel;
    RADIO_ConfigureChannel(gEeprom.RX_VFO, VFO_CONFIGURE_RELOAD);
}

static void SaveChannelData(void) {
    SETTINGS_SaveChannel(detailChannelIndex, 0, &editChannel, 2);
    
    // Update VFOs if they are using this channel
    if (detailChannelIndex == gEeprom.MrChannel[0]) RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
    if (detailChannelIndex == gEeprom.MrChannel[1]) RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);
}

static void EnterDetailMenu(uint16_t index) {
    detailChannelIndex = index;
    LoadChannelData(index);
    
    // Set title
    char name[17];
    SETTINGS_FetchChannelName(name, index);
    if (strlen(name) > 0) {
        snprintf(detailTitle, sizeof(detailTitle), "%s", name);
    } else {
        snprintf(detailTitle, sizeof(detailTitle), "CH-%03u", index + 1);
    }
    channelDetailMenu.title = detailTitle;
    channelDetailMenu.i = 0;
    
    currentMode = MEM_MODE_DETAIL;
    AG_MENU_EnterMenu(&channelDetailMenu);
}

// =============================================================================
// Main Render Item
// =============================================================================

static void Memories_RenderItem(uint16_t index, uint8_t visIndex) {
    // Translate filtered index to real channel index
    uint16_t realIndex = searchActive ? filteredChannels[index] : index;
    
    const uint8_t y = MENU_Y + visIndex * MENU_ITEM_H;
    const uint8_t baseline_y = y + MENU_ITEM_H - 2;

    bool valid = RADIO_CheckValidChannel(realIndex, false, 0);

    if (!valid) {
        AG_PrintSmall(3, baseline_y, "%03u -", realIndex + 1);
        return;
    }

    char name[17];
    SETTINGS_FetchChannelName(name, realIndex);
    uint32_t freq = SETTINGS_FetchChannelFrequency(realIndex);

    char mainLabel[24];
    char rightLabel[16];
    
    switch (gEeprom.CHANNEL_DISPLAY_MODE) {
        case MDF_NAME:
        case MDF_NAME_FREQ:
            if (strlen(name) > 0) {
                snprintf(mainLabel, sizeof(mainLabel), "%03u %s", realIndex + 1, name);
                snprintf(rightLabel, sizeof(rightLabel), "%u.%05u", freq/100000, freq%100000);
            } else {
                snprintf(mainLabel, sizeof(mainLabel), "%03u %u.%05u", realIndex + 1, freq/100000, freq%100000);
                rightLabel[0] = '\0';
            }
            break;
        case MDF_FREQUENCY:
            snprintf(mainLabel, sizeof(mainLabel), "%03u %u.%05u", realIndex + 1, freq/100000, freq%100000);
            if (strlen(name) > 0) {
                snprintf(rightLabel, sizeof(rightLabel), "%s", name);
            } else {
                rightLabel[0] = '\0';
            }
            break;
        case MDF_CHANNEL:
        default:
            if (strlen(name) > 0) {
                snprintf(mainLabel, sizeof(mainLabel), "%03u %s", realIndex + 1, name);
            } else {
                snprintf(mainLabel, sizeof(mainLabel), "%03u %u.%05u", realIndex + 1, freq/100000, freq%100000);
            }
            rightLabel[0] = '\0';
            break;
    }

    AG_PrintMedium(3, baseline_y, "%s", mainLabel);
    
    if (strlen(rightLabel) > 0) {
        AG_PrintSmallEx(LCD_WIDTH - 5, baseline_y, POS_R, C_FILL, "%s", rightLabel);
    }
}

// =============================================================================
// Main List Action Handler
// =============================================================================

static bool Memories_Action(uint16_t index, KEY_Code_t key, bool key_pressed, bool key_held) {
    // Translate filtered index to real channel index
    uint16_t realIndex = searchActive ? filteredChannels[index] : index;
    (void)key_held;
    
    // Handle EXIT on press only
    if (key == KEY_EXIT && key_pressed) {
        AG_MENU_Back();
        return true;
    }

    if (key == KEY_MENU) {
        if (key_held) {
            if (RADIO_CheckValidChannel(realIndex, false, 0)) {
                EnterDetailMenu(realIndex);
                AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
            }
            return true;
        } else if (!key_pressed) { // Released
            if (ignoreNextMenuRelease) {
                ignoreNextMenuRelease = false;
                return true;
            }

            // If we are still in LIST mode (which we are, otherwise this fn wouldn't be called),
            // and we received a Release event without a prior active Hold that changed the mode,
            // then it's a Short Press -> Select.
            
            // Note: If EnterDetailMenu was called on Hold, currentMode changed to MEM_MODE_DETAIL.
            // The subsequent Release event would undergo ProcessKeys -> switch(currentMode) -> MEM_MODE_DETAIL case.
            // It would NOT reach here.
            
            if (RADIO_CheckValidChannel(realIndex, false, 0)) {
                detailChannelIndex = realIndex;
                DoSelect();
                AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
            }
            return true;
        }
        return true; 
    }

    return false;
}

// =============================================================================
// Search Title Helper
// =============================================================================

// Check if channel number (1-indexed) matches search pattern
// Leading zeros act as wildcards (e.g., "05" matches 005, 015, 025, 105, 205, etc.)
static bool ChannelMatchesSearch(uint16_t channelNum1Indexed) {
    if (searchDigitCount == 0) return true;
    
    // Convert channel to string (1-indexed, 3 digits)
    char chanStr[4];
    snprintf(chanStr, sizeof(chanStr), "%03u", channelNum1Indexed);
    
    // Count leading zeros in search pattern
    uint8_t leadingZeros = 0;
    for (uint8_t i = 0; i < searchDigitCount; i++) {
        if (searchDigits[i] == '0') leadingZeros++;
        else break;
    }
    
    // Match from the end (rightmost digits must match)
    // With leading zeros as wildcards, we match the non-zero suffix
    uint8_t significantDigits = searchDigitCount - leadingZeros;
    if (significantDigits == 0) {
        // All zeros - match channels ending in zeros
        // "0" matches X0, "00" matches X00
        uint8_t chanLen = 3;
        for (uint8_t i = 0; i < searchDigitCount; i++) {
            if (chanStr[chanLen - 1 - i] != '0') return false;
        }
        return true;
    }
    
    // Match the rightmost 'searchDigitCount' digits
    uint8_t chanLen = 3;
    for (int8_t i = searchDigitCount - 1; i >= 0; i--) {
        int8_t chanIdx = chanLen - (searchDigitCount - i);
        if (chanIdx < 0) return false;
        // Skip leading zero positions
        if (i < leadingZeros) continue;
        if (chanStr[chanIdx] != searchDigits[i]) return false;
    }
    return true;
}

static void RebuildFilteredList(void) {
    filteredCount = 0;
    for (uint16_t ch = 0; ch <= MR_CHANNEL_LAST; ch++) {
        if (ChannelMatchesSearch(ch + 1)) {  // 1-indexed for display
            filteredChannels[filteredCount++] = ch;
        }
    }
    searchActive = (searchDigitCount > 0);
    memoriesMenu.num_items = searchActive ? filteredCount : (MR_CHANNEL_LAST + 1);
}

static void UpdateSearchTitle(void) {
    if (searchDigitCount == 0) {
        snprintf(listTitle, sizeof(listTitle), "Memories");
    } else {
        snprintf(listTitle, sizeof(listTitle), "Memories %s?", searchDigits);
    }
    memoriesMenu.title = listTitle;
}

static void ClearSearch(void) {
    searchDigits[0] = '\0';
    searchDigitCount = 0;
    searchActive = false;
    memoriesMenu.num_items = MR_CHANNEL_LAST + 1;
    UpdateSearchTitle();
}

static void AddSearchDigit(uint8_t digit) {
    if (searchDigitCount >= 3) {
        // Shift left and add new digit
        searchDigits[0] = searchDigits[1];
        searchDigits[1] = searchDigits[2];
        searchDigits[2] = '0' + digit;
        searchDigits[3] = '\0';
    } else {
        searchDigits[searchDigitCount++] = '0' + digit;
        searchDigits[searchDigitCount] = '\0';
    }
    UpdateSearchTitle();
    RebuildFilteredList();
    
    // Reset cursor to first matching channel
    memoriesMenu.i = 0;
}

// =============================================================================
// Public Functions
// =============================================================================

void MEMORIES_Init(void) {
    currentMode = MEM_MODE_LIST;
    
    // Clear search state
    ClearSearch();
    
    // Set cursor to current VFO channel if valid
    uint16_t currentChan = gEeprom.MrChannel[gEeprom.TX_VFO];
    if (currentChan <= MR_CHANNEL_LAST) {
        memoriesMenu.i = currentChan;
    } else {
        memoriesMenu.i = 0;
    }
    
    ignoreNextMenuRelease = true;
    
    AG_MENU_Init(&memoriesMenu);
}

void MEMORIES_Deinit(void) {
    AG_MENU_Deinit();
}

void MEMORIES_Render(void) {
    switch (currentMode) {
        case MEM_MODE_RENAME:
            TextInput_Render();
            break;
        case MEM_MODE_RX_FREQ:
        case MEM_MODE_TX_OFFSET:
            FreqInput_Render();
            break;
        default:
            AG_MENU_Render();
            break;
    }
    ST7565_BlitFullScreen();
}

void MEMORIES_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    gUpdateDisplay = true;
    
    switch (currentMode) {
        case MEM_MODE_RENAME:
            if (Key == KEY_MENU && bKeyPressed && !bKeyHeld) {
                // Save rename
                SETTINGS_SaveChannelName(detailChannelIndex, editBuffer);
                currentMode = MEM_MODE_DETAIL;
            } else if (Key == KEY_EXIT && bKeyPressed && !bKeyHeld) {
                // Cancel rename
                currentMode = MEM_MODE_DETAIL;
            } else {
                TextInput_HandleInput(Key, bKeyPressed, bKeyHeld);
            }
            return;
            
        case MEM_MODE_RX_FREQ:
            if (Key == KEY_MENU && bKeyPressed && !bKeyHeld) {
                // Save RX frequency
                uint32_t f = FreqInput_GetFrequency();
                editChannel.freq_config_RX.Frequency = f;
                // If simple config, set TX too
                if (editChannel.TX_OFFSET_FREQUENCY_DIRECTION == 0) {
                    editChannel.freq_config_TX.Frequency = f;
                }
                SaveChannelData();
                FreqInput_Cancel();
                currentMode = MEM_MODE_DETAIL;
            } else if (Key == KEY_EXIT && bKeyPressed && !bKeyHeld) {
                FreqInput_Cancel();
                currentMode = MEM_MODE_DETAIL;
            } else {
                FreqInput_HandleInput(Key, bKeyPressed, bKeyHeld);
            }
            return;
            
        case MEM_MODE_TX_OFFSET:
            if (Key == KEY_MENU && bKeyPressed && !bKeyHeld) {
                // Save TX Offset
                editChannel.TX_OFFSET_FREQUENCY = FreqInput_GetFrequency();
                SaveChannelData();
                FreqInput_Cancel();
                currentMode = MEM_MODE_DETAIL;
            } else if (Key == KEY_EXIT && bKeyPressed && !bKeyHeld) {
                FreqInput_Cancel();
                currentMode = MEM_MODE_DETAIL;
            } else {
                FreqInput_HandleInput(Key, bKeyPressed, bKeyHeld);
            }
            return;
            
        case MEM_MODE_DETAIL:
            if (Key == KEY_EXIT && bKeyPressed && !bKeyHeld) {
                currentMode = MEM_MODE_LIST;
                AG_MENU_Back();
                return;
            }
            // Fall through to menu handler
            break;
            
        case MEM_MODE_LIST:
            // Handle digit keys for quick jump (0-9)
            if (Key >= KEY_0 && Key <= KEY_9 && bKeyPressed && !bKeyHeld) {
                uint8_t digit = (Key == KEY_0) ? 0 : (Key - KEY_1 + 1);
                AddSearchDigit(digit);
                AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
                return;
            }
            
            // EXIT: clear search first, then exit
            if (Key == KEY_EXIT && bKeyPressed && !bKeyHeld) {
                if (searchDigitCount > 0) {
                    ClearSearch();
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                    return;
                }
                // Otherwise, normal exit handled by AG_MENU
            }
            
            // Standard Navigation
            if (Key == KEY_UP || Key == KEY_DOWN) {
                // Clear search when navigating
                if (searchDigitCount > 0) {
                    ClearSearch();
                }
                // Key repeat handling provided by APP infrastructure
                AG_MENU_HandleInput(Key, bKeyPressed, bKeyHeld);
                return;
            }
            break;
        default:
            break;
    }
    
    // Handle menu navigation
    AG_MENU_HandleInput(Key, bKeyPressed, bKeyHeld);
    
    // Check if we exited the menu system
    if (!AG_MENU_IsActive()) {
        gRequestDisplayScreen = DISPLAY_LAUNCHER;
    }
}
