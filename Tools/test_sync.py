#!/usr/bin/env python3
"""
ESP-NOW Sync Test Suite

Automated tests for Smart Lamp group sync functionality.
Controls SN001 via BLE, monitors SN003 via USB serial.

Usage:
    python3 test_sync.py                  # Run all tests
    python3 test_sync.py test_color_sync  # Run specific test
    python3 test_sync.py --list           # List available tests
    python3 test_sync.py --verbose        # Show all serial output
"""

import asyncio
import sys
import time
import traceback

from lamp_test import (
    LampBLE, SerialMonitor, SyncRxEvent,
    SN001_MAC, SN003_MAC, SERIAL_PORT,
    MODE_FLAG_AUTO, MODE_FLAG_FLAME, MODE_FLAG_CIRCADIAN,
)

# ── Test Infrastructure ──

class TestResult:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.errors = []

    def ok(self, name: str, detail: str = ""):
        self.passed += 1
        msg = f"  PASS  {name}"
        if detail:
            msg += f" — {detail}"
        print(msg)

    def fail(self, name: str, reason: str):
        self.failed += 1
        self.errors.append((name, reason))
        print(f"  FAIL  {name} — {reason}")

    def summary(self):
        total = self.passed + self.failed
        print(f"\n{'='*50}")
        print(f"Results: {self.passed}/{total} passed, {self.failed} failed")
        if self.errors:
            print("\nFailures:")
            for name, reason in self.errors:
                print(f"  - {name}: {reason}")
        print(f"{'='*50}")
        return self.failed == 0


# Global test state
lamp = LampBLE()
verbose = "--verbose" in sys.argv
monitor = SerialMonitor(verbose=verbose)
result = TestResult()

# How long to wait for sync to arrive on SN003 (seconds)
# 10 retries over ~1.5s + propagation margin
SYNC_TIMEOUT = 6.0


async def reset_baseline():
    """Reset SN001 to manual mode with known color. Wait for SN003 to settle."""
    await lamp.set_flags(0x00)
    await asyncio.sleep(0.5)
    await lamp.set_color(128, 128, 128, 200)
    # Wait for all 10 retries (~1.5s) + SN003 processing + margin
    await asyncio.sleep(3.0)
    monitor.clear()


async def setup():
    """Connect to both lamps (sequentially) and configure groups."""
    print("\n--- Setup ---")

    # Start serial monitor (may reset SN003 via DTR toggle)
    monitor.start(SERIAL_PORT)

    # Wait for SN003 to boot (~2s) and WiFi/ESP-NOW to init
    print("[Setup] Waiting for SN003 boot...")
    boot_done = monitor.wait_for("ESP-NOW sync init", timeout=8.0)
    if boot_done:
        print("[Setup] SN003 boot complete")
    else:
        print("[Setup] WARNING: Did not see boot complete marker (may already be running)")
    await asyncio.sleep(1.0)
    monitor.clear()

    # Connect to SN003 first, set group=1, then disconnect
    print("\n[Setup] Configuring SN003 (group=1)...")
    sn003 = LampBLE()
    try:
        await sn003.connect(SN003_MAC, timeout=15)
        await sn003.set_group(1)
        cur = await sn003.get_group()
        print(f"[Setup] SN003 group confirmed: {cur}")
        await asyncio.sleep(0.5)
        await sn003.disconnect()
    except Exception as e:
        print(f"[Setup] WARNING: Could not configure SN003 via BLE: {e}")
        print("[Setup] Assuming SN003 is already in group 1 (check serial output)")

    # BLE connection changes SN003's WiFi channel, breaking ESP-NOW.
    # Reset SN003 via DTR toggle so WiFi re-initializes on channel 1.
    print("[Setup] Resetting SN003 (DTR toggle) to restore WiFi channel...")
    monitor.stop()
    monitor.start(SERIAL_PORT)
    boot_done = monitor.wait_for("ESP-NOW sync init", timeout=8.0)
    if boot_done:
        print("[Setup] SN003 rebooted, ESP-NOW ready")
    else:
        print("[Setup] WARNING: Did not see boot marker after reset")
    await asyncio.sleep(1.0)
    monitor.clear()

    # Connect to SN001 — this is our control lamp
    print("\n[Setup] Connecting to SN001 (control lamp)...")
    await lamp.connect(SN001_MAC, timeout=15)
    await lamp.set_group(1)
    cur = await lamp.get_group()
    print(f"[Setup] SN001 group confirmed: {cur}")

    await reset_baseline()
    print("[Setup] Baseline set: manual mode, [128,128,128,200]")
    print("[Setup] Done\n")


async def teardown():
    """Disconnect and clean up."""
    print("\n--- Teardown ---")
    # Reset to manual before disconnecting
    try:
        await lamp.set_flags(0x00)
        await asyncio.sleep(0.3)
        await lamp.disconnect()
    except Exception:
        pass
    monitor.stop()


# ── Test Cases ──

async def test_color_sync():
    """Verify color change on SN001 propagates to SN003."""
    name = "test_color_sync"
    monitor.clear()

    await lamp.set_color(255, 100, 50, 180)
    rx = monitor.wait_for_sync_rx(timeout=SYNC_TIMEOUT)

    if rx is None:
        result.fail(name, "No Sync RX received on SN003")
        monitor.dump_recent()
        return

    if rx.warm == 255 and rx.neutral == 100 and rx.cool == 50 and rx.master == 180:
        result.ok(name, f"[{rx.warm},{rx.neutral},{rx.cool},{rx.master}]")
    else:
        result.fail(name,
            f"Expected [255,100,50,180], got [{rx.warm},{rx.neutral},{rx.cool},{rx.master}]")


async def test_on_off_sync():
    """Verify turning SN001 off/on propagates lamp_on state to SN003."""
    name = "test_on_off_sync"

    # Ensure lamp is on first with known color
    await lamp.set_color(200, 200, 200, 200)
    await asyncio.sleep(3.0)  # wait for all 12 retries (~2s) + margin
    monitor.clear()

    # Turn off
    await lamp.turn_off()

    # Wait for a sync RX with lamp_on=0 (skip stale retries from previous state)
    deadline = time.monotonic() + SYNC_TIMEOUT
    rx_off = None
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        rx = monitor.wait_for_sync_rx(timeout=max(remaining, 0.1))
        if rx is None:
            break
        if rx.lamp_on == 0:
            rx_off = rx
            break

    if rx_off is None:
        result.fail(name + " (off)", "No Sync RX with lamp_on=0 received")
        monitor.dump_recent()
        return

    # master should NOT be 0 (firmware uses s_configured_master)
    if rx_off.master == 0:
        result.fail(name + " (off)", "master=0 in broadcast (should be configured master)")
        return

    result.ok(name + " (off)", f"lamp_on=0, master={rx_off.master}")

    # Wait for all 12 "off" retries to finish (~2s) + margin before clearing
    await asyncio.sleep(3.0)
    monitor.clear()

    # Turn back on
    await lamp.turn_on(200)

    # Wait for a sync RX with lamp_on=1
    deadline = time.monotonic() + SYNC_TIMEOUT
    rx_on = None
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        rx = monitor.wait_for_sync_rx(timeout=max(remaining, 0.1))
        if rx is None:
            break
        if rx.lamp_on == 1:
            rx_on = rx
            break

    if rx_on is None:
        result.fail(name + " (on)", "No Sync RX with lamp_on=1 received")
        monitor.dump_recent()
        return

    if rx_on.master > 0:
        result.ok(name + " (on)", f"lamp_on=1, master={rx_on.master}")
    else:
        result.fail(name + " (on)", f"lamp_on=1 but master=0")


async def test_flags_sync():
    """Verify mode flag changes propagate."""
    name = "test_flags_sync"

    # Start from clean manual state
    await reset_baseline()

    # Enable auto mode
    monitor.clear()
    await lamp.set_flags(MODE_FLAG_AUTO)
    rx = monitor.wait_for_sync_rx(timeout=SYNC_TIMEOUT)
    if rx is None:
        result.fail(name + " (auto)", "No Sync RX for AUTO flag change")
        monitor.dump_recent()
    elif rx.flags & MODE_FLAG_AUTO:
        result.ok(name + " (auto)", f"flags=0x{rx.flags:02x}")
    else:
        result.fail(name + " (auto)", f"Expected AUTO flag set, got flags=0x{rx.flags:02x}")

    # Reset to manual, then test flame
    await reset_baseline()

    monitor.clear()
    await lamp.set_flags(MODE_FLAG_FLAME)
    rx = monitor.wait_for_sync_rx(timeout=SYNC_TIMEOUT)
    if rx is None:
        result.fail(name + " (flame)", "No Sync RX for FLAME flag change")
        monitor.dump_recent()
    elif rx.flags & MODE_FLAG_FLAME:
        result.ok(name + " (flame)", f"flags=0x{rx.flags:02x}")
    else:
        result.fail(name + " (flame)", f"Expected FLAME flag set, got flags=0x{rx.flags:02x}")

    # Clean up
    await reset_baseline()


async def test_full_scene_sync():
    """Verify setting all scene parameters individually propagates."""
    name = "test_full_scene_sync"
    await reset_baseline()
    monitor.clear()

    # Set distinctive color + brightness
    await lamp.set_color(77, 88, 99, 150)
    rx = monitor.wait_for_sync_rx(timeout=SYNC_TIMEOUT)

    if rx is None:
        result.fail(name, "No Sync RX for color change")
        monitor.dump_recent()
        return

    errors = []
    if rx.warm != 77: errors.append(f"warm={rx.warm} (exp 77)")
    if rx.neutral != 88: errors.append(f"neutral={rx.neutral} (exp 88)")
    if rx.cool != 99: errors.append(f"cool={rx.cool} (exp 99)")
    if rx.master != 150: errors.append(f"master={rx.master} (exp 150)")

    if errors:
        result.fail(name, "; ".join(errors))
    else:
        result.ok(name, f"[{rx.warm},{rx.neutral},{rx.cool},{rx.master}]")

    await reset_baseline()


async def test_rapid_changes():
    """Send multiple rapid color changes, verify SN003 converges to final state."""
    name = "test_rapid_changes"
    await reset_baseline()
    monitor.clear()

    # Fire off 5 rapid changes
    for i in range(5):
        w = 50 * (i + 1)
        await lamp.set_color(w, w, w, 200)
        await asyncio.sleep(0.05)

    # The TX task processes one message over ~1.5s, then picks up the next.
    # With xQueueOverwrite + retry-restart, convergence should be ~1.5s.
    # Wait long enough for all retries to complete and arrive.
    await asyncio.sleep(6.0)

    # Drain all Sync RX events from the buffer (they're already captured)
    all_rx = []
    while True:
        rx = monitor.wait_for_sync_rx(timeout=1.0)
        if rx is None:
            break
        all_rx.append(rx)

    if not all_rx:
        result.fail(name, "No Sync RX received at all")
        monitor.dump_recent()
        return

    last_rx = all_rx[-1]
    unique_states = list({(r.warm, r.neutral, r.cool) for r in all_rx})

    # Final color should be 250,250,250,200
    if last_rx.warm == 250 and last_rx.neutral == 250 and last_rx.cool == 250:
        result.ok(name,
            f"Converged to [{last_rx.warm},{last_rx.neutral},{last_rx.cool},{last_rx.master}] "
            f"({len(all_rx)} events, {len(unique_states)} unique states)")
    else:
        result.fail(name,
            f"Expected final [250,250,250], got [{last_rx.warm},{last_rx.neutral},{last_rx.cool}] "
            f"({len(all_rx)} events, states: {unique_states})")


async def test_group_isolation():
    """Verify SN003 (group 1) does NOT receive sync from SN001 (group 2)."""
    name = "test_group_isolation"

    await reset_baseline()

    # Move SN001 to group 2
    await lamp.set_group(2)
    await asyncio.sleep(1.0)
    monitor.clear()

    # Send a distinctive color
    await lamp.set_color(11, 22, 33, 44)

    # SN003 should NOT see this
    rx = monitor.wait_for_sync_rx(timeout=3.0)

    if rx is None:
        result.ok(name, "Correctly isolated — no sync received")
    else:
        result.fail(name,
            f"SN003 received sync from wrong group! [{rx.warm},{rx.neutral},{rx.cool},{rx.master}]")

    # Restore SN001 to group 1
    await lamp.set_group(1)
    await asyncio.sleep(1.0)
    monitor.clear()


async def test_sync_latency():
    """Measure single-change sync latency (time from BLE write to SN003 Sync RX)."""
    name = "test_sync_latency"
    await reset_baseline()

    latencies = []
    for trial in range(5):
        # Longer settle to ensure no stale retries contaminate the measurement
        await asyncio.sleep(2.0)
        monitor.clear()
        w = 60 + trial * 30  # Distinctive values: 60, 90, 120, 150, 180

        t_start = time.monotonic()
        await lamp.set_color(w, w, w, 200)

        # Wait for the specific value (skip stale retries from previous ops)
        deadline = t_start + SYNC_TIMEOUT
        found = False
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            rx = monitor.wait_for_sync_rx(timeout=max(remaining, 0.1))
            if rx is None:
                break
            if rx.warm == w:
                latency_ms = (time.monotonic() - t_start) * 1000
                latencies.append(latency_ms)
                found = True
                break
            # Wrong value (stale retry) — keep looking

    if not latencies:
        result.fail(name, "No successful latency measurements")
        return

    avg = sum(latencies) / len(latencies)
    worst = max(latencies)
    best = min(latencies)
    result.ok(name,
        f"{len(latencies)}/5 measured — avg={avg:.0f}ms, best={best:.0f}ms, worst={worst:.0f}ms")


async def test_rapid_convergence_time():
    """Measure how long rapid changes take to converge (tests retry-restart)."""
    name = "test_rapid_convergence_time"
    await reset_baseline()
    monitor.clear()

    # Send 5 rapid changes and record the time
    t_start = time.monotonic()
    for i in range(5):
        w = 50 * (i + 1)
        await lamp.set_color(w, w, w, 200)
        await asyncio.sleep(0.05)

    # Wait for the final value (250) to arrive on SN003.
    # Don't break on temporary gaps — keep polling until deadline.
    deadline = t_start + 12.0
    final_rx = None
    converged = False
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        rx = monitor.wait_for_sync_rx(timeout=min(remaining, 1.0))
        if rx is not None:
            final_rx = rx
            if rx.warm == 250 and rx.neutral == 250 and rx.cool == 250:
                converged = True
                break

    t_converged = time.monotonic()
    elapsed_ms = (t_converged - t_start) * 1000

    if final_rx is None:
        result.fail(name, "No Sync RX received at all")
        monitor.dump_recent()
        return

    if converged:
        result.ok(name, f"Converged in {elapsed_ms:.0f}ms")
    else:
        result.fail(name,
            f"Did not converge: last=[{final_rx.warm},{final_rx.neutral},{final_rx.cool}] "
            f"after {elapsed_ms:.0f}ms")


async def test_soak_reliability():
    """Run 50 sync operations, measure delivery rate and latency distribution."""
    name = "test_soak_reliability"
    await reset_baseline()

    N = 50
    delivered = 0
    latencies = []
    failures = []

    for i in range(N):
        await asyncio.sleep(2.0)  # settle between attempts (>1.5s retry window)
        monitor.clear()
        w = (i * 5 + 10) % 256   # unique warm value per iteration
        n = (i * 3 + 20) % 256   # unique neutral value

        t_start = time.monotonic()
        await lamp.set_color(w, n, 128, 200)

        # Wait for matching RX on SN003
        deadline = t_start + SYNC_TIMEOUT
        found = False
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            rx = monitor.wait_for_sync_rx(timeout=max(remaining, 0.1))
            if rx is None:
                break
            if rx.warm == w and rx.neutral == n:
                latency_ms = (time.monotonic() - t_start) * 1000
                latencies.append(latency_ms)
                delivered += 1
                found = True
                break

        if not found:
            failures.append(i)

        # Progress every 10
        if (i + 1) % 10 == 0:
            print(f"  [{name}] {i+1}/{N} done, {delivered} delivered so far")

    rate = delivered / N * 100
    if latencies:
        latencies.sort()
        avg = sum(latencies) / len(latencies)
        median = latencies[len(latencies) // 2]
        p95 = latencies[int(len(latencies) * 0.95)]
        detail = (f"{delivered}/{N} ({rate:.0f}%), "
                  f"avg={avg:.0f}ms, median={median:.0f}ms, "
                  f"p95={p95:.0f}ms, best={min(latencies):.0f}ms, worst={max(latencies):.0f}ms")
    else:
        detail = f"0/{N} delivered"

    # 80% threshold — ESP-NOW over shared BLE radio has high variance
    if rate >= 80:
        result.ok(name, detail)
    else:
        result.fail(name, detail)

    if failures:
        print(f"  Failed iterations: {failures[:20]}{'...' if len(failures) > 20 else ''}")


async def test_rapid_burst():
    """Fire 20 rapid changes with no delay, verify convergence to final value."""
    name = "test_rapid_burst"
    await reset_baseline()
    monitor.clear()

    BURST_SIZE = 20
    t_start = time.monotonic()
    for i in range(BURST_SIZE):
        w = 10 + i * 12  # 10, 22, 34, ... 238
        await lamp.set_color(w, w, w, 200)
        # No sleep — as fast as BLE writes complete

    final_w = 10 + (BURST_SIZE - 1) * 12  # 238

    # Wait for convergence
    deadline = t_start + 15.0
    all_rx = []
    converged = False
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        rx = monitor.wait_for_sync_rx(timeout=min(remaining, 1.0))
        if rx is not None:
            all_rx.append(rx)
            if rx.warm == final_w:
                converged = True
                break

    elapsed_ms = (time.monotonic() - t_start) * 1000
    unique = len({r.warm for r in all_rx})

    if converged:
        result.ok(name, f"Converged to {final_w} in {elapsed_ms:.0f}ms "
                        f"({len(all_rx)} events, {unique} unique values)")
    else:
        last = all_rx[-1] if all_rx else None
        last_str = f"[{last.warm},{last.neutral},{last.cool}]" if last else "none"
        result.fail(name, f"Not converged after {elapsed_ms:.0f}ms. "
                         f"Last: {last_str}, {len(all_rx)} events, {unique} unique")


async def test_per_retry_delivery_rate():
    """Measure how many of 10 TX retries arrive at SN003 per message."""
    name = "test_per_retry_delivery_rate"
    await reset_baseline()

    trials = 10
    retry_counts = []

    for trial in range(trials):
        await asyncio.sleep(2.5)  # let previous retries fully drain
        monitor.clear()
        w = 30 + trial * 20  # unique per trial: 30, 50, 70, ...

        await lamp.set_color(w, w, w, 200)

        # Wait for all 10 retries to complete (~1.5s) + margin
        await asyncio.sleep(2.0)

        # Collect all RX events that arrived
        all_rx = monitor.collect_espnow_rx(timeout=0.5)
        # Count events matching our target value
        matching = [r for r in all_rx if r["warm"] == w]
        retry_counts.append(len(matching))

    if not retry_counts:
        result.fail(name, "No RX events captured")
        return

    avg_received = sum(retry_counts) / len(retry_counts)
    per_retry_rate = avg_received / 10.0 * 100

    detail = (f"{len(retry_counts)} trials, avg {avg_received:.1f}/10 retries received "
              f"({per_retry_rate:.0f}% per-retry), "
              f"min={min(retry_counts)}, max={max(retry_counts)}")

    # Informational — always pass since per-retry rate is highly variable
    # (6-48% observed). Aggregate delivery is tested by test_soak_reliability.
    result.ok(name, detail)


async def test_on_off_rapid_toggle():
    """Rapidly toggle on/off 10 times, verify final state propagates."""
    name = "test_on_off_rapid_toggle"
    await reset_baseline()

    # Ensure we start "on" with known color
    await lamp.set_color(200, 200, 200, 200)
    await asyncio.sleep(3.0)
    monitor.clear()

    TOGGLES = 10
    t_start = time.monotonic()
    for i in range(TOGGLES):
        if i % 2 == 0:
            await lamp.turn_off()
        else:
            await lamp.turn_on(200)
        await asyncio.sleep(0.1)

    # Final state: TOGGLES=10, last action (i=9, odd) is turn_on → lamp_on=1
    final_expected_on = 1

    # Wait for convergence — use long timeout due to high latency variance
    deadline = t_start + 20.0
    final_rx = None
    while time.monotonic() < deadline:
        rx = monitor.wait_for_sync_rx(timeout=1.0)
        if rx is not None:
            final_rx = rx
            if rx.lamp_on == final_expected_on:
                break

    elapsed = (time.monotonic() - t_start) * 1000

    if final_rx and final_rx.lamp_on == final_expected_on:
        result.ok(name, f"Converged to lamp_on={final_expected_on} in {elapsed:.0f}ms")
    else:
        got = final_rx.lamp_on if final_rx else "none"
        result.fail(name, f"Expected lamp_on={final_expected_on}, got {got} after {elapsed:.0f}ms")


# ── Test Registry ──

async def test_circadian_flag_sync():
    """Verify CIRCADIAN flag (0x04) propagates via ESP-NOW."""
    name = "test_circadian_flag_sync"

    await reset_baseline()

    monitor.clear()
    await lamp.set_flags(MODE_FLAG_CIRCADIAN)
    rx = monitor.wait_for_sync_rx(timeout=SYNC_TIMEOUT)
    if rx is None:
        result.fail(name, "No Sync RX for CIRCADIAN flag change")
        monitor.dump_recent()
    elif rx.flags & MODE_FLAG_CIRCADIAN:
        result.ok(name, f"flags=0x{rx.flags:02x}")
    else:
        result.fail(name, f"Expected CIRCADIAN flag set, got flags=0x{rx.flags:02x}")

    # Clean up
    await reset_baseline()


ALL_TESTS = [
    ("test_color_sync",              test_color_sync),
    ("test_on_off_sync",             test_on_off_sync),
    ("test_flags_sync",              test_flags_sync),
    ("test_circadian_flag_sync",     test_circadian_flag_sync),
    ("test_full_scene_sync",         test_full_scene_sync),
    ("test_rapid_changes",           test_rapid_changes),
    ("test_group_isolation",         test_group_isolation),
    ("test_sync_latency",            test_sync_latency),
    ("test_rapid_convergence_time",  test_rapid_convergence_time),
    ("test_soak_reliability",        test_soak_reliability),
    ("test_rapid_burst",             test_rapid_burst),
    ("test_per_retry_delivery_rate", test_per_retry_delivery_rate),
    ("test_on_off_rapid_toggle",     test_on_off_rapid_toggle),
]


async def run_tests(names: list[str] = None):
    """Run selected tests (or all if names is None)."""
    tests = ALL_TESTS
    if names:
        tests = [(n, f) for n, f in ALL_TESTS if n in names]
        if not tests:
            print(f"No tests matched: {names}")
            print(f"Available: {[n for n, _ in ALL_TESTS]}")
            return False

    try:
        await setup()
    except Exception as e:
        print(f"\nSetup failed: {e}")
        traceback.print_exc()
        monitor.stop()
        return False

    for name, func in tests:
        print(f"\n--- {name} ---")
        try:
            await func()
        except Exception as e:
            result.fail(name, f"Exception: {e}")
            traceback.print_exc()
        # Small gap between tests
        await asyncio.sleep(0.5)

    await teardown()
    return result.summary()


def main():
    if "--list" in sys.argv:
        print("Available tests:")
        for name, _ in ALL_TESTS:
            print(f"  {name}")
        return

    names = [a for a in sys.argv[1:] if not a.startswith("-")]
    success = asyncio.run(run_tests(names or None))
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
