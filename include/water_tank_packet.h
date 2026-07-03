#pragma once

#include <Arduino.h>

struct WaterTankPacket {
  uint32_t sequence;
  float distance_m;
  float level_m;
  float volume_l;
  float battery_v;
};

static_assert(sizeof(WaterTankPacket) <= 250, "ESP-NOW packet is too large");
