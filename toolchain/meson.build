project('deltafw', 'c', 'cpp',
  version : run_command('python3', 'config_parser.py', '../config.toml', 'get_version', check : true).stdout().strip(),
  default_options : [
    'warning_level=1',
    'c_std=c11',
    'cpp_std=c++11',
    'buildtype=minsize', 
    'b_staticpic=false',
    'b_lto=true',
  ])

# Compiler configuration
cc = meson.get_compiler('c')
objcopy = find_program('objcopy')
size_prog = find_program('size')

# Project Configuration from config.toml
python3 = find_program('python3')
config_parser = files('config_parser.py')
config_toml = files('../config.toml')

project_version = run_command(python3, config_parser, config_toml, 'get_version', check : true).stdout().strip()
project_name = run_command(python3, config_parser, config_toml, 'get_name', check : true).stdout().strip()
project_authors = run_command(python3, config_parser, config_toml, 'get_authors', check : true).stdout().strip()

# Definitions
defines = []
extra_inc = []

# Git Hash and Build Date
git = find_program('git', required : false)
if git.found()
  git_hash = run_command(git, 'rev-parse', '--short', 'HEAD', check : false).stdout().strip()
  if git_hash == ''
    git_hash = 'unknown'
  endif
else
  git_hash = 'unknown'
endif

date = find_program('date', required : false)
if date.found()
  build_date = run_command(date, '+%Y-%m-%d %H:%M:%S', check : false).stdout().strip()
  build_date_file = run_command(date, '+%d%m%Y', check : false).stdout().strip()
else
  build_date = 'unknown'
  build_date_file = 'unknown'
endif

defines += '-DGIT_COMMIT_HASH="' + git_hash + '"'
defines += '-DBUILD_DATE="' + build_date + '"'
defines += '-DAUTHOR_STRING="' + project_name + '"'
defines += '-DVERSION_STRING="v' + project_version + '"'
defines += '-DAUTHOR_STRING_2="' + project_authors + '"'
defines += '-DVERSION_STRING_2="v' + project_version + '"'
defines += '-DEDITION_STRING="' + get_option('EDITION_STRING') + '"'

# Common Flags
common_flags = [
  '-mcpu=cortex-m0plus',
  '-mthumb',
  '-fno-common',
  '-ffunction-sections',
  '-fdata-sections',
  '-Wno-type-limits',
  '-fno-math-errno',
  '-fno-trapping-math',
  '-fmerge-all-constants',
  '-fno-asynchronous-unwind-tables'
]

add_project_arguments(common_flags, language : ['c', 'cpp'])
add_project_link_arguments(common_flags, language : ['c', 'cpp'])

# Linker Flags
linker_script = meson.current_source_dir() / '../src/core/py32f071xb.ld'
link_args = [
  '-T' + linker_script,
  '--specs=nano.specs',
  '-Wl,--gc-sections',
  '-Wl,--print-memory-usage',
  '-Wl,--no-warn-rwx-segment',
  '-Wl,-Map=deltafw.map',
  '-lm'
]

# Include Directories
inc_dirs = include_directories(
  '..',
  '../src',
  '../src/core',
  '../src/core/Inc',
  '../src/drivers/hal/Inc',
  '../src/drivers/cmsis/Include',
  '../src/drivers/cmsis/Device/PY32F071/Include',
  '../src/usb'
)

# Source Files
sources = files(
  # Main Entry
  '../src/core/main.c',

  # Core
  '../src/core/init.c',
  '../src/core/misc.c',
  '../src/core/scheduler.c',
  '../src/core/version.c',
  '../src/core/board.c',
  '../src/features/audio/audio.c',
  '../src/features/radio/radio.c',
  '../src/features/radio/frequencies.c',
  '../src/features/dcs/dcs.c',
  '../src/apps/settings/settings.c',
  '../src/apps/settings/settings_ui.c',
  '../src/features/radio/functions.c',
  '../src/ui/bitmaps.c',
  '../src/ui/font.c',

  # Features
  '../src/features/action/action.c',
  '../src/features/app/app.c',
  '../src/apps/scanner/chFrScanner.c',
  '../src/features/common/common.c',
  '../src/features/dtmf/dtmf.c',
  '../src/features/generic/generic.c',
  '../src/features/main_screen/main_screen.c',
  '../src/features/menu/menu.c',
  '../src/apps/security/passcode.c',
  '../src/features/storage/storage.c',
  '../src/apps/scanner/scanner.c',

  # Drivers BSP
  '../src/drivers/bsp/adc.c',
  '../src/drivers/bsp/backlight.c',
  '../src/drivers/bsp/bk4829.c',
  '../src/drivers/bsp/py25q16.c',
  '../src/drivers/bsp/gpio.c',
  '../src/drivers/bsp/i2c.c',
  '../src/drivers/bsp/keyboard.c',
  '../src/drivers/bsp/st7565.c',
  '../src/drivers/bsp/system.c',
  '../src/drivers/bsp/systick.c',

  # Apps
  '../src/apps/battery/battery.c',
  '../src/apps/battery/battery_ui.c',
  '../src/apps/memories/memories.c',
  '../src/apps/sysinfo/sysinfo.c',
  '../src/apps/launcher/launcher.c',
  '../src/apps/boot/boot.c',

  # UI
  '../src/ui/helper.c',
  '../src/ui/inputbox.c',
  '../src/ui/main.c',
  '../src/ui/bar.c',
  '../src/ui/menu.c',
  '../src/apps/scanner/scanner_ui.c',
  '../src/ui/status.c',
  '../src/ui/ui.c',
  '../src/ui/hexdump.c',
  '../src/ui/ag_graphics.c',
  '../src/ui/ag_menu.c',
  '../src/ui/textinput.c',
  '../src/ui/freqinput.c',
  '../src/apps/boot/welcome.c',

  # Third Party

  # Startup
  '../src/core/startup_py32f071xx.s',

  # Core Libs / Interrupts
  '../src/core/Src/main.c',
  '../src/core/Src/py32f071_it.c',
  '../src/core/Src/system_py32f071.c',

  # HAL
  '../src/drivers/hal/Src/py32f071_ll_rcc.c',
  '../src/drivers/hal/Src/py32f071_ll_pwr.c',
  '../src/drivers/hal/Src/py32f071_ll_dma.c',
  '../src/drivers/hal/Src/py32f071_ll_gpio.c',
  '../src/drivers/hal/Src/py32f071_ll_utils.c',
  '../src/drivers/hal/Src/py32f071_ll_adc.c',
  '../src/drivers/hal/Src/py32f071_ll_comp.c',
  '../src/drivers/hal/Src/py32f071_ll_crc.c',
  '../src/drivers/hal/Src/py32f071_ll_dac.c',
  '../src/drivers/hal/Src/py32f071_ll_exti.c',
  '../src/drivers/hal/Src/py32f071_ll_i2c.c',
  '../src/drivers/hal/Src/py32f071_ll_rtc.c',
  '../src/drivers/hal/Src/py32f071_ll_spi.c',
  '../src/drivers/hal/Src/py32f071_ll_tim.c',
  '../src/drivers/hal/Src/py32f071_ll_lptim.c',
  '../src/drivers/hal/Src/py32f071_ll_usart.c',
)

defines += '-DPY32F071x8'
defines += '-DUSE_FULL_LL_DRIVER'

# Options Processing
if get_option('CUSTOM_FIRMWARE_MODS')
    defines += '-DENABLE_CUSTOM_FIRMWARE_MODS'
    defines += '-DALERT_TOT=10'
endif

if get_option('AIRCOPY')
  defines += '-DENABLE_AIRCOPY'
  sources += files('../src/apps/aircopy/aircopy.c', '../src/apps/aircopy/aircopy_ui.c')
endif

if get_option('UART')
  defines += '-DENABLE_UART'
  sources += files('../src/drivers/bsp/uart.c', '../src/features/uart/uart.c')
endif

if get_option('USB')
  defines += '-DENABLE_USB'
  sources += files(
    '../src/drivers/bsp/vcp.c', 
    '../src/usb/usbd_cdc_if.c',
    '../src/middlewares/CherryUSB/core/usbd_core.c',
    '../src/middlewares/CherryUSB/port/usb_dc_py32.c',
    '../src/middlewares/CherryUSB/class/cdc/usbd_cdc.c'
  )
  extra_inc += include_directories(
    '../src/middlewares/CherryUSB/core',
    '../src/middlewares/CherryUSB/port',
    '../src/middlewares/CherryUSB/common',
    '../src/middlewares/CherryUSB/class/cdc'
  )
  # We can't append to inc_dirs easily without reassigning or merging lists, 
  # but here we can just add it to the executable include_directories list? 
  # Actually, inc_dirs is an IncludeDirs object. We can't merge them in Meson < 0.60 easily?
  # Meson allows lists of include_directories.
  # So we will add it to a list 'extra_inc_dirs' and pass [inc_dirs, extra_inc_dirs] to executable.
  # Or just add these paths to the main inc_dirs definition?
  # Modifying the main inc_dirs definition is cleaner but validation-wise tricky if file structure changes.
  # Let's just redefine inc_dirs to include these, or append?
  # Since inc_dirs is defined earlier, we can't append strings to it.
  # We will clear this issue by adding a new variable specific for USB includes and pass it to executable.
endif

if get_option('AIRCOPY') or get_option('UART') or get_option('USB')
  sources += files('../src/drivers/bsp/crc.c', '../src/drivers/bsp/eeprom_compat.c')
endif

if get_option('SERIAL_SCREENCAST')
  defines += '-DENABLE_SERIAL_SCREENCAST'
  sources += files('../src/features/screencast/screencast.c')
endif

if get_option('UART_CMD_RSSI')
  defines += '-DENABLE_UART_CMD_RSSI'
endif

if get_option('UART_CMD_BATT')
  defines += '-DENABLE_UART_CMD_BATT'
endif

if get_option('UART_CMD_ID')
  defines += '-DENABLE_UART_CMD_ID'
endif

if get_option('EXTRA_UART_CMD')
  defines += '-DENABLE_EXTRA_UART_CMD'
endif

if get_option('BK1080')
  defines += '-DENABLE_BK1080'
  sources += files('../src/drivers/bsp/bk1080.c')
endif

if get_option('FMRADIO')
  defines += '-DENABLE_FMRADIO'
  sources += files('../src/apps/fm/fm.c', '../src/apps/fm/fm_ui.c')
endif



if get_option('VOICE')
  defines += '-DENABLE_VOICE'
  sources += files('../src/drivers/bsp/voice.c')
endif

if get_option('PWRON_PASSWORD')
  defines += '-DENABLE_PWRON_PASSWORD'
  sources += files('../src/ui/lock.c')
endif

if get_option('FLASHLIGHT')
  defines += '-DENABLE_FLASHLIGHT'
  sources += files('../src/features/flashlight/flashlight.c')
endif

if get_option('ALARM')
  defines += '-DENABLE_ALARM'
endif

if get_option('TX1750')
  defines += '-DENABLE_TX1750'
endif

if get_option('VOX')
  defines += '-DENABLE_VOX'
endif

if get_option('NOAA')
  defines += '-DENABLE_NOAA'
endif

if get_option('NARROWER_BW_FILTER')
  defines += '-DENABLE_NARROWER_BW_FILTER'
endif

if get_option('WIDE_RX')
  defines += '-DENABLE_WIDE_RX'
endif

if get_option('TX_NON_FM')
  defines += '-DENABLE_TX_NON_FM'
endif

if get_option('SQUELCH_MORE_SENSITIVE')
  defines += '-DENABLE_SQUELCH_MORE_SENSITIVE'
endif

if get_option('CTCSS_TAIL_PHASE_SHIFT')
  defines += '-DENABLE_CTCSS_TAIL_PHASE_SHIFT'
endif

if get_option('REDUCE_LOW_MID_TX_POWER')
  defines += '-DENABLE_REDUCE_LOW_MID_TX_POWER'
endif

if get_option('BYP_RAW_DEMODULATORS')
  defines += '-DENABLE_BYP_RAW_DEMODULATORS'
endif

if get_option('RESCUE_OPERATIONS')
  defines += '-DENABLE_RESCUE_OPERATIONS'
endif

if get_option('SCAN_RANGES')
  defines += '-DENABLE_SCAN_RANGES'
endif

if get_option('BIG_FREQ')
  defines += '-DENABLE_BIG_FREQ'
endif

if get_option('SMALL_BOLD')
  defines += '-DENABLE_SMALL_BOLD'
endif

if get_option('INVERTED_LCD_MODE')
  defines += '-DENABLE_INVERTED_LCD_MODE'
endif

if get_option('LCD_CONTRAST_OPTION')
  defines += '-DENABLE_LCD_CONTRAST_OPTION'
endif

if get_option('RSSI_BAR')
  defines += '-DENABLE_RSSI_BAR'
endif

if get_option('MIC_BAR')
  defines += '-DENABLE_MIC_BAR'
endif

if get_option('SHOW_CHARGE_LEVEL')
  defines += '-DENABLE_SHOW_CHARGE_LEVEL'
endif

if get_option('USBC_CHARGING_INDICATOR')
  defines += '-DENABLE_USBC_CHARGING_INDICATOR'
endif

if get_option('REVERSE_BAT_SYMBOL')
  defines += '-DENABLE_REVERSE_BAT_SYMBOL'
endif

if get_option('BOOT_BEEPS')
  defines += '-DENABLE_BOOT_BEEPS'
endif

if get_option('BOOT_RESUME_STATE')
  defines += '-DENABLE_BOOT_RESUME_STATE'
endif

if get_option('DEEP_SLEEP_MODE')
  defines += '-DENABLE_DEEP_SLEEP_MODE'
endif

if get_option('RX_TX_TIMER_DISPLAY')
  defines += '-DENABLE_RX_TX_TIMER_DISPLAY'
endif

if get_option('NO_CODE_SCAN_TIMEOUT')
  defines += '-DENABLE_NO_CODE_SCAN_TIMEOUT'
endif

if get_option('BLMIN_TMP_OFF')
  defines += '-DENABLE_BLMIN_TMP_OFF'
endif

if get_option('KEEP_MEM_NAME')
  defines += '-DENABLE_KEEP_MEM_NAME'
endif

if get_option('COPY_CHAN_TO_VFO')
  defines += '-DENABLE_COPY_CHAN_TO_VFO'
endif

if get_option('CUSTOM_MENU_LAYOUT')
  defines += '-DENABLE_CUSTOM_MENU_LAYOUT'
endif

if get_option('CW_KEYER')
  defines += '-DENABLE_CW_KEYER'
  sources += files(
    '../src/features/cw/cw.c',
    '../src/features/cw/cw_ui.c'
  )
endif

if get_option('DTMF_CALLING')
  defines += '-DENABLE_DTMF_CALLING'
endif

if get_option('SPECTRUM')
  defines += '-DENABLE_SPECTRUM'
  if get_option('SPECTRUM_WATERFALL')
    defines += '-DENABLE_SPECTRUM_WATERFALL'
    defines += '-DENABLE_SPECTRUM_ADVANCED'
    sources += files('../src/apps/spectrum/spectrum_waterfall.c')
  else
    sources += files('../src/apps/spectrum/spectrum.c')
  endif
 
  if get_option('SPECTRUM_EXTRA_VALUES')
    defines += '-DENABLE_SPECTRUM_EXTRA_VALUES'
  endif
  if get_option('SPECTRUM_EXTENSIONS')
    defines += '-DENABLE_SPECTRUM_EXTENSIONS'
  endif
endif
if get_option('APP_BREAKOUT_GAME')
  defines += '-DENABLE_APP_BREAKOUT_GAME'
  sources += files('../src/features/breakout/breakout.c')
endif

if get_option('REGA')
  defines += '-DENABLE_REGA'
  sources += files('../src/features/rega/rega.c')
endif

if get_option('LIVESEEK')
  defines += '-DENABLE_LIVESEEK'
  sources += files('../src/apps/liveseek/liveseek.c')
endif

if get_option('PMR446_FREQUENCY_BAND')
  defines += '-DENABLE_PMR446_FREQUENCY_BAND'
endif

if get_option('GMRS_FRS_MURS_BANDS')
  defines += '-DENABLE_GMRS_FRS_MURS_BANDS'
endif

if get_option('FREQUENCY_LOCK_REGION_CA')
  defines += '-DENABLE_FREQUENCY_LOCK_REGION_CA'
endif

if get_option('SYSTEM_INFO_MENU')
  defines += '-DENABLE_SYSTEM_INFO_MENU'
endif

if get_option('F_CAL_MENU')
  defines += '-DENABLE_F_CAL_MENU'
endif

if get_option('RESET_CHANNEL_FUNCTION')
  defines += '-DENABLE_RESET_CHANNEL_FUNCTION'
endif

if get_option('EEPROM_HEXDUMP')
  defines += '-DENABLE_EEPROM_HEXDUMP'
endif

if get_option('FIRMWARE_DEBUG_LOGGING')
  defines += '-DENABLE_FIRMWARE_DEBUG_LOGGING'
endif

if get_option('AM_FIX')
  defines += '-DENABLE_AM_FIX'
  sources += files('../src/features/am_fix/am_fix.c')
endif

if get_option('AM_FIX_SHOW_DATA')
  defines += '-DENABLE_AM_FIX_SHOW_DATA'
endif

if get_option('BATTERY_CHARGING')
  defines += '-DENABLE_BATTERY_CHARGING'
endif

if get_option('IDENTIFIER')
  defines += '-DENABLE_IDENTIFIER'
  sources += files('../src/helper/identifier.c')
endif

if get_option('CRYPTO')
  defines += '-DENABLE_CRYPTO'
  sources += files(
    '../src/helper/crypto.c',
    '../src/helper/chacha20.c',
    '../src/helper/poly1305.c',
    '../src/helper/trng.c'
  )
endif

if get_option('STORAGE_ENCRYPTION')
  defines += '-DENABLE_STORAGE_ENCRYPTION'
endif

if get_option('PASSCODE')
  defines += '-DENABLE_PASSCODE'
endif

if get_option('TRNG_SENSORS')
  defines += '-DENABLE_TRNG_SENSORS'
endif

if get_option('AGC_SHOW_DATA')
  defines += '-DENABLE_AGC_SHOW_DATA'
endif

if get_option('UART_RW_BK_REGS')
  defines += '-DENABLE_UART_RW_BK_REGS'
endif

if get_option('SWD')
  defines += '-DENABLE_SWD'
endif

if get_option('FASTER_CHANNEL_SCAN')
  defines += '-DENABLE_FASTER_CHANNEL_SCAN'
endif

if get_option('SCRAMBLER')
  defines += '-DENABLE_SCRAMBLER'
endif

if get_option('ON_DEVICE_PROGRAMMING')
  defines += '-DENABLE_ON_DEVICE_PROGRAMMING'
endif

if get_option('SCAN_LIST_EDITING')
  defines += '-DENABLE_SCAN_LIST_EDITING'
endif

if get_option('TX_OFFSET')
  defines += '-DENABLE_TX_OFFSET'
endif

if get_option('TX_SOFT_START')
  defines += '-DENABLE_TX_SOFT_START'
  sources += files('../src/features/tx/tx_soft_start.c')
endif

if get_option('TX_AUDIO_COMPRESSOR')
  defines += '-DENABLE_TX_AUDIO_COMPRESSOR'
  sources += files('../src/features/tx/tx_compressor.c')
endif

if get_option('CTCSS_LEAD_IN')
  defines += '-DENABLE_CTCSS_LEAD_IN'
  sources += files('../src/features/tx/ctcss_lead.c')
endif

if get_option('SIGNAL_CLASSIFIER')
  defines += '-DENABLE_SIGNAL_CLASSIFIER'
  sources += files('../src/features/rx/signal_classifier.c')
endif

if get_option('SMART_SQUELCH')
  defines += '-DENABLE_SMART_SQUELCH'
  sources += files('../src/features/rx/smart_squelch.c')
endif

if get_option('SQUELCH_TAIL_ELIMINATION')
  defines += '-DENABLE_SQUELCH_TAIL_ELIMINATION'
  sources += files('../src/features/rx/squelch_tail.c')
endif

if get_option('SCAN_WATCH')
  defines += '-DENABLE_SCAN_WATCH'
  sources += files('../src/features/scan/scanwatch.c')
endif

if get_option('INTELLIGENT_DUAL_WATCH')
  defines += '-DENABLE_INTELLIGENT_DUAL_WATCH'
  sources += files('../src/features/scan/dual_watch_mgmt.c')
endif

if get_option('EXTRA_ROGER')
  defines += '-DENABLE_EXTRA_ROGER'
endif

if get_option('CUSTOM_ROGER')
  defines += '-DENABLE_CUSTOM_ROGER'
endif

if get_option('ANTENNA_SIGNAL_BAR')
  defines += '-DENABLE_ANTENNA_SIGNAL_BAR'
  sources += files('../src/features/rx/signal_quality.c')
endif

if get_option('MESH_NETWORK')
  defines += '-DENABLE_MESH_NETWORK'
  # Core Hermes sources (always compiled with MESH_NETWORK)
  sources += files(
    '../src/apps/hermes/hermes.c',
    '../src/apps/hermes/ui/hermes_ui.c',
    # Physical Layer
    '../src/apps/hermes/physical/phy.c',
    # Data-Link Layer
    '../src/apps/hermes/datalink/whitening.c',
    '../src/apps/hermes/datalink/framing.c',
    '../src/apps/hermes/datalink/fec.c',
    # Network Layer (addressing always needed)
    '../src/apps/hermes/network/addressing.c',
    # Transport Layer
    '../src/apps/hermes/transport/packet.c',
    '../src/apps/hermes/transport/csma.c',
    # Security Layer
    '../src/apps/hermes/security/crypto_engine.c',
    '../src/apps/hermes/security/kdf.c',
    '../src/apps/hermes/security/seal.c'
  )
  # Optional modules gated by sub-flags
  if get_option('HERMES_MESSENGER')
    defines += '-DENABLE_HERMES_MESSENGER'
    sources += files(
      '../src/apps/hermes/app/messaging.c',
      '../src/apps/hermes/transport/reliability.c'
    )
  endif
  if get_option('HERMES_ROUTER')
    defines += '-DENABLE_HERMES_ROUTER'
    sources += files(
      '../src/apps/hermes/network/routing.c',
      '../src/apps/hermes/network/fragmentation.c'
    )
  endif
  if get_option('HERMES_PING')
    defines += '-DENABLE_HERMES_PING'
    sources += files('../src/apps/hermes/app/ping.c')
  endif
  if get_option('HERMES_TELEMETRY')
    defines += '-DENABLE_HERMES_TELEMETRY'
    sources += files('../src/apps/hermes/app/telemetry.c')
  endif
  if get_option('HERMES_DISCOVERY')
    defines += '-DENABLE_HERMES_DISCOVERY'
    sources += files('../src/apps/hermes/app/discovery.c')
  endif
  if get_option('HERMES_ACK_HEALTH')
    defines += '-DENABLE_HERMES_ACK_HEALTH'
    sources += files(
      '../src/apps/hermes/app/ack.c',
      '../src/apps/hermes/transport/lqi.c'
    )
  endif
endif

# Other Defines
defines += '-DSQL_TONE=550'

# Create Executable
elf = executable('deltafw',
  sources,
  c_args : defines,
  link_args : link_args,
  include_directories : [inc_dirs, extra_inc],
  name_suffix : 'elf'
)

# Custom Targets for Binary Generation
bin_target = custom_target('bin',
  output : 'deltafw.bin',
  input : elf,
  command : [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
  build_by_default : true
)

custom_target('hex',
  output : 'deltafw.hex',
  input : elf,
  command : [objcopy, '-O', 'ihex', '@INPUT@', '@OUTPUT@'],
  build_by_default : true
)

# QSH Packer script reference
qsh_packer = files('qsh_packer.py')
