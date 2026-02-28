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
| LED level shifter | LVC1G125 (U2) | 3.3 V → 5 V buffer on LED data line (IO19 → DIN) |
| 3.3 V regulator | AP2114H-3_3TRG1 (U1) | 1 A LDO; powered from USB 5V |
| Power input | USB Micro B (J1) | 5 V from host/charger; sole power source |
| Motion sensor | BM612 (P1) | Pyroelectric IR sensor; 3.3 V; digital output (REL pin) on IO27; sensitivity set via R5 (10 KΩ) on IO25 |
| Ambient light sensor | Phototransistor (Q1) + R3 5 KΩ pull-up | Analog voltage divider; read via ESP32 ADC on IO17 (LDR_signal net) |
| Touch input | AT42QT1010-TSHR (U3) | Capacitive single-key touch IC; pad on IO13; digital OUT to IO16; C4 10 nF sets sensitivity |
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

### 2.5 LED Physical Layout

The 31 LEDs are arranged in a 7-row oval/hexagonal grid, 5 columns wide at the widest.
Rows 0 and 6 are trimmed to 3 LEDs (columns 1–3), giving the array rounded ends.
The data chain runs left-to-right, top-to-bottom: D1 → D2 → … → D31.

```
Column:   0      1      2      3      4
          ────────────────────────────
Row 0:         [ D1]  [ D2]  [ D3]
Row 1:  [ D4]  [ D5]  [ D6]  [ D7]  [ D8]
Row 2:  [ D9]  [D10]  [D11]  [D12]  [D13]
Row 3:  [D14]  [D15]  [D16]  [D17]  [D18]   ← geometric centre (2, 3)
Row 4:  [D19]  [D20]  [D21]  [D22]  [D23]
Row 5:  [D24]  [D25]  [D26]  [D27]  [D28]
Row 6:         [D29]  [D30]  [D31]
```

LED index to (col, row) lookup table (0-indexed, used by flame and effect algorithms):

| LED | col | row | LED | col | row | LED | col | row |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|
| D1  |  1  |  0  | D12 |  3  |  2  | D23 |  4  |  4  |
| D2  |  2  |  0  | D13 |  4  |  2  | D24 |  0  |  5  |
| D3  |  3  |  0  | D14 |  0  |  3  | D25 |  1  |  5  |
| D4  |  0  |  1  | D15 |  1  |  3  | D26 |  2  |  5  |
| D5  |  1  |  1  | D16 |  2  |  3  | D27 |  3  |  5  |
| D6  |  2  |  1  | D17 |  3  |  3  | D28 |  4  |  5  |
| D7  |  3  |  1  | D18 |  4  |  3  | D29 |  1  |  6  |
| D8  |  4  |  1  | D19 |  0  |  4  | D30 |  2  |  6  |
| D9  |  0  |  2  | D20 |  1  |  4  | D31 |  3  |  6  |
| D10 |  1  |  2  | D21 |  2  |  4  |     |     |     |
| D11 |  2  |  2  | D22 |  3  |  4  |     |     |     |

### 2.6 LED Protocol

SK6812WWA uses a single-wire NZR protocol compatible with WS2812B timing. Each LED
consumes one **24-bit frame (3 bytes)**: `[cool | warm | neutral]`. *(Confirmed on
SN001 hardware — not 32-bit like SK6812 RGBW.)* The ESP32 RMT peripheral drives
the data line; 31 LEDs requires a reset pulse (≥ 80 µs low) to latch.

The LED data line runs through a **LVC1G125 level-shifter** (U2) that translates the
ESP32's 3.3 V logic to the 5 V required by the SK6812WWA DIN pin.

### 2.7 Power Topology

```
USB Micro B (J1)
  └── VBUS (+5V) ──┬──► AP2114H-3_3TRG1 LDO (U1) ──► +3.3V rail
                   │       (ESP32, AT42QT1010, BM612, sensors)
                   └──► +5V rail ──► SK6812WWA LEDs (via LVC1G125 data shifter)
```

- **U1 AP2114H-3_3TRG1**: 1 A LDO, input from USB 5V, output 3.3V for all logic
- **U2 LVC1G125**: single-gate 3.3V→5V buffer on IO19 → LED DIN
- **J1 USB Micro B**: sole power input; no external DC barrel jack on current design
- Decoupling: C1/C2 on LDO output; C5 (10 µF) + C3 (0.1 µF) on ESP32 3V3; C4 (10 nF) on AT42QT1010 VDD

### 2.8 Power Budget (preliminary)

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
- Allocate a dedicated RMT channel with 384 symbol memory blocks (6 blocks) to minimise
  ISR ping-pong refill events — smaller blocks risk BLE ISR latency causing false reset
  pulses mid-frame, which corrupt channel byte alignment.
- Maintain an internal frame buffer of 31 `led_pixel_t` structs — one per LED — so that
  effects like flame can set each LED independently before flushing.
- Expose a layered API:
  ```c
  /* Per-LED control (used by flame and future effects) */
  void lamp_set_pixel(uint8_t index, uint8_t warm, uint8_t neutral, uint8_t cool);

  /* Uniform fill (used by manual mode and scenes) */
  void lamp_fill(uint8_t warm, uint8_t neutral, uint8_t cool);

  /* Master brightness scale — applied at flush time, not stored in buffer */
  void lamp_set_master(uint8_t brightness);  // 0–255

  /* Commit frame buffer to RMT and send */
  void lamp_flush(void);
  ```
- Gamma correction LUT (2.2) applied per-channel at `lamp_flush()` time. Gamma is
  applied **before** master brightness scaling so that low master values don't crush
  dim pixels into the gamma dead zone (i.e., `out = gamma(channel) × master / 255`).
- A `led_coord` lookup table (index → col, row) is compiled into firmware from the
  layout defined in §2.5 for use by spatial effect algorithms.

### 3.3 Sensor Handling

**PIR sensor (BM612)**
- IO27 (`PIR_signal`) configured as GPIO input; interrupt on rising edge (motion start)
  and falling edge (motion end).
- IO25 (`PIR_sens`) driven as **DAC output** (DAC_CHAN_0) to control sensitivity.
  BM612 SENS pin: 0V = max sensitive, VDD/2 = least sensitive, 32 discrete levels.
  DAC value = `(31 - level) * 4` where level 0–31 is the BLE/NVS setting (default: 24).
  Sensitivity is adjustable via the PIR Sensitivity BLE characteristic (0x000B) and
  persisted in NVS.
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

Auto mode and flame mode are **independent boolean toggles** (stored in NVS as a
bitmask). Either or both can be enabled simultaneously:

- **Auto only:** motion events and lux readings drive the state machine; lamp shows
  the active scene with static white LEDs.
- **Flame only:** animated candle-flame effect runs continuously at the configured
  brightness.
- **Both enabled:** auto mode controls motion-based on/off timing, flame mode provides
  the animated visual output. On motion → flame starts; on dim → flame master reduced;
  on idle timeout → flame stops and LEDs turn off.
- **Neither (manual):** user controls brightness and colour directly from the app.

Manual brightness/colour changes from the app override the current scene without
disabling auto or flame mode. The on/off button is always available and can override
auto mode.

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
- Mode flags (bitmask: bit 0 = auto enabled, bit 1 = flame enabled)
- Auto mode config (timeout, lux threshold, dim level, dim duration)
- Up to 16 named scenes (each includes colour channels + mode flags)
- Up to 16 schedules
- PIR sensitivity level (0–31, default 24)
- Flame config (8 parameters)
- BLE bond keys (managed automatically by NimBLE)

### 3.7 Flame Mode

#### Goal

Simulate a candle flame by moving a bright "hot spot" across the LED array in 2D space.
Because each LED's physical position is known (§2.5), the brightness gradient cast across
the array mirrors a real flame's light distribution — LEDs farther from the hot spot are
dimmer, replicating the moving shadows a candle casts on the surrounding room.

#### Coordinate System

The LED grid spans **cols 0–4** and **rows 0–6**, with the geometric centre at **(2.0, 3.0)**.
Rows 0 and 6 only have columns 1–3; the algorithm treats positions in those gaps as
unpopulated (no LED to write).

#### Flame Centre Random Walk

The flame centre `(fx, fy)` is a floating-point position that drifts each frame:

```
fx_new = fx + gaussian(0, σ_x) − k_restore × (fx − 2.0)
fy_new = fy + gaussian(0, σ_y) − k_restore × (fy − bias_y)
```

- `σ_x`, `σ_y` — lateral and vertical drift speed (configurable; default σ_x = 0.25, σ_y = 0.20)
- `k_restore` — spring constant pulling the flame back toward centre (default 0.08)
- `bias_y` — vertical resting position; set slightly above centre (default 2.5) to
  simulate the upward tendency of a real flame
- `fx` is clamped to [1.0, 3.0]; `fy` clamped to [0.5, 5.5] to keep the hot spot
  within the populated region of the array

#### Per-LED Intensity

Each frame, for every LED `i` at grid position `(cx_i, cy_i)`:

```
d²  = (cx_i − fx)² + (cy_i − fy)²
I_i = master_brightness × exp(−d² / (2 × σ_flame²))
```

- `σ_flame` — Gaussian radius of the hot spot (configurable; default 1.4 grid units)
- `I_i` is a 0.0–1.0 scale factor applied to the active scene's colour channels:
  `warm_out = scene.warm × I_i`, `neutral_out = scene.neutral × I_i`,
  `cool_out = scene.cool × I_i`. The flame is a pure spatial/temporal filter on
  whatever colour the user has selected — it preserves the exact channel mix.

#### Global Flicker

A separate slow oscillator adds overall brightness variation independent of position:

```
flicker = 1.0 − flicker_depth × |sin(t × flicker_freq + φ)|
```

- `flicker_depth` — amplitude of the dip (default 0.15; 0 = no flicker)
- `flicker_freq` — frequency in rad/s (default ~3–5 Hz, randomised each cycle)
- `φ` — random phase offset, re-randomised every 0.5–2.0 s to avoid regularity
- Applied as a final multiplier to all `I_i` values before `lamp_flush()`

#### Frame Rate

The flame task runs at **30 fps** (33 ms tick) on a dedicated FreeRTOS task with a
fixed-period timer. This is fast enough for smooth motion without overloading the CPU.

#### Configurable Parameters (NVS + BLE)

| Parameter | Default | Description |
|---|---|---|
| `flame_drift_x` | 0.25 | Lateral drift standard deviation per frame |
| `flame_drift_y` | 0.20 | Vertical drift standard deviation per frame |
| `flame_restore` | 0.08 | Spring constant back to centre |
| `flame_radius` | 1.4 | Gaussian σ of the hot-spot in grid units |
| `flame_bias_y` | 2.5 | Vertical resting position (0 = top, 6 = bottom) |
| `flicker_depth` | 0.15 | Global flicker amplitude (0–1) |
| `flicker_speed` | 4.0 | Flicker base frequency in Hz |
| `flame_brightness` | 200 | Master brightness ceiling (0–255) |

### 3.8 OTA Firmware Updates

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

- **ON/OFF button** — prominent toggle at the top; sets master brightness to 0 (off) or
  128 (on). Amber when on, grey when off. Works in all modes: in flame mode it
  stops/starts the flame task; in auto mode it disables/enables motion detection.
- Three sliders: **Warm**, **Neutral**, **Cool** (0–100 %).
- One master **Brightness** slider (scales all channels).
- Real-time preview: changes write immediately to the LED State characteristic.
- "Save as scene" button.

### 4.3 Mode Toggles

The main control screen provides two independent toggle switches:

- **Auto Mode** — motion-activated on/off with configurable timeout and dimming.
- **Flame Effect** — animated candle-flame simulation.

Both can be enabled simultaneously (see §3.4). All manual controls (on/off button,
colour sliders, brightness slider, save-as-scene) remain visible regardless of which
toggles are active.

### 4.3.1 Auto Mode Settings

- Settings panel (accessible via a card when auto is enabled):
  - **Motion sensitivity** — PIR detection range, 0 (closest) to 31 (farthest), default 24
  - **Lux threshold** — don't activate if room is brighter than this (default: 50 lux)
  - **Timeout** — seconds of no motion before dimming begins (default: 300 s)
  - **Dim level** — brightness during the dim-warning phase (default: 30 %)
  - **Dim duration** — seconds in dim state before full off (default: 30 s)

### 4.4 Scenes & Presets

- List of named scenes stored both on the lamp (NVS) and cached locally in the app.
- Create: set channels manually → tap Save → enter name. Current mode flags
  (auto/flame) are saved with the scene.
- Apply: tap scene card → writes to lamp via LED State + Mode characteristics.
  Mode flags (auto/flame) are restored from the scene. Scene cards show small
  flame/auto icons when those modes are stored.
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

### 4.6 Flame Mode

- Enabled via the Flame Effect toggle on the control screen (see §4.3).
- When active, the app shows a **flame visualiser**: a live 7×5 grid preview (with the
  corner cells greyed out) that mirrors the current hot-spot position and Gaussian falloff
  in real-time using Notify on the LED State characteristic.
- **Settings panel:**
  - **Intensity** — master brightness ceiling slider (0–100 %)
  - **Drift speed** — single slider mapping to both `flame_drift_x` / `flame_drift_y`
    (labelled "Calm ↔ Wild")
  - **Hot-spot size** — maps to `flame_radius` (labelled "Focused ↔ Diffuse")
  - **Flicker** — toggle + depth slider (0–100 %)
  - **Flicker speed** — labelled "Slow ↔ Fast"
- Settings are written to the Flame Config characteristic and persisted to NVS on the lamp.
- The app does **not** drive the animation — all computation runs on the ESP32. The app
  only reflects the lamp's current state via notifications.

### 4.7 OTA Firmware Update

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
| **Mode** | `...0002` | Read, Write | `[flags: u8]` — bitmask: bit 0 (`0x01`) = auto enabled, bit 1 (`0x02`) = flame enabled. `0x00`=manual, `0x01`=auto, `0x02`=flame, `0x03`=both |
| **Auto Config** | `...0003` | Read, Write | `[timeout_s: u16 LE, lux_threshold: u16 LE, dim_pct: u8, dim_duration_s: u16 LE]` |
| **Scene Write** | `...0004` | Write | `[index: u8, name_len: u8, name: utf8[name_len], warm: u8, neutral: u8, cool: u8, master: u8, mode_flags: u8]` — mode_flags optional (default 0) |
| **Scene List** | `...0005` | Read, Notify | Length-prefixed list of scenes; same struct as Scene Write (includes mode_flags) |
| **Schedule Write** | `...0006` | Write | `[index: u8, day_mask: u8, hour: u8, minute: u8, scene_index: u8, enabled: u8]` |
| **Schedule List** | `...0007` | Read, Notify | Length-prefixed list of schedules |
| **Sensor Data** | `...0008` | Read, Notify | `[lux: u16 LE, motion: u8]` — notified every 1 s (lux update) and on motion start/end |
| **OTA Control** | `...0009` | Write, Notify | `[cmd: u8, ...]` — `0x01`=start, `0x02`=end, `0xFF`=abort; notify returns status |
| **OTA Data** | `...000A` | Write Without Response | Raw firmware bytes (up to MTU−3 per write) |
| **PIR Sensitivity** | `...000B` | Read, Write | `[level: u8]` — 0 (closest) to 31 (farthest); DAC output on IO25 controls BM612 SENS pin |
| **Flame Config** | `...000C` | Read, Write | `[drift_x: u8, drift_y: u8, restore: u8, radius: u8, bias_y: u8, flicker_depth: u8, flicker_speed: u8, brightness: u8]` — all scaled 0–255 |
| **Device Info** | `...000D` | Read | `[fw_version: utf8]` |

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
App auto-connects to the first bonded lamp on launch and navigates directly
to the control screen once connected. Lamp only advertises on touch-pad long
press; it does not broadcast continuously when idle (saves power).
(Temporary: lamp always advertises on boot — see §8.4.)
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

## 8. Hardware Bring-Up Notes

### 8.1 Programmer Fixture Power Limitation

The ifuturetech ESP32-WROOM burning fixture can flash firmware successfully but **cannot
supply enough current** for the ESP32's RF calibration during BLE initialisation. The
`register_chipv7_phy()` call draws a current spike that causes the board to reset in an
infinite boot loop (`SW_CPU_RESET`).

**Workaround:** Disconnect the 3V3 line from the programmer fixture and power the board
from its own USB supply. Keep GND, TX, and RX connected to the jig for serial monitoring.
The fixture's auto-reset (DTR/RTS) still works for flashing when 3V3 is connected; just
disconnect it before booting with BLE.

### 8.2 PHY Calibration

- `CONFIG_ESP_PHY_RF_CAL_PARTIAL=y` and `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=y`
  are the correct sdkconfig settings.
- On first boot after a full erase, the PHY performs a full calibration and stores the
  result in the `phy_init` NVS partition. Subsequent boots use the stored data (partial
  cal only).
- **Do not** modify `esp_phy/src/phy_init.c` to change the fallback calibration mode —
  the default (`PHY_RF_CAL_FULL` when no stored data exists) is correct.

### 8.3 Board Status (as of 2026-02-28)

| Board | MAC (BT) | Device Name | Status |
|---|---|---|---|
| SN001 | c4:4f:33:11:aa:9f | SmartLamp-AA9F | Flashed (OTA), BLE + ESP-NOW working. LEDs and light sensor need PCB rework. |
| SN002 | 30:ae:a4:07:59:d2 | SmartLamp-59D2 | Flashed (OTA), BLE + ESP-NOW working. Motion sensor verified. LEDs and light sensor need PCB rework. |
| SmartLamp-6B68 | 30:ae:a4:07:6b:68 | SmartLamp-6B68 | Flashed, functional |

**Verified working:** BLE advertising, BLE connection + bonding, MTU 512 negotiation,
GATT read/write/notify, PIR motion detection, mode flags (auto/flame independent toggles),
OTA firmware updates (BLE, Python tool + Flutter app), ESP-NOW group sync (mostly working,
occasional stuck lamp — see §8.6), BLE re-advertising after disconnect, multi-device app switching.

**Not yet verified (pending PCB fixes):** LED output, ambient light sensor ADC reading.

### 8.6 ESP-NOW Group Sync

Lamps with the same group ID (1–255, 0 = disabled) broadcast state changes (on/off,
brightness, colour, mode flags) to each other via ESP-NOW over WiFi channel 1.

**Architecture:**
- WiFi STA mode (no AP connection) provides the radio for ESP-NOW
- BLE init must happen BEFORE WiFi init on ESP32 for coexistence
- 13-byte packed broadcast message (magic, version, group_id, sequence, WNCM, flags)
- Length-1 FreeRTOS queue with `xQueueOverwrite` for broadcast coalescing
- Loop prevention: `s_from_sync` flag in `lamp_control.c`; `lamp_control_apply_sync()`
  applies flags + state + `s_lamp_on` atomically

**Known issue:** Receiving lamp occasionally gets stuck and stops responding to sync
messages or BLE connections. Suspected cause: `lamp_control_apply_sync()` calling
`set_flags()` → mode transition (flame start/stop) that blocks or crashes in the WiFi
task context (ESP-NOW receive callback). Needs serial monitor investigation.

### 8.4 Temporary Firmware Behaviour

- **Always advertise on boot:** The lamp starts BLE advertising automatically on every
  power-up, regardless of existing bonds. This is a temporary measure because the touch
  button hardware is not yet functional. Revert to bond-count check + touch long-press
  once the AT42QT1010 circuit is verified.

### 8.5 Build & Deploy Procedures

#### Firmware (ESP-IDF)

```bash
cd Firmware
idf.py build
```

**Serial flash** (programming jig):
```bash
sudo chmod a+rw /dev/ttyUSB0
~/.espressif/python_env/idf5.3_py3.13_env/bin/python -m esptool \
  --chip esp32 -p /dev/ttyUSB0 -b 460800 --after hard_reset \
  write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xf000 build/ota_data_initial.bin \
  0x20000 build/smart_lamp.bin
```

**OTA flash** (over BLE, for assembled lamps):
```bash
python3 ota_flash.py build/smart_lamp.bin
```
Or use the Device tab in the Flutter app (file picker → progress bar).

#### Flutter App

```bash
cd Software/smart_lamp
/home/jason/flutter/bin/flutter build apk --debug
```

**Install to phone** — use `adb install` directly:
```bash
adb -s 18121JECB02951 install -r build/app/outputs/flutter-apk/app-debug.apk
```

> **WARNING:** Do NOT use `flutter install` without `--debug`. It defaults to the
> release APK (`app-release.apk`), which may be a stale build from a previous session.
> Always use `adb install -r` with the explicit debug APK path, or pass `--debug`:
> `flutter install --debug`.

---

## 9. Open Questions / TBD

| # | Item | Notes |
|---|---|---|
| ~~1~~ | ~~SK6812WWA byte order~~ | **Resolved:** 24-bit `[cool, warm, neutral]` — confirmed on SN001 hardware |
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
