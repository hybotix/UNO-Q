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

initialized = False


def parse_distance_matrix(data: str) -> list:
    """Parse row-major matrix string into a 2D list of int."""
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def parse_status_matrix(data: str) -> list:
    """Parse row-major matrix string into a 2D list of bool."""
    return [[v == "T" for v in row.split(",")] for row in data.split(";")]


def print_distance_matrix(matrix: list) -> None:
    """Print the distance matrix with mm values."""
    print("── Distance (mm) ──")
    for row in matrix:
        print("  " + "\t".join(f"{v:5d}" for v in row))
    print()


def print_status_matrix(matrix: list) -> None:
    """Print the target status matrix as True/False."""
    print("── Target Status ──")
    for row in matrix:
        print("  " + "\t".join(str(v) for v in row))
    print()


def loop():
    global initialized

    if not initialized:
        # Wait for sensor firmware upload to complete — vl53.begin() blocks
        # the Bridge for up to 10 s. Poll get_distance_data until we get
        # something other than "0", then set resolution.
        try:
            data = Bridge.call("get_distance_data", timeout=30)
            if data == "0" or not data:
                time.sleep(1.0)
                return
            result = Bridge.call("set_resolution", RESOLUTION)
            print("Resolution set to: " + result)
            initialized = True
            time.sleep(0.5)
        except TimeoutError:
            time.sleep(1.0)
        return

    time.sleep(0.1)

    try:
        distance = Bridge.call("get_distance_data")
        status   = Bridge.call("get_target_status")
    except TimeoutError:
        return

    if not distance or distance == "0":
        return
    if not status or status == "0":
        return

    print_distance_matrix(parse_distance_matrix(distance))
    print_status_matrix(parse_status_matrix(status))


App.run(user_loop=loop)
