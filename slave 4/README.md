# ESP32 Slave 4: Dual PT100 + Hall Sensor

An ESP32 project that reads **two PT100 temperature probes (through MAX31865 boards)** and **one analog Hall-effect sensor**, then prints everything as a single line (a "packet") over the serial monitor every 500 ms. A **VL53L4CD LiDAR** is planned, and a placeholder is already in the code.

## Features

- Two PT100 RTD temperature readings, each on its own SPI bus
- Hall sensor read as a raw ADC count and as a voltage (calibrated, 16-sample average)
- Each sensor fails independently: one bad sensor does not stop the others
- Startup messages that show which sensors initialised
- LiDAR placeholder with instructions in the code

## Status of each sensor

| Sensor | Status |
|---|---|
| PT100 RTD #1 (MAX31865) | Implemented |
| PT100 RTD #2 (MAX31865) | Implemented |
| Hall sensor (analog) | Implemented |
| VL53L4CD LiDAR | **Not implemented yet** (reports `-1`) |

## Hardware

| Part | Qty | Interface |
|---|---:|---|
| ESP32-WROOM-32 DevKit | 1 | n/a |
| MAX31865 RTD amplifier board | 2 | SPI |
| PT100 probe (4-wire recommended) | 2 | Connected to MAX31865 |
| Analog Hall-effect sensor | 1 | Analog |
| VL53L4CD module (planned) | 1 | I²C |

## Pin Map

| Function | GPIO |
|---|---|
| RTD 1 SCK (SPI2) | 18 |
| RTD 1 MOSI (SPI2) | 23 |
| RTD 1 MISO (SPI2) | 19 |
| RTD 1 CS | 25 |
| RTD 2 SCK (SPI3) | 14 |
| RTD 2 MOSI (SPI3) | 13 |
| RTD 2 MISO (SPI3) | 27 |
| RTD 2 CS | 26 |
| Hall sensor (ADC1_CH6) | 34 |
| LiDAR SDA (planned) | 21 |
| LiDAR SCL (planned) | 22 |

Use the **GPIO labels** printed on your board, not physical header positions.

## Wiring

### MAX31865 boards

| MAX31865 pin | RTD #1 → ESP32 | RTD #2 → ESP32 |
|---|---|---|
| VIN | 3V3 | 3V3 |
| GND | GND | GND |
| CLK | GPIO 18 | GPIO 14 |
| SDI (MOSI) | GPIO 23 | GPIO 13 |
| SDO (MISO) | GPIO 19 | GPIO 27 |
| CS | GPIO 25 | GPIO 26 |
| RDY | Not connected | Not connected |

`SDI` is the board's input, so it goes to the ESP32 **MOSI** pin. `SDO` is the board's output, so it goes to **MISO**.

### PT100 probes

For a 4-wire PT100, connect the two wires of one lead to **F+** and **RTD+**, and the two wires of the other lead to **RTD−** and **F−**.

The jumpers on the MAX31865 board must match your wiring (2-wire, 3-wire or 4-wire). If you use a different type, change `RTD_WIRING` near the top of `main.c`.

### Reference resistor

The code assumes a **430 Ω** reference resistor, which is what PT100 boards normally use. Check the resistor on your board (a marking of `431` means 430 Ω). If it differs, change `RTD_REFERENCE_OHMS` in `main.c`.

### Hall sensor

| Hall sensor pin | Connection |
|---|---|
| VCC | Supply required by your sensor |
| GND | ESP32 GND (common ground) |
| OUT | GPIO 34 |

> **Check the output voltage.** The ESP32 ADC pin must never see more than 3.3 V. Many analog Hall sensors run from 5 V and can output above 3 V. If yours can, add a voltage divider (for example 10 kΩ on top and 20 kΩ to ground, ratio 1.5) and set `HALL_DIVIDER_RATIO` to `1.5` in `main.c`.

### LiDAR (planned)

| VL53L4CD pin | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

The default I²C address is `0x29`.

## Software Requirements

- ESP-IDF v5.2 or newer (the project was set up on v6.0.2)
- Internet access on the first build, to download the MAX31865 library
- Library used: [`esp-idf-lib/max31865`](https://components.espressif.com/components/esp-idf-lib/max31865), installed through the IDF Component Manager

## Project Structure

```text
ESP Slave 4/
├── CMakeLists.txt
├── README.md
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml
    └── main.c
```

**Top-level `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_slave4)
```

**`main/CMakeLists.txt`**

```cmake
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_spi esp_driver_gpio esp_adc)
```

**`main/idf_component.yml`**

```yaml
dependencies:
  esp-idf-lib/max31865: "*"
```

## Build and Flash

```bash
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

Replace `<PORT>` with your serial port (for example `COM5` on Windows). Press `Ctrl+]` to exit the monitor.

The first build downloads the MAX31865 library into a `managed_components` folder. That folder is generated, so keep it out of Git (it should already be covered by `.gitignore`, otherwise add `managed_components/` and `dependencies.lock`).

## Expected Output

At startup:

```text
RTD 1 ready
RTD 2 ready
Hall ADC calibration enabled
```

Then one packet every 500 ms:

```text
PACKET | RTD1=27.35 C | RTD2=27.41 C | HALL_RAW=1834 | HALL_V=1.502 V | LIDAR=-1 mm
```

## Packet Format

| Field | Meaning | Unit | Error value |
|---|---|---|---|
| `RTD1` | PT100 #1 temperature | °C | -999.00 |
| `RTD2` | PT100 #2 temperature | °C | -999.00 |
| `HALL_RAW` | Hall sensor ADC count (12-bit, averaged) | counts | -1 |
| `HALL_V` | Voltage at the Hall sensor output | V | -1.000 |
| `LIDAR` | Distance | mm | -1 (also until implemented) |

## Settings You May Need to Change

These are near the top of `main/main.c`:

| Setting | Default | Change it when |
|---|---|---|
| `RTD_REFERENCE_OHMS` | 430 | Your board uses a different reference resistor |
| `RTD_WIRING` | `MAX31865_4WIRE` | Your probes are 2-wire or 3-wire |
| `RTD_FILTER` | `MAX31865_FILTER_50HZ` | Your mains frequency is 60 Hz |
| `HALL_DIVIDER_RATIO` | 1.0 | You added a voltage divider on the Hall output |

## Troubleshooting

| Problem | Likely cause |
|---|---|
| Build error: `max31865.h` not found | The library was not downloaded. Check `idf_component.yml` and your internet connection, then run `idf.py reconfigure` |
| Build error about `v_bias` | Your library version does not have that field. Delete the `.v_bias = true,` line in `init_rtd()` |
| `RTD` shows -999.00 | Check the SPI wiring, the CS pin, and the probe connections to F+, RTD+, RTD−, F− |
| Temperature far too high or low | Wrong reference resistor value, or wiring mode does not match the board jumpers |
| Temperature reads about -242 °C or very high | Open or shorted probe wires |
| Hall reading stuck at 4095 | The sensor output is above the ADC range. Add a voltage divider |
| Hall reading stuck at 0 | Sensor not powered, or no common ground |
| `LIDAR=-1` | Expected until the VL53L4CD driver is added |

## Notes

- This project uses GPIO 25, 26 and 34, which the separate **ESP32 Multi-Sensor** project also uses for other purposes. Run the two projects on separate ESP32 boards, or change pins before combining them.
- Both SPI buses use pins that differ from the ESP32's default SPI pins. This works at 1 MHz and is more than fast enough for the MAX31865.

## Adding the LiDAR

1. Add a VL53L4CD driver (ST's ultra-lite driver or an ESP-IDF component).
2. Create an I²C bus on GPIO 21 (SDA) and GPIO 22 (SCL) and add the device at `0x29`.
3. Start ranging in `lidar_init()`.
4. In `lidar_read_distance()`, check data-ready, read the distance, clear the interrupt, and return the value in mm.

The packet format will not need to change.