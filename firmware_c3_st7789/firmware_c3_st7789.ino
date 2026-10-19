#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#include "display_setup.h"
#include "web_streamer_html.h"

// =========================================================================
// SYSTEM CONSTANTS & NETWORK CONFIGURATION
// =========================================================================
constexpr uint16_t UDP_PORT = 4210;
constexpr uint16_t WS_PORT  = 81;
constexpr uint16_t HTTP_PORT = 80;

constexpr size_t VID0_PAYLOAD = 1024;
constexpr size_t VID0_PACKET  = 4 + VID0_PAYLOAD;

const char* AP_SSID = "ESP32-C3-Streamer";
const char* AP_PASS = ""; // Open Hotspot for 1-tap phone connection

// Hardware Instances
LGFX_ST7789 tft;
LGFX_Sprite fullCanvas(&tft);
LGFX_Sprite videoSprite(&tft);
WiFiUDP udp;
WebServer server(HTTP_PORT);
WiFiServer wsServer(WS_PORT);
WiFiClient wsClient;
Preferences prefs;

// State Variables
bool isApMode = false;
bool wsHandshakeDone = false;
bool inStreamMode = false;
bool isVid0Active = false;
unsigned long lastFrameArrival = 0;
unsigned long frameCount = 0;
unsigned long lastFpsTime = 0;
float currentFps = 0.0f;

uint8_t colorTheme = 0;
const uint16_t THEME_COLORS[] = { TFT_WHITE, 0x07FF, 0x07E0, 0xFBE0 };

// Buffers
uint8_t rxPacket[1500];
uint8_t wsBuffer[8192];

// =========================================================================
// WEBSOCKET RFC 6455 HANDSHAKE (ZERO-DEPENDENCY MBEDTLS)
// =========================================================================
String computeWebSocketAccept(const String& key) {
    String combined = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char hash[20];
    mbedtls_sha1((const unsigned char*)combined.c_str(), combined.length(), hash);
    unsigned char b64[36];
    size_t outLen = 0;
    mbedtls_base64_encode(b64, sizeof(b64), &outLen, hash, 20);
    b64[outLen] = '\0';
    return String((char*)b64);
}

// =========================================================================
// DISPLAY RENDERING ROUTINES
// =========================================================================
void drawWaitingScreen() {
    tft.fillScreen(TFT_BLACK);
    
    // Header
    tft.fillRect(0, 0, 240, 30, isApMode ? 0x7380 : 0x0186);
    tft.drawFastHLine(0, 30, 240, isApMode ? 0xFBE0 : 0x07FF);
    tft.setTextColor(isApMode ? 0xFBE0 : 0x07FF);
    tft.setTextSize(1);
    tft.drawCenterString(isApMode ? "HOTSPOT AP ACTIVE" : "WI-FI STATION CONNECTED", 120, 10);
    
    // Status Card Box
    tft.drawRect(12, 42, 216, 150, 0x3186);
    tft.drawRect(14, 44, 212, 146, 0x2124);
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawCenterString("WAITING FOR", 120, 60);
    tft.drawCenterString("VIDEO STREAM", 120, 82);
    
    tft.setTextSize(1);
    if (isApMode) {
        tft.setTextColor(0xFBE0);
        tft.drawCenterString(String("Wi-Fi: ") + AP_SSID, 120, 114);
        tft.setTextColor(0x07E0);
        tft.drawCenterString("Open: http://192.168.4.1", 120, 134);
    } else {
        tft.setTextColor(0x07E0);
        tft.drawCenterString(String("IP: ") + WiFi.localIP().toString(), 120, 114);
        tft.setTextColor(0x07FF);
        tft.drawCenterString(String("Web: http://") + WiFi.localIP().toString(), 120, 134);
    }
    
    tft.setTextColor(0x7BEF);
    tft.drawCenterString(String("WS: ") + String(WS_PORT) + " | UDP: " + String(UDP_PORT), 120, 158);
    
    // Footer Guide
    tft.drawFastHLine(0, 210, 240, 0x2124);
    tft.setTextColor(0x7BEF);
    tft.drawCenterString("Stream via Phone/PC Browser or EXE", 120, 222);
}

void drawStreamHud() {
    tft.fillRect(0, 0, 240, 56, TFT_BLACK);
    tft.drawFastHLine(0, 56, 240, 0x07FF);
    
    tft.setTextColor(0x07FF);
    tft.setTextSize(1);
    tft.drawString("ESP32-C3 STREAM", 8, 8);
    
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString(isApMode ? "192.168.4.1" : WiFi.localIP().toString(), 232, 8);
    
    char statBuf[48];
    snprintf(statBuf, sizeof(statBuf), "FPS: %.1f  |  FRAMES: %lu", currentFps, frameCount);
    tft.setTextColor(0x07E0);
    tft.drawString(statBuf, 8, 26);
    
    tft.setTextColor(0x7BEF);
    tft.drawRightString(String("PORT: ") + String(WS_PORT) + "/" + String(UDP_PORT), 232, 26);

    tft.fillRect(0, 184, 240, 56, TFT_BLACK);
    tft.drawFastHLine(0, 184, 240, 0x07FF);
    
    tft.setTextColor(0xFBE0);
    tft.drawCenterString("ST7789 240x240 IPS // WIRELESS VIDEO", 120, 196);
    tft.setTextColor(0x7BEF);
    tft.drawCenterString("Hardware SPI DMA @ 40MHz", 120, 216);
}

void renderVid0Frame(const uint8_t* raw) {
    uint8_t* buf = (uint8_t*)videoSprite.getBuffer();
    if (!buf) return;
    memset(buf, 0, 1024);
    for (int p = 0; p < 8; p++) {
        int baseY = p * 8;
        int pageOffset = p * 128;
        for (int x = 0; x < 128; x++) {
            uint8_t byteVal = raw[pageOffset + x];
            if (!byteVal) continue;
            int byteX = x >> 3;
            uint8_t mask = 0x80 >> (x & 7);
            for (int b = 0; b < 8; b++) {
                if (byteVal & (1 << b)) {
                    buf[(baseY + b) * 16 + byteX] |= mask;
                }
            }
        }
    }
    videoSprite.pushRotateZoom(120, 120, 0.0f, 1.875f, 1.875f);
}

// =========================================================================
// WEBSOCKET & UDP VIDEO FRAME PROCESSOR (INSTANT DMA BLIT)
// =========================================================================
void processRawVideoPayload(const uint8_t* data, size_t len) {
    if (len >= 4 && memcmp(data, "VID0", 4) == 0 && len >= VID0_PACKET) {
        if (!inStreamMode || !isVid0Active) {
            inStreamMode = true;
            isVid0Active = true;
            tft.fillScreen(TFT_BLACK);
            drawStreamHud();
        }
        renderVid0Frame(data + 4);
        frameCount++;
        lastFrameArrival = millis();
    } else if (len >= 4 && memcmp(data, "V240", 4) == 0) {
        if (!inStreamMode || isVid0Active) {
            inStreamMode = true;
            isVid0Active = false;
            tft.fillScreen(TFT_BLACK);
        }
        uint8_t* canvasBuf = (uint8_t*)fullCanvas.getBuffer();
        if (canvasBuf) {
            if (len >= 7204) {
                // Direct full-frame stream (4B header + 7200B bitmap)
                memcpy(canvasBuf, data + 4, 7200);
                fullCanvas.pushSprite(0, 0);
                frameCount++;
            } else if (len >= 1208) {
                // Chunked stream (4B header + chunkIdx(1B) + total(1B) + seq(2B) + 1200B data)
                uint8_t chunkIdx = data[4];
                if (chunkIdx < 6) {
                    memcpy(canvasBuf + (chunkIdx * 1200), data + 8, 1200);
                    // Blit atomically to display only when final chunk arrives (zero tearing!)
                    if (chunkIdx == 5) {
                        fullCanvas.pushSprite(0, 0);
                        frameCount++;
                    }
                }
            }
        }
        lastFrameArrival = millis();
    }
}

void handleWebSocketClient() {
    if (wsServer.hasClient()) {
        if (wsClient && wsClient.connected()) wsClient.stop();
        wsClient = wsServer.available();
        wsHandshakeDone = false;
        Serial.println("[WS] New client connected!");
    }

    if (!wsClient || !wsClient.connected()) return;

    if (!wsHandshakeDone) {
        String req = "";
        while (wsClient.available()) {
            req += (char)wsClient.read();
            if (req.endsWith("\r\n\r\n")) break;
        }

        if (req.indexOf("Upgrade: websocket") != -1) {
            int keyIndex = req.indexOf("Sec-WebSocket-Key: ");
            if (keyIndex != -1) {
                int keyEnd = req.indexOf("\r\n", keyIndex);
                String key = req.substring(keyIndex + 19, keyEnd);
                key.trim();
                String acceptKey = computeWebSocketAccept(key);

                String response = "HTTP/1.1 101 Switching Protocols\r\n"
                                  "Upgrade: websocket\r\n"
                                  "Connection: Upgrade\r\n"
                                  "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
                wsClient.print(response);
                wsHandshakeDone = true;
                Serial.println("[WS] Handshake established successfully!");
            }
        }
        return;
    }

    while (wsClient.available() >= 2) {
        uint8_t b0 = wsClient.read();
        uint8_t b1 = wsClient.read();
        uint8_t opcode = b0 & 0x0F;
        bool isMasked = (b1 & 0x80) != 0;
        size_t payloadLen = b1 & 0x7F;

        if (opcode == 0x08) {
            wsClient.stop();
            wsHandshakeDone = false;
            return;
        }

        if (payloadLen == 126) {
            while (wsClient.available() < 2) delay(1);
            payloadLen = ((size_t)wsClient.read() << 8) | wsClient.read();
        } else if (payloadLen == 127) {
            for (int i = 0; i < 8; i++) wsClient.read();
            return;
        }

        uint8_t mask[4] = {0, 0, 0, 0};
        if (isMasked) {
            while (wsClient.available() < 4) delay(1);
            wsClient.read(mask, 4);
        }

        if (payloadLen > sizeof(wsBuffer)) {
            size_t drained = 0;
            while (drained < payloadLen && wsClient.available()) {
                wsClient.read();
                drained++;
            }
            return;
        }

        size_t bytesRead = 0;
        unsigned long readStart = millis();
        while (bytesRead < payloadLen && (millis() - readStart < 250)) {
            size_t avail = wsClient.available();
            if (avail > 0) {
                size_t toRead = (payloadLen - bytesRead < avail) ? (payloadLen - bytesRead) : avail;
                int r = wsClient.read(wsBuffer + bytesRead, toRead);
                if (r > 0) bytesRead += r;
            } else {
                delayMicroseconds(50);
            }
        }
        if (bytesRead < payloadLen) return;

        if (isMasked) {
            for (size_t i = 0; i < payloadLen; i++) {
                wsBuffer[i] ^= mask[i % 4];
            }
        }

        processRawVideoPayload(wsBuffer, payloadLen);
    }
}

// =========================================================================
// HTTP WEBSERVER ROUTES (SCAN, SAVE, RESET, STATUS, WEB DECK)
// =========================================================================
void setupWebServerRoutes() {
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    server.on("/scan", HTTP_GET, []() {
        int n = WiFi.scanNetworks();
        String json = "[";
        for (int i = 0; i < n; ++i) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        }
        json += "]";
        server.send(200, "application/json", json);
    });

    server.on("/save-wifi", HTTP_POST, []() {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        if (ssid.length() > 0) {
            prefs.putString("ssid", ssid);
            prefs.putString("pass", pass);
            server.send(200, "text/plain", "OK");
            delay(800);
            ESP.restart();
        } else {
            server.send(400, "text/plain", "SSID cannot be empty");
        }
    });

    server.on("/reset-wifi", HTTP_POST, []() {
        prefs.remove("ssid");
        prefs.remove("pass");
        server.send(200, "text/plain", "RESET_OK");
        delay(800);
        ESP.restart();
    });

    server.on("/status", HTTP_GET, []() {
        String json = "{\"ap\":";
        json += isApMode ? "true" : "false";
        json += ",\"ip\":\"" + (isApMode ? String("192.168.4.1") : WiFi.localIP().toString()) + "\"";
        json += ",\"fps\":" + String(currentFps, 1);
        json += ",\"frames\":" + String(frameCount);
        json += "}";
        server.send(200, "application/json", json);
    });

    server.on("/theme", HTTP_GET, []() {
        if (server.hasArg("id")) {
            colorTheme = server.arg("id").toInt() % 4;
            fullCanvas.setPaletteColor(1, THEME_COLORS[colorTheme]);
            videoSprite.setPaletteColor(1, THEME_COLORS[colorTheme]);
        }
        server.send(200, "text/plain", String(colorTheme));
    });

    server.begin();
    Serial.println("[HTTP] Web Server started on port 80");
}

// =========================================================================
// SETUP & SYSTEM INITIALIZATION
// =========================================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n==============================================");
    Serial.println("  ESP32-C3 ST7789 VIDEO STREAMER (WEB & UDP)  ");
    Serial.println("==============================================");

    tft.init();
    tft.setRotation(0);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    fullCanvas.setColorDepth(1);
    if (!fullCanvas.createSprite(240, 240)) {
        Serial.println("[ERROR] Failed to allocate fullCanvas sprite!");
    } else {
        fullCanvas.setPaletteColor(0, TFT_BLACK);
        fullCanvas.setPaletteColor(1, THEME_COLORS[colorTheme]);
        fullCanvas.fillSprite(0);
        Serial.println("[DISPLAY] 240x240 1-bit DMA Sprite allocated (7200 bytes)");
    }

    videoSprite.setColorDepth(1);
    if (!videoSprite.createSprite(128, 64)) {
        Serial.println("[ERROR] Failed to allocate videoSprite!");
    } else {
        videoSprite.setPaletteColor(0, TFT_BLACK);
        videoSprite.setPaletteColor(1, THEME_COLORS[colorTheme]);
        videoSprite.fillSprite(0);
    }

    prefs.begin("stream_wifi", false);
    String savedSsid = prefs.getString("ssid", "");
    String savedPass = prefs.getString("pass", "");

    bool connected = false;
    if (savedSsid.length() > 0) {
        Serial.printf("[WI-FI] Connecting to saved network: %s\n", savedSsid.c_str());
        tft.setTextColor(0x07FF);
        tft.setTextSize(1);
        tft.drawCenterString("CONNECTING TO WI-FI...", 120, 95);
        tft.setTextColor(0x7BEF);
        tft.drawCenterString(savedSsid, 120, 115);

        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSsid.c_str(), savedPass.c_str());

        unsigned long startMs = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startMs < 12000) {
            delay(300);
            Serial.print('.');
        }
        Serial.println();
        connected = (WiFi.status() == WL_CONNECTED);
    }

    if (!connected) {
        isApMode = true;
        Serial.println("[WI-FI] Starting SoftAP Hotspot...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASS);
        delay(100);
        Serial.printf("[WI-FI] Hotspot '%s' active! IP: 192.168.4.1\n", AP_SSID);
    } else {
        isApMode = false;
        Serial.printf("[WI-FI] Station connected! IP: %s\n", WiFi.localIP().toString().c_str());
    }

    udp.begin(UDP_PORT);
    wsServer.begin();
    setupWebServerRoutes();

    drawWaitingScreen();
    lastFpsTime = millis();
}

// =========================================================================
// MAIN RUNTIME LOOP
// =========================================================================
void loop() {
    server.handleClient();
    handleWebSocketClient();

    int maxUdp = 12;
    while (maxUdp-- > 0 && udp.parsePacket() > 0) {
        int bytesRead = udp.read(rxPacket, sizeof(rxPacket));
        if (bytesRead >= 4) {
            processRawVideoPayload(rxPacket, bytesRead);
        }
    }

    if (inStreamMode && (millis() - lastFrameArrival > 3500)) {
        inStreamMode = false;
        drawWaitingScreen();
    }

    if (millis() - lastFpsTime >= 1000) {
        currentFps = (float)frameCount * 1000.0f / (float)(millis() - lastFpsTime);
        if (inStreamMode && isVid0Active) drawStreamHud();
        frameCount = 0;
        lastFpsTime = millis();
    }
}
