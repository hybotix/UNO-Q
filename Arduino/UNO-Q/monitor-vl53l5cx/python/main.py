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
WIDTH = 4 if RESOLUTION == "4x4" else 8

# Maximum time to wait for sensor firmware upload (~10 s typical)
SENSOR_INIT_TIMEOUT = 30.0

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
init_start  = None


def format_error(status: str) -> str:
    """Format 'init_failed:step:code' or 'error:step:code' for display."""
    parts = status.split(":")
    if len(parts) == 3:
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
        print("  " + "\t".join(f"{v:5d}" for v in row))
    print()


def print_status_matrix(matrix: list) -> None:
    print("── Target Status ──")
    for row in matrix:
        print("  " + "\t".join(str(v) for v in row))
    print()


def loop():
    global initialized, init_start

    if not initialized:
        if init_start is None:
            init_start = time.time()
            print("Waiting for sensor firmware upload...")

        try:
            status = Bridge.call("get_sensor_status", timeout=15)
        except TimeoutError:
            print("ERROR: Bridge timeout — Arduino not responding")
            time.sleep(2.0)
            return

        if status.startswith("init_failed"):
            print("ERROR: Sensor init failed — " + format_error(status))
            print("Check QWIIC connection and sensor power.")
            time.sleep(5.0)
            return

        if status.startswith("error"):
            print("ERROR: Sensor runtime error — " + format_error(status))
            time.sleep(2.0)
            return

        if status == "initializing":
            elapsed = time.time() - init_start
            if elapsed > SENSOR_INIT_TIMEOUT:
                print(f"ERROR: Sensor init timeout after {elapsed:.0f}s")
                print("Check QWIIC connection and sensor power.")
                time.sleep(5.0)
                return
            time.sleep(1.0)
            return

        if status == "ready":
            result = Bridge.call("set_resolution", RESOLUTION)
            print("Sensor ready. Resolution set to: " + result)
            initialized = True
            time.sleep(0.5)
            return

        print("ERROR: Unexpected sensor status: " + status)
        time.sleep(2.0)
        return

    time.sleep(0.1)

    try:
        distance = Bridge.call("get_distance_data")
        status   = Bridge.call("get_target_status")
    except TimeoutError:
        print("ERROR: Bridge timeout reading sensor data")
        return

    if not distance or distance == "0":
        return
    if not status or status == "0":
        return

    try:
        print_distance_matrix(parse_distance_matrix(distance))
        print_status_matrix(parse_status_matrix(status))
    except (ValueError, IndexError) as e:
        print(f"ERROR: Failed to parse sensor data: {e}")
        print(f"  distance='{distance}'")
        print(f"  status='{status}'")


App.run(user_loop=loop)
