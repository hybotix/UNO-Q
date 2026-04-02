from arduino.app_utils import *
import time

started = False

def loop():
    global started

    # Wait for MCU Bridge to be ready on first run
    if not started:
        time.sleep(5)
        started = True

    scd_data = Bridge.call("get_scd_data")

    if scd_data and scd_data != "0,0,0":
        co2, temp_c, humidity = scd_data.split(",")
        temp_c = float(temp_c)
        temp_f = (temp_c * 9.0 / 5.0) + 32.0
        print(f"{temp_f:.1f}°F ({temp_c:.1f}°C)  {float(humidity):.1f}%  {float(co2):.1f} ppm")
    else:
        print("SCD30: no data")

    time.sleep(5)

App.run(user_loop=loop)
