from arduino.app_utils import *
import time
import math

# Scroll timing constants — must match sketch
PIXELS_PER_CHAR   = 6
MS_PER_PIXEL      = 125
SCROLLING_ENABLED = True  # Set to False to disable matrix scrolling in production

started = False

def scroll_duration(msg):
    """Calculate how long the message takes to scroll once in seconds."""
    return len(msg) * PIXELS_PER_CHAR * MS_PER_PIXEL / 1000

def fmt(value, decimals=1):
    """Format a float — drop decimal if zero, otherwise show specified decimal places."""

    # Return integer string if value has no decimal part
    if round(value, decimals) == int(value):
        return str(int(value))

    return f"{value:.{decimals}f}"

def tilt_description(x, y, z):
    """
    Return a human readable tilt description based on acceleration axes.
    Uses dominant axis to determine orientation.
    """
    ax, ay, az = abs(x), abs(y), abs(z)

    # Z axis dominant — sensor is flat or inverted
    if az > ax and az > ay:
        return "Flat" if z > 0 else "Inverted"
    elif ax > ay:
        return "Left" if x < 0 else "Right"
    else:
        return "Forward" if y < 0 else "Backward"

def loop():
    global started

    # First run — wait for sensor to initialize
    if not started:
        time.sleep(5)
        started = True

    accel_data = Bridge.call("get_lis3dh_data")
    click_data = Bridge.call("get_lis3dh_click")
    freefall   = Bridge.call("get_lis3dh_freefall")

    x = None
    y = None
    z = None

    # Parse accelerometer data
    if accel_data:
        parts = accel_data.split(",")
        x = float(parts[0])
        y = float(parts[1])
        z = float(parts[2])

    # Free fall — highest priority message
    if freefall == "true":
        print("FREE FALL!")
        msg = " FREE FALL! "

        # Scroll the message on the LED matrix
        if SCROLLING_ENABLED:
            Bridge.call("set_matrix_msg", msg)
            time.sleep(scroll_duration(msg))

        return

    # Tap detection
    if click_data and click_data != "none":

        # Double tap detected
        if click_data == "double":
            print("Double tap!")
            msg = " Double Tap! "
        else:
            print("Single tap!")
            msg = " Tap! "

        # Scroll the message on the LED matrix
        if SCROLLING_ENABLED:
            Bridge.call("set_matrix_msg", msg)
            time.sleep(scroll_duration(msg))

    # Message 1 — acceleration data
    if x:
        print(f"X:{x:.4f} Y:{y:.4f} Z:{z:.4f} m/s²")
        msg1 = f" X:{x:.4f} Y:{y:.4f} Z:{z:.4f} "

        # Scroll the message on the LED matrix
        if SCROLLING_ENABLED:
            Bridge.call("set_matrix_msg", msg1)
            time.sleep(scroll_duration(msg1))

    # Message 2 — tilt description
    if x:
        tilt = tilt_description(x, y, z)
        print(f"Tilt: {tilt}")
        msg2 = f" {tilt} "

        # Scroll the message on the LED matrix
        if SCROLLING_ENABLED:
            Bridge.call("set_matrix_msg", msg2)
            time.sleep(scroll_duration(msg2))

App.run(user_loop=loop)
