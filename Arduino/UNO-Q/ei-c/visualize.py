#!/usr/bin/env python3
"""
visualize.py — Edge Impulse Data Visualizer
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Reads a CSV file collected by ei-c and displays each 8x8 frame as a
2D heatmap. The Y axis (columns) is flipped to show the view from
behind the sensor — matching the robot's forward-facing perspective.

Usage:
    python3 visualize.py <csv_file> [--auto] [--delay 0.2]

Arguments:
    csv_file        Path to CSV file collected by ei-c
    --auto          Auto-play through frames (default: manual stepping)
    --delay <sec>   Delay between frames in auto-play mode (default: 0.2s)

Controls (manual mode):
    Right arrow / Space / Enter  — next frame
    Left arrow                   — previous frame
    Q                            — quit

Dependencies:
    pip3 install matplotlib numpy pandas
"""

import sys
import os
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
from matplotlib.widgets import Button

# ── Parse arguments ────────────────────────────────────────────────────────────
parser = argparse.ArgumentParser(description="ei-c Frame Visualizer")
parser.add_argument("csv_file",           help="CSV file collected by ei-c")
parser.add_argument("--auto",             action="store_true", help="Auto-play frames")
parser.add_argument("--delay", type=float, default=0.2,       help="Auto-play delay (seconds)")
args = parser.parse_args()

if not os.path.exists(args.csv_file):
    print(f"ERROR: File not found: {args.csv_file}")
    sys.exit(1)

# ── Load CSV ───────────────────────────────────────────────────────────────────
print(f"Loading: {args.csv_file}")
df = pd.read_csv(args.csv_file)

# Extract distance columns (d00..d77) and label
dist_cols = [f"d{r}{c}" for r in range(8) for c in range(8)]
missing   = [c for c in dist_cols if c not in df.columns]
if missing:
    print(f"ERROR: Missing columns in CSV: {missing[:5]}...")
    sys.exit(1)

frames     = df[dist_cols].values.reshape(-1, 8, 8)
labels     = df["label"].values if "label" in df.columns else ["unknown"] * len(frames)
n_frames   = len(frames)

print(f"Frames loaded: {n_frames}")
print(f"Labels: {sorted(set(labels))}")
print()

# ── Build display ──────────────────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(7, 7))
plt.subplots_adjust(bottom=0.15)

# Colormap — closer = warmer
cmap    = plt.cm.RdYlGn_r
vmin    = 0
vmax    = 4000   # mm — adjust if your scene is deeper

current = [0]    # mutable for closure


def get_frame(idx):
    """Return frame flipped on Y axis (columns flipped) for behind-sensor view."""
    return np.fliplr(frames[idx])


def draw_frame(idx):
    ax.clear()
    frame        = get_frame(idx)
    label        = labels[idx]
    im           = ax.imshow(frame, cmap=cmap, vmin=vmin, vmax=vmax,
                             interpolation="nearest")
    ax.set_title(f"Frame {idx + 1} / {n_frames}    Label: [{label}]",
                 fontsize=13, fontweight="bold")
    ax.set_xlabel("Column (robot right →)")
    ax.set_ylabel("Row (↓ robot down)")

    # Column labels — flipped: col 7 on left, col 0 on right
    ax.set_xticks(range(8))
    ax.set_xticklabels([f"C{7-c}" for c in range(8)])
    ax.set_yticks(range(8))
    ax.set_yticklabels([f"R{r}" for r in range(8)])

    # Annotate each cell with distance value
    for r in range(8):
        for c in range(8):
            val  = frame[r][c]
            color = "white" if val > vmax * 0.6 else "black"
            ax.text(c, r, str(val), ha="center", va="center",
                    fontsize=8, color=color)

    fig.canvas.draw_idle()


def on_key(event):
    idx = current[0]
    if event.key in ("right", " ", "enter"):
        current[0] = min(idx + 1, n_frames - 1)
    elif event.key == "left":
        current[0] = max(idx - 1, 0)
    elif event.key in ("q", "Q"):
        plt.close()
        return
    draw_frame(current[0])


# ── Initial draw ───────────────────────────────────────────────────────────────
draw_frame(0)

if args.auto:
    print(f"Auto-play mode — {args.delay}s per frame. Close window to stop.")
    fig.canvas.mpl_connect("key_press_event", on_key)

    def auto_play(frame_idx=0):
        if frame_idx >= n_frames:
            print("Auto-play complete.")
            return
        current[0] = frame_idx
        draw_frame(frame_idx)
        plt.pause(args.delay)
        auto_play(frame_idx + 1)

    plt.pause(0.5)
    auto_play(0)
else:
    print("Manual mode — arrow keys or space to step, Q to quit.")
    fig.canvas.mpl_connect("key_press_event", on_key)

plt.tight_layout()
plt.show()
