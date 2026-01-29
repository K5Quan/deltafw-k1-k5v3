#!/bin/bash
# Rename macros in all source files
# Order matters: Longest matches first

find src -name "*.[ch]" -o -name "CMakeLists.txt" > files_list.txt
echo "CMakePresets.json" >> files_list.txt

while IFS= read -r file; do
    # echo "Processing $file"
    sed -i 's/ENABLE_FEAT_F4HWN_SCREENSHOT/ENABLE_SERIAL_SCREENCAST/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_GMRS_FRS_MURS/ENABLE_GMRS_FRS_MURS_BANDS/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_RESET_CHANNEL/ENABLE_RESET_CHANNEL_FUNCTION/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_RX_TX_TIMER/ENABLE_RX_TX_TIMER_DISPLAY/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_CHARGING_C/ENABLE_USBC_CHARGING_INDICATOR/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_RESUME_STATE/ENABLE_BOOT_RESUME_STATE/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_RESCUE_OPS/ENABLE_RESCUE_OPERATIONS/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_NARROWER/ENABLE_NARROWER_BW_FILTER/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_SPECTRUM/ENABLE_SPECTRUM_EXTENSIONS/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_SLEEP/ENABLE_DEEP_SLEEP_MODE/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_GAME/ENABLE_APP_BREAKOUT_GAME/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_DEBUG/ENABLE_FIRMWARE_DEBUG_LOGGING/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_PMR/ENABLE_PMR446_FREQUENCY_BAND/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_INV/ENABLE_INVERTED_LCD_MODE/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_CTR/ENABLE_LCD_CONTRAST_OPTION/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_VOL/ENABLE_SYSTEM_INFO_MENU/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN_CA/ENABLE_FREQUENCY_LOCK_REGION_CA/g' "$file"
    sed -i 's/ENABLE_FEAT_F4HWN/ENABLE_CUSTOM_FIRMWARE_MODS/g' "$file"
done < files_list.txt

rm files_list.txt
