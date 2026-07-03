#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "water_tank_packet.h"

constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr int kSdaPin = 9;
constexpr int kSclPin = 8;
constexpr int kButtonPin = 10;
constexpr int kStatusLedPin = 11;

Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
WaterTankPacket latest = {};
bool has_data = false;
bool display_on = true;
uint32_t last_received_ms = 0;
uint32_t display_wake_ms = 0;

void set_display(bool on) {
  if (display_on == on) return;
  display_on = on;
  display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void wake_display() {
  display_wake_ms = millis();
  set_display(true);
}

void draw() {
  if (!display_on) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Water Tank");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  if (!has_data) {
    display.setCursor(0, 24);
    display.println("Waiting for sender");
    display.display();
    return;
  }

  display.setCursor(0, 16);
  display.printf("Level: %.2f m\n", latest.level_m);
  display.printf("Volume: %.0f L\n", latest.volume_l);
  display.printf("Dist: %.2f m\n", latest.distance_m);
  display.printf("Batt: %.2f V\n", latest.battery_v);
  display.printf("Age: %lus\n", (millis() - last_received_ms) / 1000);
  display.display();
}

void on_receive(const esp_now_recv_info_t *, const uint8_t *data, int len) {
  if (len != sizeof(WaterTankPacket)) return;
  memcpy(&latest, data, sizeof(latest));
  has_data = true;
  last_received_ms = millis();
  digitalWrite(kStatusLedPin, HIGH);
  wake_display();
  Serial.printf("rx seq=%lu level=%.3fm volume=%.1fL battery=%.2fV\n", latest.sequence, latest.level_m, latest.volume_l, latest.battery_v);
  draw();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(kButtonPin, INPUT_PULLUP);
  pinMode(kStatusLedPin, OUTPUT);
  digitalWrite(kStatusLedPin, LOW);

  Wire.begin(kSdaPin, kSclPin);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
  }
  wake_display();
  draw();

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.print("receiver mac: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(on_receive);
}

void loop() {
  static uint32_t last_draw_ms = 0;
  static int previous_button = HIGH;
  const int button = digitalRead(kButtonPin);
  if (previous_button == HIGH && button == LOW) wake_display();
  previous_button = button;

  if (display_on && millis() - display_wake_ms > OLED_TIMEOUT_MS) set_display(false);
  if (has_data && millis() - last_received_ms > 1000) digitalWrite(kStatusLedPin, LOW);

  if (millis() - last_draw_ms > 1000) {
    last_draw_ms = millis();
    draw();
  }
}
