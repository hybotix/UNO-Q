"""
SecureSMARS Python Application
Hybrid RobotiX - Dale Weber (N7PKT)

Bridges the Arduino RouterBridge to MQTT broker on pimqtt.
Publishes sensor data and subscribes to motor commands.

MQTT Topics:
    smars/scd  - SCD30 sensor data (CO2, temperature, humidity)
    smars/bno  - BNO055 sensor data (full 9-DoF IMU)
    smars/cmd  - Motor commands (subscribed)

Motor command JSON format:
    {"action": "forward",  "speed": 128}
    {"action": "backward", "speed": 128}
    {"action": "strafe_left",  "speed": 128}
    {"action": "strafe_right", "speed": 128}
    {"action": "rotate_cw",    "speed": 128}
    {"action": "rotate_ccw",   "speed": 128}
    {"action": "diag_fl",  "speed": 128}
    {"action": "diag_fr",  "speed": 128}
    {"action": "diag_rl",  "speed": 128}
    {"action": "diag_rr",  "speed": 128}
    {"action": "move", "x": 100, "y": 100, "r": 0}
    {"action": "stop"}
"""

from arduino.app_utils import *
from secrets import SECRETS
import time
import json
import paho.mqtt.client as mqtt

# ── MQTT connection ───────────────────────────────────────────────────────────
BROKER    = SECRETS["mqtt_broker"]
MQTT_PORT = SECRETS["mqtt_port"]
MQTT_USER = SECRETS["mqtt_user"]
MQTT_PASS = SECRETS["mqtt_pass"]

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.connect(BROKER, MQTT_PORT)
client.loop_start()

# ── Sensor publishing ─────────────────────────────────────────────────────────

def publish_scd(data):
    """Parse SCD30 CSV data and publish to smars/scd as JSON."""
    co2, temp, humidity = data.split(",")
    payload = json.dumps({
        "co2":         float(co2),
        "temperature": float(temp),
        "humidity":    float(humidity)
    })
    client.publish("smars/scd", payload)
    print("SCD: " + payload)

def publish_bno(data):
    """Parse BNO055 CSV data and publish to smars/bno as JSON."""
    values = data.split(",")
    payload = json.dumps({
        # Euler angles
        "heading":          float(values[0]),
        "pitch":            float(values[1]),
        "roll":             float(values[2]),
        # Gyroscope (rad/s)
        "gyro_x":           float(values[3]),
        "gyro_y":           float(values[4]),
        "gyro_z":           float(values[5]),
        # Linear acceleration (m/s^2, gravity removed)
        "linear_accel_x":   float(values[6]),
        "linear_accel_y":   float(values[7]),
        "linear_accel_z":   float(values[8]),
        # Gravity vector (m/s^2)
        "gravity_x":        float(values[9]),
        "gravity_y":        float(values[10]),
        "gravity_z":        float(values[11]),
        # Magnetometer (uT)
        "mag_x":            float(values[12]),
        "mag_y":            float(values[13]),
        "mag_z":            float(values[14]),
        # Raw accelerometer (m/s^2)
        "accel_x":          float(values[15]),
        "accel_y":          float(values[16]),
        "accel_z":          float(values[17]),
        # Quaternion
        "quat_w":           float(values[18]),
        "quat_x":           float(values[19]),
        "quat_y":           float(values[20]),
        "quat_z":           float(values[21]),
        # Calibration status (0-3, 3=fully calibrated)
        "cal_sys":          int(values[22]),
        "cal_gyro":         int(values[23]),
        "cal_accel":        int(values[24]),
        "cal_mag":          int(values[25]),
        # Chip temperature (C)
        "temperature":      float(values[26])
    })
    client.publish("smars/bno", payload)
    print("BNO: " + payload)

# ── Motor control functions ───────────────────────────────────────────────────

def move_forward(speed=128):
    """Drive forward at given speed (0-255)."""
    Bridge.call("move_forward", speed)

def move_backward(speed=128):
    """Drive backward at given speed (0-255)."""
    Bridge.call("move_backward", speed)

def strafe_left(speed=128):
    """Strafe left at given speed (0-255)."""
    Bridge.call("strafe_left", speed)

def strafe_right(speed=128):
    """Strafe right at given speed (0-255)."""
    Bridge.call("strafe_right", speed)

def rotate_cw(speed=128):
    """Rotate clockwise at given speed (0-255)."""
    Bridge.call("rotate_cw", speed)

def rotate_ccw(speed=128):
    """Rotate counter-clockwise at given speed (0-255)."""
    Bridge.call("rotate_ccw", speed)

def move_diagonal_fl(speed=128):
    """Move diagonally forward-left at given speed (0-255)."""
    Bridge.call("move_diagonal_fl", speed)

def move_diagonal_fr(speed=128):
    """Move diagonally forward-right at given speed (0-255)."""
    Bridge.call("move_diagonal_fr", speed)

def move_diagonal_rl(speed=128):
    """Move diagonally rear-left at given speed (0-255)."""
    Bridge.call("move_diagonal_rl", speed)

def move_diagonal_rr(speed=128):
    """Move diagonally rear-right at given speed (0-255)."""
    Bridge.call("move_diagonal_rr", speed)

def mecanum_move(x, y, r):
    """
    Full omnidirectional mecanum move.
    x = strafe  (-255 left,  +255 right)
    y = drive   (-255 back,  +255 forward)
    r = rotate  (-255 CCW,   +255 CW)
    """
    Bridge.call("mecanum_move", x, y, r)

def stop():
    """Stop and release all motors."""
    Bridge.call("stop_motors")

# ── MQTT command handler ──────────────────────────────────────────────────────

def on_command(client, userdata, message):
    """
    Handle incoming motor commands on smars/cmd.
    Parses JSON payload and dispatches to appropriate motor function.
    """
    try:
        cmd    = json.loads(message.payload.decode())
        action = cmd.get("action")
        speed  = cmd.get("speed", 128)

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
            # Full mecanum move with x, y, r components
            x = cmd.get("x", 0)
            y = cmd.get("y", 0)
            r = cmd.get("r", 0)
            mecanum_move(x, y, r)
        elif action == "stop":         stop()

    except Exception as e:
        print("Command error: " + str(e))

# Subscribe to motor command topic
client.subscribe("smars/cmd")
client.on_message = on_command

# ── Main loop ─────────────────────────────────────────────────────────────────

def loop():
    """
    Main application loop.
    Reads sensor data from the Bridge and publishes to MQTT.
    Motor commands are handled asynchronously via on_command callback.
    """
    time.sleep(5)

    # Read and publish SCD30 data
    scd_data = Bridge.call("get_scd30_data")
    if scd_data and scd_data != "0,0,0":
        publish_scd(scd_data)

    # Read and publish BNO055 data
    bno_data = Bridge.call("get_bno055_data")
    if bno_data:
        publish_bno(bno_data)

    time.sleep(2)

App.run(user_loop=loop)
