from arduino.app_utils import *
import time

def loop():
    try:
        result = Bridge.call("get_diag")
        print("VL53L5CX diag: " + result)
        if result == "uploading":
            time.sleep(1.0)
        else:
            time.sleep(2.0)
    except Exception as e:
        print("ERROR: " + str(e))
        time.sleep(1.0)

App.run(user_loop=loop)
