"""
VL53L5CX Validation
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Validates VL53L5CX distance accuracy at fixed reference distances.
All 64 zones are evaluated. Valid zones must read within TOLERANCE
of the reference distance.

Test distances: 50, 100, 250, 500 mm
Tolerance:      +/- 50 mm
Frames:         10 per test point
"""

from arduino.app_utils import *
import time
import statistics

# ── Configuration ──────────────────────────────────────────────────────────────
TEST_DISTANCES_MM = [50, 100, 250, 500]
TOLERANCE_MM      = 50
FRAMES            = 10

# ── State ──────────────────────────────────────────────────────────────────────
initialized  = False
test_index   = 0
results      = []


def parse_matrix(data: str) -> list[list]:
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def parse_status(data: str) -> list[list]:
    return [[v == "T" for v in row.split(",")] for row in data.split(";")]


def collect_frames(n: int) -> list[list[list]]:
    """Collect n distance frames, skipping invalid reads."""
    frames = []
    while len(frames) < n:
        try:
            dist = Bridge.call("get_distance_data")
            stat = Bridge.call("get_target_status")
            if not dist or dist == "0" or not stat or stat == "0":
                time.sleep(0.1)
                continue
            d_matrix = parse_matrix(dist)
            s_matrix = parse_status(stat)
            frames.append((d_matrix, s_matrix))
            time.sleep(0.1)
        except Exception as e:
            print(f"  WARNING: {e}")
            time.sleep(0.2)
    return frames


def validate_distance(target_mm: int, frames: list) -> dict:
    """
    Evaluate all valid zones across all frames against target distance.
    Returns a result dict with per-zone stats and overall pass/fail.
    """
    zone_values = {}  # (row, col) -> list of valid readings

    for d_matrix, s_matrix in frames:
        for row in range(8):
            for col in range(8):
                if s_matrix[row][col]:
                    key = (row, col)
                    if key not in zone_values:
                        zone_values[key] = []
                    zone_values[key].append(d_matrix[row][col])

    zone_results = {}
    passing_zones = 0
    total_zones   = 0

    for (row, col), values in zone_values.items():
        if len(values) < FRAMES // 2:
            continue  # skip zones with too few valid readings
        mean   = statistics.mean(values)
        stddev = statistics.stdev(values) if len(values) > 1 else 0.0
        error  = mean - target_mm
        passed = abs(error) <= TOLERANCE_MM
        zone_results[(row, col)] = {
            "mean":   mean,
            "stddev": stddev,
            "error":  error,
            "passed": passed,
            "n":      len(values),
        }
        total_zones += 1
        if passed:
            passing_zones += 1

    overall_pass = total_zones > 0 and passing_zones == total_zones

    return {
        "target_mm":     target_mm,
        "total_zones":   total_zones,
        "passing_zones": passing_zones,
        "overall_pass":  overall_pass,
        "zones":         zone_results,
    }


def print_result(result: dict):
    target = result["target_mm"]
    passed = result["overall_pass"]
    total  = result["total_zones"]
    good   = result["passing_zones"]

    status = "PASS ✓" if passed else "FAIL ✗"
    print(f"\n{'='*56}")
    print(f"  Target: {target}mm   Valid zones: {good}/{total}   {status}")
    print(f"{'='*56}")

    # Print 8x8 grid of errors
    print(f"  Error grid (mm, + = too far, - = too close):")
    print(f"  " + "  ".join(f"  C{c}" for c in range(8)))
    for row in range(8):
        row_str = f"R{row} "
        for col in range(8):
            z = result["zones"].get((row, col))
            if z:
                err = z["error"]
                ok  = "✓" if z["passed"] else "✗"
                row_str += f" {err:+5.0f}{ok}"
            else:
                row_str += "   ---  "
        print("  " + row_str)

    # Summary stats across all valid zones
    all_errors = [z["error"] for z in result["zones"].values()]
    if all_errors:
        print(f"\n  Mean error:   {statistics.mean(all_errors):+.1f}mm")
        print(f"  Max error:    {max(all_errors, key=abs):+.1f}mm")
        if len(all_errors) > 1:
            print(f"  Stddev:       {statistics.stdev(all_errors):.1f}mm")


def loop():
    global initialized, test_index, results

    # ── Step 1: Initialize sensor ──────────────────────────────────────────────
    if not initialized:
        try:
            print("Initializing VL53L5CX...")
            result = Bridge.call("begin_sensor", timeout=120)
            if result == "ready":
                print("Sensor ready.\n")
                initialized = True
            elif result.startswith("init_failed"):
                print(f"ERROR: Sensor init failed — {result}")
                raise SystemExit(1)
            else:
                print(f"ERROR: Unexpected result: {result}")
                time.sleep(2.0)
        except Exception as e:
            print(f"ERROR: {e}")
            time.sleep(2.0)
        return

    # ── Step 2: Run validation tests ───────────────────────────────────────────
    if test_index >= len(TEST_DISTANCES_MM):
        # All tests done — print summary and exit
        print(f"\n{'='*56}")
        print("  VALIDATION SUMMARY")
        print(f"{'='*56}")
        all_passed = all(r["overall_pass"] for r in results)
        for r in results:
            status = "PASS ✓" if r["overall_pass"] else "FAIL ✗"
            print(f"  {r['target_mm']:4d}mm — {r['passing_zones']}/{r['total_zones']} zones — {status}")
        print(f"{'='*56}")
        print(f"  Overall: {'PASS ✓' if all_passed else 'FAIL ✗'}")
        print(f"{'='*56}\n")
        raise SystemExit(0)

    target = TEST_DISTANCES_MM[test_index]

    # Prompt user
    print(f"\nTest {test_index + 1}/{len(TEST_DISTANCES_MM)}: "
          f"Place flat target at {target}mm from sensor.")
    print(f"Press Enter when ready...")
    input()

    print(f"Collecting {FRAMES} frames...")
    frames = collect_frames(FRAMES)
    result = validate_distance(target, frames)
    results.append(result)
    print_result(result)

    test_index += 1
    time.sleep(0.5)


App.run(user_loop=loop)
