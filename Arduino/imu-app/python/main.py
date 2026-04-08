from arduino.app_utils import *
import time

def loop():
    time.sleep(5)
    result = Bridge.call("get_lsm6dsox_data")
    if result:
        ax, ay, az, gx, gy, gz, temp = result.split(",")
        print("Accel X: " + ax + " m/s^2")
        print("Accel Y: " + ay + " m/s^2")
        print("Accel Z: " + az + " m/s^2")
        print("Gyro X: " + gx + " rad/s")
        print("Gyro Y: " + gy + " rad/s")
        print("Gyro Z: " + gz + " rad/s")
        print("Temp: " + temp + " C")
    time.sleep(2)

App.run(user_loop=loop)
