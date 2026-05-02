#!/usr/bin/env python3
"""
ei-space.py — Edge Impulse Disk Space Calculator
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
San Diego, CA

Calculates estimated disk space required for ei-c data collection
and shows current available space in ~/data/ei-c.

Usage:
    python3 ei-space.py [frames]

    frames — number of frames to estimate (optional, prompts if not given)

Examples:
    python3 ei-space.py
    python3 ei-space.py 500
    python3 ei-space.py 1000
"""

import sys
import os
import shutil

# ── Constants ──────────────────────────────────────────────────────────────────
BYTES_PER_FRAME = 332    # 64 values x ~5 chars + comma + 10 char label + newline
BYTES_HEADER    = 215    # header row
OUTPUT_DIR      = os.path.expanduser("~/data/ei-c")


def format_size(n_bytes: int) -> str:
    if n_bytes < 1024:
        return f"{n_bytes} bytes"
    elif n_bytes < 1024 * 1024:
        return f"{n_bytes / 1024:.1f} KB"
    else:
        return f"{n_bytes / (1024 * 1024):.2f} MB"


def estimate(n_frames: int):
    estimated = BYTES_HEADER + (n_frames * BYTES_PER_FRAME)

    # Get available disk space
    try:
        usage    = shutil.disk_usage(os.path.expanduser("~"))
        free     = usage.free
        total    = usage.total
        used_pct = (usage.used / total) * 100
        fits     = estimated <= free
    except Exception:
        free     = None
        fits     = None

    print()
    print(f"  Frames requested : {n_frames:,}")
    print(f"  Estimated size   : {format_size(estimated)}")
    print(f"  Output directory : {OUTPUT_DIR}")
    print()

    if free is not None:
        print(f"  Disk free        : {format_size(free)}")
        print(f"  Disk used        : {used_pct:.1f}%")
        print()
        if fits:
            print(f"  ✓ Sufficient space available.")
        else:
            print(f"  ✗ WARNING: Not enough space! "
                  f"Need {format_size(estimated)}, have {format_size(free)}.")
    print()

    # Show existing ei-c files
    if os.path.isdir(OUTPUT_DIR):
        files = sorted([
            f for f in os.listdir(OUTPUT_DIR) if f.endswith(".csv")
        ])
        if files:
            total_size = sum(
                os.path.getsize(os.path.join(OUTPUT_DIR, f)) for f in files
            )
            print(f"  Existing files   : {len(files)} CSV files "
                  f"({format_size(total_size)} total)")
            for f in files[-5:]:   # show last 5
                fpath = os.path.join(OUTPUT_DIR, f)
                fsize = os.path.getsize(fpath)
                print(f"    {f}  ({format_size(fsize)})")
            if len(files) > 5:
                print(f"    ... and {len(files) - 5} more")
            print()


# ── Main ───────────────────────────────────────────────────────────────────────
print("=== ei-space — Edge Impulse Disk Space Calculator ===")

if len(sys.argv) > 1:
    try:
        n = int(sys.argv[1])
        if n <= 0:
            print("ERROR: Frame count must be greater than zero.")
            sys.exit(1)
        estimate(n)
    except ValueError:
        print(f"ERROR: '{sys.argv[1]}' is not a valid number.")
        sys.exit(1)
else:
    # Interactive mode
    common = [100, 200, 500, 1000, 2000]
    print()
    print("  Common frame counts:")
    for n in common:
        est = BYTES_HEADER + (n * BYTES_PER_FRAME)
        print(f"    {n:>6,} frames — {format_size(est)}")
    print()

    while True:
        try:
            response = input("  How many frames? [500]: ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            sys.exit(0)

        if response == "":
            n = 500
        else:
            try:
                n = int(response)
                if n <= 0:
                    print("  ERROR: Must be greater than zero.")
                    continue
            except ValueError:
                print(f"  ERROR: '{response}' is not a valid number.")
                continue
        estimate(n)
        break
