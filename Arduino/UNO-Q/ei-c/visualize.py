#!/usr/bin/env python3
"""
visualize.py — Interactive Frame Labeler / Relabeler
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Reads any ei-c CSV — labeled, unlabeled, or partially labeled.
Shows each frame as a 2D heatmap from the sensor's forward-facing
perspective (columns flipped for display only — raw data never touched).

  - Empty label   → prompt for label
  - Existing label → show it, ask to keep (Enter) or type a new one

Writes a NEW output file every run — input file is NEVER modified.
Output: <input_basename>_labeled_<TIMESTAMP>.csv

Usage:
    python3 visualize.py <csv_file>

Controls:
    Type label + Enter    — apply label and advance to next frame
    Enter (no input)      — keep existing label and advance
    S + Enter             — skip this frame (written as-is to output)
    Q + Enter             — quit and save all progress to output file

Valid labels (max 10 characters):
    UP, DOWN, LEFT, RIGHT, CENTER
    (or any custom label up to 10 characters)

Display:
    Columns are flipped (np.fliplr) so you see the sensor's forward view.
    Raw distance data written to output is NEVER modified.

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
from datetime import datetime

MAX_LABEL_LEN = 10
VMIN          = 0
VMAX          = 4000   # mm

# ── Load input CSV ─────────────────────────────────────────────────────────────
if len(sys.argv) < 2:
    print("Usage: python3 visualize.py <csv_file>")
    sys.exit(1)

input_path = sys.argv[1]

if not os.path.exists(input_path):
    print(f"ERROR: File not found: {input_path}")
    sys.exit(1)

# ── Build output path — new file every run ─────────────────────────────────────
base, ext    = os.path.splitext(input_path)
timestamp    = datetime.now().strftime("%Y%m%d_%H%M%S")
output_path  = f"{base}_labeled_{timestamp}.csv"

print(f"Input:  {input_path}")
print(f"Output: {output_path}")
print()

# ── Load CSV ───────────────────────────────────────────────────────────────────
df = pd.read_csv(input_path, dtype=str)   # all as str to preserve empty labels

dist_cols = [f"d{r}{c}" for r in range(8) for c in range(8)]
missing   = [c for c in dist_cols if c not in df.columns]

if missing:
    print(f"ERROR: Missing columns: {missing[:5]}...")
    sys.exit(1)

if "label" not in df.columns:
    df["label"] = ""

n_frames      = len(df)
already_labeled = df["label"].apply(lambda x: str(x).strip() not in ("", "nan")).sum()

print(f"Frames:           {n_frames}")
print(f"Already labeled:  {already_labeled}")
print(f"Unlabeled:        {n_frames - already_labeled}")
print()
print("Controls:")
print("  Type label + Enter  — apply label and advance")
print("  Enter alone         — keep existing label and advance")
print("  S + Enter           — skip (written as-is to output)")
print("  Q + Enter           — quit and save progress to output file")
print()

# ── Open output CSV ────────────────────────────────────────────────────────────
out_file   = open(output_path, "w", newline="")
out_writer = csv.writer(out_file)
out_writer.writerow(dist_cols + ["label"])

# ── Build figure ───────────────────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(7, 7))
plt.ion()

labeled_count = 0
changed_count = 0
skipped_count = 0
written_count = 0


def draw_frame(idx, current_label):
    """
    Display frame with columns flipped (Y axis) for sensor forward view.
    Raw data in df is never modified — only frame_display is flipped.
    """
    ax.clear()

    raw_vals      = df[dist_cols].iloc[idx].astype(int).values
    frame_raw     = raw_vals.reshape(8, 8)
    frame_display = np.fliplr(frame_raw)   # display only — raw data unchanged

    label_str = f"[{current_label}]" if current_label else "[unlabeled]"

    ax.imshow(frame_display, cmap="RdYlGn_r", vmin=VMIN, vmax=VMAX,
              interpolation="nearest")

    ax.set_title(
        f"Frame {idx + 1} / {n_frames}    {label_str}\n"
        f"Labeled: {labeled_count}  Changed: {changed_count}  "
        f"Skipped: {skipped_count}  Written: {written_count}",
        fontsize=11, fontweight="bold"
    )
    ax.set_xlabel("← Robot RIGHT          Robot LEFT →  (display flipped)")
    ax.set_ylabel("Row (↓ robot down)")

    ax.set_xticks(range(8))
    ax.set_xticklabels([f"C{7-c}" for c in range(8)], fontsize=8)
    ax.set_yticks(range(8))
    ax.set_yticklabels([f"R{r}" for r in range(8)], fontsize=8)

    for r in range(8):
        for c in range(8):
            val   = frame_display[r][c]
            color = "white" if val > VMAX * 0.6 else "black"
            ax.text(c, r, str(val), ha="center", va="center",
                    fontsize=8, color=color)

    fig.canvas.draw()
    fig.canvas.flush_events()


def write_row(idx, label):
    """Write raw distance data + label to output CSV. Raw data never modified."""
    global written_count
    raw_vals = df[dist_cols].iloc[idx].astype(int).values.tolist()
    out_writer.writerow(raw_vals + [label])
    out_file.flush()
    written_count += 1


def finish(reason="complete"):
    out_file.close()
    plt.close()
    print()
    print(f"Session {reason}.")
    print(f"  Frames written: {written_count}")
    print(f"  Labeled:        {labeled_count}")
    print(f"  Changed:        {changed_count}")
    print(f"  Skipped:        {skipped_count}")
    print(f"  Output file:    {output_path}")


# ── Main labeling loop ─────────────────────────────────────────────────────────
for idx in range(n_frames):
    current_label = str(df.at[idx, "label"]).strip()
    has_label     = current_label not in ("", "nan")
    if not has_label:
        current_label = ""

    draw_frame(idx, current_label)

    if has_label:
        prompt = (f"Frame {idx + 1}/{n_frames} "
                  f"[{current_label}] — Enter=keep, new label, S=skip, Q=quit: ")
    else:
        prompt = (f"Frame {idx + 1}/{n_frames} "
                  f"[unlabeled] — Label, S=skip, Q=quit: ")

    while True:
        try:
            response = input(prompt).strip().upper()
        except (EOFError, KeyboardInterrupt):
            print("\nInterrupted — saving progress.")
            finish("interrupted")
            sys.exit(0)

        if response == "Q":
            finish("saved")
            sys.exit(0)

        elif response == "S":
            # Write row as-is with its existing label (may be empty)
            write_row(idx, current_label)
            skipped_count += 1
            print(f"  Skipped frame {idx + 1} — written as-is.")
            break

        elif response == "":
            if has_label:
                write_row(idx, current_label)
                print(f"  Kept [{current_label}].")
                break
            else:
                print("  No existing label — enter a label, S to skip, Q to quit.")

        elif len(response) > MAX_LABEL_LEN:
            print(f"  Label too long ({len(response)} chars, max {MAX_LABEL_LEN}). Try again.")

        else:
            # Apply new label — raw distance data is NEVER modified
            write_row(idx, response)
            if has_label and response != current_label:
                changed_count += 1
                print(f"  Relabeled [{current_label}] → [{response}].")
            else:
                labeled_count += 1
                print(f"  Labeled [{response}].")
            break

# ── All frames processed ───────────────────────────────────────────────────────
finish()
