\# ESP Multi Sensor — Master



\## Overview



The \*\*Master\*\* is the central controller of the ESP Multi Sensor system.



The system consists of:



\- \*\*1 Master ESP32\*\*

\- \*\*4 Slave ESP32 nodes\*\*

\- A shared \*\*CAN bus\*\*

\- Multiple distributed sensors connected to the slave nodes

\- A CAN transceiver / isolation interface between the ESP32 controllers and the physical CAN bus



The Master is responsible for receiving, identifying, decoding, and monitoring sensor data transmitted by all four slave nodes.



The current Master firmware is focused on \*\*CAN communication and sensor-data reception\*\*. Hardware testing, relay control, contactor control, and higher-level vehicle/system control logic are intentionally kept separate from this initial communication layer.



\---



\# System Architecture



```text

&#x20;                        ESP MULTI SENSOR SYSTEM



&#x20;                             ┌─────────────┐

&#x20;                             │   MASTER    │

&#x20;                             │    ESP32    │

&#x20;                             │             │

&#x20;                             │ CAN Receive │

&#x20;                             │ CAN Decoder │

&#x20;                             │ Monitoring  │

&#x20;                             └──────┬──────┘

&#x20;                                    │

&#x20;                             CAN TRANSCEIVER

&#x20;                                    │

═════════════════════════════════════╪═════════════════════════════════════

&#x20;                                    │

&#x20;                                 CAN BUS

&#x20;                                    │

&#x20;         ┌──────────────────────────┼──────────────────────────┐

&#x20;         │                          │                          │

&#x20;         ▼                          ▼                          ▼

&#x20;  ┌─────────────┐            ┌─────────────┐            ┌─────────────┐

&#x20;  │   SLAVE 1   │            │   SLAVE 2   │            │   SLAVE 3   │

&#x20;  │    ESP32    │            │    ESP32    │            │    ESP32    │

&#x20;  │             │            │             │            │             │

&#x20;  │ IMU         │            │ ADC         │            │ Temperature │

&#x20;  │ Gyro        │            │ Voltage     │            │ LiDAR       │

&#x20;  │ Hall        │            │ Monitoring  │            │ Pressure    │

&#x20;  │ Temperature │            │             │            │             │

&#x20;  └─────────────┘            └─────────────┘            └─────────────┘

&#x20;                                    │

&#x20;                                    │

&#x20;                                    ▼

&#x20;                             ┌─────────────┐

&#x20;                             │   SLAVE 4   │

&#x20;                             │    ESP32    │

&#x20;                             │             │

&#x20;                             │ RTD/PT100   │

&#x20;                             │ Hall        │

&#x20;                             │ LiDAR       │

&#x20;                             └─────────────┘

```



All slaves communicate with the Master using the same CAN communication standard.



\---



\# Hardware Platform



\## Master Controller



The Master uses:



\*\*ESP32-WROOM-32 DevKit\*\*



Target:



```text

ESP32

```



Build environment:



```text

ESP-IDF v6.0.2

```



Compiler:



```text

xtensa-esp-elf

```



\---



\# CAN Communication



\## CAN Standard



The Master uses the ESP-IDF \*\*TWAI\*\* driver for CAN communication.



Configuration:



| Parameter | Value |

|---|---|

| Protocol | CAN / TWAI |

| CAN type | Classical CAN |

| CAN ID | Standard 11-bit |

| Bitrate | 500 kbit/s |

| Data length | 8 bytes |

| Byte encoding | Big-endian |

| Communication | Master ↔ Slaves |

| Driver | `esp\_driver\_twai` |



All slave nodes use the same bitrate and data encoding format.



This common format makes the distributed sensor network easier to maintain and expand.



\---



\# Master CAN Pins



The current Master firmware is configured for:



| Function | ESP32 GPIO |

|---|---:|

| CAN TX | GPIO 5 |

| CAN RX | GPIO 4 |



These pins connect to the CAN transceiver/interface rather than directly to CAN-H/CAN-L.



Conceptually:



```text

ESP32 Master

&#x20;   │

&#x20;   ├── GPIO 5 ───── TX

&#x20;   │

&#x20;   └── GPIO 4 ───── RX

&#x20;                     │

&#x20;                     ▼

&#x20;               CAN Transceiver

&#x20;                     │

&#x20;               ┌─────┴─────┐

&#x20;               │           │

&#x20;              CAN-H       CAN-L

&#x20;               │           │

&#x20;               └─────┬─────┘

&#x20;                     │

&#x20;                   CAN BUS

```



The physical CAN bus must use the appropriate CAN transceiver and bus termination for the final hardware implementation.



\---



\# CAN Message Architecture



Each slave has its own CAN ID range.



This allows the Master to determine which slave produced a message simply by examining the CAN identifier.



\## CAN ID Allocation



| Slave | CAN ID Range | Purpose |

|---|---:|---|

| Slave 1 | `0x100–0x102` | IMU, gyro, Hall, temperature |

| Slave 2 | `0x200` | ADC / voltage |

| Slave 3 | `0x300–0x302` | Temperature, LiDAR, pressure, status |

| Slave 4 | `0x400–0x402` | RTD/PT100, Hall, LiDAR |



\---



\# Slave 1



\## `0x100` — IMU



Contains:



```text

Byte 0–1 : AX

Byte 2–3 : AY

Byte 4–5 : AZ

Byte 6–7 : Sequence

```



All multi-byte values use big-endian encoding.



The Master decodes these values and prints:



```text

S1 | IMU | AX=... AY=... AZ=... SEQ=...

```



\---



\## `0x101` — Gyroscope + Hall



```text

Byte 0–1 : GX

Byte 2–3 : GY

Byte 4–5 : GZ

Byte 6–7 : Hall ADC

```



Master output:



```text

S1 | GYRO/HALL | GX=... GY=... GZ=... HALL=...

```



\---



\## `0x102` — Temperature + Status



```text

Byte 0–1 : Temperature 1 × 100

Byte 2–3 : Temperature 2 × 100

Byte 4   : Hall digital state

Byte 5   : RTD 1 fault

Byte 6   : RTD 2 fault

Byte 7   : Status

```



Temperatures are converted back to °C by dividing by 100.



Example:



```text

2500 → 25.00 °C

```



\---



\# Slave 2



\## `0x200` — ADC / Voltage



The Slave 2 frame contains:



```text

Byte 0–1 : Raw ADC

Byte 2–3 : ADC voltage in mV

Byte 4–5 : Input voltage in mV

Byte 6–7 : Sequence

```



The Master converts millivolt values into volts for display.



Example:



```text

5000 mV → 5.000 V

```



Example Master output:



```text

S2 | ADC | RAW=... ADC=... V INPUT=... V SEQ=...

```



\---



\# Slave 3



\## `0x300` — Temperature



Contains two temperature sensor measurements:



```text

Byte 0–1 : Temperature 1 ambient

Byte 2–3 : Temperature 1 object

Byte 4–5 : Temperature 2 ambient

Byte 6–7 : Temperature 2 object

```



Temperatures are transmitted using ×100 scaling.



Example:



```text

2735 → 27.35 °C

```



\---



\## `0x301` — LiDAR + Pressure



```text

Byte 0–1 : LiDAR distance in mm

Byte 2–5 : Pressure in Pa

Byte 6–7 : Sequence

```



Example:



```text

LiDAR:

250 → 250 mm



Pressure:

101325 → 101325 Pa

```



Master output:



```text

S3 | DIST/PRESS | LIDAR=... mm PRESSURE=... Pa SEQ=...

```



\---



\## `0x302` — Sensor Status



Contains the sensor status information.



The Master interprets individual status bits for:



```text

Temperature sensor 1

Temperature sensor 2

LiDAR

Pressure sensor

```



Example:



```text

S3 | STATUS | 0x0F | T1=OK T2=OK LIDAR=OK PRESSURE=OK

```



\---



\# Slave 4



\## `0x400` — RTD/PT100



Contains the two RTD/PT100 temperature measurements.



```text

Byte 0–1 : PT100 / RTD 1 temperature × 100

Byte 2–3 : PT100 / RTD 2 temperature × 100

Byte 4   : Status

Byte 5–6 : Sequence

```



Master output:



```text

S4 | RTD | T1=... C T2=... C STATUS=... SEQ=...

```



\---



\## `0x401` — Hall Sensor



```text

Byte 0–1 : Hall raw ADC

Byte 2–3 : Hall voltage in mV

Byte 4   : Status

Byte 5–6 : Sequence

```



Example output:



```text

S4 | HALL | RAW=... V=... V STATUS=... SEQ=...

```



\---



\## `0x402` — LiDAR



```text

Byte 0–1 : Distance in mm

Byte 2–3 : Sequence

Byte 4   : Status

```



The Master detects an unavailable/invalid distance and reports it separately.



Example:



```text

S4 | LIDAR | DIST=... mm STATUS=... SEQ=...

```



\---



\# Big-Endian Encoding



All multi-byte values follow a common big-endian convention.



For a 16-bit value:



```text

Value = 0x1234



CAN DATA:



Byte 0 = 0x12

Byte 1 = 0x34

```



For a 32-bit value:



```text

Value = 0x12345678



CAN DATA:



Byte 0 = 0x12

Byte 1 = 0x34

Byte 2 = 0x56

Byte 3 = 0x78

```



The Master contains helper functions for decoding:



```c

read\_be16()

read\_be\_signed16()

read\_be32()

```



This keeps decoding consistent across every slave.



\---



\# Master Software Architecture



The Master deliberately separates CAN reception from CAN data processing.



```text

&#x20;                CAN BUS

&#x20;                   │

&#x20;                   ▼

&#x20;         ┌──────────────────┐

&#x20;         │ TWAI Controller  │

&#x20;         └────────┬─────────┘

&#x20;                  │

&#x20;                  ▼

&#x20;         ┌──────────────────┐

&#x20;         │ RX Callback      │

&#x20;         │                  │

&#x20;         │ Receive frame    │

&#x20;         │ Copy frame       │

&#x20;         └────────┬─────────┘

&#x20;                  │

&#x20;                  ▼

&#x20;         ┌──────────────────┐

&#x20;         │ FreeRTOS Queue   │

&#x20;         └────────┬─────────┘

&#x20;                  │

&#x20;                  ▼

&#x20;         ┌──────────────────┐

&#x20;         │ CAN Processing   │

&#x20;         │ Task             │

&#x20;         └────────┬─────────┘

&#x20;                  │

&#x20;           ┌──────┼──────┐

&#x20;           │      │      │

&#x20;           ▼      ▼      ▼

&#x20;          S1     S2/S3   S4

&#x20;        Decoder Decoder Decoder

&#x20;           │      │      │

&#x20;           └──────┼──────┘

&#x20;                  │

&#x20;                  ▼

&#x20;            Sensor Data

```



\---



\# Why a Queue Is Used



The CAN receive callback should remain lightweight.



Instead of performing all sensor decoding inside the callback:



```text

CAN frame

&#x20;  ↓

Callback

&#x20;  ↓

Copy frame

&#x20;  ↓

Queue

&#x20;  ↓

Processing task

&#x20;  ↓

Decode

```



This prevents lengthy processing from taking place in the receive callback and keeps the CAN reception path responsive.



\---



\# Slave Monitoring



The Master maintains a timestamp for each slave.



```text

slave1\_last\_rx

slave2\_last\_rx

slave3\_last\_rx

slave4\_last\_rx

```



The Master currently considers a slave online if it has received a frame from that slave within the configured timeout period.



Current timeout:



```text

2 seconds

```



The Master periodically prints:



```text

STATUS | S1=ONLINE S2=ONLINE S3=ONLINE S4=ONLINE

```



If a slave stops communicating:



```text

STATUS | S1=ONLINE S2=OFFLINE S3=ONLINE S4=ONLINE

```



This provides the foundation for future communication-health monitoring.



\---



\# Unknown CAN IDs



If the Master receives an ID that is not assigned to any of the four slaves, it does not attempt to decode it as sensor data.



Instead, it reports:



```text

UNKNOWN CAN ID: 0x...

```



This is useful for debugging and future expansion of the CAN network.



\---



\# Project Structure



```text

master/

│

├── CMakeLists.txt

├── sdkconfig

│

├── main/

│   ├── CMakeLists.txt

│   └── main.c

│

└── build/

&#x20;   ├── bootloader/

&#x20;   ├── partition\_table/

&#x20;   └── master.bin

```



\---



\# `main/CMakeLists.txt`



The Master currently requires the ESP-IDF TWAI driver:



```cmake

idf\_component\_register(

&#x20;   SRCS "main.c"

&#x20;   INCLUDE\_DIRS "."

&#x20;   PRIV\_REQUIRES

&#x20;       esp\_driver\_twai

)

```



\---



\# Building the Master



Open an ESP-IDF PowerShell terminal and navigate to:



```powershell

cd "C:\\Users\\Prana\\Documents\\LOOP\\ESP Multi Sensor\\master"

```



Build:



```powershell

idf.py build

```



A successful build ends with:



```text

Project build complete.

```



The generated firmware is:



```text

build\\master.bin

```



\---



\# Build Verification



The Master has been successfully built using:



```text

ESP-IDF v6.0.2

Target: ESP32

Compiler: xtensa-esp-elf

```



The successful build generated:



```text

master.bin

```



with approximately:



```text

84% of the application partition free

```



Therefore, the current Master source is \*\*compilation-verified\*\* in the project's ESP-IDF 6.0.2 environment.



\---



\# Flashing



Once hardware testing begins, the Master can be flashed using:



```powershell

idf.py flash

```



or:



```powershell

idf.py -p PORT flash

```



For example:



```powershell

idf.py -p COM5 flash

```



The actual COM port depends on the connected ESP32.



\---



\# Monitoring Serial Output



After flashing:



```powershell

idf.py monitor

```



or:



```powershell

idf.py -p COM5 monitor

```



The Master should print its initialization information followed by received CAN data.



Typical startup information:



```text

ESP MULTI SENSOR MASTER

CAN bitrate : 500 kbit/s

CAN TX      : GPIO 5

CAN RX      : GPIO 4

Encoding    : BIG-ENDIAN

CAN initialized

Master initialized.

Waiting for Slave 1, Slave 2, Slave 3 and Slave 4...

```



\---



\# Current Development Status



\## Completed



\- \[x] Master ESP32 project created

\- \[x] Master `main.c` created

\- \[x] Master switched to `main.c`

\- \[x] ESP-IDF TWAI driver integrated

\- \[x] CAN bitrate configured to 500 kbit/s

\- \[x] CAN receive architecture implemented

\- \[x] FreeRTOS receive queue implemented

\- \[x] Slave 1 CAN decoder implemented

\- \[x] Slave 2 CAN decoder implemented

\- \[x] Slave 3 CAN decoder implemented

\- \[x] Slave 4 CAN decoder implemented

\- \[x] Big-endian decoding standardized

\- \[x] Slave communication monitoring implemented

\- \[x] Unknown CAN ID handling implemented

\- \[x] Master firmware successfully compiled

\- \[x] `master.bin` successfully generated



\## Not Yet Tested



The following are intentionally outside the current code-only verification stage:



\- \[ ] Physical CAN bus communication

\- \[ ] CAN transceiver communication

\- \[ ] Optocoupler/isolation hardware

\- \[ ] Master ↔ Slave hardware connection

\- \[ ] Actual sensor data transmission

\- \[ ] CAN termination

\- \[ ] Relay operation

\- \[ ] Contactor operation

\- \[ ] Full system integration



A successful software build does \*\*not\*\* mean the physical CAN network has been tested.



\---



\# Troubleshooting



\## `esp\_driver\_twai` not found



Make sure the project is using the intended ESP-IDF environment.



Check:



```powershell

idf.py --version

```



The project was developed using:



```text

ESP-IDF v6.0.2

```



\---



\## Build directory problems



If the project configuration becomes corrupted, clean the build:



```powershell

idf.py fullclean

```



Then rebuild:



```powershell

idf.py build

```



\---



\## Master builds but receives no data



A successful compilation does not guarantee CAN communication.



Check:



```text

Master TX/RX wiring

CAN transceiver

CAN-H

CAN-L

GND

CAN bitrate

CAN IDs

CAN termination

Slave power

Slave firmware

```



All nodes must use the same:



```text

500 kbit/s

```



CAN bitrate.



\---



\## Unknown CAN ID



If the Master prints:



```text

UNKNOWN CAN ID: 0x...

```



check whether the transmitting slave is using the correct CAN ID allocation.



Expected ranges:



```text

Slave 1 → 0x100–0x102

Slave 2 → 0x200

Slave 3 → 0x300–0x302

Slave 4 → 0x400–0x402

```



\---



\# CAN ID Quick Reference



| ID | Node | Data |

|---:|---|---|

| `0x100` | Slave 1 | IMU |

| `0x101` | Slave 1 | Gyro + Hall |

| `0x102` | Slave 1 | Temperature + status |

| `0x200` | Slave 2 | ADC / voltage |

| `0x300` | Slave 3 | Temperature |

| `0x301` | Slave 3 | LiDAR + pressure |

| `0x302` | Slave 3 | Status |

| `0x400` | Slave 4 | RTD/PT100 |

| `0x401` | Slave 4 | Hall |

| `0x402` | Slave 4 | LiDAR |



\---



\# Design Philosophy



The Master is being developed in layers.



\### Layer 1 — CAN Communication



```text

CAN initialization

&#x20;      ↓

Frame reception

&#x20;      ↓

Frame queue

&#x20;      ↓

Frame decoding

```



\### Layer 2 — Sensor Data Management



```text

Decoded sensor data

&#x20;      ↓

Validation

&#x20;      ↓

Communication status

&#x20;      ↓

Centralized data structure

```



\### Layer 3 — System Control



```text

Sensor data

&#x20;      ↓

Decision logic

&#x20;      ↓

Relay / contactor control

&#x20;      ↓

System response

```



Keeping these layers separate prevents sensor communication code from becoming tightly coupled to the control system.



\---



\# Important Development Rule



The Master should remain compatible with the standardized slave CAN protocol.



If a sensor value or CAN frame changes on a slave, the corresponding CAN frame definition should be updated deliberately rather than silently changing the Master interpretation.



The CAN protocol is effectively the interface contract between:



```text

Slave firmware

&#x20;      ↕

&#x20;   CAN BUS

&#x20;      ↕

Master firmware

```



Maintaining the same:



\- CAN bitrate

\- CAN IDs

\- byte ordering

\- byte positions

\- scaling

\- status definitions



is essential for reliable communication.



\---



\# Repository



This project is part of the:



\*\*ESP Multi Sensor\*\*



repository.



The Master is located in:



```text

ESP Multi Sensor/

└── master/

```



The four sensor nodes remain separate slave projects:



```text

ESP Multi Sensor/

├── master/

├── slave 1/

├── slave 2/

├── slave 3/

└── slave 4/

```



This keeps each ESP32 firmware independently buildable and maintainable while sharing a common CAN protocol.



\---



\# Summary



The Master is the central CAN communication node of the ESP Multi Sensor architecture.



Its current responsibility is:



```text

&#x20;               ┌───────────────┐

&#x20;               │    MASTER     │

&#x20;               │     ESP32     │

&#x20;               └───────┬───────┘

&#x20;                       │

&#x20;                   CAN / TWAI

&#x20;                       │

&#x20;       ┌───────────────┼───────────────┐

&#x20;       │               │               │

&#x20;       ▼               ▼               ▼

&#x20;    Slave 1          Slave 2        Slave 3

&#x20;       │                               │

&#x20;       │                               │

&#x20;       └───────────────┬───────────────┘

&#x20;                       │

&#x20;                     Slave 4

```



It provides a standardized interface for receiving data from all four slave controllers and forms the communication foundation for the larger ESP Multi Sensor system.



\*\*Current state: BUILD VERIFIED — HARDWARE COMMUNICATION NOT YET TESTED.\*\*

