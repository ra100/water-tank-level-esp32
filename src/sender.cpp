#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_system.h>

constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr int kTriggerPin = D8;
constexpr int kEchoPin = D7;
constexpr int kPowerPin = POWER_PIN;
constexpr int kSdaPin = D10;
constexpr int kSclPin = D9;
constexpr int kButtonPin = D5;
constexpr int kStatusLedPin = LED_BUILTIN;
constexpr uint32_t kSavedReadingMagic = 0x57415453;
constexpr uint16_t kSensorWarmupMs = SENSOR_WARMUP_MS;
constexpr uint32_t kMeasureIntervalMs = static_cast<uint32_t>(MEASURE_INTERVAL_S) * 1000UL;

struct Reading {
  float distance_m;
  float battery_v;
};

Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
Reading latest = {};
RTC_DATA_ATTR Reading saved_latest = {};
RTC_DATA_ATTR uint32_t saved_reading_magic = 0;
bool has_data = false;
bool restored_data = false;
bool display_on = true;
uint32_t last_measured_ms = 0;
uint32_t display_wake_ms = 0;

float level_from_distance(float distance_m) {
  if (!isfinite(distance_m)) return NAN;
  return constrain(TANK_HEIGHT_M - distance_m + SENSOR_OFFSET_M, 0.0f, TANK_HEIGHT_M);
}

float volume_from_level(float level_m) {
  return isfinite(level_m) ? level_m * TANK_LENGTH_M * TANK_WIDTH_M * 1000.0f : NAN;
}

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

void draw() {
  if (!display_on) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  if (!has_data) {
    display.setCursor(0, 24);
    display.println("Measuring...");
    display.display();
    return;
  }

  const float level_m = level_from_distance(latest.distance_m);
  const float volume_l = volume_from_level(level_m);
  char age[8];
  format_age((millis() - last_measured_ms) / 1000, age, sizeof(age));

  if (!isfinite(level_m)) {
    display.setCursor(0, 16);
    display.println("No echo");
    display.printf("Batt: %.2f V\n", latest.battery_v);
    display.printf("Age: %s\n", age);
    display.display();
    return;
  }

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("m3 D%.2f B%.2f", latest.distance_m, latest.battery_v);
  display.setTextSize(5);
  display.setCursor(0, 14);
  display.printf("%.2f", volume_l / 1000.0f);
  display.setTextSize(1);
  display.setCursor(0, 56);
  if (restored_data) {
    display.print("last value restored");
  } else {
    display.printf("updated %s ago", age);
  }
  display.display();
}

void measure() {
  Serial.printf("measuring trigger=%d echo=%d power=%d warmup=%ums\n",
                kTriggerPin, kEchoPin, kPowerPin, kSensorWarmupMs);

  digitalWrite(kPowerPin, LOW);
  Serial.println("sensor power=on");
  delay(kSensorWarmupMs);

  Reading reading = {
    .distance_m = read_distance_m(),
    .battery_v = read_battery_v(),
  };

  digitalWrite(kPowerPin, HIGH);
  Serial.println("sensor power=off");

  latest = reading;
  has_data = true;
  restored_data = false;
  last_measured_ms = millis();
  saved_latest = reading;
  saved_reading_magic = kSavedReadingMagic;
  digitalWrite(kStatusLedPin, HIGH);
  wake_display();

  const float level_m = level_from_distance(reading.distance_m);
  const float volume_l = volume_from_level(level_m);
  Serial.printf("measured distance=%.3fm level=%.3fm volume=%.1fL battery=%.2fV\n",
                reading.distance_m, level_m, volume_l, reading.battery_v);
  draw();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("unit boot, reset_reason=%d\n", esp_reset_reason());

  if (saved_reading_magic == kSavedReadingMagic) {
    latest = saved_latest;
    has_data = true;
    restored_data = true;
    last_measured_ms = millis();
    Serial.printf("restored distance=%.3fm battery=%.2fV from RTC memory\n",
                  latest.distance_m, latest.battery_v);
  }

  pinMode(kTriggerPin, OUTPUT);
  pinMode(kEchoPin, INPUT);
  pinMode(kPowerPin, OUTPUT);
  digitalWrite(kPowerPin, HIGH);
  Serial.printf("pins configured; sensor power=off, battery_adc=%d\n", BATTERY_ADC_PIN);
  pinMode(kButtonPin, INPUT_PULLUP);
  pinMode(kStatusLedPin, OUTPUT);
  digitalWrite(kStatusLedPin, LOW);

#ifdef SENDER_POWER_TEST
  digitalWrite(kPowerPin, LOW);
  Serial.println("POWER TEST: D2=LOW; MOSFET on; sensor VCC should measure 3.3V");
  return;
#endif

  Wire.begin(kSdaPin, kSclPin);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C) && !display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    Serial.println("OLED init failed");
  }
  wake_display();
  draw();

  measure();
}

void loop() {
#ifdef SENDER_POWER_TEST
  delay(1000);
  return;
#endif

  static uint32_t last_draw_ms = 0;
  static int previous_button = HIGH;
  const int button = digitalRead(kButtonPin);
  if (previous_button == HIGH && button == LOW) {
    wake_display();
    draw();
  }
  previous_button = button;

  if (display_on && millis() - display_wake_ms > OLED_TIMEOUT_MS) set_display(false);
  if (has_data && millis() - last_measured_ms > 1000) digitalWrite(kStatusLedPin, LOW);

  if (millis() - last_measured_ms >= kMeasureIntervalMs) measure();

  if (millis() - last_draw_ms > 1000) {
    last_draw_ms = millis();
    draw();
  }
}
