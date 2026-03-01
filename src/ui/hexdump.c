#include <string.h>
#include "hexdump.h"
#include "ui.h"
#include "ag_graphics.h"
#include "helper.h"
#include "drivers/bsp/st7565.h"
#include "drivers/bsp/i2c.h" // For EEPROM read
#include "ui/menu.h"
#include "core/misc.h" // For AG_MENU_Back/HandleInput if needed, or just manual logic.

#define LINE_H 8
#define HEADER_H 10
#define BYTES_PER_LINE 8

static char IsPrintable(uint8_t c) {
    return (c >= 32 && c <= 126) ? c : '.';
}

void HexDump_Render(const char *title, HexDump_ReadCb read_cb, uint32_t total_size, int scroll_y) {
    UI_DisplayClear();
    
    int startY = HEADER_H;
    int total_lines = (total_size + BYTES_PER_LINE - 1) / BYTES_PER_LINE;
    char buf[16];
    uint8_t line_buffer[BYTES_PER_LINE];

    for (int i = 0; i < total_lines; i++) {
        int y = startY + (i * LINE_H) - (scroll_y * LINE_H);
        
        // Visibility Check
        if (y < HEADER_H || y >= LCD_HEIGHT) continue;
        
        uint32_t offset = i * BYTES_PER_LINE;
        
        // Render Offset
        // (Moved to the end of loop for overlay/consistency, or rendered here)
        // AG_PrintSmallEx(0, y + (LINE_H-2), POS_L, C_FILL, "%02lX", offset); 
        // Let's use %04X for general use, or adjust based on size.
        // For CPU ID (256 bytes) %02X is fine. For EEPROM (8K+) need %04X.
        // 0000 -> 4 chars * 5px = 20px. 
        // Hex starts at 16 in sysinfo example. If we use 4 chars, we need more space.
        // Hex start at 26? 
        // Let's adjust layout to fit 4-digit offset.
        // Offset: 0-20px (approx)
        // Hex: 24px start? 
        
        // Adapting layout:
        // Offset: "%04X" -> ~24px width
        // Hex: 8 bytes * 2 chars = 16 chars + spaces?
        // Previous sysinfo: Start X=16. 
        
        bool read_ok = read_cb(offset, line_buffer, BYTES_PER_LINE);
        if (!read_ok) __builtin_memset(line_buffer, 0, BYTES_PER_LINE);

        for (int j = 0; j < BYTES_PER_LINE; j++) {
            if (offset + j >= total_size) break;
            
            uint8_t val = line_buffer[j];
            
            // Hex
            // X POS: 26 + (j*10) -> 26, 36, ... 96
            // Last hex end: 96+9=105.
            // ASCII Start: 108? 
            // 128-108 = 20px. 5 chars? 
            // Screen width 128 is tight for 4-digit offset + 8 bytes hex + 8 ASCII.
            
            // If we want to support EEPROM, we likely need 4-digit offset.
            // "0000"
            // Let's compress.
            // Offset: 0. "%04X" font width 5 -> 24px.
            // Hex: 8 bytes. 2 chars each.
            // If we drop spaces between hex? 16 chars * 5 = 80px.
            // 24 + 80 = 104.
            // ASCII: 8 chars * 4 = 32px.
            // 104 + 32 = 136. Overflow.
            
            // Maybe reducing hex spacing?
            // Or only show 4 bytes per line?
            // Or drop ASCII?
            // "this is how an incompatible codebase shows hex mem on screen"
            // Their code:
            // PrintSmall(0, ... offset)
            // PrintSmall(16 + col * 9, ... hex)
            // PrintSmall(88 + col * 5, ... ascii)
            // They show 8 bytes.
            // 16 + 7*9 = 16+63 = 79. + width of last byte (2 chars) ~10px = 89.
            // ASCII starts at 88. Overlap?
            // 88 + 7*5 = 88+35 = 123. width~5? 128.
            // So they fit 8 bytes. But their offset is "%u" -> page*64 + i.
            // If offset is small ok. If large?
            
            // SysInfo implementation used "%02X" (2 chars).
            // For EEPROM we need 4 hex digits for offset (0x0000 - 0x2000).
            
            // Dynamic formatting based on total_size?
 
            
            // Wait, if I shift hex start, I shift everything right.
            // If X_HEX = 26.
            // 8*10 = 80 width. 26+80 = 106.
            // LCD=128. 128-106 = 22px left for ASCII.
            // 8 chars. Can't fit.
            
            // Maybe offset in header? Or scrollable offset?
            // Or just minimal offset: "00", "08" and rely on user knowing page?
            // User requested "HexDump", usually implies address.
            
            // Let's stick to the user's provided layout for now, maybe use %02X for low byte of offset? 
            // Or toggle offset view?
            // Or just overlay offset? 
            
            // Let's try to fit 4 bytes if size > 256? No, user wanted "MemView Style".
            // The user's example shows 8 bytes.
            
            // Let's try tight packing.
            // Offset: 4 chars (20px). X=0.
            // Hex: 8 bytes. "%02X". 2 chars = 10px. 
            // If we use 1px separator? 11px. 8*11 = 88. 20+88 = 108.
            // ASCII: 20px left. 4 chars?
            
            // Compromise: Standardize on SysInfo layout (2-digit offset) for CPU ID, 
            // but for EEPROM we offset the base address in the header?
            // e.g. "EEPROM 0000" in title?
            // Actually, `HexDump_Render` receives title. We can update title with scroll? 
            // No, scroll is pixel smooth? No line-based.
            
            // For now, let's use %04X offset but potentially hide ASCII for EEPROM or accept overlap/cut-off?
            // Or assume 8-byte lines.
            
            // Let's fallback to %02X offset (low byte only) if space is tight, or just render 4 hex digits and squeeze.
            
            // Using standard sysinfo layout logic for now, tailored for CPU ID.
            // Offset: %02X (8 bits). For EEPROM check if we can format differently or user is fine with 00..F8 looping.
            
            // Pack everything:
            // Offset: %04X -> 20px (X:0-20)
            // Hex: 8 bytes. If 9px per byte -> 72px. (X:24-96)
            // ASCII: 8 bytes. 4px per char -> 32px. (X:96-128)
            // Total: 20 + 4 + 72 + 32 = 128. Correct.
            
            // sprintf_(buf, "%02X", val);
            NUMBER_ToHex(buf, val, 2);
            AG_PrintSmallEx(24 + (j * 9), y + (LINE_H-2), POS_L, C_FILL, buf);
            
            // sprintf_(buf, "%c", IsPrintable(val));
            buf[0] = IsPrintable(val);
            buf[1] = '\0';
            AG_PrintSmallEx(96 + (j * 4), y + (LINE_H-2), POS_L, C_FILL, buf);
        }
        
        // Offset (4 digits)
        NUMBER_ToHex(buf, offset, 4);
        AG_PrintSmallEx(0, y + (LINE_H-2), POS_L, C_FILL, buf);
    }
    
    ST7565_BlitFullScreen();
}

// --- Full Screen App Wrapper ---

static int mHexDumpScrollY = 0;

static bool HexDump_EepromRead(uint32_t offset, uint8_t *buffer, uint16_t size) {
    // EEPROM size 0x0000 -> 0x2000 (8KB) for 24C64? Or 25Q16 emulation?
    // User mentioned "EEPROM Hexdump".
    // Codebase uses PY25Q16 (Flash). 
    // EEPROM usually refers to the emulated area in Flash or actual I2C EEPROM.
    // If usage is PY25Q16_ReadBuffer, it accesses the whole 2MB flash?
    // Or just the 8KB EEPROM emulation area (Settings)?
    // `SETTINGS_InitEEPROM` reads from 0x004000 etc.
    // Let's assume user wants to see the "EEPROM" content (emulated).
    // Let's allow dumping the whole logical EEPROM range 0x0000 - 0x2000?
    // Or just raw flash?
    // User said "Eeprom Hexdump".
    // I2C_ReadBuffer reads the I2C EEPROM (if present) or internal?
    // This device (UV-K5) has an external I2C EEPROM (24C64 usually).
    // `drivers/bsp/i2c.h` -> I2C_ReadBuffer.
    
    // Let's try to read via I2C_ReadBuffer.
    // Need to declare it if header doesn't.
    // wrapper:
    extern void EEPROM_ReadBuffer(uint16_t Address, void *pBuffer, uint8_t Size);
    EEPROM_ReadBuffer(offset, buffer, (uint8_t)size);
    return true;
}

void UI_DisplayHexDump(void) {
    // Title "EEPROM"
    // Size 8KB (0x2000)
    HexDump_Render("EEPROM", HexDump_EepromRead, 0x2000, mHexDumpScrollY);
}

void UI_HexDump_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    if (bKeyPressed) {
        if (Key == KEY_EXIT) {
            gRequestDisplayScreen = DISPLAY_MAIN;
            return;
        }
        if (Key == KEY_UP) {
            if (mHexDumpScrollY > 0) mHexDumpScrollY--;
        }
        if (Key == KEY_DOWN) {
             // Limit? 0x2000 bytes / 8 = 1024 lines.
             if (mHexDumpScrollY < (0x2000/8) - 10) mHexDumpScrollY++;
        }
        // PgUp/PgDown? 
        if (Key == KEY_3) { // Page Up
             mHexDumpScrollY -= 10;
             if (mHexDumpScrollY < 0) mHexDumpScrollY = 0;
        }
        if (Key == KEY_9) { // Page Down
             mHexDumpScrollY += 10;
        }
        
        gUpdateDisplay = true;
    }
}
