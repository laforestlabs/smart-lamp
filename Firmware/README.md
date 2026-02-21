# Smart Lamp Firmware

ESP-IDF firmware for the Smart Lamp, a tunable-white LED desk lamp built around the ESP32-WROOM-32D. Controls 31 SK6812WWA LEDs (warm/neutral/cool channels), reads PIR motion and ambient light sensors, drives an automatic lighting mode, runs a flame animation, and exposes everything over BLE GATT for the companion Flutter app.

## Hardware

| Component | Part | GPIO |
|-----------|------|------|
| LEDs | 31x SK6812WWA (WWA = Warm / Neutral / Cool) | IO19 (RMT) |
| PIR sensor | BM612 | IO27 (signal), IO25 (sensitivity) |
| Touch sensor | AT42QT1010 capacitive | IO16 (output), IO13 (pad) |
| Ambient light | Phototransistor + 5K pull-up | IO17 (ADC1 CH7) |

The 31 LEDs are arranged in a 7-row oval grid (up to 5 columns wide). The physical layout is defined in `components/led_driver/led_layout.c`.

## Architecture

```
main.c                          Boot sequence & task creation
  |
  +-- led_driver/               RMT encoder, framebuffer, gamma LUT, layout table
  +-- sensor/                   PIR (GPIO ISR), touch (short/long press), ambient (ADC)
  +-- lamp_nvs/                 NVS persistence: scenes, schedules, configs
  +-- auto_mode/                IDLE -> ON -> DIMMING -> IDLE state machine
  +-- flame_mode/               30 fps Gaussian hot-spot random walk + flicker
  +-- ble_service/              NimBLE GAP + GATT (12 characteristics)
  +-- lamp_ota/                 Two-partition OTA via BLE
  +-- lamp_control/             Central event loop & mode manager
```

### FreeRTOS Tasks

| Task | Priority | Stack | Purpose |
|------|----------|-------|---------|
| `lamp_control_task` | 5 | 4096 | Main event loop: sensor events, touch, BLE commands |
| `flame_task` | 4 | 4096 | 30 fps animation; created/deleted on mode switch |
| `sensor_adc_task` | 3 | 2048 | 1 s ambient light ADC polling |
| NimBLE host | 6 | 4096 | Internal BLE stack |

### Component Details

**led_driver** -- Drives 31 SK6812WWA LEDs via the RMT peripheral on IO19. Custom NZR encoder (T0H = 300 ns, T1H = 600 ns, T0L = 900 ns, T1L = 300 ns, reset >= 80 us). Applies gamma 2.2 correction and master brightness scaling before each flush. The framebuffer is mutex-protected for thread safety.

**sensor** -- PIR motion detection via GPIO ISR on IO27 (both edges); IO25 driven HIGH for maximum sensitivity. Touch via GPIO ISR on IO16 with software timer discriminating short press (< 1 s, toggles on/off) from long press (>= 3 s, starts BLE advertising). Ambient light via ADC1 on IO17, sampled every 1 s with a 5-sample median filter, mapped to 0-100 (inverted: high voltage = dark). All events are posted to a shared FreeRTOS queue consumed by `lamp_control`.

**lamp_nvs** -- Wraps ESP-IDF NVS for persistent storage. Stores up to 16 scenes (`scene_00` - `scene_15`), 7 schedules, auto mode config, flame mode config, active LED state, and current mode. Writes are debounced (2 s timer) to reduce flash wear from slider changes.

**auto_mode** -- State machine with three states: IDLE (lamp off, waiting for motion in dark room), ON (lamp at full brightness while motion detected), DIMMING (warning phase after timeout with no motion). Configurable lux threshold, timeout, dim level, and dim duration. Transitions are driven by sensor events fed through `auto_mode_process_event()`.

**flame_mode** -- Creates a dedicated 30 fps FreeRTOS task. Simulates a candle with a 2D Gaussian hot-spot that random-walks across the LED grid (Box-Muller RNG via `esp_random()`). A global flicker oscillator modulates overall brightness. Per-LED intensity is computed as `exp(-d^2 / 2*sigma^2)` from each LED's distance to the hot-spot center. Output splits between warm and neutral channels for natural color. All parameters (drift, radius, flicker depth/speed, brightness) are adjustable at runtime via BLE.

**ble_service** -- NimBLE-based BLE peripheral advertising as `SmartLamp-XXXX` (last 4 hex digits of MAC). Just Works bonding, 512-byte MTU, 60 s advertising timeout. Defines a custom GATT service (`F000AA00-0451-4000-B000-000000000000`) with 12 characteristics:

| Characteristic | UUID suffix | Properties | Size |
|---------------|-------------|------------|------|
| LED State | AA01 | Read, Write, Notify | 4 B |
| Mode | AA02 | Read, Write | 1 B |
| Auto Config | AA03 | Read, Write | 7 B |
| Scene Write | AA04 | Write | variable |
| Scene List | AA05 | Read, Notify | variable |
| Schedule Write | AA06 | Write | 6 B |
| Schedule List | AA07 | Read, Notify | variable |
| Sensor Data | AA08 | Read, Notify | 3 B |
| OTA Control | AA09 | Write | 1 B |
| OTA Data | AA0A | Write No Rsp | variable |
| Flame Config | AA0B | Read, Write | 8 B |
| Device Info | AA0C | Read | variable |

BLE writes post events to a queue; `lamp_control` consumes them. LED State notifications are rate-limited to 10 Hz; Sensor Data notifies on motion change and every 5 s.

**lamp_ota** -- Two-partition OTA using `esp_ota_begin/write/end`. The app receives firmware chunks over BLE (OTA Data characteristic) and streams them to the inactive OTA partition. On success the device reboots into the new firmware. On boot, `lamp_ota_check_rollback()` validates the running image and rolls back if it was marked pending verification.

**lamp_control** -- Central event loop running as a FreeRTOS task. Consumes sensor events from the shared queue, dispatches touch actions (short tap = on/off toggle, long press = BLE advertising), manages mode switching (manual/auto/flame), and routes BLE commands to the appropriate subsystem. Restores saved state from NVS on boot.

## Partition Table

```
nvs,       data, nvs,  0x9000,   0x6000    # 24 KB
otadata,   data, ota,  0xf000,   0x2000    #  8 KB
phy_init,  data, phy,  0x11000,  0x1000    #  4 KB
ota_0,     app,  ota_0,0x20000,  0x1C0000  # 1.75 MB
ota_1,     app,  ota_1,0x1E0000, 0x1C0000  # 1.75 MB
```

Current firmware binary is ~556 KB, well within the 1.75 MB OTA slot.

## Building

### Prerequisites

- [ESP-IDF v5.3](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32/get-started/)

### Build & Flash

```bash
# Source ESP-IDF environment
. ~/esp/esp-idf/export.sh

# Build
cd Firmware
idf.py build

# Flash (adjust port as needed)
idf.py -p /dev/ttyUSB0 flash monitor
```

### Key sdkconfig Options

The `sdkconfig.defaults` file sets:
- Target: ESP32, 4 MB flash @ 80 MHz
- BLE-only mode (no classic BT) via NimBLE
- 1 max BLE connection, 512-byte MTU
- Custom partition table with OTA rollback enabled
- FreeRTOS tick rate: 1000 Hz
- Compiler optimization: size (`-Os`)
