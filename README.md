# 📺 ESP32-C3 Wireless Video Streamer

[![ESP32-C3](https://img.shields.io/badge/Hardware-ESP32--C3%20SuperMini-E7352C.svg?style=flat-square&logo=espressif)](https://www.espressif.com/)
[![Web App](https://img.shields.io/badge/Web%20App-GitHub%20Pages-00F0FF.svg?style=flat-square&logo=github)](https://joyboycoffee.github.io/ESP32-C3-Video-Streamer/)
[![Windows EXE](https://img.shields.io/badge/Windows-Standalone%20EXE-0078D6.svg?style=flat-square&logo=windows)](dist/)
[![Display ST7789](https://img.shields.io/badge/Display-ST7789%20240x240%20IPS-00F0FF.svg?style=flat-square)](https://github.com/lovyan03/LovyanGFX)
[![Display SSD1306](https://img.shields.io/badge/Display-SSD1306%20128x64%20OLED-39FF14.svg?style=flat-square)](https://github.com/adafruit/Adafruit_SSD1306)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)

Stream any **MP4, WEBM, AVI, MOV, or GIF** video from your phone or PC in real-time over **Wi-Fi (WebSocket & UDP)** directly to an **ESP32-C3** driving a tiny display!

Supports both:
1. **1.3" 240×240 IPS ST7789 LCD** (High-speed hardware SPI DMA @ 40MHz via LovyanGFX) — *the keychain display*.
2. **0.96" 128×64 Monochrome SSD1306 OLED** (Fast I2C @ 400kHz via Adafruit SSD1306).

---

## 🌐 1-Click Stream from Any Mobile or PC Browser

No installation or Python needed! You can stream videos or your live camera directly from your browser:

👉 **[Launch Web Streamer on GitHub Pages](https://joyboycoffee.github.io/ESP32-C3-Video-Streamer/)**

- **iPhone & Android**: Works in Safari, Chrome, and Samsung Internet.
- **PC & Mac**: Works in Chrome, Edge, and Firefox.
- **Offline Hotspot**: The exact same web deck is hosted directly inside the ESP32 at `http://192.168.4.1/`.

---

## 📡 Built-In Wi-Fi Hotspot & Wi-Fi Manager

If no Wi-Fi credentials have been configured or if connection to your home network fails, the ESP32-C3 automatically launches its own standalone Wi-Fi hotspot:

1. **Connect your phone or PC to the Hotspot**:
   - **Network Name (SSID)**: `ESP32-C3-Streamer`
   - **Password**: *(Open / None)*
2. **Open the Web Deck in your browser**:
   - Navigate to: **`http://192.168.4.1`**
3. **Stream immediately**, or tap **`⚙️ WI-FI CONFIG`** to scan your nearby home/mobile hotspot networks, enter your password, and save. The ESP32 will reboot and connect to your home Wi-Fi!

---

## 💻 Standalone Windows Application (`.exe`)

For Windows users who prefer a desktop app without opening a browser:
- Pre-built binary available in `dist/ESP32-C3-Video-Streamer.exe`.
- Run `build_exe.bat` to rebuild with PyInstaller anytime.
- Supports both a graphical dark-mode window and command-line usage.

---

## 🔌 Hardware Wiring & Pinout

### 1. ESP32-C3 SuperMini ➜ 1.3" ST7789 240×240 IPS Display (SPI)

| Display Pin (7-Pin ST7789) | ESP32-C3 Pin | Function / Description |
| :--- | :--- | :--- |
| **VCC** | **3.3V** | Power (3.3V) |
| **GND** | **GND** | Ground |
| **SCL / SCK** | **GPIO 6** | SPI Clock (40MHz DMA) |
| **SDA / MOSI** | **GPIO 10** | SPI Data (MOSI) |
| **RES / RST** | **GPIO 5** | Hardware Reset |
| **DC** | **GPIO 3** | Data / Command Select |
| **BLK / BL** | **GPIO 4** | Backlight Control |
| **CS** | *N/A (Tied to GND)* | Chip Select (Not used on 7-pin module) |

---

### 2. ESP32-C3 SuperMini ➜ 0.96" SSD1306 128×64 OLED (I2C)

| OLED Pin | ESP32-C3 Pin | Description |
| :--- | :--- | :--- |
| **VCC** | **3.3V** | Power (3.3V) |
| **GND** | **GND** | Ground |
| **SDA** | **GPIO 8** | I2C Data |
| **SCL** | **GPIO 9** | I2C Clock (400kHz) |

*(I2C Address: `0x3C` default, or `0x3D`)*

---

## 🚀 Getting Started

### 1. Flash the ESP32-C3
- **Arduino IDE**: Open `firmware_c3_st7789/firmware_c3_st7789.ino` (requires `LovyanGFX` library).
- **PlatformIO**: `pio run -e esp32c3_st7789 -t upload`

### 2. Start Streaming!

#### Option A: From Phone / Browser (Easiest)
1. Connect to Wi-Fi `ESP32-C3-Streamer`.
2. Open `http://192.168.4.1` (or [GitHub Pages](https://joyboycoffee.github.io/ESP32-C3-Video-Streamer/)).
3. Pick any video or tap **`📷 CAMERA`** to stream live video!

#### Option B: Standalone Windows App
Double-click `dist/ESP32-C3-Video-Streamer.exe` (or run `py streamer_gui.py`).

#### Option C: Python CLI
```powershell
py streamer.py video.mp4 --ip 192.168.4.1 --fps 15
```

---

## 👤 Author & Acknowledgments

- **Lead Developer**: [Gourav Kumar](https://github.com/Joyboycoffee)
- **License**: MIT
