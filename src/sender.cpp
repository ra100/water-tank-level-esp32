#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_wifi.h>

#include "water_tank_packet.h"

constexpr uint8_t kBroadcastAddress[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
constexpr int kTriggerPin = D8;
constexpr int kEchoPin = D7;
constexpr int kPowerPin = POWER_PIN;
constexpr uint64_t kSleepUs = static_cast<uint64_t>(SLEEP_INTERVAL_S) * 1000000ULL;
constexpr uint16_t kSensorWarmupMs = SENSOR_WARMUP_MS;

float read_distance_m() {
  digitalWrite(kTriggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(kTriggerPin, HIGH);
  delayMicroseconds(20);
  digitalWrite(kTriggerPin, LOW);

  const unsigned long duration_us = pulseIn(kEchoPin, HIGH, 30000);
  Serial.printf("sensor echo=%luus\n", duration_us);
  if (duration_us == 0) return NAN;

  return (duration_us * 0.000343f) / 2.0f;
}

float read_battery_v() {
  const uint32_t millivolts = analogReadMilliVolts(BATTERY_ADC_PIN);
  Serial.printf("battery adc=%lumV\n", millivolts);
  return (millivolts / 1000.0f) * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION;
}

void on_send(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("ESP-NOW delivery=%s to %02X:%02X:%02X:%02X:%02X:%02X\n",
                status == ESP_NOW_SEND_SUCCESS ? "ok" : "failed",
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

void sleep_now() {
  Serial.printf("sleeping for %llus\n", kSleepUs / 1000000ULL);
  Serial.flush();
#ifdef SENDER_STAY_AWAKE
  return;
#endif
  digitalWrite(kPowerPin, HIGH);
  esp_sleep_enable_timer_wakeup(kSleepUs);
  esp_deep_sleep_start();
}

void measure_and_send() {
  Serial.printf("measuring trigger=%d echo=%d power=%d warmup=%ums\n",
                kTriggerPin, kEchoPin, kPowerPin, kSensorWarmupMs);

  digitalWrite(kPowerPin, LOW);
  Serial.println("sensor power=on");
  delay(kSensorWarmupMs);

  const float distance_m = read_distance_m();

  digitalWrite(kPowerPin, HIGH);
  Serial.println("sensor power=off");

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
  Serial.printf("ESP-NOW queue=%s (%s), packet=%uB seq=%lu distance=%.3fm level=%.3fm volume=%.1fL battery=%.2fV\n",
                result == ESP_OK ? "ok" : "failed", esp_err_to_name(result), sizeof(packet), packet.sequence,
                packet.distance_m, packet.level_m, packet.volume_l, packet.battery_v);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("sender boot, reset_reason=%d\n", esp_reset_reason());

  pinMode(kTriggerPin, OUTPUT);
  pinMode(kEchoPin, INPUT);
  pinMode(kPowerPin, OUTPUT);
  digitalWrite(kPowerPin, HIGH);
  Serial.printf("pins configured; sensor power=off, battery_adc=%d\n", BATTERY_ADC_PIN);

#ifdef SENDER_POWER_TEST
  digitalWrite(kPowerPin, LOW);
  Serial.println("POWER TEST: D2=LOW; MOSFET on; sensor VCC should measure 3.3V");
  return;
#endif

  WiFi.mode(WIFI_STA);
  Serial.printf("Wi-Fi station MAC=%s channel=%d\n", WiFi.macAddress().c_str(), WIFI_CHANNEL);
  const esp_err_t channel_result = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (channel_result != ESP_OK) {
    Serial.printf("Wi-Fi channel setup failed: %s\n", esp_err_to_name(channel_result));
    sleep_now();
  }

  const esp_err_t init_result = esp_now_init();
  if (init_result != ESP_OK) {
    Serial.printf("ESP-NOW init failed: %s\n", esp_err_to_name(init_result));
    sleep_now();
  }
  esp_now_register_send_cb(on_send);
  Serial.println("ESP-NOW initialized");

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, kBroadcastAddress, 6);
  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false;
  const esp_err_t peer_result = esp_now_add_peer(&peer);
  if (peer_result != ESP_OK) {
    Serial.printf("ESP-NOW peer add failed: %s\n", esp_err_to_name(peer_result));
    sleep_now();
  }
  Serial.println("ESP-NOW broadcast peer added");

  measure_and_send();
  delay(100);
  sleep_now();
}

void loop() {
#ifdef SENDER_STAY_AWAKE
#ifdef SENDER_POWER_TEST
  delay(1000);
#else
  measure_and_send();
  delay(5000);
#endif
#endif
}
