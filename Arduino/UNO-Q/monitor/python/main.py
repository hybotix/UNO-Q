"""
VL53L5CX Monitor Python Application
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Reads distance, target status, signal, sigma from the VL53L5CX and
displays them as an 8x8 matrix with per-zone confidence values.

Confidence formula:
  - target_status not 5 or 9 → 0.00%
  - signal_score = min(signal_per_spad / SIGNAL_MAX, 1.0)
  - sigma_score  = max(0, 1 - range_sigma_mm / SIGMA_MAX)
  - confidence   = (signal_score * 0.6 + sigma_score * 0.4) * 99.99
"""

from hybx_app import Bridge, App
import time

RESOLUTION = "8x8"
WIDTH      = 4 if RESOLUTION == "4x4" else 8

# Confidence scaling constants (tuned for VL53L5CX at 400kHz, 8x8)
SIGNAL_MAX = 8000.0   # kcps/SPAD — typical max for good returns
SIGMA_MAX  = 30.0     # mm — anything above this is poor

ERROR_STEPS = {
    "0": "none", "1": "vl53l5cx_init", "2": "vl53l5cx_set_resolution",
    "3": "vl53l5cx_set_ranging_frequency_hz", "4": "vl53l5cx_start_ranging",
    "5": "vl53l5cx_stop_ranging", "6": "vl53l5cx_check_data_ready",
    "7": "vl53l5cx_get_ranging_data",
}

initialized = False


def parse_int_matrix(data: str) -> list:
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def parse_bool_matrix(data: str) -> list:
    return [[v == "T" for v in row.split(",")] for row in data.split(";")]


def confidence(status: bool, signal: int, sigma: int) -> float:
    if not status:
        return 0.0
    sig_score = min(signal / SIGNAL_MAX, 1.0)
    sig_score = max(sig_score, 0.0)
    sma_score = max(0.0, 1.0 - sigma / SIGMA_MAX)
    return min((sig_score * 0.6 + sma_score * 0.4) * 99.99, 99.99)


def print_distance(dist):
    print("── Distance (mm) ──")
    for row in dist:
        print("  " + "  ".join(f"{v:5d}" for v in row))
    print()


def print_confidence(dist, stat, signal, sigma):
    print("── Confidence (%) ──")
    for r in range(8):
        vals = []
        for c in range(8):
            valid = stat[r][c]
            conf  = confidence(valid, signal[r][c], sigma[r][c])
            vals.append(f"{conf:6.2f}")
        print("  " + "  ".join(vals))
    print()


def format_error(status: str) -> str:
    parts = status.split(":")
    if len(parts) >= 3:
        step_name = ERROR_STEPS.get(parts[1], f"step_{parts[1]}")
        return f"{parts[0]}: {step_name} (ULD code {parts[2]})"
    return status


def loop():
    global initialized

    if not initialized:
        try:
            print("Triggering sensor firmware upload...")
            result = Bridge.call("begin_sensor", timeout=120)
            if result.startswith("init_failed"):
                print("ERROR: " + format_error(result))
                time.sleep(5.0)
                return
            if result in ("ready", "already_started"):
                res = Bridge.call("set_resolution", RESOLUTION)
                print("Sensor ready. Resolution: " + res)
                initialized = True
            else:
                print("ERROR: " + result)
                time.sleep(2.0)
        except Exception as e:
            print("ERROR: " + str(e))
            time.sleep(2.0)
        return

    time.sleep(0.1)

    try:
        dist_raw   = Bridge.call("get_distance_data")
        stat_raw   = Bridge.call("get_target_status")
        signal_raw = Bridge.call("get_signal_data")
        sigma_raw  = Bridge.call("get_sigma_data")
    except Exception as e:
        print(f"ERROR: {e}")
        return

    if "0" in (dist_raw, stat_raw, signal_raw, sigma_raw):
        return
    if dist_raw.startswith("error:"):
        print("ERROR: " + format_error(dist_raw))
        return

    try:
        dist   = parse_int_matrix(dist_raw)
        stat   = parse_bool_matrix(stat_raw)
        signal = parse_int_matrix(signal_raw)
        sigma  = parse_int_matrix(sigma_raw)
        print_distance(dist)
        print_confidence(dist, stat, signal, sigma)
    except Exception as e:
        print(f"ERROR parsing: {e}")


App.run(user_loop=loop)
