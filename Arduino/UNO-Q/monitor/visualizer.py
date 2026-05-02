#!/usr/bin/env python3
"""
visualizer.py
VL53L5CX Data Visualizer
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Reads a collected CSV data file, displays each frame as two side-by-side
8x8 grids (distance and confidence), and prompts for a label per frame.

Display is Y-axis flipped (columns mirrored left-to-right) so the view
matches what the sensor sees.

Features:
  - File selection from numbered list (newest first)
  - Resume: seeks to first unlabeled row on open
  - Forward/backward navigation
  - Skip frames (blank label)
  - Writes to temp file, renames to original on save

Usage:
    ~/Virtual/testing/bin/python3 visualizer.py
    (or: start visualizer — once added to UNO-Q apps)

Controls:
    <label> + Enter  — label current frame and advance
    -                — go back one frame
    .                — skip frame (leave blank) and advance
    q                — quit and save
"""

import os
import sys
import csv
import shutil
import tempfile
from datetime import datetime
from pathlib import Path

# ── Paths ──────────────────────────────────────────────────────────────────────

APP_DIR  = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(APP_DIR, "data")

# ── Grid dimensions ────────────────────────────────────────────────────────────

WIDTH = 8

# ── Column indices in CSV ──────────────────────────────────────────────────────
# Header: timestamp, d_0_0..d_7_7 (64), c_0_0..c_7_7 (64), label
# Indices: 0=timestamp, 1..64=distance, 65..128=confidence, 129=label

TS_IDX   = 0
DIST_IDX = 1          # d_0_0 .. d_7_7 — 64 values
CONF_IDX = 65         # c_0_0 .. c_7_7 — 64 values
LABEL_IDX = 129


# ── File selection ─────────────────────────────────────────────────────────────

def list_data_files() -> list[str]:
    """Return list of CSV files in DATA_DIR, newest first."""
    if not os.path.isdir(DATA_DIR):
        return []
    files = sorted(
        [f for f in os.listdir(DATA_DIR) if f.endswith(".csv")],
        reverse=True
    )
    return [os.path.join(DATA_DIR, f) for f in files]


def select_file() -> str:
    """Prompt user to select a file from the list. Returns full path."""
    files = list_data_files()
    if not files:
        print(f"No CSV files found in {DATA_DIR}")
        sys.exit(1)

    print("Available data files:")
    for i, path in enumerate(files):
        name     = os.path.basename(path)
        size     = os.path.getsize(path)
        size_str = f"{size/1024:.1f} KB" if size < 1024**2 else f"{size/1024**2:.1f} MB"
        print(f"  {i+1:2d}. {name}  ({size_str})")

    print()
    while True:
        raw = input(f"Select file [1-{len(files)}]: ").strip()
        if raw.isdigit() and 1 <= int(raw) <= len(files):
            return files[int(raw) - 1]
        print(f"  Please enter a number between 1 and {len(files)}.")


# ── CSV loading ────────────────────────────────────────────────────────────────

def load_csv(path: str) -> tuple[list, list[dict]]:
    """
    Load CSV file.
    Returns (header_row, rows) where each row is a dict with keys:
        'raw'   : original list of string values
        'label' : current label string (may be empty)
        'index' : row index (0-based)
    """
    with open(path, newline="") as f:
        reader = csv.reader(f)
        header = next(reader)
        rows = []
        for i, row in enumerate(reader):
            # Pad row if label column is missing
            while len(row) <= LABEL_IDX:
                row.append("")
            rows.append({
                "raw":   row,
                "label": row[LABEL_IDX].strip(),
                "index": i,
            })
    return header, rows


def find_resume_index(rows: list[dict]) -> int:
    """Return index of first unlabeled row. 0 if all unlabeled, len if all done."""
    for i, row in enumerate(rows):
        if not row["label"]:
            return i
    return len(rows)


# ── Display ────────────────────────────────────────────────────────────────────

def flip_row(values: list) -> list:
    """Flip a row left-to-right (mirror columns)."""
    return list(reversed(values))


def get_dist_grid(row: dict) -> list[list[int]]:
    """Extract 8x8 distance grid from row, Y-flipped."""
    vals = [int(row["raw"][DIST_IDX + r * WIDTH + c])
            for r in range(WIDTH) for c in range(WIDTH)]
    grid = [vals[r*WIDTH:(r+1)*WIDTH] for r in range(WIDTH)]
    return [flip_row(r) for r in grid]


def get_conf_grid(row: dict) -> list[list[float]]:
    """Extract 8x8 confidence grid from row, Y-flipped."""
    vals = [float(row["raw"][CONF_IDX + r * WIDTH + c])
            for r in range(WIDTH) for c in range(WIDTH)]
    grid = [vals[r*WIDTH:(r+1)*WIDTH] for r in range(WIDTH)]
    return [flip_row(r) for r in grid]


def display_frame(row: dict, current: int, total: int):
    """Display one frame — distance and confidence side by side."""
    dist = get_dist_grid(row)
    conf = get_conf_grid(row)
    label = row["label"] or "(unlabeled)"

    ts = int(row["raw"][TS_IDX])
    dt = datetime.fromtimestamp(ts / 1000).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]

    print()
    print(f"  Frame {current+1}/{total}   {dt}   Label: {label}")
    print()
    print(f"  {'── Distance (mm) ──':^48}  {'── Confidence (%) ──':^48}")
    print()

    for r in range(WIDTH):
        dist_row = "  ".join(f"{v:5d}" for v in dist[r])
        conf_row = "  ".join(f"{v:6.2f}" for v in conf[r])
        print(f"  {dist_row}    {conf_row}")

    print()


# ── Save ───────────────────────────────────────────────────────────────────────

def save_csv(path: str, header: list, rows: list[dict]):
    """Write to temp file then rename to original."""
    tmp_path = path + ".tmp"
    with open(tmp_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        for row in rows:
            out = list(row["raw"])
            out[LABEL_IDX] = row["label"]
            writer.writerow(out)
    os.replace(tmp_path, path)


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    print("═" * 60)
    print("  VL53L5CX Data Visualizer")
    print("  Hybrid RobotiX — HybX Development System")
    print("═" * 60)
    print()
    print("Controls:")
    print("  <label> + Enter  — label frame and advance")
    print("  -                — go back one frame")
    print("  .                — skip frame and advance")
    print("  q                — quit and save")
    print()

    # File selection
    path = select_file()
    print(f"\nOpened: {os.path.basename(path)}")

    # Load
    header, rows = load_csv(path)
    total = len(rows)

    if total == 0:
        print("ERROR: File contains no data rows.")
        sys.exit(1)

    # Resume — seek to first unlabeled row
    current = find_resume_index(rows)
    if current == total:
        print(f"All {total} frames already labeled.")
        yn = input("Re-label from beginning? [y/n] ").strip().lower()
        if yn == "y":
            current = 0
        else:
            sys.exit(0)
    elif current > 0:
        print(f"Resuming from frame {current+1} (first unlabeled).")

    labeled_count = sum(1 for r in rows if r["label"])
    print(f"Labeled: {labeled_count}/{total}")

    # Label loop
    try:
        while 0 <= current < total:
            row = rows[current]
            display_frame(row, current, total)

            prompt = f"  Label [{current+1}/{total}]: "
            try:
                entry = input(prompt).strip()
            except EOFError:
                break

            if entry == "q":
                break
            elif entry == "-":
                # Go back
                if current > 0:
                    current -= 1
                else:
                    print("  Already at first frame.")
            elif entry == ".":
                # Skip — leave blank
                row["label"] = ""
                current += 1
            else:
                # Label — truncate to 10 chars
                row["label"] = entry[:10]
                rows[current] = row
                current += 1

            if current >= total:
                print()
                print(f"  End of file reached.")
                break

    except KeyboardInterrupt:
        print("\n  Interrupted.")

    # Save
    print()
    labeled_count = sum(1 for r in rows if r["label"])
    print(f"Saving... ({labeled_count}/{total} frames labeled)")
    save_csv(path, header, rows)
    print(f"Saved: {os.path.basename(path)}")


if __name__ == "__main__":
    main()
