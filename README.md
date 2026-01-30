<p align="center">
  <h1 align="center">⚡ deltafw</h1>
  <p align="center">
    <strong>CFW for Quansheng UV-K1 & UV-K5 V3 radios</strong>
  </p>
  <p align="center">
    <em>Elegant UX • Extra Features • Open Source</em>
  </p>
</p>

<p align="center">
  <a href="https://github.com/qshosfw/deltafw-k1-k5v3/actions/workflows/main.yml">
    <img src="https://github.com/qshosfw/deltafw-k1-k5v3/actions/workflows/main.yml/badge.svg" alt="Build Status">
  </a>
  <a href="https://github.com/qshosfw/deltafw-k1-k5v3/releases">
    <img src="https://img.shields.io/github/v/release/qshosfw/deltafw-k1-k5v3?include_prereleases" alt="Latest Release">
  </a>
  <a href="https://github.com/qshosfw/deltafw-k1-k5v3/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/qshosfw/deltafw-k1-k5v3" alt="License">
  </a>
  <a href="https://github.com/qshosfw/deltafw-k1-k5v3/issues">
    <img src="https://img.shields.io/github/issues/qshosfw/deltafw-k1-k5v3" alt="Issues">
  </a>
  <a href="https://github.com/qshosfw/deltafw-k1-k5v3/stargazers">
    <img src="https://img.shields.io/github/stars/qshosfw/deltafw-k1-k5v3?style=flat" alt="Stars">
  </a>
</p>

---

> [!WARNING]
> **Use at your own risk.** There is no guarantee this firmware will work on your radio. It may brick your device. Always backup your calibration data before flashing!

---

## 📷 Screenshots

| VFO Mode | Spectrum Analyzer | FM Radio |
|:--------:|:-----------------:|:--------:|
| ![VFO](images/MO_Classic_Classic.png) | ![Spectrum](images/Spectrum.png) | ![FM](images/FM.png) |
| **Classic dual-watch display** | **Real-time spectrum view** | **Built-in FM receiver** |

| Scan Mode | Menu | AirCopy |
|:---------:|:----:|:-------:|
| ![Scan](images/SCAN_ALL.png) | ![Menu](images/Menu.png) | ![AirCopy](images/AIR_COPY.png) |
| **Full frequency scanning** | **Intuitive menu system** | **Wireless config transfer** |

---

## ✨ Features

### 📻 Radio
- **AM Fix** — Dramatically improved AM reception quality
- **Spectrum Analyzer** — Real-time RF visualization (`F+5`)
- **Wide-band RX** — Extended frequency coverage
- **Fast Scanning** — Optimized scan speed
- **Scan Lists** — Multiple configurable scan groups

### 🖥️ Interface  
- **Modern UI** — Enhanced graphics with AG_Graphics system
- **Memories App** — Storage with T9 text input
- **System Info** — Version, commit hash, serial, battery status
- **Settings Menu** — Categorized option organization
- **Backlight Control** — Adjustable brightness and timeout

### ⚙️ Customization
- **Configurable Buttons** — Assign custom functions to keys
- **Display Modes** — Multiple VFO display styles
- **Battery Options** — Percentage or voltage on status bar
- **Squelch Tuning** — More sensitive squelch options

---

## 📦 Installation

### Download
Get the latest firmware from [**Releases**](https://github.com/qshosfw/deltafw-k1-k5v3/releases).

### Flash with UVTools2

1. **Backup** your calibration data first!
2. Put radio in **DFU mode** (hold `PTT` + `Power On`)
3. Connect via **USB-C**
4. Open [UVTools2](https://armel.github.io/uvtools2/)
5. Select the `.bin` firmware file
6. Click **Flash Firmware**

> [!TIP]
> **Fusion** is recommended for new UV-K1/K5 V3 radios with expanded flash.
> **Custom** is a balanced build for standard radios.

---

## 🔧 Building from Source

### Quick Start

```bash
# Clone the repository
git clone https://github.com/qshosfw/deltafw-k1-k5v3.git
cd deltafw

# See all available commands
make

# Build Fusion (recommended for new radios)
make fusion

# Interactive configuration menu
make menuconfig
```

### Build Commands

| Command | Description |
|---------|-------------|
| `make fusion` | Full features, for radios with expanded flash ✨ |
| `make custom` | Balanced features for standard radios |
| `make menuconfig` | Interactive build configurator |
| `make basic` | Minimal stable build |
| `make bandscope` | Spectrum analyzer focused |
| `make broadcast` | FM radio focused |
| `make all-presets` | Build all presets |
| `make clean` | Remove build artifacts |

### Build Output

Firmware files are generated in `build/<Preset>/`:
| File | Description |
|------|-------------|
| `deltafw.<preset>.bin` | Binary for flashing |
| `deltafw.<preset>.hex` | Intel HEX format |

### Prerequisites
- Docker installed and running
- Python 3 (for menuconfig)
- Bash environment (Linux, macOS, WSL)

---

## 🤝 Credits

This firmware builds upon the incredible work of the UV-K5 open-source community:

| Contributor | Contribution |
|-------------|--------------|
| [**DualTachyon**](https://github.com/DualTachyon) | Original open-source UV-K5 firmware |
| [**Egzumer**](https://github.com/egzumer) | Custom firmware with merged features |
| [**OneOfEleven**](https://github.com/OneOfEleven) | AM fix and numerous enhancements |
| [**F4HWN/Armel**](https://github.com/armel) | UV-K1/K5 V3 (PY32F071) port |
| [**Muzkr**](https://github.com/muzkr) | PY32F071 port collaboration |
| [**Fagci**](https://github.com/fagci) | Spectrum analyzer and graphics |

---

## 📚 Related Projects

Explore other UV-K5 firmware projects:

- 🔗 [egzumer/uv-k5-firmware-custom](https://github.com/egzumer/uv-k5-firmware-custom)
- 🔗 [fagci/uv-k5-firmware-fagci-mod](https://github.com/fagci/uv-k5-firmware-fagci-mod)
- 🔗 [OneOfEleven/uv-k5-firmware-custom](https://github.com/OneOfEleven/uv-k5-firmware-custom)
- 🔗 [spm81/MCFW_UV-K5_Open_Source_Firmware](https://github.com/spm81/MCFW_UV-K5_Open_Source_Firmware)

---

## ⚖️ License

**deltafw** is licensed under the [**GNU General Public License v3.0**](LICENSE).

This project incorporates prior work under various licenses:
- Original UV-K5 firmware by DualTachyon — Apache 2.0
- Contributions by Egzumer, OneOfEleven, F4HWN — Apache 2.0 / GPL-compatible
- Graphics and spectrum code by Fagci — MIT

All original authors retain copyright to their respective contributions. This project is a derivative work that combines, modifies, and extends the original codebases under GPL v3. See individual source files for specific attribution.

---

<p align="center">
  Made with ❤️ by <a href="https://github.com/qshosfw">qshosfw</a>
</p>
