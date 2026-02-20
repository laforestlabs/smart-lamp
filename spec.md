# Smart Lamp — Product Specification

## 1. Overview

The Smart Lamp is a tunable-white LED desk/room lamp built around an ESP32-WROOM-32D
microcontroller. An array of 31 individually addressable SK6812WWA LEDs provides smooth
blending across three colour temperatures (warm, neutral, cool). A PIR motion sensor and
an ambient light sensor enable an automatic mode that turns the lamp on when presence is
detected in a dark room and turns it off after the room has been unoccupied for a
configurable timeout. The lamp is configured and controlled from a companion Flutter mobile
app over Bluetooth Low Energy (BLE).

---

## 2. Hardware

| Component | Part | Notes |
|---|---|---|
| MCU | ESP32-WROOM-32D | Dual-core Xtensa LX6, 240 MHz, integrated BLE 4.2 / Wi-Fi |
| LEDs | 31× SK6812WWA | Individually addressable, 3-channel: Warm (~2700 K), Neutral (~4000 K), Cool (~6500 K) |
| Motion sensor | BM612 | Pyroelectric IR sensor; 3.3 V; digital output (REL pin) on IO27; sensitivity set via R5 (10 KΩ) on IO25 |
| Ambient light sensor | Phototransistor (Q1) + R3 5 KΩ pull-up | Analog voltage divider; read via ESP32 ADC on IO17 (LDR_signal net) |
| Touch input | AT42QT1010 | Capacitive single-key touch IC; pad connected to IO13; digital OUT to IO16 (touchIC_sig net) |
| Power supply | TBD — 5 V / 3 A minimum recommended | LED peak current: 31 LEDs × ~20 mA × 3 channels ≈ 1.9 A; add MCU headroom |

### 2.1 Confirmed GPIO Pin Assignments

| GPIO | Net label | Connected to | Direction |
|---|---|---|---|
| IO13 | `touch` | AT42QT1010 — capacitive sense pad | Input (capacitive) |
| IO16 | `touchIC_sig` | AT42QT1010 — OUT pin | Input |
| IO17 | `LDR_signal` | Phototransistor Q1 collector via R3 pull-up | Input (ADC) |
| IO19 | `LED_signal` | SK6812WWA data line | Output |
| IO25 | `PIR_sens` | BM612 SENS pin via R5 (10 KΩ) | Output (sensitivity control) |
| IO27 | `PIR_signal` | BM612 REL (motion output) | Input |

### 2.2 BM612 PIR Sensor Wiring

| BM612 Pin | Signal | Connection |
|---|---|---|
| 1 — SENS | Sensitivity | R5 (10 KΩ) → IO25; sets detection sensitivity |
| 2 — OEN | Output enable | R4 (1 MΩ) → GND; pulled low = output always enabled |
| 3 — VSS | Ground | GND |
| 4 — VDD | Power | +3.3 V |
| 5 — REL | Motion output | IO27 (PIR_signal); high when motion detected |
| 6 — ONTIME | On-time | Sets output pulse duration (resistor/capacitor to GND) |

### 2.3 AT42QT1010 Touch Sensor

The AT42QT1010 is a single-key QTouch capacitive IC. It requires no external controller;
it drives its OUT pin high when a touch is detected and low otherwise.

| AT42QT1010 Pin | Connection | Notes |
|---|---|---|
| SNSK / pad | IO13 (`touch` net) | Capacitive sense electrode |
| OUT | IO16 (`touchIC_sig` net) | Active-high digital output to ESP32 |
| VDD | +3.3 V | |
| GND | GND | |

**Touch behaviour in firmware:**
- Short tap (< 1 s): toggle lamp on/off
- Long press (≥ 3 s): enter BLE pairing/advertising mode (replaces tactile button)
- The OUT pin is a simple GPIO input with interrupt on edge; no I2C/SPI required.

### 2.4 Ambient Light Circuit

Q1 (phototransistor) and R3 (5 KΩ pull-up to 3.3 V) form a simple analog voltage
divider. More ambient light → lower collector resistance → lower voltage at IO17.
The ESP32 ADC reads IO17; firmware maps the raw ADC count to a relative brightness
value for auto-mode threshold comparison. Absolute lux calibration can be done at
runtime via a one-point or two-point mapping stored in NVS.

### 2.5 LED Protocol

SK6812WWA uses a single-wire NZR protocol compatible with WS2812B timing. Each LED
consumes one 32-bit frame (4 bytes): `[W_cool | W_neutral | W_warm | unused]`. Byte order
must be confirmed against the specific batch datasheet. The ESP32 RMT peripheral drives
the data line; 31 LEDs requires a reset pulse (≥ 80 µs low) to latch.

### 2.6 Power Budget (preliminary)

| Rail | Consumer | Max current |
|---|---|---|
| 5 V | 31× SK6812WWA (all channels max) | ~1.86 A |
| 3.3 V | ESP32-WROOM-32D | ~500 mA (incl. BLE TX peaks) |
| 3.3 V | Sensors, button pull-ups | ~10 mA |

Recommend a 5 V / 3 A supply with an onboard 3.3 V LDO or buck converter for the MCU.

---

## 3. Firmware

### 3.1 Development Environment

**Framework: ESP-IDF (official Espressif IoT Development Framework)**

Rationale for choosing ESP-IDF over alternatives:

- **vs. Arduino/ESP-Arduino**: The Arduino NeoPixel and FastLED libraries drive SK6812
  via the RMT peripheral using the same DMA channel that the BLE stack requires. Running
  BLE and addressable LEDs simultaneously with Arduino causes known timing glitches and
  crashes. ESP-IDF provides lower-level RMT control that coexists safely with the NimBLE
  stack.
- **vs. MicroPython**: Insufficient determinism for single-wire LED timing at 31 LEDs;
  BLE support is incomplete. Not suitable for a production device.
- **ESP-IDF provides**: native NimBLE stack, NVS (persistent key-value storage), OTA
  partition management, RTOS task isolation, and stable I2C/ADC drivers out of the box.

Recommended ESP-IDF version: **v5.x** (latest stable).

### 3.2 LED Driver

- Use the **RMT (Remote Control) peripheral** to generate SK6812-compatible waveforms.
- Allocate a dedicated RMT channel and DMA buffer large enough for 31 × 32 bits.
- Expose a simple API:
  ```c
  void lamp_set_channel(uint8_t warm, uint8_t neutral, uint8_t cool); // 0–255 each
  void lamp_set_master(uint8_t brightness);  // scales all channels proportionally
  void lamp_update(void);                    // commits buffer to RMT and sends
  ```
- Gamma correction LUT (2.2 or configurable) applied before writing to the RMT buffer.

### 3.3 Sensor Handling

**PIR sensor (BM612)**
- IO27 (`PIR_signal`) configured as GPIO input; interrupt on rising edge (motion start)
  and falling edge (motion end).
- IO25 (`PIR_sens`) driven as GPIO output to control the sensitivity resistor network.
  Initial value: logic high (maximum sensitivity); can be adjusted in firmware or NVS.
- Interrupt handler posts to a FreeRTOS event queue; a sensor task consumes events and
  drives the auto-mode state machine.

**Touch sensor (AT42QT1010)**
- IO16 (`touchIC_sig`) configured as GPIO input with interrupt on both edges.
- A software timer distinguishes short tap (< 1 s) from long press (≥ 3 s):
  - Short tap: toggle lamp on/off (or step through brightness in manual mode — TBD)
  - Long press: enter BLE advertising / pairing mode
- Debounce handled in firmware (AT42QT1010 output is already debounced internally, but
  edge-glitch filtering recommended).

**Ambient light (phototransistor Q1)**
- IO17 (`LDR_signal`) sampled via the ESP32 ADC1 at a configurable interval (default: 1 s).
- Raw 12-bit ADC reading mapped to a relative brightness value (0–100).
- Auto-mode threshold stored in NVS as a raw ADC count or mapped brightness unit.
- Optional one-point calibration: user places lamp in a known-bright or known-dark
  environment and triggers calibration from the app; offset written to NVS.

### 3.4 Auto Mode Logic

See §7 for the full state machine.

Auto mode is active when the user has enabled it (stored in NVS). In auto mode:
- Motion events and periodic lux readings drive the state machine.
- Manual brightness/colour changes from the app override the current scene without
  disabling auto mode.

### 3.5 BLE GATT Server

- Stack: **NimBLE** (lighter weight than Bluedroid; recommended for ESP-IDF v5+).
- One custom primary service (see §5).
- Characteristics use 512-byte MTU negotiation for OTA data throughput.
- Authorisation: bonding with *Just Works* pairing by default (no passkey required).
  Upgrade to passkey/numeric comparison is a future option.
- Connection event triggers a NVS read to restore current lamp state.

### 3.6 NVS (Persistent Storage)

Stored across power cycles:
- Active scene (channel values + master brightness)
- Mode (manual / auto)
- Auto mode config (timeout, lux threshold, dim level, dim duration)
- Up to 16 named scenes
- Up to 16 schedules
- BLE bond keys (managed automatically by NimBLE)

### 3.7 OTA Firmware Updates

- Uses ESP-IDF's standard **two-partition OTA scheme** (`ota_0` / `ota_1`).
- Update flow initiated by the app writing to the OTA Control characteristic.
- Firmware binary chunked to ≤ 512 bytes and streamed over the OTA Data characteristic.
- On successful write, lamp verifies image and reboots; rolls back to previous partition
  on boot failure (ESP-IDF rollback mechanism).

---

## 4. Mobile Application

**Framework: Flutter** (Android + iOS from a single codebase)

**BLE library: `flutter_reactive_ble`** — recommended over `flutter_blue_plus` for
more reliable connection state management and characteristic streaming on both platforms.

### 4.1 Pairing & Connection Flow

See §6 for full detail. Summary:
1. User taps **Add Lamp** in the app.
2. App scans for BLE advertisements matching the `SmartLamp` service UUID.
3. User holds the AT42QT1010 touch pad on the lamp for 3 s to start advertising.
4. App shows discovered lamps; user taps to connect and bond.
5. On success, the lamp is saved to local app storage and auto-reconnects on future launches.

### 4.2 Manual Control Screen

- Three sliders: **Warm**, **Neutral**, **Cool** (0–100 %).
- One master **Brightness** slider (scales all channels).
- Real-time preview: changes write immediately to the LED State characteristic.
- "Save as scene" button.

### 4.3 Auto Mode

- Toggle to enable / disable auto mode.
- Settings panel (visible when enabled):
  - **Lux threshold** — don't activate if room is brighter than this (default: 50 lux)
  - **Timeout** — seconds of no motion before dimming begins (default: 300 s)
  - **Dim level** — brightness during the dim-warning phase (default: 30 %)
  - **Dim duration** — seconds in dim state before full off (default: 30 s)

### 4.4 Scenes & Presets

- List of named scenes stored both on the lamp (NVS) and cached locally in the app.
- Create: set channels manually → tap Save → enter name.
- Apply: tap scene card → writes to lamp via LED State + Scene characteristics.
- Delete: swipe to delete; syncs deletion to lamp.

### 4.5 Schedules & Timers

- List of schedule rules, each with:
  - Day-of-week mask (Mon–Sun, individually selectable)
  - Time (HH:MM, 24-hour)
  - Action: apply a named scene, or turn off
  - Enable/disable toggle
- Schedules stored on the lamp; lamp executes them using its internal clock.

> **Note**: Reliable schedule execution requires either an onboard RTC or Wi-Fi NTP
> sync. See §8 (Open Questions).

### 4.6 OTA Firmware Update

- Settings screen shows current firmware version (read from a device info characteristic).
- "Update Firmware" button lets the user pick a `.bin` file from local storage.
- App streams the binary in chunks over BLE; progress bar shown.
- On completion, app displays reboot confirmation.

---

## 5. BLE GATT Protocol

**Primary Service UUID** (128-bit, to be formally assigned):
`F000AA00-0451-4000-B000-000000000000` *(placeholder — replace before production)*

### Characteristics

| Name | UUID (suffix) | Properties | Payload format |
|---|---|---|---|
| **LED State** | `...0001` | Read, Write, Notify | `[warm: u8, neutral: u8, cool: u8, master: u8]` |
| **Mode** | `...0002` | Read, Write | `[mode: u8]` — `0`=manual, `1`=auto |
| **Auto Config** | `...0003` | Read, Write | `[timeout_s: u16 LE, lux_threshold: u16 LE, dim_pct: u8, dim_duration_s: u16 LE]` |
| **Scene Write** | `...0004` | Write | `[index: u8, name_len: u8, name: utf8[name_len], warm: u8, neutral: u8, cool: u8, master: u8]` |
| **Scene List** | `...0005` | Read, Notify | Length-prefixed list of scenes; same struct as Scene Write |
| **Schedule Write** | `...0006` | Write | `[index: u8, day_mask: u8, hour: u8, minute: u8, scene_index: u8, enabled: u8]` |
| **Schedule List** | `...0007` | Read, Notify | Length-prefixed list of schedules |
| **Sensor Data** | `...0008` | Read, Notify | `[lux: u16 LE, motion: u8]` |
| **OTA Control** | `...0009` | Write, Notify | `[cmd: u8, ...]` — `0x01`=start, `0x02`=end, `0xFF`=abort; notify returns status |
| **OTA Data** | `...000A` | Write Without Response | Raw firmware bytes (up to MTU−3 per write) |
| **Device Info** | `...000B` | Read | `[fw_version: utf8]` |

All multi-byte integers are **little-endian**.

---

## 6. Pairing & Connection Flow

```
[User: tap "Add Lamp"]
        │
        ▼
App starts BLE scan ─── filters by service UUID ──────────────────┐
                                                                    │
[User: hold AT42QT1010 touch pad 3 s on lamp]                      │
        │                                                          │
        ▼                                                          │
Lamp enters ADVERTISING MODE                                       │
  • LED pulses slowly (breathing effect)                           │
  • Broadcasts connectable undirected advertisement                │
  • Device name: "SmartLamp-XXXX" (last 4 hex of MAC)             │
  • Advertising times out after 60 s if no connection             │
        │                                                          │
        └─────────────────► App shows discovered lamp in list      │
                                                                   │
[User: taps lamp in list]                                         │
        │                                                          │
        ▼                                                          │
App connects + requests MTU upgrade (512 bytes)                   │
App initiates BLE bonding (Just Works)                            │
Bond keys stored on both sides                                    │
        │                                                          │
        ▼                                                          │
App reads Device Info, LED State, Mode, Scene List, Schedule List │
UI transitions to main control screen                             │

[Subsequent sessions]
App auto-scans for bonded lamp on foreground and reconnects silently.
Lamp only advertises on touch-pad long press; it does not broadcast continuously
when idle (saves power).
```

---

## 7. Auto Mode State Machine

```
        ┌──────────────────────────────────────────────────────────┐
        │                        IDLE                              │
        │                   (LEDs off)                             │
        └──────────────┬───────────────────────────────────────────┘
                       │ motion detected
                       │ AND lux < threshold
                       ▼
        ┌──────────────────────────────────────────────────────────┐
        │                         ON                               │
        │           (active scene, full brightness)                │
        └──────────────┬───────────────────────────────────────────┘
                       │ no motion for [timeout] seconds
                       ▼
        ┌──────────────────────────────────────────────────────────┐
        │                      DIMMING                             │
        │        (brightness reduced to dim_pct %)                 │
        └────────┬─────────────────────────┬────────────────────────┘
                 │ motion detected          │ [dim_duration] seconds
                 │                          │ elapsed
                 ▼                          ▼
               ON                        IDLE
```

**Configurable parameters** (stored in NVS, settable from app):

| Parameter | Default | Description |
|---|---|---|
| `lux_threshold` | 50 lux | Suppress activation above this ambient brightness |
| `timeout_s` | 300 s | No-motion period before entering DIMMING |
| `dim_pct` | 30 % | Master brightness during DIMMING phase |
| `dim_duration_s` | 30 s | Time in DIMMING before going to IDLE |

---

## 8. Open Questions / TBD

| # | Item | Notes |
|---|---|---|
| 1 | SK6812WWA byte order | Verify `[cool, neutral, warm, X]` from the specific batch datasheet |
| 2 | Power supply spec | Confirm rail voltages, connector type, and current headroom with enclosure design |
| 3 | Enclosure / PCB | Mechanical design, LED diffuser, touch pad placement |
| 4 | Schedule timekeeping | Add external RTC (e.g. DS3231 over I2C) **or** enable Wi-Fi + SNTP for NTP sync; without one of these, schedules will drift after power cycles |
| 5 | OTA binary distribution | Local `.bin` file picked from phone, or hosted on a server URL? Affects app UX and update workflow |
| 6 | BLE security level | *Just Works* is convenient but unauthenticated. Upgrade to Passkey Entry if lamp is used in shared/public spaces |
| 7 | Custom service UUID | Generate a proper random 128-bit UUID before finalising the protocol |
| 8 | Maximum scene / schedule count | 16 each is a working assumption; verify against available NVS flash space |
| 9 | IO25 / PIR_sens usage | Schematic routes IO25 to BM612 SENS via 10 KΩ — confirm whether firmware drives this pin or if it is purely a passive resistor network |
| 10 | Ambient light ADC calibration | Determine whether a one-point NVS calibration is sufficient or if a lookup table is needed for the phototransistor response curve |
| 11 | Touch tap behaviour | Decide short-tap action: simple on/off toggle, or step through brightness levels |
