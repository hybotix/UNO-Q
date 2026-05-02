#!/home/arduino/.hybx/venv/bin/python3
"""
monitor-v1.0.1.py
VL53L5CX Live Raw Data Monitor
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Displays live 8x8 raw sensor data continuously:
  - Distance (mm)
  - Signal (kcps/SPAD)
  - Sigma (mm)
  - Target Status (T/F)

Display is Y-axis flipped (columns mirrored left-to-right) so the view
matches what the sensor sees.

Press Ctrl+C to stop cleanly.
"""

import sys
import os
import time

sys.path.insert(0, os.path.expanduser("~/lib"))

from hybx_app import Bridge, BridgeError

# ── Constants ──────────────────────────────────────────────────────────────────

RESOLUTION = "8x8"
WIDTH      = 8

ERROR_STEPS = {
    "0": "none",
    "1": "vl53l5cx_init",
    "2": "vl53l5cx_set_resolution",
    "3": "vl53l5cx_set_ranging_frequency_hz",
    "4": "vl53l5cx_start_ranging",
    "5": "vl53l5cx_stop_ranging",
    "6": "vl53l5cx_check_data_ready",
    "7": "vl53l5cx_get_ranging_data",
}


# ── Parsing ────────────────────────────────────────────────────────────────────

def parse_int_matrix(data: str) -> list:
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def parse_bool_matrix(data: str) -> list:
    return [[v == "T" for v in row.split(",")] for row in data.split(";")]


def flip_row(values: list) -> list:
    return list(reversed(values))


def flip_grid(grid: list) -> list:
    return [flip_row(row) for row in grid]


def format_error(status: str) -> str:
    parts = status.split(":")
    if len(parts) >= 3:
        step_name = ERROR_STEPS.get(parts[1], f"step_{parts[1]}")
        return f"{parts[0]}: {step_name} (ULD code {parts[2]})"
    return status


# ── Sensor init ────────────────────────────────────────────────────────────────

def init_sensor():
    print("Initializing VL53L5CX...")
    print("  (Firmware upload — up to 120 seconds)")
    try:
        result = Bridge.call("begin_sensor", timeout=120)
    except BridgeError as e:
        print(f"  ERROR: {e}")
        sys.exit(1)

    if result.startswith("init_failed"):
        print(f"  ERROR: {format_error(result)}")
        sys.exit(1)

    if result not in ("ready", "already_started"):
        print(f"  ERROR: unexpected status: {result}")
        sys.exit(1)

    res = Bridge.call("set_resolution", RESOLUTION)
    print(f"  Sensor ready. Resolution: {res}")
    print()


# ── Stop sensor ────────────────────────────────────────────────────────────────

def stop_sensor():
    try:
        Bridge.call("end_sensor")
    except Exception:
        pass


# ── Display ────────────────────────────────────────────────────────────────────

def display_frame(dist, signal, sigma, status, frame_count):
    # Clear screen
    print("\033[2J\033[H", end="")

    print(f"  VL53L5CX Live Monitor  v1.0.0   Frame: {frame_count}")
    print(f"  Ctrl+C to stop")
    print()

    # Distance + Signal side by side
    print(f"  {'── Distance (mm) ──':^48}  {'── Signal (kcps/SPAD) ──':^48}")
    print()
    for r in range(WIDTH):
        dist_row   = "  ".join(f"{dist[r][c]:5d}" for c in range(WIDTH))
        signal_row = "  ".join(f"{signal[r][c]:5d}" for c in range(WIDTH))
        print(f"  {dist_row}    {signal_row}")

    print()

    # Sigma + Status side by side
    print(f"  {'── Sigma (mm) ──':^48}  {'── Target Status ──':^48}")
    print()
    for r in range(WIDTH):
        sigma_row  = "  ".join(f"{sigma[r][c]:5d}" for c in range(WIDTH))
        status_row = "  ".join(f"{'  T  ' if status[r][c] else '  F  '}" for c in range(WIDTH))
        print(f"  {sigma_row}    {status_row}")

    print()


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    print("═" * 60)
    print("  VL53L5CX Live Raw Data Monitor  v1.0.0")
    print("  Hybrid RobotiX — HybX Development System")
    print("═" * 60)
    print()

    init_sensor()

    frame_count = 0

    try:
        while True:
            try:
                dist_raw   = Bridge.call("get_distance_data")
                signal_raw = Bridge.call("get_signal_data")
                sigma_raw  = Bridge.call("get_sigma_data")
                stat_raw   = Bridge.call("get_target_status")
            except BridgeError as e:
                print(f"  ERROR: {e}")
                time.sleep(0.5)
                continue

            if "0" in (dist_raw, signal_raw, sigma_raw, stat_raw):
                time.sleep(0.05)
                continue

            if dist_raw.startswith("error:"):
                print(f"  ERROR: {format_error(dist_raw)}")
                time.sleep(0.5)
                continue

            try:
                dist   = flip_grid(parse_int_matrix(dist_raw))
                signal = flip_grid(parse_int_matrix(signal_raw))
                sigma  = flip_grid(parse_int_matrix(sigma_raw))
                status = flip_grid(parse_bool_matrix(stat_raw))
                frame_count += 1
                display_frame(dist, signal, sigma, status, frame_count)
            except Exception as e:
                print(f"  ERROR parsing: {e}")
                time.sleep(0.5)

    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        stop_sensor()
        print("Stopped.")


if __name__ == "__main__":
    main()
