from arduino.app_utils import *
import time

def loop():
    time.sleep(5)
    result = Bridge.call("get_scd30_data")
    if result:
        co2, temp, humidity = result.split(",")
        print("CO2: " + co2 + " ppm")
        print("Temp: " + temp + " C")
        print("Humidity: " + humidity + " %")
    time.sleep(2)

App.run(user_loop=loop)
