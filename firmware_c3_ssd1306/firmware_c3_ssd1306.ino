#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#include "web_streamer_html.h"

// =========================================================================
// SYSTEM CONSTANTS & NETWORK CONFIGURATION
// =========================================================================
constexpr uint16_t UDP_PORT = 4210;
constexpr uint16_t WS_PORT  = 81;
constexpr uint16_t HTTP_PORT = 80;
constexpr size_t FRAME_SIZE = 1024;

const char* AP_SSID = "ESP32-C3-Streamer";
const char* AP_PASS = "";

#define PIN_OLED_SDA  8
#define PIN_OLED_SCL  9
#define OLED_I2C_ADDR 0x3C

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WiFiUDP udp;
WebServer server(HTTP_PORT);
WiFiServer wsServer(WS_PORT);
WiFiClient wsClient;
Preferences prefs;

bool isApMode = false;
bool wsHandshakeDone = false;
unsigned long frameCounter = 0;
uint8_t packet[4 + FRAME_SIZE];
uint8_t wsBuffer[2048];

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

void processRawVideoPayload(const uint8_t* data, size_t len) {
    if (len >= 4 && memcmp(data, "VID0", 4) == 0 && len >= (4 + FRAME_SIZE)) {
        memcpy(display.getBuffer(), data + 4, FRAME_SIZE);
        display.display();
        frameCounter++;
    }
}

void handleWebSocketClient() {
    if (wsServer.hasClient()) {
        if (wsClient && wsClient.connected()) wsClient.stop();
        wsClient = wsServer.available();
        wsHandshakeDone = false;
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
        }

        uint8_t mask[4] = {0, 0, 0, 0};
        if (isMasked) {
            while (wsClient.available() < 4) delay(1);
            wsClient.read(mask, 4);
        }

        if (payloadLen > sizeof(wsBuffer)) {
            while (payloadLen-- && wsClient.available()) wsClient.read();
            return;
        }

        size_t bytesRead = 0;
        while (bytesRead < payloadLen) {
            if (wsClient.available()) wsBuffer[bytesRead++] = wsClient.read();
            else delay(1);
        }

        if (isMasked) {
            for (size_t i = 0; i < payloadLen; i++) wsBuffer[i] ^= mask[i % 4];
        }

        processRawVideoPayload(wsBuffer, payloadLen);
    }
}

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
        String ssid = server.arg("ssid"), pass = server.arg("pass");
        if (ssid.length() > 0) {
            prefs.putString("ssid", ssid);
            prefs.putString("pass", pass);
            server.send(200, "text/plain", "OK");
            delay(800);
            ESP.restart();
        } else server.send(400, "text/plain", "Empty SSID");
    });
    server.on("/reset-wifi", HTTP_POST, []() {
        prefs.remove("ssid");
        prefs.remove("pass");
        server.send(200, "text/plain", "RESET_OK");
        delay(800);
        ESP.restart();
    });
    server.begin();
}

void setup() {
    Serial.begin(115200);
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setClock(400000);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        while (true) delay(1000);
    }

    prefs.begin("stream_wifi", false);
    String savedSsid = prefs.getString("ssid", "");
    String savedPass = prefs.getString("pass", "");

    bool connected = false;
    if (savedSsid.length() > 0) {
        showMessage("CONNECTING WI-FI", savedSsid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSsid.c_str(), savedPass.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
            delay(300);
        }
        connected = (WiFi.status() == WL_CONNECTED);
    }

    if (!connected) {
        isApMode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASS);
        showMessage("HOTSPOT AP ACTIVE", String("SSID: ") + AP_SSID, "http://192.168.4.1");
    } else {
        isApMode = false;
        showMessage("WI-FI CONNECTED", String("IP: ") + WiFi.localIP().toString(), "http://" + WiFi.localIP().toString());
    }

    udp.begin(UDP_PORT);
    wsServer.begin();
    setupWebServerRoutes();
}

void loop() {
    server.handleClient();
    handleWebSocketClient();

    int len = udp.parsePacket();
    if (len > 0) {
        int n = udp.read(packet, sizeof(packet));
        if (n >= 4) {
            processRawVideoPayload(packet, n);
        }
    }
}
