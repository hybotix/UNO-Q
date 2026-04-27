"""
VL53L5CX Monitor Python Application
Hybrid RobotiX - Dale Weber (N7PKT)

Reads the 8x8 depth map from the VL53L5CX via the Arduino RouterBridge
and prints each frame as a tab-separated 8x8 grid, matching the output
format of the SparkFun Example1_DistanceArray.

Zone layout (as viewed from the front of the sensor):
    Zone  0 = top-left,  Zone  7 = top-right
    Zone 56 = bot-left,  Zone 63 = bot-right

The ST library returns data transposed from the datasheet zone map —
this app applies the same x/y correction as the SparkFun example to
reflect physical reality.
"""

from arduino.app_utils import *
import time

IMAGE_WIDTH = 8


def print_depth_map(zones: list) -> None:
    """
    Pretty-print the 8x8 depth map as tab-separated values.
    Applies the ST transpose correction (increasing y, decreasing x)
    to match the SparkFun Example1_DistanceArray output exactly.
    """
    for y in range(0, IMAGE_WIDTH * IMAGE_WIDTH, IMAGE_WIDTH):
        row = ""
        for x in range(IMAGE_WIDTH - 1, -1, -1):
            row += "\t" + str(zones[x + y])
        print(row)
    print()


def loop():
    time.sleep(0.1)  # ~10 Hz poll — sensor runs at 15 Hz

    result = Bridge.call("get_vl53l5cx_data")

    if not result or result == "0":
        return  # data not ready yet

    zones = [int(v) for v in result.split(",")]
    print_depth_map(zones)


App.run(user_loop=loop)
