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
        co2, temp, humidity = scd_data.split(",")
        print(f"CO2: {float(co2):.1f} ppm  Temp: {float(temp):.1f} C  Humidity: {float(humidity):.1f} %")
    else:
        print("SCD30: no data")

    time.sleep(5)

App.run(user_loop=loop)
