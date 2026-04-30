"""
VL53L5CX Monitor Python Application
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Reads distance and target status from the VL53L5CX via the Arduino
RouterBridge and displays them as a 4x4 or 8x8 matrix.

Change RESOLUTION to "4x4" or "8x8".
"""

from arduino.app_utils import *
import time

# Set to "4x4" or "8x8"
RESOLUTION = "8x8"
WIDTH      = 4 if RESOLUTION == "4x4" else 8

# Error step names for human-readable reporting
ERROR_STEPS = {
    "0": "none",
    "1": "vl53l5cx_init",
    "2": "vl53l5cx_set_resolution",
    "3": "vl53l5cx_set_ranging_frequency_hz",
    "4": "vl53l5cx_start_ranging",
    "5": "vl53l5cx_stop_ranging",
    "6": "vl53l5cx_check_data_ready",
    "7": "vl53l5cx_get_ranging_data",
    "8": "invalid_resolution",
    "9": "not_initialized",
}

initialized = False


def format_error(status: str) -> str:
    parts = status.split(":")
    if len(parts) >= 3:
        step_name = ERROR_STEPS.get(parts[1], f"step_{parts[1]}")
        return f"{parts[0]}: {step_name} (ULD code {parts[2]})"
    return status


def parse_distance_matrix(data: str) -> list:
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def parse_status_matrix(data: str) -> list:
    return [[v == "T" for v in row.split(",")] for row in data.split(";")]


def print_distance_matrix(matrix: list) -> None:
    print("── Distance (mm) ──")
    for row in matrix:
        print("  " + "  ".join(f"{v:5d}" for v in row))
    print()


def print_status_matrix(matrix: list) -> None:
    print("── Target Status ──")
    for row in matrix:
        print("  " + "  ".join(f"{'True' if v else 'False':5}" for v in row))
    print()


def loop():
    global initialized

    if not initialized:
        try:
            print("Triggering sensor firmware upload...")
            result = Bridge.call("begin_sensor", timeout=120)
            if result.startswith("init_failed"):
                print("ERROR: Sensor init failed — " + format_error(result))
                print("Check QWIIC connection and sensor power.")
                time.sleep(5.0)
                return
            if result == "ready":
                res = Bridge.call("set_resolution", RESOLUTION)
                print("Sensor ready. Resolution: " + res)
                initialized = True
            else:
                print("ERROR: Unexpected result: " + result)
                time.sleep(2.0)
        except Exception as e:
            print("ERROR: " + str(e))
            time.sleep(2.0)
        return

    time.sleep(0.1)

    try:
        distance = Bridge.call("get_distance_data")
        status   = Bridge.call("get_target_status")
    except Exception as e:
        print(f"ERROR: {e}")
        return

    if not distance or distance == "0":
        return
    if distance.startswith("error:"):
        print("ERROR: " + format_error(distance))
        return
    if not status or status == "0":
        return

    try:
        print_distance_matrix(parse_distance_matrix(distance))
        print_status_matrix(parse_status_matrix(status))
    except Exception as e:
        print(f"ERROR parsing data: {e}")


App.run(user_loop=loop)
