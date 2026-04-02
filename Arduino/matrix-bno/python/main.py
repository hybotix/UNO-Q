from arduino.app_utils import *
import time

# Scroll timing constants — must match sketch
PIXELS_PER_CHAR = 6
MS_PER_PIXEL    = 125

started = False

def compass_point(heading):
    """Return 8-point compass direction for a heading in degrees."""
    points = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
    index = int((heading + 22.5) / 45) % 8
    return points[index]

def scroll_duration(msg):
    """Calculate how long the message takes to scroll once in seconds."""
    return len(msg) * PIXELS_PER_CHAR * MS_PER_PIXEL / 1000

def loop():
    global started

    if not started:
        time.sleep(5)
        started = True

    scd_data  = Bridge.call("get_scd_data")
    sht_data  = Bridge.call("get_sht45_data")
    bno_data  = Bridge.call("get_bno_data")

    co2         = None
    scd_temp_c  = None
    sht_temp_c  = None
    humidity    = None
    heading     = None
    pitch       = None
    roll        = None

    if scd_data and scd_data != "0,0,0":
        parts      = scd_data.split(",")
        co2        = float(parts[0])
        scd_temp_c = float(parts[1])

    if sht_data and sht_data != "0,0":
        parts      = sht_data.split(",")
        sht_temp_c = float(parts[0])
        humidity   = float(parts[1])

    if bno_data:
        values  = bno_data.split(",")
        heading = float(values[0])
        pitch   = float(values[1])
        roll    = float(values[2])

    # Sensor fusion — average temps if both available, else use whichever we have
    if scd_temp_c is not None and sht_temp_c is not None:
        temp_c = (scd_temp_c + sht_temp_c) / 2
        print(f"Temp fusion: SCD={scd_temp_c:.1f}C SHT={sht_temp_c:.1f}C Avg={temp_c:.1f}C")
    elif sht_temp_c is not None:
        temp_c = sht_temp_c
    elif scd_temp_c is not None:
        temp_c = scd_temp_c
    else:
        temp_c = None

    if temp_c is not None and co2 is not None:
        temp_f = (temp_c * 9.0 / 5.0) + 32.0
        print(f"{temp_f:.1f}\u00b0F ({temp_c:.1f}\u00b0C)  {humidity:.1f}%  {co2:.1f} ppm")

        msg1 = f" {temp_f:.0f}\u00b0F({temp_c:.0f}\u00b0C) {humidity:.0f}% {co2:.0f}ppm "
        Bridge.call("set_matrix_msg", msg1)
        time.sleep(scroll_duration(msg1))

    if heading is not None:
        cp = compass_point(heading)
        print(f"H{heading:.1f}\u00b0{cp}  P{pitch:.1f}\u00b0  R{roll:.1f}\u00b0")
        msg2 = f" H{heading:.0f}\u00b0{cp} P{pitch:.0f}\u00b0 R{roll:.0f}\u00b0 "
        Bridge.call("set_matrix_msg", msg2)
        time.sleep(scroll_duration(msg2))

App.run(user_loop=loop)
