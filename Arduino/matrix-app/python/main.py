from arduino.app_utils import *
import time
import json

def loop():
    scd_data = Bridge.call("get_scd_data")
    bno_data = Bridge.call("get_bno_data")

    if scd_data and scd_data != "0,0,0":
        co2, temp, humidity = scd_data.split(",")
        print(f"CO2: {float(co2):.1f} ppm  Temp: {float(temp):.1f} C  Humidity: {float(humidity):.1f} %")
    else:
        print("SCD30: no data")

    if bno_data:
        values = bno_data.split(",")
        print(f"Heading: {float(values[0]):.1f} deg  Pitch: {float(values[1]):.1f}  Roll: {float(values[2]):.1f}")
    else:
        print("BNO055: no data")

    time.sleep(5)

App.run(user_loop=loop)
