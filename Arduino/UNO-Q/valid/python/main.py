"""
VL53L5CX Validation
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Validates VL53L5CX ranging accuracy at user-specified distances.

Procedure:
  1. Sensor initializes
  2. User places flat target at prompted distance
  3. App collects SAMPLE_COUNT frames
  4. Validates center 2x2 zones against expected distance ± TOLERANCE_PCT%
  5. Reports pass/fail with statistics
  6. Repeat for additional distances or quit

Usage: Place a flat target (e.g. book, board) perpendicular to the
sensor at the prompted distance. Hold steady during sampling.
"""

from arduino.app_utils import *
import time
import statistics

# ── Confidence ────────────────────────────────────────────────────────────────
SIGNAL_MAX = 8000.0
SIGMA_MAX  = 30.0

def confidence(status: bool, signal: int, sigma: int) -> float:
    if not status:
        return 0.0
    sig_score = min(max(signal / SIGNAL_MAX, 0.0), 1.0)
    sma_score = max(0.0, 1.0 - sigma / SIGMA_MAX)
    return min((sig_score * 0.6 + sma_score * 0.4) * 99.99, 99.99)


# ── Config ────────────────────────────────────────────────────────────────────
SAMPLE_COUNT  = 20       # frames to collect per validation
TOLERANCE_PCT = 10.0     # acceptable error ±%
CENTER_ZONES  = [        # 8x8 zone indices for center 2x2
    (3, 3), (3, 4),
    (4, 3), (4, 4),
]

# ── State ─────────────────────────────────────────────────────────────────────
phase        = "init"
samples      = []
expected_mm  = None
results      = []


def parse_matrix(data: str) -> list:
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def parse_status(data: str) -> list:
    return [[v == "T" for v in row.split(",")] for row in data.split(";")]


def center_values(dist: list, stat: list) -> list:
    """Return valid center zone distances."""
    vals = []
    for r, c in CENTER_ZONES:
        if stat[r][c]:
            vals.append(dist[r][c])
    return vals


def validate(expected: int, vals: list) -> dict:
    if not vals:
        return {"pass": False, "reason": "no valid center zones"}
    mean   = statistics.mean(vals)
    stdev  = statistics.stdev(vals) if len(vals) > 1 else 0.0
    error  = abs(mean - expected)
    pct    = (error / expected) * 100
    passed = pct <= TOLERANCE_PCT
    return {
        "pass":     passed,
        "expected": expected,
        "mean":     mean,
        "stdev":    stdev,
        "min":      min(vals),
        "max":      max(vals),
        "error_mm": error,
        "error_pct": pct,
        "n":        len(vals),
    }


def print_result(r: dict):
    status = "PASS ✓" if r["pass"] else "FAIL ✗"
    print(f"\n  {status}")
    print(f"  Expected:  {r['expected']} mm")
    print(f"  Mean:      {r['mean']:.1f} mm")
    print(f"  Std dev:   {r['stdev']:.1f} mm")
    print(f"  Min/Max:   {r['min']} / {r['max']} mm")
    print(f"  Error:     {r['error_mm']:.1f} mm ({r['error_pct']:.1f}%)")
    print(f"  Tolerance: ±{TOLERANCE_PCT:.0f}%")
    print(f"  Samples:   {r['n']} valid zones × {SAMPLE_COUNT} frames")


def print_summary():
    print("\n" + "═" * 40)
    print("  VALIDATION SUMMARY")
    print("═" * 40)
    for i, r in enumerate(results):
        status = "PASS ✓" if r["pass"] else "FAIL ✗"
        print(f"  {r['expected']:5d} mm — {status}  "
              f"(mean {r['mean']:.1f} mm, ±{r['error_pct']:.1f}%)")
    passed = sum(1 for r in results if r["pass"])
    print("─" * 40)
    print(f"  {passed}/{len(results)} tests passed")
    print("═" * 40)


def loop():
    global phase, samples, expected_mm

    # ── Init ─────────────────────────────────────────────────────────────────
    if phase == "init":
        print("VL53L5CX Validation")
        print("Initializing sensor firmware upload...")
        try:
            result = Bridge.call("begin_sensor", timeout=120)
        except Exception as e:
            print(f"ERROR: {e}")
            time.sleep(2.0)
            return
        if result != "ready":
            print(f"ERROR: Sensor init failed — {result}")
            time.sleep(5.0)
            return
        print("Sensor ready.\n")
        phase = "prompt"
        return

    # ── Prompt ────────────────────────────────────────────────────────────────
    if phase == "prompt":
        print("─" * 40)
        dist_str = input(
            "Enter target distance in mm (or 'done' to finish): "
        ).strip()
        if dist_str.lower() in ("done", "q", "quit", ""):
            print_summary()
            raise SystemExit(0)
        try:
            expected_mm = int(dist_str)
            if expected_mm <= 0:
                raise ValueError
        except ValueError:
            print("  Please enter a positive integer distance in mm.")
            return
        print(f"\nPlace flat target at {expected_mm} mm, perpendicular to sensor.")
        input("  Press Enter when ready...")
        print(f"  Collecting {SAMPLE_COUNT} frames...")
        samples = []
        phase   = "collect"
        return

    # ── Collect ───────────────────────────────────────────────────────────────
    if phase == "collect":
        try:
            dist_raw   = Bridge.call("get_distance_data")
            stat_raw   = Bridge.call("get_target_status")
            signal_raw = Bridge.call("get_signal_data")
            sigma_raw  = Bridge.call("get_sigma_data")
        except Exception as e:
            print(f"  WARNING: {e}")
            time.sleep(0.5)
            return

        if "0" in (dist_raw, stat_raw, signal_raw, sigma_raw):
            time.sleep(0.1)
            return

        dist   = parse_matrix(dist_raw)
        stat   = parse_status(stat_raw)
        signal = parse_matrix(signal_raw)
        sigma  = parse_matrix(sigma_raw)
        vals   = center_values(dist, stat)
        confs  = [confidence(stat[r][c], signal[r][c], sigma[r][c])
                  for r, c in CENTER_ZONES if stat[r][c]]
        if vals:
            samples.extend(vals)
            mean_conf = sum(confs) / len(confs) if confs else 0.0
            print(f"  Frame {len(samples) // len(CENTER_ZONES):2d}/{SAMPLE_COUNT}  "
                  f"center: {[f'{v}mm' for v in vals]}  "
                  f"conf: {mean_conf:.1f}%")

        time.sleep(0.1)

        if len(samples) >= SAMPLE_COUNT * len(CENTER_ZONES):
            r = validate(expected_mm, samples)
            print_result(r)
            results.append(r)
            phase = "prompt"
        return


App.run(user_loop=loop)
