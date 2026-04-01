from arduino.app_utils import *
from secrets import SECRETS
import time
import json
import paho.mqtt.client as mqtt

BROKER = SECRETS["mqtt_broker"]
MQTT_PORT = SECRETS["mqtt_port"]
MQTT_USER = SECRETS["mqtt_user"]
MQTT_PASS = SECRETS["mqtt_pass"]

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.connect(BROKER, MQTT_PORT)
client.loop_start()

def publish_scd(data):
    co2, temp, humidity = data.split(",")
    payload = json.dumps({
        "co2": float(co2),
        "temperature": float(temp),
        "humidity": float(humidity)
    })
    client.publish("smars/scd", payload)
    print("SCD: " + payload)

def publish_bno(data):
    values = data.split(",")
    payload = json.dumps({
        "heading": float(values[0]),
        "pitch": float(values[1]),
        "roll": float(values[2]),
        "gyro_x": float(values[3]),
        "gyro_y": float(values[4]),
        "gyro_z": float(values[5]),
        "linear_accel_x": float(values[6]),
        "linear_accel_y": float(values[7]),
        "linear_accel_z": float(values[8]),
        "gravity_x": float(values[9]),
        "gravity_y": float(values[10]),
        "gravity_z": float(values[11]),
        "mag_x": float(values[12]),
        "mag_y": float(values[13]),
        "mag_z": float(values[14]),
        "accel_x": float(values[15]),
        "accel_y": float(values[16]),
        "accel_z": float(values[17]),
        "quat_w": float(values[18]),
        "quat_x": float(values[19]),
        "quat_y": float(values[20]),
        "quat_z": float(values[21]),
        "cal_sys": int(values[22]),
        "cal_gyro": int(values[23]),
        "cal_accel": int(values[24]),
        "cal_mag": int(values[25]),
        "temperature": float(values[26])
    })
    client.publish("smars/bno", payload)
    print("BNO: " + payload)

def move_forward(speed=128):
    Bridge.call("move_forward", speed)

def move_backward(speed=128):
    Bridge.call("move_backward", speed)

def strafe_left(speed=128):
    Bridge.call("strafe_left", speed)

def strafe_right(speed=128):
    Bridge.call("strafe_right", speed)

def rotate_cw(speed=128):
    Bridge.call("rotate_cw", speed)

def rotate_ccw(speed=128):
    Bridge.call("rotate_ccw", speed)

def move_diagonal_fl(speed=128):
    Bridge.call("move_diagonal_fl", speed)

def move_diagonal_fr(speed=128):
    Bridge.call("move_diagonal_fr", speed)

def move_diagonal_rl(speed=128):
    Bridge.call("move_diagonal_rl", speed)

def move_diagonal_rr(speed=128):
    Bridge.call("move_diagonal_rr", speed)

def mecanum_move(x, y, r):
    Bridge.call("mecanum_move", x, y, r)

def stop():
    Bridge.call("stop_motors")

def on_command(client, userdata, message):
    try:
        cmd = json.loads(message.payload.decode())
        action = cmd.get("action")
        speed = cmd.get("speed", 128)
        if action == "forward":        move_forward(speed)
        elif action == "backward":     move_backward(speed)
        elif action == "strafe_left":  strafe_left(speed)
        elif action == "strafe_right": strafe_right(speed)
        elif action == "rotate_cw":    rotate_cw(speed)
        elif action == "rotate_ccw":   rotate_ccw(speed)
        elif action == "diag_fl":      move_diagonal_fl(speed)
        elif action == "diag_fr":      move_diagonal_fr(speed)
        elif action == "diag_rl":      move_diagonal_rl(speed)
        elif action == "diag_rr":      move_diagonal_rr(speed)
        elif action == "move":
            x = cmd.get("x", 0)
            y = cmd.get("y", 0)
            r = cmd.get("r", 0)
            mecanum_move(x, y, r)
        elif action == "stop":         stop()
    except Exception as e:
        print("Command error: " + str(e))

client.subscribe("smars/cmd")
client.on_message = on_command

def loop():
    time.sleep(5)

    scd_data = Bridge.call("get_scd_data")
    if scd_data and scd_data != "0,0,0":
        publish_scd(scd_data)

    bno_data = Bridge.call("get_bno_data")
    if bno_data:
        publish_bno(bno_data)

    time.sleep(2)

App.run(user_loop=loop)
