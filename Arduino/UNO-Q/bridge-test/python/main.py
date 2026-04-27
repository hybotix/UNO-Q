from arduino.app_utils import *
import time

def loop():
    time.sleep(1)
    result = Bridge.call("echo", "hello")
    print("Echo: " + str(result))

App.run(user_loop=loop)
