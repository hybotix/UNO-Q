from arduino.app_utils import *
import time

started = False

def loop():
    global started
    try:
        if not started:
            print("Triggering firmware upload...")
            # begin_sensor() blocks the Bridge during upload — use long timeout
            result = Bridge.call("begin_sensor", timeout=120)
            print("VL53L5CX result: " + result)
            started = True
        else:
            result = Bridge.call("get_diag")
            print("VL53L5CX status: " + result)
        time.sleep(2.0)
    except Exception as e:
        print("Waiting... (" + str(e) + ")")
        time.sleep(2.0)

App.run(user_loop=loop)
