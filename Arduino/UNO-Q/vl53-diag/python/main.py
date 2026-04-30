from arduino.app_utils import *
import time

def loop():
    try:
        # Firmware upload takes up to 30s — use a long timeout
        result = Bridge.call("get_diag", timeout=60)
        print("VL53L5CX diag result: " + result)
        time.sleep(2.0)
    except Exception as e:
        print("ERROR: " + str(e))
        time.sleep(1.0)

App.run(user_loop=loop)
