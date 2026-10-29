# ESP Multi Sensor - Master

Central CAN receiver for the ESP Multi Sensor system. The Master listens on a
500 kbit/s CAN bus, decodes the frames sent by four ESP32 slave nodes, prints
the decoded sensor values on the serial monitor, and tracks whether each slave
is still alive.

> **Status:** receive, decode and monitor only. No hardware control is
> implemented yet.

| Item        | Value                                   |
|-------------|-----------------------------------------|
| Board       | ESP32-WROOM-32 DevKit                   |
| SDK         | ESP-IDF v6.x                            |
| Language    | C (single file, `main/main.c`)          |
| CAN bitrate | 500 kbit/s                              |
| CAN format  | Standard 11-bit ID, 8-byte classic frames |
| Byte order  | Big-endian for all multi-byte values    |

---

## 1. System overview

+-----------+  +-----------+  +-----------+  +-----------+  +-----------+  +-----------+
|  Slave 1  |  |  Slave 2  |  |  Slave 3  |  |  Slave 4  |  |  HV BMS   |  |  LV BMS   |
| IMU, Hall |  |  ADS1113  |  |2x IR temp |  | 2x PT100  |  | High Volt |  | Low Volt  |
| 2x PT100  |  |    ADC    |  |LiDAR, pres|  |Hall, LiDAR|  | Pack Mgmt |  | System Mgm|
+-----------+  +-----------+  +-----------+  +-----------+  +-----------+  +-----------+
      |              |              |              |              |              |
  [CAN XCVR]     [CAN XCVR]     [CAN XCVR]     [CAN XCVR]         |              |
      |              |              |              |              |              |
======+==============+==============+==============+==============+==============+======= CAN bus
                                                                                 (CANH / CANL)
                                                            |
                                                       [CAN XCVR]
                                                            |
                                                      +-----------+
                                                      |  MASTER   | decode + print + monitor
                                                      +-----------+

Every node runs its own firmware and sends fixed-format 8-byte frames at a
fixed rate. The Master never polls the slaves; it only listens.

---

## 2. Hardware

### 2.1 Master wiring

| ESP32 pin | Connects to           |
|-----------|-----------------------|
| GPIO 5    | CAN transceiver TXD   |
| GPIO 4    | CAN transceiver RXD   |
| 3V3 / GND | Transceiver supply    |

The ESP32 has an on-chip CAN (TWAI) controller but **no CAN transceiver**.
You need an external 3.3 V compatible transceiver (for example SN65HVD230 or
TJA1051T/3) between the ESP32 and the bus.

### 2.2 Bus requirements

- Two-wire bus: CANH and CANL, shared by all five nodes.
- **120 ohm termination at each physical end of the bus, and nowhere else.**
  With the correct two resistors you should measure about 60 ohm between CANH
  and CANL with everything powered off.
- All nodes share a common ground.
- Every node uses 500 kbit/s. A single node with a different bitrate will
  disturb the whole bus.
- Keep stubs short.

### 2.3 Why the Master must be powered

A CAN transmitter expects at least one receiver to acknowledge each frame. The
Master is an active node, so it provides that acknowledgement. If the Master is
off or not connected, slaves will see transmit errors even though their own
wiring is fine.

---

## 3. CAN ID map

Lower ID = higher priority on the bus. IDs are grouped by node.

| Node    | CAN ID | Contents                         |
|---------|--------|----------------------------------|
| Slave 1 | 0x100  | Accelerometer + sequence         |
| Slave 1 | 0x101  | Gyroscope + Hall ADC             |
| Slave 1 | 0x102  | PT100 temperatures + status      |
| Slave 2 | 0x200  | ADS1113 measurement              |
| Slave 3 | 0x300  | Two IR temperature sensors       |
| Slave 3 | 0x301  | LiDAR distance + pressure        |
| Slave 3 | 0x302  | Status flags                     |
| Slave 4 | 0x400  | PT100 temperatures + status      |
| Slave 4 | 0x401  | Hall sensor                      |
| Slave 4 | 0x402  | LiDAR distance                   |

### 3.1 Frame layouts

All multi-byte fields are big-endian (most significant byte first).
"x100" means the value is multiplied by 100 (hundredths of a unit).

**Slave 1**

| ID    | Bytes 0-1 | Bytes 2-3 | Bytes 4-5 | Bytes 6-7        | Other |
|-------|-----------|-----------|-----------|------------------|-------|
| 0x100 | AX (int16)| AY (int16)| AZ (int16)| sequence (uint16)|       |
| 0x101 | GX (int16)| GY (int16)| GZ (int16)| Hall A0 raw (uint16) |   |
| 0x102 | T1 x100 (int16) | T2 x100 (int16) | byte 4: Hall D0 | byte 5: RTD1 fault, byte 6: RTD2 fault | byte 7: status (bit0 IMU OK, bit1 Hall ADC OK) |

IMU raw scale (ICM-42688 power-on defaults): accelerometer 2048 LSB/g
(+-16 g), gyroscope 16.4 LSB/(deg/s) (+-2000 dps).

**Slave 2**

| ID    | Bytes 0-1        | Bytes 2-3            | Bytes 4-5               | Bytes 6-7        |
|-------|------------------|----------------------|-------------------------|------------------|
| 0x200 | raw ADC (int16)  | ADC pin voltage (mV, uint16) | input voltage (mV, uint16) | packet counter (uint16) |

**Slave 3**

| ID    | Bytes 0-1 | Bytes 2-3 | Bytes 4-5 | Bytes 6-7 |
|-------|-----------|-----------|-----------|-----------|
| 0x300 | T1 ambient x100 | T1 object x100 | T2 ambient x100 | T2 object x100 |
| 0x301 | LiDAR (mm, uint16) | pressure (Pa, uint32) across bytes 2-5 | | sequence (uint16) |
| 0x302 | byte 0 status: bit0 GY-906 #1, bit1 GY-906 #2, bit2 LiDAR, bit3 pressure | bytes 1-7 reserved | | |

Invalid values sent by Slave 3: temperature `0x8000` (-32768), LiDAR `0xFFFF`,
pressure `0xFFFFFFFF`.

**Slave 4 (layout the Master expects)**

| ID    | Bytes 0-1 | Bytes 2-3 | Byte 4 | Bytes 5-6 |
|-------|-----------|-----------|--------|-----------|
| 0x400 | PT100 #1 x100 (int16) | PT100 #2 x100 (int16) | status | sequence (uint16) |
| 0x401 | Hall raw (uint16) | Hall voltage (mV, uint16) | status | sequence (uint16) |
| 0x402 | LiDAR distance (mm, int16, negative = unavailable) | sequence (uint16) | status | - |

---

## 4. Project setup

### 4.1 Files

```
master/
  CMakeLists.txt            (project-level, created by idf.py)
  main/
    CMakeLists.txt
    main.c
  README.md
```

`main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_twai esp_driver_gpio)
```

### 4.2 Build and flash

```
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

Replace `<PORT>` with your serial port (for example `COM7` on Windows). Exit
the monitor with `Ctrl+]`.

### 4.3 Expected output

```
I (xxx) MASTER: CAN initialized
I (xxx) MASTER: Master initialized.
I (xxx) MASTER: Waiting for Slave 1, Slave 2, Slave 3 and Slave 4...
I (xxx) MASTER: S1 | IMU | AX=12 AY=-3 AZ=2046 SEQ=118
I (xxx) MASTER: S1 | TEMP | T1=24.51 C T2=24.60 C HALL_D0=1 RTD1F=0 RTD2F=0 STATUS=0x03
I (xxx) MASTER: S2 | ADC | RAW=16384 ADC=1.024 V INPUT=1.536 V SEQ=42
I (xxx) MASTER: S3 | STATUS | 0x0F | T1=OK T2=OK LIDAR=OK PRESSURE=OK
I (xxx) MASTER: STATUS | S1=ONLINE S2=ONLINE S3=ONLINE S4=OFFLINE
```

---

## 5. How the code works

### 5.1 Architecture

```
 CAN bus
    |
    v
 TWAI hardware RX
    |  (frame received)
    v
 can_rx_callback()          runs in interrupt context, kept very short
    |  copies id + 8 bytes
    v
 FreeRTOS queue (32 slots)
    |
    v
 can_processing_task        normal task: identify slave, decode, print
    |
    +--> process_can_message() -> decode_slave1..4()

 master_status_task         separate task: prints ONLINE/OFFLINE every 1 s
```

The key design decision is **never doing real work in the interrupt**. The
callback only copies the frame into a queue and returns. Logging, decoding and
float math all happen in a normal task, so the CAN controller is never kept
waiting.

### 5.2 Section by section

**Configuration and ID defines.** Pins, bitrate and queue length are `#define`s
at the top. Every CAN ID has a named constant (`S1_ID_IMU`, `S3_ID_STATUS`,
...), so the decoders read like documentation.

**`can_rx_message_t`.** A small struct holding the CAN ID, the 8 data bytes and
the length. This is the item stored in the queue.

**Last-receive timestamps.** `slave1_last_rx` to `slave4_last_rx` store the
FreeRTOS tick count at which a frame from that slave last arrived. They are
`volatile` because they are written by one task and read by another.

**Big-endian helpers.** `read_be16`, `read_be_signed16` and `read_be32` rebuild
numbers from bytes (`data[0] << 8 | data[1]`). The signed version simply casts
the result to `int16_t`, which is how negative temperatures and accelerations
come out correctly.

**`can_rx_callback()`.** Called by the TWAI driver when a frame arrives. It:
1. points a `twai_frame_t` at a local buffer,
2. calls `twai_node_receive_from_isr()` to pull the frame out of the hardware,
3. copies the ID,
4. pushes the message to the queue with `xQueueSendFromISR()`,
5. returns `true` if a higher-priority task was woken, so FreeRTOS switches to
   it immediately.

**`can_init()`.** Creates the queue first (so the callback can never run
without a queue), creates the on-chip TWAI node with the pins and bitrate,
registers the RX callback, and only then enables the node. The order matters:
registering the callback before `twai_node_enable()` guarantees no frame is
missed.

**Decoders.** `decode_slave1()` to `decode_slave4()` each switch on the CAN ID,
pull the fields out with the helpers, scale them (for example `/ 100.0f` for
temperatures, `/ 1000.0f` for millivolts), and print one log line.

**`process_can_message()`.** The dispatcher. It looks at the ID, records the
current tick time for the owning slave, then calls that slave's decoder.
Unknown IDs produce a warning.

**`can_processing_task`.** Blocks on `xQueueReceive(..., portMAX_DELAY)` and
calls `process_can_message()` for each frame. Priority 10.

**`master_status_task`.** Every second, compares `now - last_rx` for each slave
against a 2 second timeout and prints `ONLINE` or `OFFLINE`. Priority 5 (lower
than processing).

**`app_main()`.** Prints a banner, calls `can_init()`, starts the two tasks and
returns. Everything afterwards is event-driven.

---

## 6. Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| No frames at all | Missing or wrong transceiver, TX/RX swapped, no common ground, bitrate mismatch, missing termination |
| Slaves report CAN ERROR | Master off/disconnected (nobody acknowledges), or a bus fault |
| Frames appear and vanish, bus errors | Termination wrong (measure ~60 ohm CANH-CANL, power off) |
| `UNKNOWN CAN ID` warnings | A slave is sending an ID the Master does not know |
| Slave 4 always OFFLINE | Slave 4 firmware does not send CAN yet (see below) |
| Slave 2 values look wrong | Byte-order mismatch (see below) |

---

## 7. Known issues and to-do

1. **Slave 2 byte order mismatch.** The Master decodes everything as
   big-endian, but Slave 2 firmware packs frame 0x200 as little-endian. Until
   one side changes, the Master will show wrong Slave 2 values. Easiest fix:
   change Slave 2 to big-endian.
2. **Slave 4 has no CAN yet.** The Master already decodes 0x400-0x402, but the
   current Slave 4 firmware only prints to serial. Slave 4 will show OFFLINE.
3. **Startup shows ONLINE for the first 2 seconds.** The `last_rx` timestamps
   start at 0, so `now - 0 < timeout` is true right after boot. Fix by
   initializing them to a "never received" value or tracking a received flag.
4. **Invalid-value markers are printed as numbers.** Slave 3 sends `0x8000`,
   `0xFFFF` and `0xFFFFFFFF` for invalid readings; the Master prints them as
   -327.68 C, 65535 mm and 4294967295 Pa. Check the markers and print `N/A`.
5. **Serial load.** At full rate the slaves produce well over 100 decoded
   lines per second. At the default 115200 baud the log can fall behind and
   the 32-slot queue can overflow. Raise the monitor baud rate or print only
   every Nth frame.
6. **No frame-loss detection.** Sequence numbers are decoded but not compared.
7. **No bus-off recovery.** The TWAI node is not restarted if it goes bus-off.
8. **Acceptance filter not used.** All IDs reach the callback; frames with
   unexpected IDs only produce a warning.
9. **Planned:** hardware control, command frames from Master to slaves, and a
   fault/health summary derived from the slave status bytes.
