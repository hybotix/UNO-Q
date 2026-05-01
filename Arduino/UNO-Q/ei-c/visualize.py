#!/usr/bin/env python3
"""
visualize.py — Interactive Frame Labeler
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Reads a raw unlabeled CSV collected by ei-c. Shows each frame as a
2D heatmap from the sensor's forward-facing perspective (Y axis / columns
flipped so robot left appears on right, robot right appears on left).

You are prompted for a label for each frame. The raw distance data is
written to the output CSV UNCHANGED — only the display is flipped.

Usage:
    python3 visualize.py <input_csv>

Output:
    <input_csv>_labeled.csv  — Edge Impulse format with label column

Controls:
    UP / DOWN / LEFT / RIGHT / CENTER  — label this frame and advance
    S                                   — skip this frame (not written)
    Q                                   — quit and save progress

Valid labels:
    UP, DOWN, LEFT, RIGHT, CENTER

Dependencies:
    pip3 install matplotlib numpy pandas
"""

import sys
import os
import csv
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt

VALID_LABELS = {"UP", "DOWN", "LEFT", "RIGHT", "CENTER"}
VMIN         = 0
VMAX         = 4000   # mm

# ── Load input CSV ─────────────────────────────────────────────────────────────
if len(sys.argv) < 2:
    print("Usage: python3 visualize.py <input_csv>")
    sys.exit(1)

input_path = sys.argv[1]

if not os.path.exists(input_path):
    print(f"ERROR: File not found: {input_path}")
    sys.exit(1)

base, ext    = os.path.splitext(input_path)
output_path  = f"{base}_labeled.csv"

print(f"Input:  {input_path}")
print(f"Output: {output_path}")
print()

df        = pd.read_csv(input_path)
dist_cols = [f"d{r}{c}" for r in range(8) for c in range(8)]
missing   = [c for c in dist_cols if c not in df.columns]

if missing:
    print(f"ERROR: Missing columns: {missing[:5]}...")
    sys.exit(1)

frames    = df[dist_cols].values  # shape: (n_frames, 64) — raw, never modified
n_frames  = len(frames)

print(f"Frames loaded: {n_frames}")
print()
print("Controls:")
print("  Type label + Enter: UP, DOWN, LEFT, RIGHT, CENTER")
print("  S + Enter          : skip this frame")
print("  Q + Enter          : quit and save progress")
print()

# ── Open output CSV ────────────────────────────────────────────────────────────
out_file   = open(output_path, "w", newline="")
out_writer = csv.writer(out_file)
out_writer.writerow(dist_cols + ["label"])

# ── Build figure ───────────────────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(7, 7))
plt.ion()

labeled_count = 0
skipped_count = 0


def draw_frame(idx, raw_row):
    """
    Display frame as 2D heatmap with Y axis (columns) flipped.
    raw_row is the original 64-value flat array — display only is transformed.
    """
    ax.clear()

    # Reshape to 8x8 — raw data, never modified
    frame_raw = raw_row.reshape(8, 8)

    # Flip columns for display — shows sensor view from front of robot
    # raw data is NOT changed — only this display variable is flipped
    frame_display = np.fliplr(frame_raw)

    ax.imshow(frame_display, cmap="RdYlGn_r", vmin=VMIN, vmax=VMAX,
              interpolation="nearest")

    ax.set_title(
        f"Frame {idx + 1} / {n_frames}\n"
        f"Labeled: {labeled_count}  Skipped: {skipped_count}  "
        f"Remaining: {n_frames - idx - 1}",
        fontsize=11, fontweight="bold"
    )
    ax.set_xlabel("← Robot RIGHT          Robot LEFT →  (display flipped)")
    ax.set_ylabel("Row (↓ robot down)")

    # Column tick labels — show flipped column numbers
    ax.set_xticks(range(8))
    ax.set_xticklabels([f"C{7-c}" for c in range(8)], fontsize=8)
    ax.set_yticks(range(8))
    ax.set_yticklabels([f"R{r}" for r in range(8)], fontsize=8)

    # Annotate cells with raw distance values
    for r in range(8):
        for c in range(8):
            val   = frame_display[r][c]
            color = "white" if val > VMAX * 0.6 else "black"
            ax.text(c, r, str(val), ha="center", va="center",
                    fontsize=8, color=color)

    fig.canvas.draw()
    fig.canvas.flush_events()


# ── Main labeling loop ─────────────────────────────────────────────────────────
for idx in range(n_frames):
    raw_row = frames[idx]   # original flat 64-value row — never modified
    draw_frame(idx, raw_row)

    while True:
        try:
            response = input(
                f"Frame {idx + 1}/{n_frames} — "
                f"Label (UP/DOWN/LEFT/RIGHT/CENTER/S=skip/Q=quit): "
            ).strip().upper()
        except (EOFError, KeyboardInterrupt):
            print("\nInterrupted — saving progress.")
            response = "Q"

        if response == "Q":
            out_file.close()
            plt.close()
            print()
            print(f"Saved {labeled_count} labeled frames to: {output_path}")
            print(f"Skipped {skipped_count} frames.")
            sys.exit(0)

        elif response == "S":
            skipped_count += 1
            print(f"  Skipped frame {idx + 1}.")
            break

        elif response in VALID_LABELS:
            # Write raw data unchanged + label to output CSV
            out_writer.writerow(list(raw_row) + [response])
            out_file.flush()
            labeled_count += 1
            print(f"  Labeled [{response}] — frame {idx + 1}.")
            break

        else:
            print(f"  Invalid input '{response}'. "
                  f"Valid: UP, DOWN, LEFT, RIGHT, CENTER, S, Q")

# ── All frames done ────────────────────────────────────────────────────────────
out_file.close()
plt.close()
print()
print(f"All frames processed.")
print(f"  Labeled:  {labeled_count}")
print(f"  Skipped:  {skipped_count}")
print(f"  Output:   {output_path}")
