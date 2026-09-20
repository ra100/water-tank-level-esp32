# Water Tank Level Monitor - ESP32-C6

Low-power water tank monitor on a single Seeed Studio XIAO ESP32-C6 board: ultrasonic level sensor, battery monitoring, and a local SSD1306 OLED display. No WiFi, no second board.

## Hardware

| Unit | Parts                                                                              |
| ---- | ---------------------------------------------------------------------------------- |
| Monitor | ESP32-C6, JSN-SR04T ultrasonic sensor, SSD1306 128x64 I2C OLED, battery, solar charger, battery ADC divider, wake button, status LED |

## Wiring

| ESP32-C6     | JSN-SR04T              | IRLML6402 (SOT-23) | OLED / other        |
| ------------ | ---------------------- | ------------------ | ------------------- |
| D7           | Echo                   |                    |                     |
| D8           | Trig                   |                    |                     |
| D2           |                        | Pin 1 (Gate)       | via 1k to Gate      |
| 3V3          |                        | Pin 2 (Source)     | 10k pullup to Gate  |
|              | VCC                    | Pin 3 (Drain)      |                     |
| A0           | Battery divider output |                    |                     |
| D9           |                        |                    | OLED SCL            |
| D10          |                        |                    | OLED SDA            |
| D5           |                        |                    | Button to GND       |
| Built-in LED |                        |                    | Status LED          |
| 3V3          |                        |                    | OLED VCC            |
| GND          | GND                    |                    | OLED GND, button GND|

The MOSFET power-gates the JSN-SR04T so it draws zero current between measurements.

```
                 IRLML6402 (SOT-23)
               ┌────────────────────┐
          Gate │1                  2│ Source
      ┌────────┤                    ├──────────┐
      │        │       P-ch MOSFET  │         │
      │        │                    │         │
      │        └────────┬───────────┘          │
      │             Pin3│Drain                │
      │                 │                     │
      │           ┌─────┘                     │
      │           │                           │
   [1kΩ]        JSN-SR04T VCC                 |
      │                                     [10kΩ]
      |                                       │
      │                                       |
      │                                      3V3
      │
      D2
```

D2 LOW = sensor ON, D2 HIGH = sensor OFF. The 10k pullup keeps the sensor off during boot.

The direct D2-to-Gate circuit above is safe only with a 3.3V MOSFET source. If your sensor works at 3.3V, keep this wiring. To switch a 5V-only sensor, use a transistor/MOSFET gate driver; do not connect a 5V gate pullup directly to D2. Add a voltage divider or level shifter between a 5V Echo output and D7.

## Configure

Tank dimensions and behavior are in `platformio.ini`:

```ini
-D TANK_HEIGHT_M=1.488
-D TANK_LENGTH_M=2.194
-D TANK_WIDTH_M=1.8
-D SENSOR_OFFSET_M=0.1
-D BATTERY_ADC_PIN=A0
-D BATTERY_DIVIDER_RATIO=2.0
-D BATTERY_CALIBRATION=1.0
-D POWER_PIN=D2
-D MEASURE_INTERVAL_S=60
-D SENSOR_WARMUP_MS=1000
-D OLED_TIMEOUT_MS=30000
```

## Build And Flash

```bash
pio run -e sender -t upload
pio device monitor -e sender
```

The board measures every `MEASURE_INTERVAL_S` seconds (default: 60 s). The sensor is power-gated via the P-channel MOSFET on `POWER_PIN` — it is only energized for the ~1 s warmup plus measurement window. The OLED shows distance, battery voltage, tank fill percentage with a progress bar, and how long ago the reading was taken. It turns off after `OLED_TIMEOUT_MS` and wakes on button press or new data. Pressing the button (`D5`) triggers a fresh measurement immediately. The last reading survives resets via RTC memory and is shown as "last value restored" until a new measurement completes.

A debug environment verifies the sensor power circuit:

```bash
pio run -e sender-debug -t upload
```

It powers the sensor on and halts; check that the sensor VCC measures 3.3V with the board powered.

## Notes

- No WiFi router is used.
- On XIAO ESP32-C6, `A0` maps to `GPIO0`.
- Battery voltage assumes a 2:1 divider into `A0`; change `BATTERY_DIVIDER_RATIO` to match your resistors.
- ESP32 ADC readings are approximate. Measure battery with a multimeter and adjust `BATTERY_CALIBRATION` if needed.
- Level = `TANK_HEIGHT_M - distance + SENSOR_OFFSET_M`, clamped to `[0, TANK_HEIGHT_M]`. Volume (liters) is logged over serial as `level × TANK_LENGTH_M × TANK_WIDTH_M × 1000`; the OLED shows fill percentage instead.
