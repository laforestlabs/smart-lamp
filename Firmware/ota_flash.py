#!/usr/bin/env python3
"""
BLE OTA flasher for Smart Lamp firmware.
Connects by device name (works regardless of service UUID version).

Usage:
    python3 ota_flash.py <firmware.bin> [device_name_or_mac]

Examples:
    python3 ota_flash.py build/smart_lamp.bin
    python3 ota_flash.py build/smart_lamp.bin SmartLamp-AA01
    python3 ota_flash.py build/smart_lamp.bin AA:BB:CC:DD:EE:FF
"""

import asyncio
import sys
import os
from bleak import BleakScanner, BleakClient

OTA_CMD_START  = bytes([0x01])
OTA_CMD_END    = bytes([0x02])
OTA_CMD_ABORT  = bytes([0xFF])

OTA_STATUS_READY = 0x00
OTA_STATUS_BUSY  = 0x01
OTA_STATUS_OK    = 0x02
OTA_STATUS_ERROR = 0x03

CHUNK_SIZE = 490   # safe below 512-byte MTU with ATT overhead

# Known OTA characteristic UUID suffixes (bytes 12-13 of service UUID).
# Works for both old firmware (broken base) and new firmware (correct base).
OTA_CTRL_SUFFIX = "aa09"
OTA_DATA_SUFFIX = "aa0a"


def find_chr_by_suffix(services, suffix):
    """Find a characteristic whose UUID contains the given hex suffix."""
    suffix = suffix.lower()
    for svc in services:
        for chr in svc.characteristics:
            if suffix in chr.uuid.lower():
                return chr
    return None


async def scan_for_lamp(name_filter=None, mac_filter=None, timeout=15):
    print(f"Scanning for SmartLamp devices ({timeout}s)...")
    devices = await BleakScanner.discover(timeout=timeout)
    lamps = []
    for d in devices:
        name = d.name or ""
        if mac_filter and d.address.upper() == mac_filter.upper():
            return d
        if name.startswith("SmartLamp"):
            if name_filter is None or name_filter.upper() in name.upper():
                lamps.append(d)
    if not lamps:
        return None
    if len(lamps) == 1:
        return lamps[0]
    print("Multiple SmartLamp devices found:")
    for i, d in enumerate(lamps):
        print(f"  [{i}] {d.name} ({d.address})")
    idx = int(input("Select device index: "))
    return lamps[idx]


async def ota_flash(firmware_path, device_hint=None):
    firmware = open(firmware_path, "rb").read()
    total = len(firmware)
    print(f"Firmware: {firmware_path} ({total:,} bytes)")

    # Determine if hint is a MAC or name
    mac_filter  = device_hint if device_hint and ":" in device_hint else None
    name_filter = device_hint if device_hint and ":" not in device_hint else None

    device = await scan_for_lamp(name_filter, mac_filter)
    if not device:
        print("No SmartLamp device found. Make sure the lamp is advertising.")
        print("Trigger advertising: hold the touch pad for 3 seconds.")
        sys.exit(1)

    print(f"Found: {device.name} ({device.address})")

    ota_status_event = asyncio.Event()
    ota_status_value = [None]

    def on_notify(sender, data):
        status = data[0] if data else 0xFF
        ota_status_value[0] = status
        names = {0: "READY", 1: "BUSY", 2: "OK", 3: "ERROR"}
        print(f"  OTA status: {names.get(status, hex(status))}")
        ota_status_event.set()

    async with BleakClient(device.address) as client:
        print("Connected. Discovering services...")
        services = client.services

        ota_ctrl = find_chr_by_suffix(services, OTA_CTRL_SUFFIX)
        ota_data = find_chr_by_suffix(services, OTA_DATA_SUFFIX)

        if not ota_ctrl or not ota_data:
            print("Could not find OTA characteristics.")
            print("Characteristics available:")
            for svc in services:
                for c in svc.characteristics:
                    print(f"  {c.uuid}  props={c.properties}")
            sys.exit(1)

        print(f"OTA control: {ota_ctrl.uuid}")
        print(f"OTA data:    {ota_data.uuid}")

        # Subscribe to OTA control notifications
        await client.start_notify(ota_ctrl.uuid, on_notify)

        # Start OTA
        print("Starting OTA...")
        ota_status_event.clear()
        await client.write_gatt_char(ota_ctrl.uuid, OTA_CMD_START, response=True)
        await asyncio.wait_for(ota_status_event.wait(), timeout=10)
        if ota_status_value[0] != OTA_STATUS_BUSY:
            print(f"OTA start failed (status={ota_status_value[0]})")
            sys.exit(1)

        # Send firmware chunks
        print(f"Sending firmware in {CHUNK_SIZE}-byte chunks...")
        offset = 0
        while offset < total:
            chunk = firmware[offset:offset + CHUNK_SIZE]
            await client.write_gatt_char(ota_data.uuid, chunk, response=False)
            offset += len(chunk)
            pct = offset * 100 // total
            print(f"\r  {pct:3d}%  {offset:,}/{total:,} bytes", end="", flush=True)
        print()

        # Finish OTA
        print("Finalising OTA...")
        ota_status_event.clear()
        try:
            await client.write_gatt_char(ota_ctrl.uuid, OTA_CMD_END, response=True)
            await asyncio.wait_for(ota_status_event.wait(), timeout=30)
            if ota_status_value[0] != OTA_STATUS_OK:
                print(f"OTA finish failed (status={ota_status_value[0]})")
                sys.exit(1)
        except Exception:
            # Board reboots immediately after CMD_END, so the BLE
            # connection drops before the write response arrives.
            # If we already got OTA_STATUS_OK, this is expected.
            if ota_status_value and ota_status_value[0] == OTA_STATUS_OK:
                pass
            else:
                raise

        print("OTA complete! Board will reboot into new firmware.")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    fw = sys.argv[1]
    hint = sys.argv[2] if len(sys.argv) > 2 else None

    if not os.path.exists(fw):
        print(f"Firmware file not found: {fw}")
        sys.exit(1)

    asyncio.run(ota_flash(fw, hint))
