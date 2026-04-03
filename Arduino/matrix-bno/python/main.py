from arduino.app_utils import *
import time
import os

# Scroll timing constants — must match sketch
PIXELS_PER_CHAR = 6
MS_PER_PIXEL    = 125

# Calibration flag file — lives in $HOME on the UNO Q
# $HOME is bind-mounted into the container by the start command
CALIBRATION_FILE = os.path.expanduser("~/.scd30-calibrated")

started = False

def compass_point(heading):
    """Return 8-point compass direction for a heading in degrees."""
    points = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
    index = int((heading + 22.5) / 45) % 8
    return points[index]

def scroll_duration(msg):
    """Calculate how long the message takes to scroll once in seconds."""
    return len(msg) * PIXELS_PER_CHAR * MS_PER_PIXEL / 1000

def calibrate():
    """
    Calibrate SCD30 temperature offset using SHT45 as reference.
    Only runs if ~/.scd30-calibrated does not exist.
    Creates the flag file on success to prevent future recalibration.
    """
    if os.path.exists(CALIBRATION_FILE):
        print("SCD30: calibration file found — skipping calibration")
        return

    print("SCD30: calibrating temperature offset using SHT45 reference...")
    cal_msg = " Calibrating SCD-30... "
    Bridge.call("set_matrix_msg", cal_msg)
    time.sleep(scroll_duration(cal_msg))
    result = Bridge.call("calibrate_scd30")
    print(f"SCD30: calibration result: {result}")

    if result and result.startswith("offset:"):
        with open(CALIBRATION_FILE, "w") as f:
            f.write(result)
        print(f"SCD30: calibration complete — {result}")
    elif result == "skipped":
        print("SCD30: offset out of bounds — calibration skipped")
    else:
        print("SCD30: calibration failed")

def loop():
    global started

    if not started:
        time.sleep(5)
        calibrate()
        # Wait for valid SCD30 data before starting scroll
        while True:
            scd_check = Bridge.call("get_scd_data")
            if scd_check and scd_check != "0,0,0":
                break
            time.sleep(1)
        started = True

    scd_data = Bridge.call("get_scd_data")
    sht_data = Bridge.call("get_sht45_data")
    bno_data = Bridge.call("get_bno_data")
    # as7343_data = Bridge.call("get_as7343_data")  # Uncomment when AS7343 connected

    co2      = None
    temp_c   = None
    humidity = None
    heading  = None
    pitch    = None
    roll     = None

    if scd_data and scd_data != "0,0,0":
        co2 = float(scd_data.split(",")[0])

    if sht_data and sht_data != "0,0":
        parts    = sht_data.split(",")
        temp_c   = float(parts[0])
        humidity = float(parts[1])

    if bno_data:
        values  = bno_data.split(",")
        heading = float(values[0])
        pitch   = float(values[1])
        roll    = float(values[2])

    # Skip loop iteration entirely if primary sensor data not ready
    if temp_c is None or co2 is None:
        time.sleep(1)
        return

    # Message 1 — environmental data (always first)
    temp_f = (temp_c * 9.0 / 5.0) + 32.0
    print(f"{temp_f:.1f}\u00b0F ({temp_c:.1f}\u00b0C)  {humidity:.1f}%  {co2:.1f} ppm")
    msg1 = f" {temp_f:.1f}\u00b0F({temp_c:.1f}\u00b0C) {humidity:.1f}% {co2:.1f}ppm "
    Bridge.call("set_matrix_msg", msg1)
    time.sleep(scroll_duration(msg1))

    # Message 2 — orientation data
    if heading is not None:
        cp = compass_point(heading)
        print(f"H{heading:.1f}\u00b0 {cp}  P{pitch:.1f}\u00b0  R{roll:.1f}\u00b0")
        msg2 = f" H{heading:.1f}\u00b0 {cp} P{pitch:.1f}\u00b0 R{roll:.1f}\u00b0 "
        Bridge.call("set_matrix_msg", msg2)
        time.sleep(scroll_duration(msg2))

    # Message 3 — spectral/color data (AS7343, when connected)
    # Uncomment when AS7343 is connected and get_as7343_data is active

App.run(user_loop=loop)
