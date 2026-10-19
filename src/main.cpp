#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// =========================================================================
// WI-FI & UDP STREAMING CONFIGURATION
// =========================================================================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
constexpr uint16_t UDP_PORT = 4210;

#if defined(USE_SSD1306)
// =========================================================================
// DRIVER: SSD1306 128x64 OLED (I2C)
// =========================================================================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PIN_OLED_SDA  8
#define PIN_OLED_SCL  9
#define OLED_I2C_ADDR 0x3C
constexpr size_t FRAME_SIZE = 1024;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WiFiUDP udp;
uint8_t packet[4 + FRAME_SIZE];
unsigned long frameCounter = 0;

void showMessage(const char* l1, const String& l2 = "", const String& l3 = "") {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(l1);
    display.println();
    if (l2.length() > 0) display.println(l2);
    if (l3.length() > 0) display.println(l3);
    display.display();
}

void setup() {
    Serial.begin(115200);
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setClock(400000);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("SSD1306 OLED init failed!");
        while (true) delay(1000);
    }
    showMessage("CONNECTING WI-FI...", String("SSID: ") + WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(300);
        Serial.print('.');
    }
    if (WiFi.status() != WL_CONNECTED) {
        showMessage("WiFi FAILED");
        return;
    }
    if (!udp.begin(UDP_PORT)) {
        showMessage("UDP FAILED");
        return;
    }
    showMessage("WAITING FOR VIDEO", String("IP: ") + WiFi.localIP().toString(), String("PORT: ") + String(UDP_PORT));
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) return;
    int len = udp.parsePacket();
    if (len <= 0) return;
    if (len != sizeof(packet)) {
        while (udp.available()) udp.read();
        return;
    }
    if (udp.read(packet, sizeof(packet)) != sizeof(packet)) return;
    if (memcmp(packet, "VID0", 4) != 0) return;
    memcpy(display.getBuffer(), packet + 4, FRAME_SIZE);
    display.display();
    frameCounter++;
    if (frameCounter % 30 == 0) {
        Serial.printf("[SSD1306] Rendered %lu frames from %s\n", frameCounter, udp.remoteIP().toString().c_str());
    }
}

#else
// =========================================================================
// DRIVER: ST7789 240x240 IPS TFT (SPI DMA via LovyanGFX)
// =========================================================================
#include "display_setup.h"

constexpr size_t VID0_PAYLOAD = 1024;
constexpr size_t VID0_PACKET  = 4 + VID0_PAYLOAD;

LGFX_ST7789 tft;
LGFX_Sprite videoSprite(&tft);
WiFiUDP udp;

uint8_t rxPacket[1500];
unsigned long frameCount = 0;
unsigned long lastFpsTime = 0;
float currentFps = 0.0f;
bool inStreamMode = false;
unsigned long lastFrameArrival = 0;

uint8_t colorTheme = 0;
const uint16_t THEME_COLORS[] = { TFT_WHITE, 0x07FF, 0x07E0, 0xFBE0 };

void drawWaitingScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, 240, 28, 0x18E3);
    tft.drawFastHLine(0, 28, 240, 0x07FF);
    tft.setTextColor(0x07FF);
    tft.setTextSize(1);
    tft.drawCenterString("ESP32-C3 // VIDEO STREAMER", 120, 8);
    
    tft.drawRect(15, 45, 210, 145, 0x3186);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawCenterString("WAITING FOR", 120, 65);
    tft.drawCenterString("VIDEO STREAM", 120, 88);
    
    tft.setTextSize(1);
    tft.setTextColor(0x07E0);
    tft.drawCenterString(String("IP: ") + WiFi.localIP().toString(), 120, 120);
    tft.setTextColor(0xFBE0);
    tft.drawCenterString(String("PORT: ") + String(UDP_PORT), 120, 140);
    tft.setTextColor(0x7BEF);
    tft.drawCenterString(String("Wi-Fi Signal: ") + String(WiFi.RSSI()) + " dBm", 120, 160);
    
    tft.drawFastHLine(0, 212, 240, 0x2124);
    tft.setTextColor(0x7BEF);
    tft.drawCenterString("Run: py streamer.py video.mp4", 120, 222);
}

void drawStreamHud() {
    tft.fillRect(0, 0, 240, 56, TFT_BLACK);
    tft.drawFastHLine(0, 56, 240, 0x07FF);
    tft.setTextColor(0x07FF);
    tft.setTextSize(1);
    tft.drawString("ESP32-C3 STREAM", 8, 8);
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString(WiFi.localIP().toString(), 232, 8);
    
    char statBuf[48];
    snprintf(statBuf, sizeof(statBuf), "FPS: %.1f  |  FRAMES: %lu", currentFps, frameCount);
    tft.setTextColor(0x07E0);
    tft.drawString(statBuf, 8, 26);
    tft.setTextColor(0x7BEF);
    tft.drawRightString(String("UDP:") + String(UDP_PORT), 232, 26);

    tft.fillRect(0, 184, 240, 56, TFT_BLACK);
    tft.drawFastHLine(0, 184, 240, 0x07FF);
    tft.setTextColor(0xFBE0);
    tft.drawCenterString("ST7789 240x240 IPS // DITHERED VIDEO", 120, 196);
    tft.setTextColor(0x7BEF);
    tft.drawCenterString("Hardware SPI DMA @ 40MHz", 120, 216);
}

void renderVid0Frame(const uint8_t* raw) {
    uint16_t onColor = THEME_COLORS[colorTheme];
    videoSprite.startWrite();
    for (int p = 0; p < 8; p++) {
        int baseY = p * 8;
        int pageOffset = p * 128;
        for (int x = 0; x < 128; x++) {
            uint8_t byteVal = raw[pageOffset + x];
            for (int b = 0; b < 8; b++) {
                uint16_t col = (byteVal & (1 << b)) ? onColor : TFT_BLACK;
                videoSprite.drawPixel(x, baseY + b, col);
            }
        }
    }
    videoSprite.endWrite();
    videoSprite.pushRotateZoom(120, 120, 0.0f, 1.875f, 1.875f);
}

void renderV240Chunk(const uint8_t* chunkHeader, const uint8_t* data, size_t len) {
    uint8_t chunkIdx = chunkHeader[0];
    if (chunkIdx >= 6) return;
    int startY = chunkIdx * 40;
    uint16_t onColor = THEME_COLORS[colorTheme];
    tft.startWrite();
    for (int y = 0; y < 40; y++) {
        int rowOffset = y * 30;
        int screenY = startY + y;
        for (int bx = 0; bx < 30; bx++) {
            uint8_t b = data[rowOffset + bx];
            int startX = bx * 8;
            for (int bit = 0; bit < 8; bit++) {
                uint16_t c = (b & (0x80 >> bit)) ? onColor : TFT_BLACK;
                tft.drawPixel(startX + bit, screenY, c);
            }
        }
    }
    tft.endWrite();
}

void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(0);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    videoSprite.setColorDepth(16);
    videoSprite.createSprite(128, 64);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000) {
        delay(300);
        Serial.print('.');
    }
    if (WiFi.status() != WL_CONNECTED) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.drawCenterString("WI-FI FAILED", 120, 100);
        return;
    }
    udp.begin(UDP_PORT);
    drawWaitingScreen();
    lastFpsTime = millis();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) return;
    int packetLen = udp.parsePacket();
    if (packetLen > 0) {
        int bytesRead = udp.read(rxPacket, sizeof(rxPacket));
        if (bytesRead >= 4) {
            if (memcmp(rxPacket, "VID0", 4) == 0 && bytesRead == VID0_PACKET) {
                if (!inStreamMode) {
                    inStreamMode = true;
                    tft.fillScreen(TFT_BLACK);
                    drawStreamHud();
                }
                renderVid0Frame(rxPacket + 4);
                frameCount++;
                lastFrameArrival = millis();
            }
            else if (memcmp(rxPacket, "V240", 4) == 0 && bytesRead >= 1208) {
                if (!inStreamMode) {
                    inStreamMode = true;
                    tft.fillScreen(TFT_BLACK);
                }
                renderV240Chunk(rxPacket + 4, rxPacket + 8, bytesRead - 8);
                if (rxPacket[4] == 0) frameCount++;
                lastFrameArrival = millis();
            }
        }
    }
    if (inStreamMode && (millis() - lastFrameArrival > 3500)) {
        inStreamMode = false;
        drawWaitingScreen();
    }
    if (millis() - lastFpsTime >= 1000) {
        currentFps = (float)frameCount * 1000.0f / (float)(millis() - lastFpsTime);
        if (inStreamMode) drawStreamHud();
        frameCount = 0;
        lastFpsTime = millis();
    }
}
#endif
