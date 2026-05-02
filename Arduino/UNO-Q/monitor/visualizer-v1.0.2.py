#!/usr/bin/env python3
"""
visualizer-v1.0.1.py
VL53L5CX Data Visualizer
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Reads a collected CSV data file, displays each frame as two side-by-side
8x8 grids (distance and confidence), analyzes the depth map to suggest
a direction label, and prompts for confirmation or override.

Display is Y-axis flipped (columns mirrored left-to-right) so the view
matches what the sensor sees.

Features:
  - File selection from numbered list (newest first)
  - Centroid-based direction suggestion: left/right/up/down/center
  - Resume: seeks to first unlabeled row on open
  - Forward/backward navigation
  - Writes to temp file, renames to original on save

Controls:
  A        — accept suggested label and advance
  S        — save and quit
  B        — go back one frame
  Q        — quit without saving
  <text>   — override with custom label and advance
"""

import os
import sys
import csv
from datetime import datetime

# ── Paths ──────────────────────────────────────────────────────────────────────

APP_DIR  = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(APP_DIR, "data")

# ── Grid dimensions ────────────────────────────────────────────────────────────

WIDTH = 8

# ── CSV column indices ─────────────────────────────────────────────────────────
# Header: timestamp, d_0_0..d_7_7 (64), c_0_0..c_7_7 (64), label
# Index:  0=timestamp, 1..64=distance, 65..128=confidence, 129=label

TS_IDX    = 0
DIST_IDX  = 1
CONF_IDX  = 65
LABEL_IDX = 129

# ── Direction thresholds ───────────────────────────────────────────────────────
# Centroid ranges [0.0 .. 7.0] mapped to direction.
# Dead zone: 2.5 .. 4.5 = center

CENTER_LOW  = 2.5
CENTER_HIGH = 4.5


# ── File selection ─────────────────────────────────────────────────────────────

def list_data_files() -> list:
    if not os.path.isdir(DATA_DIR):
        return []
    files = sorted(
        [f for f in os.listdir(DATA_DIR) if f.endswith(".csv")],
        reverse=True
    )
    return [os.path.join(DATA_DIR, f) for f in files]


def select_file() -> str:
    files = list_data_files()
    if not files:
        print(f"No CSV files found in {DATA_DIR}")
        sys.exit(1)

    print("Available data files:")
    for i, path in enumerate(files):
        name     = os.path.basename(path)
        size     = os.path.getsize(path)
        size_str = f"{size/1024:.1f} KB" if size < 1024**2 else f"{size/1024**2:.1f} MB"
        # Count labeled rows
        labeled = 0
        total   = 0
        try:
            with open(path, newline="") as f:
                reader = csv.reader(f)
                next(reader)  # skip header
                for row in reader:
                    total += 1
                    if len(row) > LABEL_IDX and row[LABEL_IDX].strip():
                        labeled += 1
        except Exception:
            pass
        print(f"  {i+1:2d}. {name}  ({size_str})  [{labeled}/{total} labeled]")

    print()
    while True:
        raw = input(f"Select file [1-{len(files)}]: ").strip()
        if raw.isdigit() and 1 <= int(raw) <= len(files):
            return files[int(raw) - 1]
        print(f"  Please enter a number between 1 and {len(files)}.")


# ── CSV loading ────────────────────────────────────────────────────────────────

def load_csv(path: str) -> tuple:
    with open(path, newline="") as f:
        reader = csv.reader(f)
        header = next(reader)
        rows = []
        for i, row in enumerate(reader):
            while len(row) <= LABEL_IDX:
                row.append("")
            rows.append({
                "raw":   row,
                "label": row[LABEL_IDX].strip(),
                "index": i,
            })
    return header, rows


def find_resume_index(rows: list) -> int:
    for i, row in enumerate(rows):
        if not row["label"]:
            return i
    return len(rows)


# ── Display ────────────────────────────────────────────────────────────────────

def flip_row(values: list) -> list:
    """Mirror columns left-to-right (Y-axis flip)."""
    return list(reversed(values))


def get_dist_grid(row: dict) -> list:
    """Extract 8x8 distance grid, Y-flipped."""
    vals = [int(row["raw"][DIST_IDX + r * WIDTH + c])
            for r in range(WIDTH) for c in range(WIDTH)]
    grid = [vals[r*WIDTH:(r+1)*WIDTH] for r in range(WIDTH)]
    return [flip_row(r) for r in grid]


def get_conf_grid(row: dict) -> list:
    """Extract 8x8 confidence grid, Y-flipped."""
    vals = [float(row["raw"][CONF_IDX + r * WIDTH + c])
            for r in range(WIDTH) for c in range(WIDTH)]
    grid = [vals[r*WIDTH:(r+1)*WIDTH] for r in range(WIDTH)]
    return [flip_row(r) for r in grid]


# ── Direction analysis ─────────────────────────────────────────────────────────

def suggest_direction(dist_grid: list) -> str:
    """
    Analyze the 8x8 distance grid and suggest a direction label.

    Uses proximity-weighted centroid:
      - Closer distances get higher weight (1/distance)
      - Centroid X position → left/center/right
      - Centroid Y position → up/center/down
      - If both axes are centered → center
      - If one axis is off-center, that axis dominates
      - If both axes are off-center, the stronger deviation wins
    """
    total_weight = 0.0
    cx = 0.0   # weighted centroid X (column)
    cy = 0.0   # weighted centroid Y (row)

    for r in range(WIDTH):
        for c in range(WIDTH):
            d = dist_grid[r][c]
            if d <= 0:
                continue
            w = 1.0 / d
            cx += c * w
            cy += r * w
            total_weight += w

    if total_weight == 0:
        return "center"

    cx /= total_weight   # 0.0 = left,  7.0 = right
    cy /= total_weight   # 0.0 = top,   7.0 = bottom

    x_centered = CENTER_LOW <= cx <= CENTER_HIGH
    y_centered = CENTER_LOW <= cy <= CENTER_HIGH

    if x_centered and y_centered:
        return "center"

    if x_centered:
        return "up" if cy < 3.5 else "down"

    if y_centered:
        return "left" if cx < 3.5 else "right"

    # Both axes off-center — pick the stronger deviation
    x_dev = abs(cx - 3.5)
    y_dev = abs(cy - 3.5)

    if x_dev >= y_dev:
        return "left" if cx < 3.5 else "right"
    else:
        return "up" if cy < 3.5 else "down"


def display_frame(row: dict, current: int, total: int, suggestion: str):
    """Display one frame — distance and confidence grids side by side."""
    dist  = get_dist_grid(row)
    conf  = get_conf_grid(row)
    label = row["label"] or "(unlabeled)"

    ts = int(row["raw"][TS_IDX])
    dt = datetime.fromtimestamp(ts / 1000).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]

    print()
    print(f"  Frame {current+1}/{total}   {dt}   Label: {label}")
    print(f"  Suggested: {suggestion}")
    print()
    print(f"  {'── Distance (mm) ──':^48}  {'── Confidence (%) ──':^48}")
    print()

    for r in range(WIDTH):
        dist_row = "  ".join(f"{v:5d}" for v in dist[r])
        conf_row = "  ".join(f"{v:6.2f}" for v in conf[r])
        print(f"  {dist_row}    {conf_row}")

    print()
    print(f"  A)ccept '{suggestion}'  S)ave+quit  B)ack  Q)uit  or type label: ", end="", flush=True)


# ── Save ───────────────────────────────────────────────────────────────────────

def save_csv(path: str, header: list, rows: list):
    """Write to temp file then rename to original (atomic save)."""
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
    print("  VL53L5CX Data Visualizer  v1.0.2")
    print("  Hybrid RobotiX — HybX Development System")
    print("═" * 60)
    print()
    print("  A)ccept suggested label   S)ave and quit")
    print("  B)ack one frame           Q)uit without saving")
    print("  <text> + Enter            Override with custom label")
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

    # Resume
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
            row        = rows[current]
            dist_grid  = get_dist_grid(row)
            suggestion = suggest_direction(dist_grid)

            display_frame(row, current, total, suggestion)

            try:
                entry = input("").strip()
            except EOFError:
                break

            key = entry.upper()

            if key == "Q":
                # Quit without saving
                print("\n  Quit without saving.")
                sys.exit(0)
            elif key == "S":
                # Save and quit
                break
            elif key == "B":
                if current > 0:
                    current -= 1
                else:
                    print("  Already at first frame.")
            elif key == "A":
                row["label"] = suggestion
                rows[current] = row
                current += 1
            elif entry:
                # Any text — use as label
                row["label"] = entry[:10]
                rows[current] = row
                current += 1
            else:
                # Empty Enter — skip frame, leave blank
                row["label"] = ""
                rows[current] = row
                current += 1

            if current >= total:
                print()
                print("  End of file reached.")
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
