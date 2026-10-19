#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =========================================================================
// WI-FI & UDP STREAMING CONFIGURATION
// =========================================================================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
constexpr uint16_t UDP_PORT = 4210;
constexpr size_t FRAME_SIZE = 1024;

// ESP32-C3 I2C Pinout (SuperMini default: SDA=8, SCL=9; adjust if needed)
#define PIN_OLED_SDA  8
#define PIN_OLED_SCL  9
#define OLED_I2C_ADDR 0x3C

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WiFiUDP udp;
uint8_t packet[4 + FRAME_SIZE];
unsigned long lastFrame = 0;
unsigned long frameCounter = 0;

void showMessage(const char* line1, const String& line2 = "", const String& line3 = "") {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(line1);
    display.println();
    if (line2.length() > 0) display.println(line2);
    if (line3.length() > 0) display.println(line3);
    display.display();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n==============================================");
    Serial.println("  ESP32-C3 SSD1306 OLED VIDEO RECEIVER (UDP)  ");
    Serial.println("==============================================");

    // Initialize I2C for ESP32-C3
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setClock(400000); // 400kHz Fast I2C

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("[ERROR] OLED init failed. Check I2C address (0x3C/0x3D) and SDA/SCL pins!");
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
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        showMessage("WiFi FAILED", "Check credentials in", "sketch source code");
        Serial.println("[ERROR] WiFi connection failed");
        return;
    }

    if (!udp.begin(UDP_PORT)) {
        showMessage("UDP FAILED", "Port bind error");
        Serial.println("[ERROR] UDP bind failed");
        return;
    }

    Serial.printf("[INFO] ESP32 IP: %s | UDP Port: %u\n", WiFi.localIP().toString().c_str(), UDP_PORT);
    showMessage("WAITING FOR VIDEO", String("IP: ") + WiFi.localIP().toString(), String("PORT: ") + String(UDP_PORT));
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) return;

    int len = udp.parsePacket();
    if (len <= 0) return;

    if (len != sizeof(packet)) {
        Serial.printf("[WARN] Ignored packet, size=%d (expected %d)\n", len, sizeof(packet));
        while (udp.available()) udp.read();
        return;
    }

    int n = udp.read(packet, sizeof(packet));
    if (n != sizeof(packet)) return;

    if (memcmp(packet, "VID0", 4) != 0) {
        Serial.println("[WARN] Ignored packet: invalid header (expected VID0)");
        return;
    }

    // Direct buffer copy into SSD1306 internal memory
    memcpy(display.getBuffer(), packet + 4, FRAME_SIZE);
    display.display();

    frameCounter++;
    lastFrame = millis();

    if (frameCounter % 30 == 0) {
        Serial.printf("[STREAM] Rendered %lu frames from %s\n", frameCounter, udp.remoteIP().toString().c_str());
    }
}
