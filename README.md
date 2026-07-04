# Water Tank Level Monitor - ESP32-C6 ESP-NOW

Low-power wireless water tank monitor using two Seeed Studio XIAO ESP32-C6 boards.

## Hardware

| Unit     | Parts                                                                              |
| -------- | ---------------------------------------------------------------------------------- |
| Sender   | ESP32-C6, JSN-SR04T ultrasonic sensor, battery, solar charger, battery ADC divider |
| Receiver | ESP32-C6, SSD1306 128x64 I2C OLED, wake button, status LED, USB power              |

## Wiring

### Sender

| ESP32-C6 | JSN-SR04T              |
| -------- | ---------------------- |
| D7       | Trig                   |
| D6       | Echo                   |
| A0       | Battery divider output |
| 3V3      | VCC                    |
| GND      | GND                    |

### Receiver

| ESP32-C6     | OLED          |
| ------------ | ------------- |
| D8           | SCL           |
| D9           | SDA           |
| D10          | Button to GND |
| Built-in LED | Status LED    |
| 3V3          | VCC           |
| GND          | GND           |

## Configure

Tank dimensions are in `platformio.ini`:

```ini
-D TANK_HEIGHT_M=2.0
-D TANK_LENGTH_M=2.0
-D TANK_WIDTH_M=1.0
-D SENSOR_OFFSET_M=0.1
-D BATTERY_ADC_PIN=A0
-D BATTERY_DIVIDER_RATIO=2.0
-D BATTERY_CALIBRATION=1.0
-D OLED_TIMEOUT_MS=30000
```

Both devices must use the same `WIFI_CHANNEL`.

## Build And Flash

```bash
pio run -e receiver -t upload
pio device monitor -e receiver

pio run -e sender -t upload
pio device monitor -e sender
```

The sender broadcasts one ESP-NOW packet, waits briefly, then sleeps for 5 minutes. The receiver stays awake for packets, turns the OLED off after 30 seconds, and wakes the OLED on `D10` button press or on new data.

## Notes

- No WiFi router is used.
- On XIAO ESP32-C6, `A0` maps to `GPIO0`.
- Battery voltage assumes a 2:1 divider into `A0`; change `BATTERY_DIVIDER_RATIO` to match your resistors.
- ESP32 ADC readings are approximate. Measure battery with a multimeter and adjust `BATTERY_CALIBRATION` if needed.
- Broadcast ESP-NOW is simple and low power, but has no delivery ACK. If packet loss matters, switch to a receiver MAC peer.
