#pragma once

#include <Arduino.h>

struct WaterTankPacket {
  uint32_t sequence;
  float distance_m;
  float battery_v;
};

static_assert(sizeof(WaterTankPacket) <= 250, "ESP-NOW packet is too large");
