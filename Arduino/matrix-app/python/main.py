from arduino.app_utils import *
import time

started = False

def loop():
    global started

    if not started:
        time.sleep(5)
        started = True

    scd_data = Bridge.call("get_scd_data")

    if scd_data and scd_data != "0,0,0":
        co2, temp_c, humidity = scd_data.split(",")
        temp_c = float(temp_c)
        temp_f = (temp_c * 9.0 / 5.0) + 32.0
        co2 = float(co2)
        humidity = float(humidity)

        print(f"{temp_f:.1f}\u00b0F ({temp_c:.1f}\u00b0C)  {humidity:.1f}%  {co2:.1f} ppm")

        msg = f" {temp_f:.0f}\u00b0F({temp_c:.0f}\u00b0C) {humidity:.0f}% {co2:.0f}ppm "
        Bridge.call("set_matrix_msg", msg)
    else:
        print("SCD30: no data")

    time.sleep(30)

App.run(user_loop=loop)
