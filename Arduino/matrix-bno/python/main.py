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

    scd_data = Bridge.call("get_scd_data")
    bno_data = Bridge.call("get_bno_data")

    if scd_data and scd_data != "0,0,0":
        co2, temp_c, humidity = scd_data.split(",")
        temp_c   = float(temp_c)
        temp_f   = (temp_c * 9.0 / 5.0) + 32.0
        co2      = float(co2)
        humidity = float(humidity)

        heading = 0.0
        if bno_data:
            values = bno_data.split(",")
            heading = float(values[0])

        print(f"{temp_f:.1f}\u00b0F ({temp_c:.1f}\u00b0C)  {humidity:.1f}%  {co2:.1f} ppm  {heading:.1f}\u00b0")

        msg1 = f" {temp_f:.0f}\u00b0F({temp_c:.0f}\u00b0C) {humidity:.0f}% {co2:.0f}ppm "
        Bridge.call("set_matrix_msg", msg1)
        time.sleep(scroll_duration(msg1))

        if bno_data:
            pitch = float(values[1])
            roll  = float(values[2])
            cp    = compass_point(heading)
            print(f"H{heading:.1f}\u00b0{cp}  P{pitch:.1f}\u00b0  R{roll:.1f}\u00b0")
            msg2 = f" H{heading:.0f}\u00b0{cp} P{pitch:.0f}\u00b0 R{roll:.0f}\u00b0 "
            Bridge.call("set_matrix_msg", msg2)
            time.sleep(scroll_duration(msg2))
    else:
        print("SCD30: no data")
        time.sleep(5)

App.run(user_loop=loop)
