from arduino.app_utils import *
import time

# Scroll timing constants — must match sketch
PIXELS_PER_CHAR   = 6
MS_PER_PIXEL      = 125
SCROLLING_ENABLED = True  # Set to False to disable matrix scrolling in production

def fmt(value, decimals=1):
    """Format a float — drop decimal if zero, otherwise show specified decimal places."""
    if round(value, decimals) == int(value):
        return str(int(value))
    return f"{value:.{decimals}f}"

def scroll_duration(msg):
    """Calculate how long the message takes to scroll once in seconds."""
    return len(msg) * PIXELS_PER_CHAR * MS_PER_PIXEL / 1000

def loop():
    accel_data = Bridge.call("get_lis3dh_data")
    click_data = Bridge.call("get_lis3dh_click")
    freefall   = Bridge.call("get_lis3dh_freefall")

    x = y = z = None

    if accel_data:
        parts = accel_data.split(",")
        x = float(parts[0])
        y = float(parts[1])
        z = float(parts[2])

    # Message 1 — acceleration data
    if x is not None:
        print(f"X:{fmt(x)} Y:{fmt(y)} Z:{fmt(z)} m/s²")
        msg1 = f" X:{fmt(x)} Y:{fmt(y)} Z:{fmt(z)} "

        if SCROLLING_ENABLED:
            Bridge.call("set_matrix_msg", msg1)
            time.sleep(scroll_duration(msg1))

    # Message 2 — tap detection
    if click_data and click_data != "none":
        print(f"Tap: {click_data}")
        msg2 = f" {click_data.capitalize()} Tap! "

        if SCROLLING_ENABLED:
            Bridge.call("set_matrix_msg", msg2)
            time.sleep(scroll_duration(msg2))

    # Message 3 — free fall
    if freefall == "true":
        print("FREE FALL!")
        msg3 = " Free Fall! "

        if SCROLLING_ENABLED:
            Bridge.call("set_matrix_msg", msg3)
            time.sleep(scroll_duration(msg3))

    time.sleep(0.1)

App.run(user_loop=loop)
