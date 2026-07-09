#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "water_tank_packet.h"

constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr int kSdaPin = D9;
constexpr int kSclPin = D8;
constexpr int kButtonPin = D10;
constexpr int kStatusLedPin = LED_BUILTIN;

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

void format_age(uint32_t age_s, char *out, size_t size) {
  if (age_s < 60) {
    snprintf(out, size, "%lus", age_s);
  } else if (age_s < 3600) {
    snprintf(out, size, "%lum", age_s / 60);
  } else if (age_s < 86400) {
    snprintf(out, size, "%luh", age_s / 3600);
  } else {
    snprintf(out, size, "%.1fd", age_s / 86400.0f);
  }
}

void draw() {
  if (!display_on) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  if (!has_data) {
    display.setCursor(0, 24);
    display.println("Waiting for sender");
    display.display();
    return;
  }

  display.setCursor(0, 16);
  if (isnan(latest.level_m)) {
    display.println("Ping received");
    display.printf("Seq: %lu\n", latest.sequence);
    display.printf("Age: %lus\n", (millis() - last_received_ms) / 1000);
    display.printf("Batt: %.2f V\n", latest.battery_v);
    display.display();
    return;
  }

  display.setTextSize(1);
  display.setCursor(0, 0);
  char age[8];
  format_age((millis() - last_received_ms) / 1000, age, sizeof(age));
  display.printf("m3 D%.2f B%.2f", latest.distance_m, latest.battery_v);
  display.setTextSize(5);
  display.setCursor(0, 14);
  display.printf("%.2f", latest.volume_l / 1000.0f);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.printf("updated %s ago", age);
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
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C) && !display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
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
  if (previous_button == HIGH && button == LOW) {
    wake_display();
    draw();
  }
  previous_button = button;

  if (display_on && millis() - display_wake_ms > OLED_TIMEOUT_MS) set_display(false);
  if (has_data && millis() - last_received_ms > 1000) digitalWrite(kStatusLedPin, LOW);

  if (millis() - last_draw_ms > 1000) {
    last_draw_ms = millis();
    draw();
  }
}
