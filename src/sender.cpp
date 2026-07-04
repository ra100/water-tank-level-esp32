#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

#include "water_tank_packet.h"

constexpr uint8_t kBroadcastAddress[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
constexpr int kTriggerPin = D7;
constexpr int kEchoPin = D6;
constexpr uint64_t kSleepUs = 5ULL * 60ULL * 1000000ULL;

float read_distance_m() {
  digitalWrite(kTriggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(kTriggerPin, HIGH);
  delayMicroseconds(20);
  digitalWrite(kTriggerPin, LOW);

  const unsigned long duration_us = pulseIn(kEchoPin, HIGH, 30000);
  if (duration_us == 0) return NAN;

  return (duration_us * 0.000343f) / 2.0f;
}

float read_battery_v() {
  const uint32_t millivolts = analogReadMilliVolts(BATTERY_ADC_PIN);
  return (millivolts / 1000.0f) * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION;
}

void sleep_now() {
  Serial.flush();
  esp_sleep_enable_timer_wakeup(kSleepUs);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(kTriggerPin, OUTPUT);
  pinMode(kEchoPin, INPUT);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    sleep_now();
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, kBroadcastAddress, 6);
  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("ESP-NOW peer add failed");
    sleep_now();
  }

  const float distance_m = read_distance_m();
  float level_m = TANK_HEIGHT_M - distance_m + SENSOR_OFFSET_M;
  if (isnan(distance_m)) level_m = NAN;
  if (!isnan(level_m)) level_m = constrain(level_m, 0.0f, TANK_HEIGHT_M);
  const float volume_l = isnan(level_m) ? NAN : level_m * TANK_LENGTH_M * TANK_WIDTH_M * 1000.0f;

  WaterTankPacket packet = {
    .sequence = static_cast<uint32_t>(esp_random()),
    .distance_m = distance_m,
    .level_m = level_m,
    .volume_l = volume_l,
    .battery_v = read_battery_v(),
  };

  const esp_err_t result = esp_now_send(kBroadcastAddress, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
  Serial.printf("sent=%s distance=%.3fm level=%.3fm volume=%.1fL battery=%.2fV\n", result == ESP_OK ? "ok" : "fail", packet.distance_m, packet.level_m, packet.volume_l, packet.battery_v);
  delay(100);
  sleep_now();
}

void loop() {}
