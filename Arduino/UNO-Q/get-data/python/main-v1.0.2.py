"""
main-v1.0.2.py
VL53L5CX Interactive Data Collector
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Collects labeled 8x8 depth map frames from the VL53L5CX sensor
and writes them as Edge Impulse-compatible CSV files.

CSV format per row:
    timestamp,d_0_0,...,d_7_7,c_0_0,...,c_7_7,label

    - timestamp : milliseconds since epoch
    - d_x_y     : distance in mm (64 values, row-major)
    - c_x_y     : confidence % (64 values, row-major)
    - label     : orientation label set during collection

Data directory: <app>/data/
Filename:       <label>_<YYYYMMDD_HHMMSS>.csv

v1.0.1: Label prompted during collection, included in filename.
        Each file represents one deliberate sensor orientation.
        Valid labels: center, left, right, up, down
v1.0.2: Clean Ctrl+C handling — stop_sensor() always called on exit.
"""

from hybx_app import Bridge, BridgeError
import os
import sys
import time
import shutil
from datetime import datetime

# ── Paths ──────────────────────────────────────────────────────────────────────

APP_DIR  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_DIR = os.path.join(APP_DIR, "data")

# ── Sensor constants ───────────────────────────────────────────────────────────

RESOLUTION = "8x8"
WIDTH      = 8
SIGNAL_MAX = 8000.0
SIGMA_MAX  = 30.0

# ── Size constants ─────────────────────────────────────────────────────────────

BYTES_PER_FRAME = 729   # worst case: 13+64*4+64*5+10+129 (all max values)
HEADER_BYTES    = 200   # one-time CSV header row
LABEL_WIDTH     = 10    # reserved label field width

# ── Valid labels ───────────────────────────────────────────────────────────────

VALID_LABELS = ["center", "left", "right", "up", "down"]

# ── Error step map ─────────────────────────────────────────────────────────────

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


def confidence(status: bool, signal: int, sigma: int) -> float:
    if not status:
        return 0.0
    sig_score = min(max(signal / SIGNAL_MAX, 0.0), 1.0)
    sma_score = max(0.0, 1.0 - sigma / SIGMA_MAX)
    return min((sig_score * 0.6 + sma_score * 0.4) * 99.99, 99.99)


def format_error(status: str) -> str:
    parts = status.split(":")
    if len(parts) >= 3:
        step_name = ERROR_STEPS.get(parts[1], f"step_{parts[1]}")
        return f"{parts[0]}: {step_name} (ULD code {parts[2]})"
    return status


# ── Storage ────────────────────────────────────────────────────────────────────

def get_free_bytes() -> int:
    os.makedirs(DATA_DIR, exist_ok=True)
    stat = shutil.disk_usage(DATA_DIR)
    return stat.free


def format_size(n: int) -> str:
    if n < 1024:
        return f"{n} B"
    if n < 1024 ** 2:
        return f"{n/1024:.1f} KB"
    if n < 1024 ** 3:
        return f"{n/1024**2:.1f} MB"
    return f"{n/1024**3:.2f} GB"


def estimate_size(frames: int) -> int:
    return HEADER_BYTES + frames * BYTES_PER_FRAME


# ── CSV ────────────────────────────────────────────────────────────────────────

CSV_HEADER = (
    "timestamp,"
    + ",".join(f"d_{r}_{c}" for r in range(8) for c in range(8))
    + ","
    + ",".join(f"c_{r}_{c}" for r in range(8) for c in range(8))
    + ",label\n"
)


def make_filename(label: str) -> str:
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    return os.path.join(DATA_DIR, f"{label}_{ts}.csv")


def make_row(dist: list, stat: list, signal: list, sigma: list, label: str) -> str:
    ts = int(time.time() * 1000)

    dist_vals = [str(dist[r][c]) for r in range(8) for c in range(8)]
    conf_vals = [
        f"{confidence(stat[r][c], signal[r][c], sigma[r][c]):.2f}"
        for r in range(8) for c in range(8)
    ]

    return (
        str(ts)
        + ","
        + ",".join(dist_vals)
        + ","
        + ",".join(conf_vals)
        + ","
        + label
        + "\n"
    )


# ── Sensor init ────────────────────────────────────────────────────────────────

def init_sensor():
    print()
    print("Initializing VL53L5CX...")
    print("  (Firmware upload in progress — this takes up to 120 seconds)")

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


# ── Frame fetch ────────────────────────────────────────────────────────────────

def fetch_frame():
    try:
        dist_raw   = Bridge.call("get_distance_data")
        stat_raw   = Bridge.call("get_target_status")
        signal_raw = Bridge.call("get_signal_data")
        sigma_raw  = Bridge.call("get_sigma_data")
    except BridgeError as e:
        print(f"  ERROR: {e}")
        return None

    if "0" in (dist_raw, stat_raw, signal_raw, sigma_raw):
        return None
    if dist_raw.startswith("error:"):
        print(f"  ERROR: {format_error(dist_raw)}")
        return None

    try:
        dist   = parse_int_matrix(dist_raw)
        stat   = parse_bool_matrix(stat_raw)
        signal = parse_int_matrix(signal_raw)
        sigma  = parse_int_matrix(sigma_raw)
        return dist, stat, signal, sigma
    except Exception as e:
        print(f"  ERROR parsing frame: {e}")
        return None


# ── Interactive setup ──────────────────────────────────────────────────────────

def prompt_label() -> str:
    """Prompt for orientation label. Returns validated label string."""
    print("Valid labels: " + "  ".join(VALID_LABELS))
    print()
    while True:
        raw = input("Orientation label: ").strip().lower()
        if raw in VALID_LABELS:
            return raw
        print(f"  Invalid label '{raw}'. Choose from: {', '.join(VALID_LABELS)}")


def prompt_frames(label: str) -> int:
    """Prompt for frame count and confirm storage fits. Returns approved count."""
    free = get_free_bytes()

    print(f"Storage available: {format_size(free)}")
    print(f"Bytes per frame:   {BYTES_PER_FRAME}")
    print(f"Max frames:        {(free - HEADER_BYTES) // BYTES_PER_FRAME:,}")
    print()

    while True:
        raw = input("How many frames? ").strip()
        if not raw.isdigit() or int(raw) < 1:
            print("  Please enter a positive integer.")
            continue

        frames    = int(raw)
        estimated = estimate_size(frames)
        fits      = estimated <= free

        print()
        print(f"  Label:             {label}")
        print(f"  Frames requested:  {frames:,}")
        print(f"  Estimated size:    {format_size(estimated)}")
        print(f"  Available:         {format_size(free)}")
        print(f"  Fits:              {'YES' if fits else 'NO — not enough space'}")
        print()

        if not fits:
            print("  Not enough space. Please enter a smaller frame count.")
            print()
            continue

        while True:
            choice = input("  Continue or retry? [c/r] ").strip().lower()
            if choice in ("c", "continue"):
                return frames
            if choice in ("r", "retry"):
                print()
                break
            print("  Please enter 'c' to continue or 'r' to retry.")


# ── Collection ─────────────────────────────────────────────────────────────────

def collect(frames: int, label: str):
    """Collect exactly N frames and write to CSV."""
    filename = make_filename(label)
    print()
    print(f"Collecting {frames:,} frames — label: {label}")
    print(f"File: {filename}")
    print()

    collected = 0
    skipped   = 0

    with open(filename, "w") as f:
        f.write(CSV_HEADER)

        while collected < frames:
            frame = fetch_frame()
            if frame is None:
                skipped += 1
                time.sleep(0.05)
                continue

            dist, stat, signal, sigma = frame
            f.write(make_row(dist, stat, signal, sigma, label))
            collected += 1

            interval = max(1, min(frames // 10, 10))
            if collected % interval == 0 or collected == frames:
                pct = collected / frames * 100
                print(f"  {collected:>{len(str(frames))}}/{frames}  ({pct:.0f}%)"
                      + (f"  [{skipped} skipped]" if skipped else ""))

    size = os.path.getsize(filename)
    print()
    print(f"Done.")
    print(f"  Frames collected: {collected:,}")
    if skipped:
        print(f"  Frames skipped:   {skipped:,}")
    print(f"  File size:        {format_size(size)}")
    print(f"  File:             {filename}")


# ── Sensor stop ────────────────────────────────────────────────────────────────

def stop_sensor():
    """Stop ranging cleanly. Always called on exit — even on Ctrl+C."""
    try:
        Bridge.call("end_sensor")
        print("Sensor stopped.")
    except Exception:
        pass


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    print("═" * 60)
    print("  VL53L5CX Data Collector  v1.0.2")
    print("  Hybrid RobotiX — HybX Development System")
    print("═" * 60)

    # Phase 1 — Initialize sensor
    init_sensor()

    # Phase 2 — Label
    label = prompt_label()

    # Phase 3 — Frame count and storage check
    frames = prompt_frames(label)

    # Phase 4 — Collect — always stop sensor on exit
    try:
        collect(frames, label)
    finally:
        stop_sensor()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nAborted.")
        stop_sensor()
        sys.exit(0)
