from arduino.app_utils import *
import time

def parse_click(click):
    """Parse click status string into human readable form."""

    # Single tap
    if click == "single":
        return "Single tap!"
    elif click == "double":
        return "Double tap!"

    return None

def loop():
    accel_data  = Bridge.call("get_lis3dh_data")
    click_data  = Bridge.call("get_lis3dh_click")
    freefall    = Bridge.call("get_lis3dh_freefall")

    # Parse accelerometer data
    if accel_data:
        x, y, z = accel_data.split(",")
        print(f"Accel X: {float(x):.4f} m/s²")
        print(f"Accel Y: {float(y):.4f} m/s²")
        print(f"Accel Z: {float(z):.4f} m/s²")

    # Tap event detected
    if click_data:
        tap = parse_click(click_data)

        # Recognized tap type
        if tap:
            print(f"Tap: {tap}")

    # Free fall detected
    if freefall == "true":
        print("FREE FALL DETECTED!")

    time.sleep(0.1)

App.run(user_loop=loop)
