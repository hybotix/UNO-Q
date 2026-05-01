"""
ei-c — Edge Impulse Data Collector
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Collects labeled 8x8 distance frames from the VL53L5CX and writes
them to a CSV file in Edge Impulse format.

Usage:
    start ei-c <LABEL>
    mon

    Label must be one of: UP, DOWN, LEFT, RIGHT, CENTER

Output:
    ~/data/ei-c/ei-c_<LABEL>_<TIMESTAMP>.csv

CSV format (Edge Impulse compatible):
    d00,d01,...,d77,label
    <distance values>,<LABEL>
    ...

Orientation (raw data — not transformed):
    Row 0 = top of FOV,    Row 7 = bottom of FOV
    Col 0 = robot left,    Col 7 = robot right
"""

from arduino.app_utils import *
import time
import os
import sys
import csv
from datetime import datetime

# ── Configuration ──────────────────────────────────────────────────────────────
RESOLUTION   = "8x8"
VALID_LABELS = {"UP", "DOWN", "LEFT", "RIGHT", "CENTER"}
OUTPUT_DIR   = os.path.expanduser("~/data/ei-c")

# Sensor error step lookup
ERROR_STEPS = {
    "0": "none", "1": "vl53l5cx_init", "2": "vl53l5cx_set_resolution",
    "3": "vl53l5cx_set_ranging_frequency_hz", "4": "vl53l5cx_start_ranging",
    "5": "vl53l5cx_stop_ranging", "6": "vl53l5cx_check_data_ready",
    "7": "vl53l5cx_get_ranging_data",
}

# ── State ──────────────────────────────────────────────────────────────────────
initialized  = False
label        = None
csv_path     = None
csv_file     = None
csv_writer   = None
frame_count  = 0


def parse_int_matrix(data: str) -> list:
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def format_error(status: str) -> str:
    parts = status.split(":")
    if len(parts) >= 3:
        step_name = ERROR_STEPS.get(parts[1], f"step_{parts[1]}")
        return f"{parts[0]}: {step_name} (ULD code {parts[2]})"
    return status


def open_csv(lbl: str) -> tuple:
    """Create output directory and open CSV file for writing."""
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    path      = os.path.join(OUTPUT_DIR, f"ei-c_{lbl}_{timestamp}.csv")
    f         = open(path, "w", newline="")

    # Build header: d00, d01, ... d77, label
    header = [f"d{r}{c}" for r in range(8) for c in range(8)] + ["label"]
    writer = csv.writer(f)
    writer.writerow(header)
    f.flush()
    return path, f, writer


def loop():
    global initialized, label, csv_path, csv_file, csv_writer, frame_count

    # ── One-time setup ─────────────────────────────────────────────────────────
    if not initialized:

        # ── Validate label from command line ───────────────────────────────────
        if label is None:
            args = sys.argv[1:]
            if not args:
                print("ERROR: No label provided.")
                print(f"  Usage: start ei-c <LABEL>")
                print(f"  Valid labels: {', '.join(sorted(VALID_LABELS))}")
                time.sleep(5.0)
                return

            lbl = args[0].upper()
            if lbl not in VALID_LABELS:
                print(f"ERROR: Invalid label '{lbl}'.")
                print(f"  Valid labels: {', '.join(sorted(VALID_LABELS))}")
                time.sleep(5.0)
                return

            label = lbl

        # ── Open CSV ───────────────────────────────────────────────────────────
        if csv_file is None:
            try:
                csv_path, csv_file, csv_writer = open_csv(label)
                print(f"Label:  {label}")
                print(f"Output: {csv_path}")
                print()
            except Exception as e:
                print(f"ERROR: Could not open output file: {e}")
                time.sleep(5.0)
                return

        # ── Init sensor ────────────────────────────────────────────────────────
        try:
            print("Initializing VL53L5CX...")
            result = Bridge.call("begin_sensor", timeout=120)
            if result.startswith("init_failed"):
                print("ERROR: " + format_error(result))
                time.sleep(5.0)
                return
            if result == "ready":
                res = Bridge.call("set_resolution", RESOLUTION)
                print(f"Sensor ready. Resolution: {res}")
                print(f"Collecting frames for label [{label}] — Ctrl+C to stop.")
                print()
                initialized = True
            else:
                print("ERROR: unexpected response: " + result)
                time.sleep(2.0)
        except Exception as e:
            print(f"ERROR: sensor init failed: {e}")
            time.sleep(2.0)
        return

    # ── Collect frame ──────────────────────────────────────────────────────────
    time.sleep(0.05)

    try:
        dist_raw = Bridge.call("get_distance_data")
    except Exception as e:
        print(f"ERROR: sensor read failed: {e}")
        return

    if not dist_raw or dist_raw == "0":
        return

    if dist_raw.startswith("error:"):
        print("ERROR: " + format_error(dist_raw))
        return

    try:
        dist = parse_int_matrix(dist_raw)
    except Exception as e:
        print(f"ERROR: could not parse frame: {e}")
        return

    # ── Write frame to CSV ─────────────────────────────────────────────────────
    try:
        row = [dist[r][c] for r in range(8) for c in range(8)] + [label]
        csv_writer.writerow(row)
        csv_file.flush()
        frame_count += 1

        # Progress update every 10 frames
        if frame_count % 10 == 0:
            print(f"  Collected {frame_count} frames → {csv_path}")

    except Exception as e:
        print(f"ERROR: could not write frame: {e}")


def on_stop():
    """Called when the app is stopped — close CSV cleanly."""
    if csv_file:
        csv_file.close()
        print()
        print(f"Collection complete.")
        print(f"  Frames collected: {frame_count}")
        print(f"  Output file:      {csv_path}")


App.run(user_loop=loop, on_stop=on_stop)
