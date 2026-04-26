"""
VL53L5CX ToF Imager Python Application
Hybrid RobotiX - Dale Weber (N7PKT)

Reads the 8x8 depth map from the VL53L5CX via the Arduino RouterBridge
and prints each zone's distance in millimeters as a formatted 8x8 grid.

Zone layout (as viewed from the front of the sensor):
    Zone  0 = top-left,  Zone  7 = top-right
    Zone 56 = bot-left,  Zone 63 = bot-right

Distance values:
    > 0    = valid range in mm (up to ~4000 mm)
    0      = data not ready or read error
"""

from arduino.app_utils import *
import time


def parse_depth_map(data: str) -> list[int]:
    """Parse the 64-value CSV string returned by get_vl53l5cx_data."""
    return [int(v) for v in data.split(",")]


def print_depth_map(zones: list[int]) -> None:
    """Pretty-print the 8x8 depth map grid to stdout."""
    print("── VL53L5CX 8×8 Depth Map (mm) ──")
    for row in range(8):
        values = []
        for col in range(8):
            mm = zones[row * 8 + col]
            values.append(f"{mm:5d}" if mm > 0 else "    -")
        print("  " + "  ".join(values))
    print()


def loop():
    time.sleep(0.1)  # ~10 Hz poll — sensor runs at 15 Hz

    result = Bridge.call("get_vl53l5cx_data")

    if not result or result == "0":
        return  # data not ready yet

    zones = parse_depth_map(result)
    print_depth_map(zones)


App.run(user_loop=loop)
