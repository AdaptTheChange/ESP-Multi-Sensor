\# ESP32 Slave 2: ADS1113 Analog Voltage over CAN



An ESP32 node that reads an analog voltage using the \*\*TI ADS1113 16-bit ADC\*\* and transmits the measured voltage over \*\*CAN/TWAI\*\* at 500 kbit/s.



The intended input range is \*\*0–2.5 V\*\*.



\## Hardware



| Part | Qty | Interface |

|---|---:|---|

| ESP32-WROOM-32 DevKit | 1 | n/a |

| ADS1113 | 1 | I²C |

| 3.3 V CAN transceiver | 1 | CAN |

| 120 Ω resistor | 2 | CAN termination |



\## Pin Map



| Function | ESP32 GPIO |

|---|---:|

| I²C SDA | GPIO 21 |

| I²C SCL | GPIO 22 |

| CAN TX | GPIO 5 |

| CAN RX | GPIO 4 |



\## ADS1113 Wiring



| ADS1113 | ESP32 / Connection |

|---|---|

| VDD | 3.3 V |

| GND | GND |

| SDA | GPIO 21 |

| SCL | GPIO 22 |

| AINP | Analog input, 0–2.5 V |

| AINN | GND |



The ADS1113 uses its I²C interface to communicate with the ESP32.



The ADS1113 default I²C address is:



```text

0x48

```



\### Important



The input voltage must remain within the permitted ADS1113 input range.



For this project the intended measurement range is:



```text

0.0 V → 2.5 V

```



Do not apply a voltage higher than the permitted ADC input/supply limits.



\## CAN Wiring



| ESP32 | CAN Transceiver |

|---|---|

| GPIO 5 | TXD |

| GPIO 4 | RXD |

| 3.3 V | VCC |

| GND | GND |



Connect:



```text

CANH → CANH

CANL → CANL

```



A 120 Ω termination resistor should be present at each physical end of the CAN bus.



Use a \*\*3.3 V-compatible CAN transceiver\*\*.



\## ADC Measurement



The ADS1113 provides a 16-bit ADC conversion.



The software converts the ADC result into voltage and reports the measured value in millivolts.



Example:



```text

ADC = 0 V       → 0 mV

ADC = 1.25 V    → 1250 mV

ADC = 2.50 V    → 2500 mV

```



\## CAN Frame



Slave 2 uses the `0x200` CAN ID range.



\### Frame 1 — ADC Voltage



```text

CAN ID: 0x200

DLC:    8

```



| Byte | Data |

|---|---|

| 0–1 | Voltage in mV, uint16, big-endian |

| 2–3 | Raw ADC value, int16, big-endian |

| 4–5 | Sequence number, uint16, big-endian |

| 6 | ADC status |

| 7 | Reserved |



\### Status



```text

0x01 = ADC reading valid

0x00 = ADC reading invalid

```



\## CAN Configuration



```text

Bitrate: 500 kbit/s

Frame type: Standard 11-bit CAN

CAN ID: 0x200

```



Slave 2 therefore occupies the `0x200` range, while the other slaves can use their own ranges.



Example:



```text

Slave 1 → 0x100+

Slave 2 → 0x200+

Slave 3 → 0x300+

Slave 4 → 0x400+

```



\## Serial Output



The ESP32 prints the ADC reading approximately every 100 ms.



Example:



```text

ADC RAW: 16384 | Voltage: 1250 mV | CAN: OK

```



For a 2.5 V input, the output should be approximately:



```text

Voltage: 2500 mV

```



Actual readings may vary slightly because of ADC characteristics and electrical noise.



\## Project Structure



```text

slave 2/

├── CMakeLists.txt

├── README.md

├── sdkconfig

└── main/

&#x20;   ├── CMakeLists.txt

&#x20;   └── main.c

```



\## Build



From the `slave 2` directory:



```powershell

idf.py set-target esp32

idf.py build

```



\## Flash



After connecting the ESP32:



```powershell

idf.py -p COM5 flash

```



Replace `COM5` with the actual ESP32 COM port.



\## Monitor



```powershell

idf.py -p COM5 monitor

```



Or flash and monitor together:



```powershell

idf.py -p COM5 flash monitor

```



Press:



```text

Ctrl+]

```



to exit the monitor.



\## Troubleshooting



| Problem | Possible cause |

|---|---|

| ADS1113 not detected | Check SDA, SCL, VCC and GND |

| Voltage always 0 | Check AINP and AINN wiring |

| Incorrect voltage | Check ADC configuration and input voltage |

| CAN transmission fails | Check CAN transceiver, CANH/CANL and bus termination |

| ESP32 does not boot | Disconnect CAN wiring and test the ESP32 separately |

| CAN frames not acknowledged | Connect another active CAN node |



\## Important



The hardware has \*\*not yet been physically tested\*\* if the ADS1113 and other required sensors/transceiver hardware are unavailable.



The project should therefore be treated as \*\*software-prepared and build-testable\*\*, rather than hardware-verified.

