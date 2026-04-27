"""
VL53L5CX Monitor Python Application
Hybrid RobotiX - Dale Weber (N7PKT)

Reads distance and target status from the VL53L5CX via the Arduino
RouterBridge and displays them as a 4x4 or 8x8 matrix.

The resolution is set by calling set_resolution() on first loop iteration.
Change RESOLUTION to 16 for 4x4 or 64 for 8x8.
"""

from arduino.app_utils import *
import time

# Set to 16 for 4x4 or 64 for 8x8
RESOLUTION = 64
WIDTH = 4 if RESOLUTION == 16 else 8

initialized = False


def parse_matrix(data: str) -> list:
    """Parse a row-major matrix string (rows=';', cols=',') into a 2D list."""
    return [row.split(",") for row in data.split(";")]


def print_distance_matrix(matrix: list) -> None:
    """Print the distance matrix with mm values."""
    print("── Distance (mm) ──")
    for row in matrix:
        print("  " + "\t".join(f"{int(v):5d}" for v in row))
    print()


def print_status_matrix(matrix: list) -> None:
    """Print the target status matrix as True/False."""
    print("── Target Status ──")
    for row in matrix:
        print("  " + "\t".join("True " if v == "T" else "False" for v in row))
    print()


def loop():
    global initialized

    if not initialized:
        Bridge.call("set_resolution", RESOLUTION)
        initialized = True
        time.sleep(0.5)
        return

    time.sleep(0.1)

    distance = Bridge.call("get_distance_data")
    status   = Bridge.call("get_target_status")

    if not distance or distance == "0":
        return
    if not status or status == "0":
        return

    print_distance_matrix(parse_matrix(distance))
    print_status_matrix(parse_matrix(status))


App.run(user_loop=loop)
