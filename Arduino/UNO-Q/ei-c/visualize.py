#!/usr/bin/env python3
"""
visualize.py — Interactive Frame Labeler / Relabeler
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Reads any ei-c CSV — labeled, unlabeled, or partially labeled.
Shows each frame as a 2D heatmap from the sensor's forward-facing
perspective (columns flipped for display only — raw data never touched).

  - Empty label   → prompt for label, write it into that row
  - Existing label → show it, ask to keep (Enter) or type a new one

Updates the input file in place when done.

Usage:
    python3 visualize.py <csv_file>

Controls:
    Type label + Enter    — apply label and advance to next frame
    Enter (no input)      — keep existing label and advance
    S + Enter             — skip this frame (label stays unchanged)
    Q + Enter             — quit and save all progress

Valid labels (max 10 characters):
    UP, DOWN, LEFT, RIGHT, CENTER
    (or any custom label up to 10 characters)

Display:
    Columns are flipped (np.fliplr) so you see the sensor's forward view.
    Raw data written to file is NEVER modified — only the label column changes.

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

MAX_LABEL_LEN = 10
VMIN          = 0
VMAX          = 4000   # mm
VALID_LABELS  = {"UP", "DOWN", "LEFT", "RIGHT", "CENTER"}

# ── Load input CSV ─────────────────────────────────────────────────────────────
if len(sys.argv) < 2:
    print("Usage: python3 visualize.py <csv_file>")
    sys.exit(1)

input_path = sys.argv[1]

if not os.path.exists(input_path):
    print(f"ERROR: File not found: {input_path}")
    sys.exit(1)

print(f"Loading: {input_path}")
df = pd.read_csv(input_path, dtype=str)   # read all as str to preserve empty labels

dist_cols = [f"d{r}{c}" for r in range(8) for c in range(8)]
missing   = [c for c in dist_cols if c not in df.columns]

if missing:
    print(f"ERROR: Missing columns: {missing[:5]}...")
    sys.exit(1)

if "label" not in df.columns:
    df["label"] = ""

n_frames      = len(df)
labeled_count = df["label"].apply(lambda x: str(x).strip() != "").sum()

print(f"Frames:  {n_frames}")
print(f"Labeled: {labeled_count}  Unlabeled: {n_frames - labeled_count}")
print()
print("Controls:")
print("  Type label + Enter  — apply label and advance")
print("  Enter alone         — keep existing label and advance")
print("  S + Enter           — skip (no change)")
print("  Q + Enter           — quit and save")
print()

# ── Build figure ───────────────────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(7, 7))
plt.ion()

changed_count = 0
skipped_count = 0


def draw_frame(idx):
    """
    Display frame with columns flipped (Y axis) for sensor forward view.
    Raw data in df is never modified — only frame_display is flipped.
    """
    ax.clear()

    # Get raw distance values — never modified
    raw_vals = df[dist_cols].iloc[idx].astype(int).values
    frame_raw = raw_vals.reshape(8, 8)

    # Flip columns for display only
    frame_display = np.fliplr(frame_raw)

    current_label = str(df.at[idx, "label"]).strip()
    label_str     = f"[{current_label}]" if current_label else "[unlabeled]"

    ax.imshow(frame_display, cmap="RdYlGn_r", vmin=VMIN, vmax=VMAX,
              interpolation="nearest")

    ax.set_title(
        f"Frame {idx + 1} / {n_frames}    {label_str}\n"
        f"Labeled: {labeled_count}  Changed: {changed_count}  "
        f"Skipped: {skipped_count}",
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


def save_csv():
    """Write updated dataframe back to the input file in place."""
    df.to_csv(input_path, index=False)
    print(f"Saved: {input_path}")


# ── Main labeling loop ─────────────────────────────────────────────────────────
for idx in range(n_frames):
    draw_frame(idx)

    current_label = str(df.at[idx, "label"]).strip()
    has_label     = current_label != "" and current_label != "nan"

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
            save_csv()
            plt.close()
            sys.exit(0)

        if response == "Q":
            save_csv()
            plt.close()
            print()
            print(f"Progress saved.")
            print(f"  Changed: {changed_count}  Skipped: {skipped_count}")
            sys.exit(0)

        elif response == "S":
            skipped_count += 1
            print(f"  Skipped frame {idx + 1}.")
            break

        elif response == "":
            # Keep existing label — only valid if frame already has one
            if has_label:
                print(f"  Kept [{current_label}].")
                break
            else:
                print("  No existing label — please enter a label, S to skip, Q to quit.")

        elif len(response) > MAX_LABEL_LEN:
            print(f"  Label too long ({len(response)} chars, max {MAX_LABEL_LEN}). Try again.")

        else:
            # Apply new label — raw distance data is NEVER modified
            old_label = current_label
            df.at[idx, "label"] = response
            if has_label:
                changed_count += 1
                print(f"  Relabeled [{old_label}] → [{response}].")
            else:
                labeled_count += 1
                print(f"  Labeled [{response}].")
            break

# ── All frames done ────────────────────────────────────────────────────────────
save_csv()
plt.close()
print()
print(f"All frames processed.")
print(f"  Labeled:  {labeled_count}")
print(f"  Changed:  {changed_count}")
print(f"  Skipped:  {skipped_count}")
print(f"  Saved to: {input_path}")
