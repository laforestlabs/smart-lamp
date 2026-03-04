"""
Smart Lamp BLE Control + Serial Monitor Library

Provides programmatic control of lamps via BLE (using bleak) and
serial output monitoring (using pyserial) for automated sync testing.
"""

import asyncio
import re
import struct
import threading
import time
from dataclasses import dataclass, field
from typing import Optional

import serial
from bleak import BleakClient, BleakScanner

# Strip ANSI escape sequences from serial output
_ANSI_RE = re.compile(r'\x1b\[[0-9;]*m')

# ── Constants ──

SERVICE_UUID = "f000aa00-0451-4000-b000-000000000000"

def _char_uuid(suffix: int) -> str:
    return f"f000aa{suffix:02x}-0451-4000-b000-000000000000"

CHAR_LED_STATE    = _char_uuid(0x01)  # [warm, neutral, cool, master] 4B R/W/N
CHAR_MODE_FLAGS   = _char_uuid(0x02)  # [flags] 1B R/W
CHAR_AUTO_CONFIG  = _char_uuid(0x03)  # [timeout_s:LE16, lux:LE16] 4B R/W
CHAR_SCENE_WRITE  = _char_uuid(0x04)  # variable W
CHAR_SCENE_LIST   = _char_uuid(0x05)  # variable R/N
CHAR_SENSOR_DATA  = _char_uuid(0x08)  # [lux:LE16, motion:u8] 3B R/N
CHAR_PIR_SENS     = _char_uuid(0x0B)  # [level] 1B R/W
CHAR_FLAME_CONFIG = _char_uuid(0x0C)  # 8B R/W
CHAR_SYNC_CONFIG  = _char_uuid(0x0E)  # [group_id:u8, mac:6B] 7B R/W
CHAR_LAMP_NAME    = _char_uuid(0x0F)  # UTF-8 string R/W

# Known devices
SN001_MAC = "C4:4F:33:11:AA:9F"
SN003_MAC = "30:AE:A4:07:6B:6A"
SERIAL_PORT = "/dev/ttyUSB0"
SERIAL_BAUD = 115200

MODE_FLAG_AUTO  = 0x01
MODE_FLAG_FLAME = 0x02


# ── BLE Lamp Controller ──

class LampBLE:
    """Async BLE client for controlling a Smart Lamp."""

    def __init__(self):
        self._client: Optional[BleakClient] = None
        self.mac: Optional[str] = None

    @property
    def connected(self) -> bool:
        return self._client is not None and self._client.is_connected

    async def connect(self, mac: str, timeout: float = 15.0):
        """Connect to lamp by MAC address."""
        self.mac = mac
        print(f"[BLE] Scanning for {mac}...")
        device = await BleakScanner.find_device_by_address(mac, timeout=timeout)
        if device is None:
            raise ConnectionError(f"Device {mac} not found")
        print(f"[BLE] Found {device.name or mac}, connecting...")
        self._client = BleakClient(device)
        await self._client.connect(timeout=timeout)
        print(f"[BLE] Connected to {device.name or mac}")

    async def disconnect(self):
        """Disconnect from lamp."""
        if self._client and self._client.is_connected:
            await self._client.disconnect()
            print(f"[BLE] Disconnected from {self.mac}")
        self._client = None

    # ── Color / Brightness ──

    async def set_color(self, warm: int, neutral: int, cool: int, master: int):
        """Write LED state: [warm, neutral, cool, master] (0-255 each)."""
        data = bytes([warm, neutral, cool, master])
        await self._client.write_gatt_char(CHAR_LED_STATE, data)
        print(f"[BLE] set_color({warm}, {neutral}, {cool}, {master})")

    async def get_color(self) -> tuple:
        """Read LED state. Returns (warm, neutral, cool, master)."""
        data = await self._client.read_gatt_char(CHAR_LED_STATE)
        return tuple(data[:4])

    async def turn_off(self):
        """Turn lamp off by setting master=0."""
        color = await self.get_color()
        await self.set_color(color[0], color[1], color[2], 0)

    async def turn_on(self, master: int = 200):
        """Turn lamp on by setting master to given value."""
        color = await self.get_color()
        await self.set_color(color[0], color[1], color[2], master)

    # ── Mode Flags ──

    async def set_flags(self, flags: int):
        """Write mode flags (0x01=AUTO, 0x02=FLAME)."""
        await self._client.write_gatt_char(CHAR_MODE_FLAGS, bytes([flags]))
        print(f"[BLE] set_flags(0x{flags:02x})")

    async def get_flags(self) -> int:
        """Read mode flags."""
        data = await self._client.read_gatt_char(CHAR_MODE_FLAGS)
        return data[0]

    # ── Auto Config ──

    async def set_auto_config(self, timeout_s: int, lux_threshold: int):
        """Write auto config: [timeout_s:LE16, lux:LE16]."""
        data = struct.pack("<HH", timeout_s, lux_threshold)
        await self._client.write_gatt_char(CHAR_AUTO_CONFIG, data)
        print(f"[BLE] set_auto_config(timeout={timeout_s}s, lux={lux_threshold})")

    # ── Group / Sync ──

    async def set_group(self, group_id: int):
        """Set ESP-NOW sync group ID (0=disabled, 1-255=active)."""
        await self._client.write_gatt_char(CHAR_SYNC_CONFIG, bytes([group_id]))
        print(f"[BLE] set_group({group_id})")

    async def get_group(self) -> int:
        """Read group ID (first byte of sync config)."""
        data = await self._client.read_gatt_char(CHAR_SYNC_CONFIG)
        return data[0]

    # ── PIR Sensitivity ──

    async def set_pir_sensitivity(self, level: int):
        """Set PIR sensitivity (0-31)."""
        await self._client.write_gatt_char(CHAR_PIR_SENS, bytes([level & 0x1F]))
        print(f"[BLE] set_pir_sensitivity({level})")

    # ── Flame Config ──

    async def set_flame_config(self, drift_x=128, drift_y=102, restore=20,
                                radius=128, bias_y=128, flicker_depth=13,
                                flicker_speed=13, brightness=255):
        """Write flame config (8 bytes)."""
        data = bytes([drift_x, drift_y, restore, radius, bias_y,
                      flicker_depth, flicker_speed, brightness])
        await self._client.write_gatt_char(CHAR_FLAME_CONFIG, data)
        print(f"[BLE] set_flame_config(...)")

    # ── Scene Write ──

    async def write_scene(self, index: int, name: str, warm: int, neutral: int,
                          cool: int, master: int, flags: int = 0,
                          fade_in_s: int = 3, fade_out_s: int = 10,
                          auto_timeout_s: int = 300, auto_lux: int = 185,
                          flame_drift_x=128, flame_drift_y=102, flame_restore=20,
                          flame_radius=128, flame_bias_y=128, flame_flicker_depth=13,
                          flame_flicker_speed=13, flame_brightness=255,
                          pir_sensitivity=16):
        """Write a full scene to char AA04."""
        name_bytes = name.encode("utf-8")[:16]
        data = bytearray()
        data.append(index)
        data.append(len(name_bytes))
        data.extend(name_bytes)
        data.extend([warm, neutral, cool, master])
        data.append(flags)
        data.append(fade_in_s)
        data.append(fade_out_s)
        data.extend(struct.pack("<HH", auto_timeout_s, auto_lux))
        data.extend([flame_drift_x, flame_drift_y, flame_restore, flame_radius,
                     flame_bias_y, flame_flicker_depth, flame_flicker_speed,
                     flame_brightness])
        data.append(pir_sensitivity)
        await self._client.write_gatt_char(CHAR_SCENE_WRITE, bytes(data))
        print(f"[BLE] write_scene(idx={index}, name='{name}', "
              f"[{warm},{neutral},{cool},{master}] flags=0x{flags:02x})")

    # ── Sensor Data ──

    async def get_sensor_data(self) -> tuple:
        """Read sensor data. Returns (lux, motion_bool)."""
        data = await self._client.read_gatt_char(CHAR_SENSOR_DATA)
        lux = struct.unpack("<H", data[:2])[0]
        motion = bool(data[2])
        return lux, motion

    # ── Lamp Name ──

    async def get_name(self) -> str:
        data = await self._client.read_gatt_char(CHAR_LAMP_NAME)
        return data.decode("utf-8", errors="replace")

    async def set_name(self, name: str):
        await self._client.write_gatt_char(CHAR_LAMP_NAME, name.encode("utf-8")[:32])
        print(f"[BLE] set_name('{name}')")


# ── Serial Monitor ──

@dataclass
class SyncRxEvent:
    """Parsed 'Sync RX' log line from lamp_ctrl."""
    warm: int = 0
    neutral: int = 0
    cool: int = 0
    master: int = 0
    flags: int = 0
    lamp_on: int = 0


class SerialMonitor:
    """Threaded serial port monitor that captures and parses firmware logs."""

    # Regex for lamp_ctrl "Sync RX" lines
    SYNC_RX_RE = re.compile(
        r"Sync RX: \[(\d+),(\d+),(\d+),(\d+)\] flags=0x([0-9a-fA-F]+) lamp_on=(\d+)"
    )
    # Regex for esp_now_sync "RX from" lines
    ESPNOW_RX_RE = re.compile(
        r"RX from ([0-9A-Fa-f:]+) seq=(\d+) \[(\d+),(\d+),(\d+),(\d+)\] "
        r"flags=0x([0-9a-fA-F]+) lamp_on=(\d+)"
    )

    def __init__(self, verbose: bool = False):
        self._serial: Optional[serial.Serial] = None
        self._thread: Optional[threading.Thread] = None
        self._running = False
        self._lines: list[str] = []
        self._lock = threading.Lock()
        self._event = threading.Event()
        self.verbose = verbose

    def start(self, port: str = SERIAL_PORT, baud: int = SERIAL_BAUD):
        """Open serial port and start reader thread.
        Note: opening the serial port may reset the ESP32 (DTR toggle).
        The caller should wait for boot to complete before starting tests."""
        self._serial = serial.Serial(
            port, baud, timeout=0.1,
            dsrdtr=False, rtscts=False
        )
        self._running = True
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()
        print(f"[Serial] Monitoring {port} @ {baud}")

    def stop(self):
        """Stop monitoring and close port."""
        self._running = False
        if self._thread:
            self._thread.join(timeout=2)
        if self._serial and self._serial.is_open:
            self._serial.close()
        print("[Serial] Stopped")

    def _reader(self):
        while self._running:
            try:
                line = self._serial.readline()
                if line:
                    decoded = line.decode("utf-8", errors="replace").strip()
                    decoded = _ANSI_RE.sub('', decoded)
                    if decoded:
                        if self.verbose:
                            print(f"[SER] {decoded}")
                        with self._lock:
                            self._lines.append(decoded)
                        self._event.set()
            except Exception:
                if self._running:
                    time.sleep(0.1)

    def clear(self):
        """Flush all captured lines."""
        with self._lock:
            self._lines.clear()
        self._event.clear()

    def get_lines(self) -> list[str]:
        """Return and clear all captured lines."""
        with self._lock:
            lines = list(self._lines)
            self._lines.clear()
        self._event.clear()
        return lines

    def wait_for(self, pattern: str, timeout: float = 5.0) -> Optional[re.Match]:
        """Wait for a line matching regex pattern. Returns the match or None."""
        regex = re.compile(pattern)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with self._lock:
                for i, line in enumerate(self._lines):
                    m = regex.search(line)
                    if m:
                        # Remove this line and all before it
                        self._lines = self._lines[i + 1:]
                        return m
            remaining = deadline - time.monotonic()
            if remaining > 0:
                self._event.wait(timeout=min(remaining, 0.2))
                self._event.clear()
        return None

    def wait_for_sync_rx(self, timeout: float = 5.0) -> Optional[SyncRxEvent]:
        """Wait for a 'Sync RX' log line and parse it."""
        m = self.wait_for(self.SYNC_RX_RE.pattern, timeout)
        if m:
            return SyncRxEvent(
                warm=int(m.group(1)),
                neutral=int(m.group(2)),
                cool=int(m.group(3)),
                master=int(m.group(4)),
                flags=int(m.group(5), 16),
                lamp_on=int(m.group(6)),
            )
        return None

    def dump_recent(self, max_lines: int = 20):
        """Print recent lines for debugging."""
        with self._lock:
            lines = list(self._lines[-max_lines:])
        if not lines:
            print("[Serial] (no recent output)")
        else:
            for line in lines:
                print(f"  | {line}")
