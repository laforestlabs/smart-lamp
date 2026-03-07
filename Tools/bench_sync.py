#!/usr/bin/env python3
"""
ESP-NOW Sync Benchmark

Runs latency, delivery, and per-retry benchmarks with JSON result persistence
for A/B comparison across firmware variants.

Usage:
    python3 bench_sync.py --label baseline                      # Run benchmark
    python3 bench_sync.py --label baseline --latency-runs 50    # More latency trials
    python3 bench_sync.py --compare results/a.json results/b.json  # Compare two runs

Requires: SN001 connected via BLE (control), SN003 on USB serial (monitor).
"""

import argparse
import asyncio
import json
import os
import statistics
import sys
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path

from lamp_test import (
    LampBLE, SerialMonitor,
    SN001_MAC, SN003_MAC, SERIAL_PORT,
)

SYNC_TIMEOUT = 6.0
RESULTS_DIR = Path(__file__).parent / "results"


# ── Statistics helper ──

def compute_stats(samples: list[float]) -> dict:
    """Compute summary statistics for a list of numeric samples."""
    if not samples:
        return {"samples": [], "count": 0}
    s = sorted(samples)
    n = len(s)
    return {
        "samples": [round(v, 1) for v in s],
        "count": n,
        "mean": round(statistics.mean(s), 1),
        "median": round(statistics.median(s), 1),
        "p25": round(s[max(0, int(n * 0.25))], 1),
        "p75": round(s[min(n - 1, int(n * 0.75))], 1),
        "p95": round(s[min(n - 1, int(n * 0.95))], 1),
        "best": round(s[0], 1),
        "worst": round(s[-1], 1),
    }


# ── Setup / teardown (mirrors test_sync.py) ──

lamp = LampBLE()
monitor = SerialMonitor(verbose=False)


async def setup():
    """Connect to both lamps and configure groups."""
    print("\n--- Setup ---")

    monitor.start(SERIAL_PORT)
    print("[Setup] Waiting for SN003 boot...")
    boot_done = monitor.wait_for("ESP-NOW sync init", timeout=8.0)
    if boot_done:
        print("[Setup] SN003 boot complete")
    else:
        print("[Setup] WARNING: Did not see boot marker (may already be running)")
    await asyncio.sleep(1.0)
    monitor.clear()

    # Configure SN003 group=1, then reset to restore WiFi channel
    print("[Setup] Configuring SN003 (group=1)...")
    sn003 = LampBLE()
    try:
        await sn003.connect(SN003_MAC, timeout=15)
        await sn003.set_group(1)
        print(f"[Setup] SN003 group confirmed: {await sn003.get_group()}")
        await asyncio.sleep(0.5)
        await sn003.disconnect()
    except Exception as e:
        print(f"[Setup] WARNING: Could not configure SN003: {e}")

    # Reset SN003 to restore WiFi channel after BLE connection
    print("[Setup] Resetting SN003 (DTR/RTS pulse)...")
    monitor.reset_device()
    boot_done = monitor.wait_for("ESP-NOW sync init", timeout=8.0)
    if boot_done:
        print("[Setup] SN003 rebooted, ESP-NOW ready")
    else:
        print("[Setup] WARNING: Did not see boot marker after reset")
    await asyncio.sleep(1.0)
    monitor.clear()

    # Connect to SN001 as control lamp
    print("[Setup] Connecting to SN001 (control lamp)...")
    await lamp.connect(SN001_MAC, timeout=15)
    await lamp.set_group(1)
    print(f"[Setup] SN001 group confirmed: {await lamp.get_group()}")

    await reset_baseline()
    print("[Setup] Done\n")


async def reset_baseline():
    """Reset to manual mode with known color, wait for retries to drain."""
    await lamp.set_flags(0x00)
    await asyncio.sleep(0.5)
    await lamp.set_color(128, 128, 128, 200)
    await asyncio.sleep(3.0)
    monitor.clear()


async def teardown():
    """Disconnect and clean up."""
    print("\n--- Teardown ---")
    try:
        await lamp.set_flags(0x00)
        await asyncio.sleep(0.3)
        await lamp.disconnect()
    except Exception:
        pass
    monitor.stop()


# ── Benchmarks ──

async def bench_latency(n_trials: int = 30) -> dict:
    """Measure single-change sync latency using ESP-NOW RX layer.

    Uses wait_for_espnow_rx (fires in WiFi task context) instead of
    wait_for_sync_rx (fires after sensor queue) for tighter measurement.
    """
    print(f"\n=== Latency Benchmark ({n_trials} trials) ===")
    latencies = []
    misses = 0

    for i in range(n_trials):
        await asyncio.sleep(2.0)  # settle between trials
        monitor.clear()
        w = (i * 7 + 31) % 246 + 10  # unique warm value 10-255

        t_start = time.monotonic()
        await lamp.set_color(w, w, w, 200)

        # Wait for matching RX (ESP-NOW layer, not sensor queue)
        deadline = t_start + SYNC_TIMEOUT
        found = False
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            rx = monitor.wait_for_espnow_rx(timeout=max(remaining, 0.1))
            if rx is None:
                break
            if rx["warm"] == w:
                latency_ms = (time.monotonic() - t_start) * 1000
                latencies.append(latency_ms)
                found = True
                break

        if not found:
            misses += 1

        # Progress
        if (i + 1) % 10 == 0:
            print(f"  [{i+1}/{n_trials}] {len(latencies)} delivered, {misses} missed")

    stats = compute_stats(latencies)
    stats["misses"] = misses
    stats["total_trials"] = n_trials

    if latencies:
        print(f"  Result: {len(latencies)}/{n_trials} delivered, "
              f"mean={stats['mean']}ms, median={stats['median']}ms, "
              f"p95={stats['p95']}ms, best={stats['best']}ms, worst={stats['worst']}ms")
    else:
        print(f"  Result: 0/{n_trials} delivered")

    return stats


async def bench_delivery(n_trials: int = 50) -> dict:
    """Measure aggregate delivery rate and latency distribution."""
    print(f"\n=== Delivery Benchmark ({n_trials} trials) ===")
    latencies = []
    failures = []

    for i in range(n_trials):
        await asyncio.sleep(2.0)
        monitor.clear()
        w = (i * 5 + 10) % 256
        n = (i * 3 + 20) % 256

        t_start = time.monotonic()
        await lamp.set_color(w, n, 128, 200)

        deadline = t_start + SYNC_TIMEOUT
        found = False
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            rx = monitor.wait_for_espnow_rx(timeout=max(remaining, 0.1))
            if rx is None:
                break
            if rx["warm"] == w and rx["neutral"] == n:
                latency_ms = (time.monotonic() - t_start) * 1000
                latencies.append(latency_ms)
                found = True
                break

        if not found:
            failures.append(i)

        if (i + 1) % 10 == 0:
            print(f"  [{i+1}/{n_trials}] {len(latencies)} delivered, "
                  f"{len(failures)} missed")

    delivered = len(latencies)
    rate = delivered / n_trials * 100

    result = {
        "delivered": delivered,
        "total": n_trials,
        "rate_pct": round(rate, 1),
        "failed_indices": failures[:20],
    }

    if latencies:
        result["latency_ms"] = compute_stats(latencies)
        print(f"  Result: {delivered}/{n_trials} ({rate:.1f}%), "
              f"mean={result['latency_ms']['mean']}ms, "
              f"median={result['latency_ms']['median']}ms, "
              f"p95={result['latency_ms']['p95']}ms")
    else:
        result["latency_ms"] = compute_stats([])
        print(f"  Result: 0/{n_trials} delivered")

    return result


async def bench_per_retry(n_trials: int = 10) -> dict:
    """Count how many of 12 TX retries physically arrive at SN003."""
    print(f"\n=== Per-Retry Rate Benchmark ({n_trials} trials) ===")
    retry_counts = []

    for trial in range(n_trials):
        await asyncio.sleep(2.5)
        monitor.clear()
        w = 30 + trial * 20

        await lamp.set_color(w, w, w, 200)

        # Wait for all retries to complete (~2s) + margin
        await asyncio.sleep(2.5)

        all_rx = monitor.collect_espnow_rx(timeout=0.5)
        matching = [r for r in all_rx if r["warm"] == w]
        retry_counts.append(len(matching))

    avg = statistics.mean(retry_counts) if retry_counts else 0
    per_retry_pct = avg / 12.0 * 100

    result = {
        "n_trials": n_trials,
        "retry_counts": retry_counts,
        "avg_received": round(avg, 1),
        "per_retry_rate_pct": round(per_retry_pct, 1),
        "min_received": min(retry_counts) if retry_counts else 0,
        "max_received": max(retry_counts) if retry_counts else 0,
    }

    print(f"  Result: avg {avg:.1f}/12 retries received ({per_retry_pct:.0f}%), "
          f"min={result['min_received']}, max={result['max_received']}")

    return result


# ── Save / Compare ──

def save_results(label: str, firmware_hint: str,
                 latency: dict, delivery: dict, per_retry: dict) -> Path:
    """Save benchmark results to JSON file."""
    RESULTS_DIR.mkdir(exist_ok=True)
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S")
    filename = f"{label}_{ts}.json"
    path = RESULTS_DIR / filename

    data = {
        "label": label,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "firmware_hint": firmware_hint,
        "latency": latency,
        "delivery": delivery,
        "per_retry": per_retry,
    }

    path.write_text(json.dumps(data, indent=2) + "\n")
    print(f"\nResults saved to {path}")
    return path


def compare_results(path_a: str, path_b: str):
    """Load two result files and print a comparison table."""
    a = json.loads(Path(path_a).read_text())
    b = json.loads(Path(path_b).read_text())

    label_a = a["label"]
    label_b = b["label"]

    print(f"\n{'Metric':<28} {label_a:>14} {label_b:>14} {'delta':>12}")
    print("-" * 70)

    def row(name, va, vb, fmt=".1f", unit="", invert=False):
        """Print a comparison row. invert=True means lower is better."""
        if va is None or vb is None:
            print(f"{name:<28} {'N/A':>14} {'N/A':>14}")
            return
        delta = vb - va
        pct = (delta / va * 100) if va != 0 else 0
        sign = "+" if delta > 0 else ""
        # For metrics where lower is better (latency), negative delta is good
        # For metrics where higher is better (delivery rate), positive is good
        indicator = ""
        if abs(pct) >= 1:
            if invert:
                indicator = " *" if delta < 0 else ""
            else:
                indicator = " *" if delta > 0 else ""
        print(f"{name:<28} {va:>13{fmt}}{unit} {vb:>13{fmt}}{unit} "
              f"{sign}{pct:>.1f}%{indicator}")

    # Delivery
    row("Delivery rate (%)",
        a.get("delivery", {}).get("rate_pct"),
        b.get("delivery", {}).get("rate_pct"),
        unit="%")

    # Latency (from latency benchmark)
    la = a.get("latency", {})
    lb = b.get("latency", {})
    for metric in ["mean", "median", "p25", "p75", "p95", "best", "worst"]:
        row(f"Latency {metric} (ms)",
            la.get(metric), lb.get(metric), invert=True)

    # Per-retry
    row("Per-retry rate (%)",
        a.get("per_retry", {}).get("per_retry_rate_pct"),
        b.get("per_retry", {}).get("per_retry_rate_pct"))
    row("Avg retries received",
        a.get("per_retry", {}).get("avg_received"),
        b.get("per_retry", {}).get("avg_received"))

    # Delivery latency
    dla = a.get("delivery", {}).get("latency_ms", {})
    dlb = b.get("delivery", {}).get("latency_ms", {})
    if dla.get("mean") and dlb.get("mean"):
        print()
        print("Delivery latency (when delivered):")
        for metric in ["mean", "median", "p95"]:
            row(f"  {metric} (ms)",
                dla.get(metric), dlb.get(metric), invert=True)

    print(f"\n  * = improved direction")
    print(f"\n  A: {a.get('firmware_hint', 'N/A')}")
    print(f"  B: {b.get('firmware_hint', 'N/A')}")


# ── Main ──

async def run_benchmark(label: str, firmware_hint: str,
                        n_latency: int, n_delivery: int, n_per_retry: int):
    """Run full benchmark suite."""
    try:
        await setup()
    except Exception as e:
        print(f"\nSetup failed: {e}")
        traceback.print_exc()
        monitor.stop()
        return False

    try:
        latency = await bench_latency(n_latency)
        delivery = await bench_delivery(n_delivery)
        per_retry = await bench_per_retry(n_per_retry)

        save_results(label, firmware_hint, latency, delivery, per_retry)
    except Exception as e:
        print(f"\nBenchmark failed: {e}")
        traceback.print_exc()
    finally:
        await teardown()


def main():
    parser = argparse.ArgumentParser(description="ESP-NOW Sync Benchmark")
    parser.add_argument("--label", help="Label for this benchmark run")
    parser.add_argument("--firmware-hint", default="",
                        help="Description of firmware variant being tested")
    parser.add_argument("--latency-runs", type=int, default=30,
                        help="Number of latency trials (default: 30)")
    parser.add_argument("--delivery-runs", type=int, default=50,
                        help="Number of delivery trials (default: 50)")
    parser.add_argument("--per-retry-runs", type=int, default=10,
                        help="Number of per-retry trials (default: 10)")
    parser.add_argument("--compare", nargs=2, metavar=("FILE_A", "FILE_B"),
                        help="Compare two result JSON files")
    parser.add_argument("--verbose", action="store_true",
                        help="Show all serial output")

    args = parser.parse_args()

    if args.compare:
        compare_results(args.compare[0], args.compare[1])
        return

    if not args.label:
        parser.error("--label is required for benchmark runs")

    if args.verbose:
        monitor.verbose = True

    asyncio.run(run_benchmark(
        args.label, args.firmware_hint,
        args.latency_runs, args.delivery_runs, args.per_retry_runs,
    ))


if __name__ == "__main__":
    main()
