# ESP32 Slave 2: ADS1113 ADC over CAN

An ESP32 node that measures one voltage with an **ADS1113 16-bit ADC** and sends it as a CAN frame every 100 ms. The same values are printed on the serial monitor.

## Features

- 16-bit voltage measurement over I²C (62.5 µV per step)
- 10 kΩ / 20 kΩ input divider, so inputs up to about 3.07 V can be measured
- CAN (TWAI) output at 500 kbit/s on frame ID `0x200`
- The program keeps running if the ADS1113 is missing, and reports the problem

## Hardware

| Part | Qty | Interface |
|---|---:|---|
| ESP32-WROOM-32 DevKit | 1 | n/a |
| ADS1113 module | 1 | I²C |
| 10 kΩ resistor | 1 | Input divider (top) |
| 20 kΩ resistor | 1 | Input divider (bottom) |
| 3.3 V CAN transceiver (for example SN65HVD230) | 1 | CAN |
| 120 Ω resistors | 2 | CAN termination, one at each end of the bus |

## Pin Map

| Function | GPIO |
|---|---|
| I²C SDA | 21 |
| I²C SCL | 22 |
| CAN TX | 5 |
| CAN RX | 4 |

Use the **GPIO labels** printed on your board, not physical header positions.

## Wiring

### ADS1113

| ADS1113 pin | Connection |
|---|---|
| VDD | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| ADDR | GND (I²C address `0x48`) |
| AIN0 | Output of the voltage divider |
| AIN1 | GND |
| ALRT / RDY | Not connected |

The ADS1113 measures the difference between AIN0 and AIN1, so **AIN1 must be tied to GND**.

### Input voltage divider

```text
INPUT ──[ 10 kΩ ]──┬── AIN0
                   │
                [ 20 kΩ ]
                   │
                  GND
```

```text
V_ADC   = V_INPUT / 1.5
V_INPUT = V_ADC × 1.5
```

The ADS1113 has a fixed input range of ±2.048 V, so the largest input you can measure is **2.048 × 1.5 = 3.072 V**. Keep the voltage at AIN0 between 0 and 2.048 V. Negative inputs are not supported.

### CAN

| ESP32 | CAN transceiver |
|---|---|
| GPIO 5 | TXD |
| GPIO 4 | RXD |
| 3V3 | VCC (for a 3.3 V transceiver) |
| GND | GND |

Connect CANH and CANL to the bus, with a 120 Ω resistor at **each end of the bus**, not at every node.

Use a **3.3 V** transceiver. A 5 V transceiver such as the TJA1050 outputs 5 V on RXD, which can damage the ESP32 pin unless you level-shift it.

## ADS1113 Settings

| Setting | Value |
|---|---|
| Input | AIN0 − AIN1 (differential) |
| Full-scale range | ±2.048 V (fixed on the ADS1113) |
| Resolution | 16 bits (1 LSB = 62.5 µV) |
| Mode | Single-shot, one conversion per reading |
| Data rate | 128 samples per second |
| Comparator | Disabled |

## CAN Frame

One frame, standard 11-bit ID `0x200`, 8 bytes, 500 kbit/s, sent every 100 ms. Values are **little-endian** (low byte first).

| Bytes | Content | Type |
|---|---|---|
| 0–1 | Raw ADC value | int16 |
| 2–3 | Voltage at the ADS1113 pin, in mV | uint16 |
| 4–5 | Original input voltage (divider reversed), in mV | uint16 |
| 6–7 | Packet counter (wraps around at 65535) | uint16 |

### Decoding values

| Value | Conversion |
|---|---|
| Voltage at the ADC pin | `bytes 2–3 / 1000.0` gives V |
| Input voltage | `bytes 4–5 / 1000.0` gives V |
| Raw to volts | `raw × 0.0000625` gives the voltage at the ADC pin |

Voltages at or below 0 V are sent as 0 in the CAN frame.

### Choosing CAN IDs across slaves

| Slave | CAN IDs | Byte order |
|---|---|---|
| Slave 1 | `0x100`–`0x102` | Big-endian |
| **Slave 2** | **`0x200`** | **Little-endian** |
| Slave 3 | `0x300`–`0x302` | Big-endian |

Slave 2 uses a different byte order from Slaves 1 and 3. If one receiver decodes all slaves, decode this frame separately or change the byte order in `can_send_ads1113()`.

## Serial Output

One line every 100 ms:

```text
RAW=12000 | ADC=0.750000 V | INPUT=1.125000 V | CAN=OK | CNT=42
```

| Field | Meaning |
|---|---|
| `RAW` | Raw 16-bit ADC value |
| `ADC` | Voltage at the ADS1113 pin |
| `INPUT` | Original input voltage, with the divider reversed |
| `CAN` | `OK` if the frame was sent and acknowledged, otherwise `ERROR` |
| `CNT` | Packet counter |

## Software Requirements

- ESP-IDF v5.5 or newer (the project builds on v6.0.2)
- No extra libraries

## Project Structure

```text
slave 2/
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
project(esp_slave2)
```

The project name is only a label for the build output. Any name works.

**`main/CMakeLists.txt`**

```cmake
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_i2c esp_driver_gpio
                                     esp_driver_twai)
```

## Build and Flash

```bash
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

Replace `<PORT>` with your serial port (for example `COM5`). Press `Ctrl+]` to exit the monitor.

## Expected Output at Startup

```text
I2C initialized: SDA=21 SCL=22
ADS1113 detected at 0x48
CAN initialized
CAN TX GPIO : 5
CAN RX GPIO : 4
CAN bitrate : 500000 bit/s
CAN ID      : 0x200
System initialized
```

## Troubleshooting

| Problem | Likely cause |
|---|---|
| `ADS1113 NOT detected` | Check VDD, GND, SDA and SCL wires, and that ADDR is tied to GND for address `0x48` |
| `ADS1113 read failed` every 500 ms | The ADS1113 is not responding. The same causes as above |
| `RAW` stuck at 32767 | The input is above the range. Check the divider and keep the input under 3.07 V |
| `RAW` drifts or is far from 0 with the input shorted | AIN1 is not tied to GND |
| `RAW` negative | The input is below AIN1, or the wires are swapped |
| `CAN=ERROR` on every line | No CAN transceiver, or no second node on the bus. Frames are only acknowledged when another node is connected |
| Board does not boot with the CAN wires connected | GPIO 5 is a boot-strapping pin. Disconnect the CAN TX wire, power up, then reconnect |

## Notes

- Slave 2 waits for each CAN frame to be acknowledged, so with no other node on the bus the `CAN=ERROR` message appears straight away. The ADC readings are still correct.
- The loop runs at about 10 Hz. Change the `vTaskDelay` at the end of `app_main()` to send faster or slower.
- The ADS1113 has no gain setting. If you need a different input range, change the divider resistors.