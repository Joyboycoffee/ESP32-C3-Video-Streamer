#include <WiFi.h>
#include <WiFiUdp.h>
#include "display_setup.h"

// =========================================================================
// WI-FI & UDP STREAMING CONFIGURATION
// =========================================================================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
constexpr uint16_t UDP_PORT = 4210;

// Protocols supported:
// 1. "VID0" : 128x64 monochrome frame (1024 bytes SSD1306 format) -> 1028 bytes UDP
// 2. "V240" : 240x240 native high-res chunks (40 lines = 1200 bytes per chunk)
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

// Color theme for video playback:
// 0: Crisp White, 1: Cyberpunk Cyan, 2: Matrix Green, 3: Retro Amber
uint8_t colorTheme = 0;
const uint16_t THEME_COLORS[] = { TFT_WHITE, 0x07FF, 0x07E0, 0xFBE0 };
const char* THEME_NAMES[] = { "CRISP WHITE", "CYBER CYAN", "MATRIX GREEN", "RETRO AMBER" };

void drawWaitingScreen() {
    tft.fillScreen(TFT_BLACK);
    
    // Top Cyber Header
    tft.fillRect(0, 0, 240, 28, 0x18E3);
    tft.drawFastHLine(0, 28, 240, 0x07FF);
    tft.setTextColor(0x07FF);
    tft.setTextSize(1);
    tft.drawCenterString("ESP32-C3 // VIDEO STREAMER", 120, 8);
    
    // Status Box
    tft.drawRect(15, 45, 210, 145, 0x3186);
    tft.drawRect(17, 47, 206, 141, 0x2124);
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawCenterString("WAITING FOR", 120, 65);
    tft.drawCenterString("VIDEO STREAM", 120, 88);
    
    tft.setTextSize(1);
    tft.setTextColor(0x07E0); // Green
    tft.drawCenterString(String("IP: ") + WiFi.localIP().toString(), 120, 120);
    
    tft.setTextColor(0xFBE0); // Amber
    tft.drawCenterString(String("PORT: ") + String(UDP_PORT), 120, 140);
    
    int rssi = WiFi.RSSI();
    tft.setTextColor(0x7BEF);
    tft.drawCenterString(String("Wi-Fi Signal: ") + String(rssi) + " dBm", 120, 160);
    
    // Footer Guide
    tft.drawFastHLine(0, 212, 240, 0x2124);
    tft.setTextColor(0x7BEF);
    tft.setTextSize(1);
    tft.drawCenterString("Run: py streamer.py video.mp4", 120, 222);
}

void drawStreamHud() {
    // Top HUD Bar (y: 0..50)
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

    // Bottom HUD Bar (y: 184..240)
    tft.fillRect(0, 184, 240, 56, TFT_BLACK);
    tft.drawFastHLine(0, 184, 240, 0x07FF);
    
    tft.setTextColor(0xFBE0);
    tft.setTextSize(1);
    tft.drawCenterString("ST7789 240x240 IPS // FLOYD-STEINBERG DITHER", 120, 196);
    
    tft.setTextColor(0x7BEF);
    tft.drawCenterString("Press button on GPIO 4/Touch to cycle color palette", 120, 216);
}

void renderVid0Frame(const uint8_t* raw) {
    uint16_t onColor = THEME_COLORS[colorTheme];
    
    // Unpack 128x64 SSD1306 page bit-packed buffer into videoSprite
    // 8 pages tall, 128 pixels wide. Each byte represents 8 vertical pixels.
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

    // Scale 128x64 sprite to 240x120 centered at (120, 120) on 240x240 screen
    // 128 * 1.875 = 240px wide; 64 * 1.875 = 120px tall
    videoSprite.pushRotateZoom(120, 120, 0.0f, 1.875f, 1.875f);
}

void renderV240Chunk(const uint8_t* chunkHeader, const uint8_t* data, size_t len) {
    uint8_t chunkIdx   = chunkHeader[0];
    uint8_t totalChunks = chunkHeader[1];
    if (chunkIdx >= 6) return;
    
    int startY = chunkIdx * 40;
    uint16_t onColor = THEME_COLORS[colorTheme];
    
    tft.startWrite();
    // 40 rows, 240 pixels per row = 9600 pixels = 1200 bytes (8 pixels per byte, MSB first)
    for (int y = 0; y < 40; y++) {
        int rowOffset = y * 30; // 240 / 8 = 30 bytes per line
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
    delay(200);
    Serial.println("\n==============================================");
    Serial.println("  ESP32-C3 ST7789 IPS VIDEO RECEIVER (UDP)    ");
    Serial.println("==============================================");

    // Initialize ST7789 Display via LovyanGFX
    tft.init();
    tft.setRotation(0);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    // Create 128x64 16-bit sprite for smooth video upscaling
    videoSprite.setColorDepth(16);
    videoSprite.createSprite(128, 64);

    // Initial boot message
    tft.setTextColor(0x07FF);
    tft.setTextSize(1);
    tft.drawCenterString("CONNECTING TO WI-FI...", 120, 100);
    tft.setTextColor(0x7BEF);
    tft.drawCenterString(WIFI_SSID, 120, 120);

    // Wi-Fi Connection
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000) {
        delay(300);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[ERROR] Wi-Fi connection failed!");
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.drawCenterString("WI-FI FAILED", 120, 90);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE);
        tft.drawCenterString("Check SSID & Password in code", 120, 130);
        return;
    }

    Serial.printf("[INFO] Wi-Fi Connected! IP: %s\n", WiFi.localIP().toString().c_str());

    // Bind UDP port
    if (!udp.begin(UDP_PORT)) {
        Serial.println("[ERROR] Failed to bind UDP port!");
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.drawCenterString("UDP BIND FAILED", 120, 110);
        return;
    }

    Serial.printf("[INFO] UDP Listener ready on port %u\n", UDP_PORT);
    drawWaitingScreen();
    lastFpsTime = millis();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        delay(500);
        return;
    }

    int packetLen = udp.parsePacket();
    if (packetLen > 0) {
        int bytesRead = udp.read(rxPacket, sizeof(rxPacket));
        if (bytesRead >= 4) {
            // Check for VID0 (128x64 stream)
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
            // Check for V240 (240x240 chunk stream: V240 + chunkIdx + totalChunks + seq_msb + seq_lsb + 1200 data)
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

    // Stream timeout -> return to waiting screen after 3 seconds of silence
    if (inStreamMode && (millis() - lastFrameArrival > 3500)) {
        inStreamMode = false;
        drawWaitingScreen();
    }

    // Update FPS calculation and telemetry every second
    if (millis() - lastFpsTime >= 1000) {
        currentFps = (float)frameCount * 1000.0f / (float)(millis() - lastFpsTime);
        if (inStreamMode) {
            drawStreamHud();
        }
        frameCount = 0;
        lastFpsTime = millis();
    }
}
