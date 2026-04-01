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
        "accel_x": float(values[6]),
        "accel_y": float(values[7]),
        "accel_z": float(values[8])
    })
    client.publish("smars/bno", payload)
    print("BNO: " + payload)

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
