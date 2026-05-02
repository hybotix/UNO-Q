"""
ei-c — Edge Impulse Raw Data Collector
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Collects raw 8x8 distance frames from the VL53L5CX and writes them
to a CSV file with an empty label column for later labeling on the Mac.

Usage:
    start ei-c          -- collect 500 frames (default)
    start ei-c 200      -- collect 200 frames
    start ei-c 1000     -- collect 1000 frames
    mon

Use ei-space.py to estimate disk space before collecting:
    python3 ~/Arduino/UNO-Q/ei-c/ei-space.py 500

Output:
    ~/data/ei-c/ei-c_<TIMESTAMP>.csv

CSV format:
    d00,d01,...,d77,label
    <distance values>,
    ...

Notes:
    - The label column is always empty — labeling happens on the Mac
    - Raw distance data is never modified
    - Progress is printed every 50 frames

Orientation (raw data — never modified):
    Row 0 = top of FOV,    Row 7 = bottom of FOV
    Col 0 = robot left,    Col 7 = robot right
"""

from hybx_app import Bridge, App
import time
import os
import sys
import csv
from datetime import datetime

# ── Configuration ──────────────────────────────────────────────────────────────
RESOLUTION     = "8x8"
OUTPUT_DIR     = os.path.expanduser("~/data/ei-c")
PROGRESS_INT   = 50
DEFAULT_FRAMES = 500

ERROR_STEPS = {
    "0": "none", "1": "vl53l5cx_init", "2": "vl53l5cx_set_resolution",
    "3": "vl53l5cx_set_ranging_frequency_hz", "4": "vl53l5cx_start_ranging",
    "5": "vl53l5cx_stop_ranging", "6": "vl53l5cx_check_data_ready",
    "7": "vl53l5cx_get_ranging_data",
}

# ── Parse frame count from command line ────────────────────────────────────────
frame_target = DEFAULT_FRAMES
_args = [a for a in sys.argv[1:] if not a.startswith("--")]
if _args:
    try:
        frame_target = int(_args[0])
        if frame_target <= 0:
            print(f"ERROR: Frame count must be greater than zero.")
            sys.exit(1)
    except ValueError:
        print(f"ERROR: '{_args[0]}' is not a valid frame count.")
        sys.exit(1)

# ── State ──────────────────────────────────────────────────────────────────────
initialized = False
csv_path    = None
csv_file    = None
csv_writer  = None
frame_count = 0
done        = False


def parse_int_matrix(data: str) -> list:
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def format_error(status: str) -> str:
    parts = status.split(":")
    if len(parts) >= 3:
        step_name = ERROR_STEPS.get(parts[1], f"step_{parts[1]}")
        return f"{parts[0]}: {step_name} (ULD code {parts[2]})"
    return status


def open_csv() -> tuple:
    """Create output directory and open CSV file for writing."""
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    path      = os.path.join(OUTPUT_DIR, f"ei-c_{timestamp}.csv")
    f         = open(path, "w", newline="")
    header    = [f"d{r}{c}" for r in range(8) for c in range(8)] + ["label"]
    writer    = csv.writer(f)
    writer.writerow(header)
    f.flush()
    return path, f, writer


def loop():
    global initialized, csv_path, csv_file, csv_writer, frame_count, done

    if done:
        time.sleep(1.0)
        return

    # ── One-time setup ─────────────────────────────────────────────────────────
    if not initialized:

        if csv_file is None:
            try:
                csv_path, csv_file, csv_writer = open_csv()
                print(f"Frames: {frame_target}")
                print(f"Output: {csv_path}")
                print()
            except Exception as e:
                print(f"ERROR: Could not open output file: {e}")
                time.sleep(5.0)
                return

        try:
            print("Initializing VL53L5CX...")
            result = Bridge.call("begin_sensor", timeout=120)
            if result.startswith("init_failed"):
                print("ERROR: " + format_error(result))
                time.sleep(5.0)
                return
            if result in ("ready", "already_started"):
                res = Bridge.call("set_resolution", RESOLUTION)
                print(f"Sensor ready. Resolution: {res}")
                print(f"Collecting {frame_target} frames...")
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

    # ── Write raw frame + empty label — data is NEVER modified ─────────────────
    try:
        row = [dist[r][c] for r in range(8) for c in range(8)] + [""]
        csv_writer.writerow(row)
        csv_file.flush()
        frame_count += 1

        if frame_count % PROGRESS_INT == 0:
            remaining = frame_target - frame_count
            print(f"  {frame_count}/{frame_target} frames collected "
                  f"({remaining} remaining)")

    except Exception as e:
        print(f"ERROR: could not write frame: {e}")
        return

    # ── Check if target reached ────────────────────────────────────────────────
    if frame_count >= frame_target:
        csv_file.close()
        done = True
        print()
        print(f"Collection complete — {frame_count} frames collected.")
        print(f"Output file: {csv_path}")
        print()
        print(f"Copy to Mac with:")
        print(f"  scp arduino@uno-q.local:{csv_path} ./")


App.run(user_loop=loop)
