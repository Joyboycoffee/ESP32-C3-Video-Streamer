# 📺 ESP32-C3 Wireless Video Streamer

[![ESP32-C3](https://img.shields.io/badge/Hardware-ESP32--C3%20SuperMini-E7352C.svg?style=flat-square&logo=espressif)](https://www.espressif.com/)
[![Display ST7789](https://img.shields.io/badge/Display-ST7789%20240x240%20IPS-00F0FF.svg?style=flat-square)](https://github.com/lovyan03/LovyanGFX)
[![Display SSD1306](https://img.shields.io/badge/Display-SSD1306%20128x64%20OLED-39FF14.svg?style=flat-square)](https://github.com/adafruit/Adafruit_SSD1306)
[![Python](https://img.shields.io/badge/Streamer-Python%203.10%2B-blue.svg?style=flat-square&logo=python)](https://www.python.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)

Stream any **MP4, AVI, MKV, or GIF** video from your computer in real-time over **Wi-Fi (UDP Port 4210)** to an **ESP32-C3** microcontroller driving a tiny display!

Supports both:
1. **1.3" 240×240 IPS ST7789 LCD** (High-speed hardware SPI DMA @ 40MHz via LovyanGFX).
2. **0.96" 128×64 Monochrome SSD1306 OLED** (Fast I2C @ 400kHz via Adafruit SSD1306).

---

## ⚡ How It Works

```
[ MP4 on PC / Laptop ]
         │
         ▼
[ Python: OpenCV + Pillow ]
  • Sequential frame decoding (no seek lag)
  • Aspect ratio fitting: Crop or Letterbox
  • Contrast enhancement & Floyd-Steinberg dithering
  • Page bit-packing
         │
         ▼
[ UDP Packets over Wi-Fi (Port 4210) ]
         │
         ▼
[ ESP32-C3 Receiver ]
         │
    ┌────┴──────────────────────────┐
    ▼                               ▼
[ ST7789 240x240 IPS ]     [ SSD1306 128x64 OLED ]
High-Speed SPI DMA @ 40MHz    Fast I2C @ 400kHz
```

---

## 🛠️ Hardware & Pinout

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

## 🚀 Quick Start Guide

### Step 1: Install Python Requirements

On your PC, open PowerShell / Terminal:

```powershell
py -m pip install -r requirements.txt
```
*(Installs `opencv-python`, `Pillow`, and `numpy`)*

---

### Step 2: Flash ESP32-C3 Firmware

#### Option A: Using Arduino IDE
1. Open the sketch matching your display:
   - For **ST7789 240x240 IPS**: Open `firmware_c3_st7789/firmware_c3_st7789.ino`
     *(Install the **LovyanGFX** library from Library Manager)*
   - For **SSD1306 128x64 OLED**: Open `firmware_c3_ssd1306/firmware_c3_ssd1306.ino`
     *(Install **Adafruit SSD1306** and **Adafruit GFX** from Library Manager)*
2. In the sketch, update your Wi-Fi credentials:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_NAME";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   ```
3. Select Board: **ESP32C3 Dev Module** (or ESP32-C3).
4. Upload the code and open Serial Monitor (`115200` baud).
5. The screen will display: **`WAITING FOR VIDEO // IP: 192.168.x.x`**. Note this IP!

#### Option B: Using PlatformIO
```powershell
# For ST7789 IPS Display:
pio run -e esp32c3_st7789 -t upload

# For SSD1306 OLED Display:
pio run -e esp32c3_ssd1306 -t upload
```

---

### Step 3: Test UDP Connection (Packet Test)

Before trying a video file, verify UDP delivery with the test script:

```powershell
py test_send.py --ip 192.168.1.47
```
*(Replace `192.168.1.47` with your actual ESP32-C3 IP)*

- If the screen flashes between **black** and **white**, your Wi-Fi path and UDP port 4210 are 100% working!

---

### Step 4: Stream Any Video!

Place your video file (`video.mp4`) in the repository folder and run:

```powershell
py streamer.py video.mp4 --ip 192.168.1.47 --fps 12
```

#### Advanced Streaming Options:

```powershell
# 1. Native High-Res 240x240 for ST7789:
py streamer.py video.mp4 --ip 192.168.1.47 --fps 12 --display st7789-native

# 2. Letterbox mode (preserves entire aspect ratio for 16:9 videos):
py streamer.py video.mp4 --ip 192.168.1.47 --fps 12 --fit letterbox

# 3. High-contrast threshold mode (great for clean logos or cartoons):
py streamer.py video.mp4 --ip 192.168.1.47 --fps 12 --mode threshold

# 4. Higher frame rate (smoother action):
py streamer.py video.mp4 --ip 192.168.1.47 --fps 18
```

Press **`Ctrl + C`** in your terminal anytime to stop streaming.

---

## 🎛️ Protocols Supported

| Protocol | Magic Header | Resolution | Packet Size | Description |
| :--- | :--- | :--- | :--- | :--- |
| **`VID0`** | `VID0` | 128×64 | 1,028 bytes | Standard Netlify/SSD1306 protocol. Rendered directly on OLED, or upscaled with Cyber HUD on ST7789. |
| **`V240`** | `V240` | 240×240 | 1,208 bytes | Native high-res chunked protocol (6 chunks of 40 lines) for full-screen ST7789 IPS playback. |

---

## 🔍 Troubleshooting

| Symptom | Cause & Solution |
| :--- | :--- |
| **Display shows "WAITING FOR VIDEO"** | Packets are not reaching the ESP32. Verify the IP using `test_send.py` and ensure your PC and ESP32 are on the same Wi-Fi network (no AP client isolation). |
| **Python sends frames but display is unchanged** | Check your Windows Firewall. Ensure outbound UDP traffic on port 4210 is allowed. |
| **Choppy or stuttering video** | Lower the frame rate using `--fps 10` or `--fps 8`. 2.4GHz Wi-Fi congestion can cause packet drops. |
| **OLED display remains black** | Check I2C wiring (SDA = GPIO 8, SCL = GPIO 9) and verify I2C address (`0x3C` vs `0x3D`). |
| **ST7789 colors are inverted** | Set `cfg.invert = true;` in `display_setup.h`. |
| **OpenCV H.264 decode error** | Convert video with ffmpeg: `ffmpeg -i video.mp4 -vf "fps=12" -c:v mjpeg test.avi` and stream `test.avi`. |

---

## 👤 Author & Acknowledgments

- **Lead Developer**: [Gourav Kumar](https://github.com/Joyboycoffee)
- **Protocol Foundation**: Inspired by the SSD1306 monochrome dithered UDP video concept.
- **License**: MIT
