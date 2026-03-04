# Smart Lamp Tools

Python utilities for testing and debugging the Smart Lamp firmware.

## ESP-NOW Sync Test Harness

Automated test suite that controls one lamp via BLE and monitors another via USB serial to verify ESP-NOW group sync behaviour.

### Prerequisites

- Python 3.10+
- `bleak` (BLE library): `pip install bleak`
- `pyserial`: `pip install pyserial`
- One lamp reachable via BLE (SN001, the "control" lamp)
- One lamp connected via USB serial (SN003, the "monitor" lamp)

### Files

| File | Purpose |
|------|---------|
| `lamp_test.py` | Reusable library: `LampBLE` (async BLE control) + `SerialMonitor` (threaded serial log capture) |
| `test_sync.py` | Automated test suite with 8 sync tests |

### Usage

```bash
cd Tools

# Run all tests
python3 test_sync.py

# Run specific test(s)
python3 test_sync.py test_color_sync test_on_off_sync

# Verbose mode (shows all serial output)
python3 test_sync.py --verbose

# List available tests
python3 test_sync.py --list
```

### Test Cases

| Test | What it verifies |
|------|-----------------|
| `test_color_sync` | Color change on SN001 propagates to SN003 |
| `test_on_off_sync` | On/off toggle propagates lamp_on state; master is never 0 in broadcast |
| `test_flags_sync` | AUTO and FLAME mode flag changes propagate |
| `test_full_scene_sync` | Full scene parameters (color + brightness) propagate |
| `test_rapid_changes` | Multiple rapid changes converge to final state on SN003 |
| `test_group_isolation` | SN003 (group 1) ignores broadcasts from SN001 (group 2) |
| `test_sync_latency` | Measures single-change sync latency (BLE write to SN003 Sync RX) |
| `test_rapid_convergence_time` | Measures how long rapid changes take to converge on SN003 |

### How It Works

1. **Setup**: Opens the serial port (resets SN003 via DTR), waits for boot, connects to SN003 via BLE to set group=1, disconnects, resets SN003 again (BLE connection changes WiFi channel), then connects to SN001 as the control lamp.
2. **Each test**: Clears the serial buffer, performs a BLE write to SN001, waits up to 5 seconds for a matching `Sync RX` log line on SN003's serial output, and asserts the expected values.
3. **Teardown**: Resets SN001 to manual mode and disconnects.

### Configuration

Edit the constants at the top of `lamp_test.py`:

```python
SN001_MAC = "C4:4F:33:11:AA:9F"   # Control lamp (BLE)
SN003_MAC = "30:AE:A4:07:6B:6A"   # Monitor lamp (BLE, for setup only)
SERIAL_PORT = "/dev/ttyUSB0"       # Monitor lamp (serial)
```

### Using the Library Directly

```python
import asyncio
from lamp_test import LampBLE, SerialMonitor

async def main():
    lamp = LampBLE()
    await lamp.connect("C4:4F:33:11:AA:9F")

    await lamp.set_color(255, 128, 0, 200)
    await lamp.set_flags(0x01)  # AUTO mode
    await lamp.set_group(1)

    print(await lamp.get_color())
    print(await lamp.get_sensor_data())

    await lamp.disconnect()

asyncio.run(main())
```
