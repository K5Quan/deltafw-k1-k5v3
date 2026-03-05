#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ENC_PLAIN,
    ENC_CPUID,
    ENC_PASSCODE
} StorageEnc_t;

#define STORAGE_RECORDS(X) \
    /* NAME,              ENC,           TYPE,    ADDR,      SZ,  C1,     S1,     C2, S2 */ \
    /* --- Channels & VFO --- */ \
    X(CHANNEL_DATA,       ENC_PASSCODE,  LINEAR,  0x000000,  16,  200,    16,     0,  0) \
    X(VFO_DATA,           ENC_PASSCODE,  DIM2,    0x001000,  16,  8,      32,     2,  16) \
    X(MR_ATTRIBUTES,      ENC_PASSCODE,  LINEAR,  0x002000,  1,   207,    1,      0,  0) \
    /* --- FM Radio --- */ \
    X(FM_CHANNELS,        ENC_CPUID,     FIXED,   0x003000,  0x50,1,      0,      0,  0) \
    X(FM_CONFIG,          ENC_CPUID,     FIXED,   0x006000,  8,   1,      0,      0,  0) \
    /* --- Global Settings --- */ \
    X(SETTINGS_MAIN,      ENC_CPUID,     FIXED,   0x004000,  16,  1,      0,      0,  0) \
    X(VFO_INDICES,        ENC_CPUID,     FIXED,   0x005000,  8,   1,      0,      0,  0) \
    X(SETTINGS_EXTRA,     ENC_CPUID,     FIXED,   0x007000,  80,  1,      0,      0,  0) \
    X(CUSTOM_ROGER,       ENC_CPUID,     FIXED,   0x007050,  96,  1,      0,      0,  0) \
    X(PASSCODE,           ENC_PLAIN,     FIXED,   0x007100,  128, 1,      0,      0,  0) \
    /* --- DTMF Codes --- */ \
    X(ANI_DTMF_ID,        ENC_CPUID,     FIXED,   0x008000,  8,   1,      0,      0,  0) \
    X(KILL_CODE,          ENC_CPUID,     FIXED,   0x008008,  8,   1,      0,      0,  0) \
    X(REVIVE_CODE,        ENC_CPUID,     FIXED,   0x008010,  8,   1,      0,      0,  0) \
    X(DTMF_UP_CODE,       ENC_CPUID,     FIXED,   0x008018,  16,  1,      0,      0,  0) \
    X(DTMF_DOWN_CODE,     ENC_CPUID,     FIXED,   0x008028,  16,  1,      0,      0,  0) \
    /* --- Scan & Audio --- */ \
    X(SCAN_LIST,          ENC_CPUID,     FIXED,   0x009000,  8,   1,      0,      0,  0) \
    X(AUDIO_SETTINGS,     ENC_CPUID,     FIXED,   0x00A000,  8,   1,      0,      0,  0) \
    X(F_LOCK,             ENC_CPUID,     FIXED,   0x00b000,  10,  1,      0,      0,  0) \
    X(CUSTOM_SETTINGS,    ENC_CPUID,     FIXED,   0x00c000,  8,   1,      0,      0,  0) \
    /* --- Extended Data --- */ \
    X(CHANNEL_NAMES,      ENC_PASSCODE,  LINEAR,  0x00e000,  16,  200,    16,     0,  0) \
    X(DTMF_CONTACTS,      ENC_CPUID,     LINEAR,  0x00f000,  16,  16,     16,     0,  0) \
    /* --- Calibration Data --- */ \
    X(CALIB_SQUELCH,      ENC_PLAIN,     DIM2,    0x010000,  1,   2,      0x60,   10, 1) \
    X(CALIB_RSSI_3,       ENC_PLAIN,     FIXED,   0x0100C0,  8,   1,      0,      0,  0) \
    X(CALIB_RSSI_0,       ENC_PLAIN,     FIXED,   0x0100C8,  8,   1,      0,      0,  0) \
    X(CALIB_TX_POWER,     ENC_PLAIN,     DIM2,    0x0100D0,  3,   7,      16,     3,  3) \
    X(CALIB_BATTERY,      ENC_PLAIN,     FIXED,   0x010140,  12,  1,      0,      0,  0) \
    X(CALIB_VOX1,         ENC_PLAIN,     LINEAR,  0x010150,  2,   6,      2,      0,  0) \
    X(CALIB_VOX0,         ENC_PLAIN,     LINEAR,  0x010168,  2,   6,      2,      0,  0) \
    X(CALIB_MISC,         ENC_PLAIN,     FIXED,   0x010188,  8,   1,      0,      0,  0) \
    /* --- Voice & High Capacity Data --- */ \
    X(VOICE_PROMPT_DATA,  ENC_PLAIN,     LINEAR,  0x14C000,  0,   2,      0x800,  0,  0) \
    X(VOICE_CLIP_DATA,    ENC_PLAIN,     LINEAR,  0x14D000,  0,   0xFFFF, 1,      0,  0) \
    /* --- CW Keyer --- */ \
    X(CW_SETTINGS,        ENC_PLAIN,     FIXED,   0x150000,  256, 1,      0,      0,  0) \
    /* --- Hermes Mesh --- */ \
    X(HERMES_SETTINGS,    ENC_PLAIN,     FIXED,   0x150100,  128, 1,      0,      0,  0) \
    X(HERMES_CONTACTS,    ENC_PLAIN,     LINEAR,  0x150180,  64,  16,     64,     0,  0)


typedef enum {
    ALLOC_FIXED,
    ALLOC_LINEAR,
    ALLOC_DIM2
} AllocType_t;

typedef struct {
    uint8_t   type;     // AllocType_t
    uint8_t   encryption; // StorageEnc_t
    uint16_t  size;     // Size of 1 record
    union {
        struct { uint32_t addr; } fixed;
        struct { uint32_t addr; uint16_t count; uint16_t stride; } linear;
        struct { uint32_t addr; uint16_t count1; uint16_t stride1; uint16_t count2; uint16_t stride2; } dim2;
    };
} RecordDescriptor_t;

typedef enum {
#define X(name, enc, type, addr, size, c1, s1, c2, s2) REC_##name,
    STORAGE_RECORDS(X)
#undef X
    REC_MAX
} RecordID_t;

// Bitfield Union for REC_SETTINGS_MAIN (16 bytes, covers 0x004000 - 0x00400F)
typedef union {
    struct {
        // Byte 0
        uint8_t CHAN_1_CALL;
        // Byte 1
        uint8_t SQUELCH_LEVEL;
        // Byte 2
        uint8_t TX_TIMEOUT_TIMER;
        // Byte 3
        uint8_t NOAA_AUTO_SCAN;
        // Byte 4
        uint8_t KEY_LOCK : 1;
        uint8_t MENU_LOCK : 1;
        uint8_t SET_KEY : 4;
        uint8_t SET_NAV : 1;
        uint8_t UNUSED_4_7 : 1;
        // Byte 5
        uint8_t VOX_SWITCH;
        // Byte 6
        uint8_t VOX_LEVEL;
        // Byte 7
        uint8_t MIC_SENSITIVITY;
        
        // Byte 8 (0x004008)
        uint8_t BACKLIGHT_MAX : 4;
        uint8_t BACKLIGHT_MIN : 4;
        // Byte 9
        uint8_t CHANNEL_DISPLAY_MODE;
        // Byte 10
        uint8_t CROSS_BAND_RX_TX;
        // Byte 11
        uint8_t BATTERY_SAVE;
        // Byte 12
        uint8_t DUAL_WATCH;
        // Byte 13
        uint8_t BACKLIGHT_TIME;
        // Byte 14
        uint8_t TAIL_TONE_ELIMINATION : 1;
        uint8_t NFM : 1; 
        uint8_t UNUSED_14_2 : 6;
        // Byte 15
        uint8_t VFO_OPEN : 1;
        uint8_t CURRENT_STATE : 3;
        uint8_t CURRENT_LIST : 3;
        uint8_t UNUSED_15_7 : 1;
    } __attribute__((packed)) fields;
    uint8_t raw[16];
} __attribute__((packed)) SettingsMain_t;

// Schema for REC_SETTINGS_EXTRA (80 bytes, 0x007000 / logical 0x0E90)
typedef union {
    struct {
        // Offset 0x00 (0x0E90)
        uint8_t BEEP_CONTROL : 1;
        uint8_t KEY_M_LONG_PRESS_ACTION : 7;
        uint8_t KEY_1_SHORT_PRESS_ACTION;
        uint8_t KEY_1_LONG_PRESS_ACTION;
        uint8_t KEY_2_SHORT_PRESS_ACTION;
        uint8_t KEY_2_LONG_PRESS_ACTION;
        uint8_t SCAN_RESUME_MODE;
        uint8_t AUTO_KEYPAD_LOCK;
        uint8_t POWER_ON_DISPLAY_MODE;
        
        // Offset 0x08 (0x0E98)
        uint32_t POWER_ON_PASSWORD;
        uint8_t UNUSED_0E9C[4];
        
        // Offset 0x10 (0x0EA0)
        uint8_t VOICE_PROMPT;
        uint8_t S0_LEVEL;
        uint8_t S9_LEVEL;
        uint8_t MIC_AGC;
        uint8_t UNUSED_0EA3[4];
        
        // Offset 0x18 (0x0EA8)
        uint8_t ALARM_MODE;
        uint8_t ROGER;
        uint8_t REPEATER_TAIL_TONE_ELIMINATION;
        uint8_t TX_VFO;
        uint8_t BATTERY_TYPE;
        uint8_t UNUSED_0EAD[3];
        
        // Offset 0x20 (0x0EB0)
        char WELCOME_STRING0[16];
        
        // Offset 0x30 (0x0EC0)
        char WELCOME_STRING1[16];
        
        // Offset 0x40 (0x0ED0)
        uint8_t DTMF_SIDE_TONE;
        uint8_t DTMF_SEPARATE_CODE;
        uint8_t DTMF_GROUP_CALL_CODE;
        uint8_t DTMF_DECODE_RESPONSE;
        uint8_t DTMF_AUTO_RESET_TIME;
        uint8_t DTMF_PRELOAD_TIME_DIV10;
        uint8_t DTMF_FIRST_CODE_PERSIST_TIME_DIV10;
        uint8_t DTMF_HASH_CODE_PERSIST_TIME_DIV10;
        
        // Offset 0x48 (0x0ED8)
        uint8_t DTMF_CODE_PERSIST_TIME_DIV10;
        uint8_t DTMF_CODE_INTERVAL_TIME_DIV10;
        uint8_t PERMIT_REMOTE_KILL;
        uint8_t UNUSED_0EDB[5];
    } fields;
    uint8_t raw[80];
} __attribute__((packed)) SettingsExtra_t;

// Schema for REC_SCAN_LIST (8 bytes, 0x009000 / logical 0x0F18)
typedef union {
    struct {
        uint8_t SCAN_LIST_DEFAULT;
        uint8_t SCAN_LIST_ENABLED : 3;
        uint8_t UNUSED_1_3 : 5;
        struct {
            uint8_t PRIORITY_CH1;
            uint8_t PRIORITY_CH2;
        } lists[3];
    } fields;
    uint8_t raw[8];
} __attribute__((packed)) ScanList_t;

// Schema for REC_F_LOCK (8 bytes, 0x00b000 / logical 0x0F40)
typedef union {
    struct {
        uint8_t F_LOCK;
        uint8_t TX_350;
        uint8_t KILLED;
        uint8_t TX_200;
        uint8_t TX_500;
        uint8_t EN_350;
        uint8_t SCRAMBLE_EN;
        uint8_t LIVE_DTMF_DECODER : 1;
        uint8_t BATTERY_TEXT : 3;
        uint8_t MIC_BAR : 1;
        uint8_t AM_FIX : 1;
        uint8_t BACKLIGHT_ON_TX_RX : 2;
        
        uint8_t TX_SOFT_START : 1;
        
        uint8_t TX_AUDIO_COMPRESSOR : 1;
        uint8_t CTCSS_LEAD_IN : 1;
        uint8_t SMART_SQUELCH : 1;
        uint8_t SIGNAL_CLASSIFIER : 1;
        uint8_t SQUELCH_TAIL_ELIMINATION : 1;
        uint8_t UNUSED : 2;
    } fields;
    uint8_t raw[10];
} __attribute__((packed)) FLockConfig_t;

// Schema for REC_MR_ATTRIBUTES (1 byte per channel, 0x002000 / logical 0x0D60)
typedef union {
    struct {
        uint8_t band : 3;
        uint8_t compander : 2;
        uint8_t scanlist1 : 1;
        uint8_t scanlist2 : 1;
        uint8_t scanlist3 : 1;
    };
    uint8_t __val;
} __attribute__((packed)) ChannelAttributes_t;

// Schema for REC_CHANNEL_DATA (16 bytes per channel)
typedef union {
    struct {
        uint32_t frequency;
        uint32_t offset;
        uint8_t  rx_code;
        uint8_t  tx_code;
        uint8_t  rx_code_type : 4;
        uint8_t  tx_code_type : 4;
        uint8_t  offset_direction : 4;
        uint8_t  modulation : 4;
        uint8_t  reverse : 1;
        uint8_t  bandwidth : 1;
        uint8_t  power : 2;
        uint8_t  UNUSED_12_4 : 1;
        uint8_t  busy_lock : 1;
        uint8_t  tx_lock : 1;
        uint8_t  UNUSED_12_7 : 1;
        uint8_t  dtmf_decoding : 1;
        uint8_t  dtmf_ptt_id : 3;
        uint8_t  UNUSED_13_4 : 4;
        uint8_t  step;
        uint8_t  scramble;
    } fields;
    uint8_t raw[16];
} __attribute__((packed)) ChannelData_t;

// Schema for REC_CALIB_MISC (8 bytes, 0x1F88)
typedef union {
    struct {
        int16_t  BK4819_XtalFreqLow;
        uint16_t LnaCalibration; // 1F8A
        uint16_t MixCalibration; // 1F8C
        uint8_t  VOLUME_GAIN;
        uint8_t  DAC_GAIN;
    } fields;
    uint8_t raw[8];
} __attribute__((packed)) CalibrationMisc_t;

// Schema for REC_PASSCODE (128 bytes, 0x007100)
typedef union {
    struct {
        uint32_t Magic;         // 0x47415350 "GSAP"
        uint8_t  Verifier[16];  // Derived Verifier
        uint8_t  Nonce[16];     // Random Salt
        uint8_t  Tries;         // Failed attempts
        uint8_t  Length;        // Passcode Length
        uint8_t  MaxTriesConfig;// Configured Max Tries (0=Default 10)
        uint8_t  ExposeLength;  // 0=Hide length, require MENU; 1=Expose, auto-verify
        uint8_t  StealthMode;   // 0=Normal; 1=No backlight/low contrast
        uint32_t Iterations;    // KDF Iterations
        uint8_t  EncryptedMasterKey[32]; // MK encrypted with KEK
        uint64_t MigratedMask;  // Bitmask of migrated records
        uint8_t  Reserved[43]; // Fill to 128 bytes
    } fields;
    uint8_t raw[128];
} __attribute__((packed)) PasscodeConfig_t;

// Schema for REC_CW_SETTINGS (256 bytes, 0x150000)
typedef union {
    struct {
        uint16_t Magic;         // 0x4357 "CW"
        uint8_t  WPM;           // Words Per Minute (e.g. 15-40)
        uint16_t ToneFreqHz;    // Sidetone/Offset Frequency (e.g. 650)
        uint8_t  DecodeMode;    // 0=Off, 1=TX, 2=RX, 3=TX/RX
        uint8_t  AutoSpacing;   // 1=Auto gap insertion
        char     Callsign[9];   // Max 8 chars + null
        char     Macros[8][17]; // 8 custom macro slots, max 16 chars + null
        
        // Advanced Keyer Settings
        uint8_t  KeyerMode;     // 0:Straight, 1:Iambic A, 2:Iambic B, 3:Ultimatic, 4:Bug
        uint8_t  Weighting;     // 30 = 3.0 ratio. Range 25-45
        uint8_t  FarnsworthWPM; // 0 = disabled, else spacing WPM
        uint8_t  TxDelayMs;     // Delay between RF on and Audio on
        bool     ReversePaddles;// Swap Dot and Dash sidekeys
        
        uint8_t  Reserved1;
        uint8_t  OutputMode;    // 0: RF, 1: Flashlight, 2: Trainer
        bool     SidetoneEnabled;
        
        uint8_t  Reserved[93];  // Future expansion (100 - 7 used bytes = 93)
    } fields;
    uint8_t raw[256];
} __attribute__((packed)) CWSettings_t;

// Schema for REC_HERMES_SETTINGS (128 bytes, 0x150100)
typedef union {
    struct {
        uint16_t Magic;           // 0x484D "HM"
        uint8_t  MacPolicy;       // 0=Hardware, 1=Custom, 2=Alias
        uint8_t  CustomMac[6];    // If policy=1
        char     Alias[13];       // Max 12 chars + null (if policy=2 or discovery alias)
        char     Passcode[32];    // Human-readable passcode for K_net
        uint8_t  Salt[16];        // 128-bit Network Salt (RFC §2.1.2)
        bool     HasPasscode;     // 1 if custom passcode/salt, 0 if default
        
        // Granular options
        uint8_t  FreqMode;        // 0=LPD66(433.00), 1=CurrentVFO, 2=MemChannel(Idx in FreqCh)
        uint8_t  FreqCh;          // Memory channel index (0-199) if FreqMode==2
        bool     Enabled;
        bool     RelayEnabled;    // 1=Active Relay, 0=Off
        uint8_t  AckMode;         // 0=Off, 1=Manual, 2=Auto
        bool     CryptoEnabled;
        uint8_t  TTL;             // Default TTL (e.g. 5)
        uint8_t  TxPower;         // 0=Low, 1=Mid, 2=High
        
        uint8_t  Reserved[40];    // (128 - 2 - 1 - 6 - 13 - 32 - 16 - 1 - 1 - 1 - 1 - 1 - 1 - 1 - 1 - 1 = 40)
    } fields;
    uint8_t raw[128];
} __attribute__((packed)) HermesSettings_t;

// Schema for REC_HERMES_CONTACTS (64 bytes each, 16 slots)
typedef union {
    struct {
        uint8_t  NodeID[6];
        char     Alias[13];
        char     Passcode[33];    // Scope secret / per-contact passcode
        uint8_t  Flags;           // bit 0: multicast
        uint8_t  Reserved[11];    // padding to 64 bytes
    } fields;
    uint8_t raw[64];
} __attribute__((packed)) HermesContactRecord_t;



typedef struct {
    uint16_t entries[16]; // 9 bits freq idx (0=pause, else 250+i*10), 7 bits duration (unit*20ms)
} __attribute__((packed)) CustomRoger_t;

typedef struct {
    CustomRoger_t rogers[3];
} __attribute__((packed)) CustomRogers_t;


// Universal Accessors
bool Storage_ReadRecord(RecordID_t id, void *pDest, uint16_t offset, uint16_t len);
bool Storage_WriteRecord(RecordID_t id, const void *pSrc, uint16_t offset, uint16_t len);

// Indexed Accessors (for channels, etc.)
bool Storage_ReadRecordIndexed(RecordID_t id, uint16_t index, void *pDest, uint16_t offset, uint16_t len);
bool Storage_WriteRecordIndexed(RecordID_t id, uint16_t index, const void *pSrc, uint16_t offset, uint16_t len);

// Dirty Flag System
void Storage_SetDirty(RecordID_t id);
bool Storage_IsDirty(RecordID_t id);
void Storage_ClearDirty(RecordID_t id);
void Storage_Commit(RecordID_t id); // Force immediate write
void Storage_SectorErase(RecordID_t id);

// Dynamic address resolution
uint32_t Storage_GetAddress(RecordID_t id, uint16_t index);

StorageEnc_t Storage_GetEncryptionType(RecordID_t id);
// Migration (internal use)
void Storage_MigrateRecord(RecordID_t id);

#ifdef ENABLE_STORAGE_ENCRYPTION
// Raw physical access (legacy/bridge) - Transparently handles encryption
void Storage_ReadBufferRaw(uint32_t addr, void *pBuffer, uint32_t size);
void Storage_WriteBufferRaw(uint32_t addr, const void *pBuffer, uint32_t size, bool AppendFlag);
#else
// Direct mapping to flash when disabled
#include "drivers/bsp/py25q16.h"
#define Storage_ReadBufferRaw(addr, pBuf, size) PY25Q16_ReadBuffer(addr, pBuf, size)
#define Storage_WriteBufferRaw(addr, pBuf, size, append) PY25Q16_WriteBuffer(addr, (void*)pBuf, size, append)
#endif

#endif // STORAGE_H
