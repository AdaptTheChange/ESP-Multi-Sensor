# ESP32 Multi-Sensor Data Collection

An ESP32 project that reads **two GY-906 (MLX90614) infrared temperature sensors, one VL53L0X laser distance sensor, and one analog hydraulic pressure sensor**, then prints all readings as a single line (a "packet") over the serial monitor every 500 ms.

## Features

- Two non-contact temperature sensors (ambient + object temperature each)
- Laser distance measurement in millimetres
- Pressure measurement in pascals (0 to 1 MPa)
- Both GY-906 sensors keep their **default address (0x5A)**, so no reprogramming is needed. Each one sits on its own I²C bus.
- Startup self-check that reports which sensors were found

## Hardware

| Part | Qty | Interface |
|---|---:|---|
| ESP32-WROOM-32 DevKit | 1 | n/a |
| GY-906 (MLX90614) | 2 | I²C |
| VL53L0X module | 1 | I²C |
| Hydraulic pressure sensor, 0.5–4.5 V output, 0–1 MPa | 1 | Analog |
| 10 kΩ resistor | 1 | n/a |
| 20 kΩ resistor | 1 | n/a |
| External 5 V supply (for the pressure sensor) | 1 | n/a |

## Pin Map

| Function | GPIO | Connected to |
|---|---|---|
| I²C bus 0 SDA | 21 | GY-906 #1, VL53L0X |
| I²C bus 0 SCL | 22 | GY-906 #1, VL53L0X |
| I²C bus 1 SDA | 25 | GY-906 #2 |
| I²C bus 1 SCL | 26 | GY-906 #2 |
| Pressure ADC (ADC1_CH6) | 34 | Voltage divider output |

Use the **GPIO labels** printed on your board, not physical header positions, since DevKit layouts vary.

## I²C Addresses

| Device | Bus | Address |
|---|---|---|
| GY-906 #1 | 0 | `0x5A` |
| VL53L0X | 0 | `0x29` |
| GY-906 #2 | 1 | `0x5A` |

The two GY-906 modules share an address, which is fine because they are on separate buses.

## Wiring

### Bus 0: GY-906 #1 and VL53L0X

| Module pin | ESP32 |
|---|---|
| VCC / VIN | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

Both modules connect to the same GPIO 21 and GPIO 22 lines. On the VL53L0X, leave GPIO1 unconnected and keep XSHUT at its module default (or tie it to 3V3).

### Bus 1: GY-906 #2

| Module pin | ESP32 |
|---|---|
| VCC / VIN | 3V3 |
| GND | GND |
| SDA | GPIO 25 |
| SCL | GPIO 26 |

### Pressure sensor

| Sensor pin | Connection |
|---|---|
| VCC | External 5 V |
| GND | ESP32 GND (common ground) |
| OUT | Voltage divider, then GPIO 34 |

**Voltage divider** (the sensor outputs up to 4.5 V, but the ESP32 ADC must stay at or below 3.3 V):

```text
Pressure OUT ──[ 10 kΩ ]──┬── GPIO 34
                          │
                       [ 20 kΩ ]
                          │
                         GND
```

```text
V_ADC = V_SENSOR × 20k / (10k + 20k) = V_SENSOR / 1.5
4.5 V → 3.0 V
```

**Common ground is required.** The external 5 V supply's ground, the pressure sensor ground, and the ESP32 ground must all be connected together.

## Pressure Conversion

```text
V_SENSOR = V_ADC × 1.5
Pressure (Pa) = (V_SENSOR − 0.5) / (4.5 − 0.5) × 1,000,000
```

The result is clamped between 0 and 1,000,000 Pa. The code averages 16 ADC samples and uses the ESP32's factory ADC calibration when available.

## Software Requirements

- ESP-IDF **v5.2 or newer** (uses the `i2c_master` driver and `i2c_master_probe`)
- Target: `esp32`

## Project Structure

```text
ESP-Multi-Sensor/
├── CMakeLists.txt
├── README.md
└── main/
    ├── CMakeLists.txt
    └── main.c
```

**Top-level `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_multi_sensor)
```

**`main/CMakeLists.txt`**

```cmake
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver esp_adc)
```

## Build and Flash

```bash
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

Replace `<PORT>` with your serial port (for example `COM5` on Windows or `/dev/ttyUSB0` on Linux). Press `Ctrl+]` to exit the monitor.

## Expected Output

At startup:

```text
GY-906 #1 (bus 0) detected at 0x5A
GY-906 #2 (bus 1) detected at 0x5A
VL53L0X   (bus 0) detected at 0x29
VL53L0X initialised
```

Then one packet every 500 ms:

```text
PACKET | T1_AMB=27.41 C | T1_OBJ=31.72 C | T2_AMB=27.38 C | T2_OBJ=29.64 C | LIDAR=183 mm | PRESSURE=245000 Pa
```

## Packet Format

| Field | Meaning | Unit | Error value |
|---|---|---|---|
| `T1_AMB` | Sensor 1 ambient temperature | °C | -999.00 |
| `T1_OBJ` | Sensor 1 object temperature | °C | -999.00 |
| `T2_AMB` | Sensor 2 ambient temperature | °C | -999.00 |
| `T2_OBJ` | Sensor 2 object temperature | °C | -999.00 |
| `LIDAR` | Distance | mm | -1 |
| `PRESSURE` | Pressure | Pa | -1 |

## Troubleshooting

| Problem | Likely cause |
|---|---|
| A sensor says "NOT detected" | Check VCC, GND, SDA and SCL wires, and that SDA/SCL are not swapped |
| Temperature shows -999.00 | Sensor not responding on its bus, or loose wiring |
| `LIDAR=-1` | VL53L0X not initialised, or nothing within range (about 2 m max) |
| Pressure stuck at 0 | Missing common ground, sensor not powered at 5 V, or divider wired wrongly |
| Pressure stuck at max | Divider missing or wired wrongly, so the ADC is saturated |
| Pressure reads slightly off | The ESP32 ADC is least accurate near 3 V. Compare with a reference gauge and add a calibration offset if needed |

## Known Limitations

- The VL53L0X code is a **simplified driver** that skips ST's full calibration sequence. Distances may be rough. Check them against a ruler, and swap in a full VL53L0X driver if you need accuracy.
- The ESP32 ADC is nonlinear near the top of its range. A 3.0 V input (sensor at 4.5 V) sits in that region.
- Pressure sensor accuracy depends on its datasheet tolerance and the 5 V supply being stable.

## Status

Under development. Planned additions include filtering, calibration, and wireless data transmission.
