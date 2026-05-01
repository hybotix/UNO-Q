"""
Robot Python Application
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Mecanum-wheel robot with VL53L5CX ToF obstacle detection and
BNO055 IMU-guided scan-and-recover navigation.

Navigation behavior:
  FORWARD     -- drive forward until obstacle detected
  OBSTACLE    -- stop, back up BACKUP_DISTANCE_MM, then scan
  SCANNING    -- rotate CW in 10° increments via BNO055 heading,
                 check sensor at each step for a clear path
  RECOVERING  -- turn to the clear heading found during scan
  FULL_BLOCK  -- no clear heading found after 360° — stop and wait

"Clear" is defined as: all center columns (3 and 4) in the forward
rows (0-4) reading > OBSTACLE_MM, with valid target status.

Tunable constants:
  OBSTACLE_MM       -- react distance (mm)
  BACKUP_MS         -- backup duration (ms) — swap for odometry later
  SCAN_STEP_DEG     -- heading increment per scan step (degrees)
  TURN_SETTLE_MS    -- wait after each rotate command before reading heading
  CENTER_COLS       -- which sensor columns define "forward path"
  FORWARD_ROWS      -- which sensor rows to check for obstacles
"""

from arduino.app_utils import *
import time

# ── Tunable constants ─────────────────────────────────────────────────────────
RESOLUTION      = "8x8"
WIDTH           = 8
OBSTACLE_MM     = 100       # react if any center zone ≤ this
BACKUP_MS       = 800       # ms to reverse before scanning (tune to ~5cm)
SCAN_STEP_DEG   = 10        # degrees per scan increment
TURN_SETTLE_MS  = 300       # ms to wait after rotate command before reading heading
CENTER_COLS     = (3, 4)    # columns defining the forward path
FORWARD_ROWS    = range(0, 5)  # rows 0-4: upper and center zones

# Confidence constants
SIGNAL_MAX = 8000.0
SIGMA_MAX  = 30.0

# ── States ────────────────────────────────────────────────────────────────────
STATE_INIT       = "INIT"
STATE_FORWARD    = "FORWARD"
STATE_OBSTACLE   = "OBSTACLE"
STATE_SCANNING   = "SCANNING"
STATE_RECOVERING = "RECOVERING"
STATE_FULL_BLOCK = "FULL_BLOCK"

ERROR_STEPS = {
    "0": "none", "1": "vl53l5cx_init", "2": "vl53l5cx_set_resolution",
    "3": "vl53l5cx_set_ranging_frequency_hz", "4": "vl53l5cx_start_ranging",
    "5": "vl53l5cx_stop_ranging", "6": "vl53l5cx_check_data_ready",
    "7": "vl53l5cx_get_ranging_data",
}

# ── State ─────────────────────────────────────────────────────────────────────
state           = STATE_INIT
sensor_ready    = False
imu_ready       = False
scan_start_hdg  = 0.0    # heading at start of scan
scan_degrees    = 0.0    # total degrees scanned so far
clear_heading   = None   # heading where clear path was found

# ── Helpers ───────────────────────────────────────────────────────────────────

def parse_int_matrix(data: str) -> list:
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def parse_bool_matrix(data: str) -> list:
    return [[v == "T" for v in row.split(",")] for row in data.split(";")]


def format_error(status: str) -> str:
    parts = status.split(":")
    if len(parts) >= 3:
        step_name = ERROR_STEPS.get(parts[1], f"step_{parts[1]}")
        return f"{parts[0]}: {step_name} (ULD code {parts[2]})"
    return status


def get_sensor_data():
    """Fetch all four sensor arrays. Returns (dist, stat, signal, sigma) or None on error."""
    try:
        dist_raw   = Bridge.call("get_distance_data")
        stat_raw   = Bridge.call("get_target_status")
        signal_raw = Bridge.call("get_signal_data")
        sigma_raw  = Bridge.call("get_sigma_data")
    except Exception as e:
        print(f"ERROR: sensor read failed: {e}")
        return None

    if "0" in (dist_raw, stat_raw, signal_raw, sigma_raw):
        return None
    if dist_raw.startswith("error:"):
        print("ERROR: " + format_error(dist_raw))
        return None

    try:
        dist   = parse_int_matrix(dist_raw)
        stat   = parse_bool_matrix(stat_raw)
        signal = parse_int_matrix(signal_raw)
        sigma  = parse_int_matrix(sigma_raw)
        return dist, stat, signal, sigma
    except Exception as e:
        print(f"ERROR parsing sensor data: {e}")
        return None


def is_path_clear(dist, stat) -> bool:
    """
    Returns True if all center columns in forward rows are valid and
    reading greater than OBSTACLE_MM.
    """
    for row in FORWARD_ROWS:
        for col in CENTER_COLS:
            if not stat[row][col]:
                return False
            if dist[row][col] <= OBSTACLE_MM:
                return False
    return True


def get_heading() -> float:
    """Read BNO055 heading (0.0-360.0). Returns -1.0 on error."""
    try:
        raw = Bridge.call("get_heading")
        return float(raw)
    except Exception:
        return -1.0


def heading_diff(target: float, current: float) -> float:
    """Shortest signed angular difference target - current, range (-180, 180]."""
    diff = (target - current + 360.0) % 360.0
    if diff > 180.0:
        diff -= 360.0
    return diff


def drive(command: str):
    try:
        result = Bridge.call("drive", command)
        if result != "ok":
            print(f"ERROR: drive({command}) returned: {result}")
    except Exception as e:
        print(f"ERROR: drive({command}) failed: {e}")


# ── State handlers ─────────────────────────────────────────────────────────────

def handle_init():
    global sensor_ready, imu_ready, state

    if not sensor_ready:
        print("Initializing VL53L5CX...")
        try:
            result = Bridge.call("begin_sensor", timeout=120)
            if result.startswith("init_failed"):
                print("ERROR: VL53L5CX init failed: " + format_error(result))
                time.sleep(5.0)
                return
            if result == "ready":
                res = Bridge.call("set_resolution", RESOLUTION)
                print(f"VL53L5CX ready. Resolution: {res}")
                sensor_ready = True
            else:
                print("ERROR: " + result)
                time.sleep(2.0)
                return
        except Exception as e:
            print(f"ERROR: VL53L5CX init exception: {e}")
            time.sleep(2.0)
            return

    if not imu_ready:
        print("Initializing BNO055...")
        try:
            result = Bridge.call("begin_imu")
            if result == "ready":
                print("BNO055 ready.")
                imu_ready = True
            else:
                print(f"ERROR: BNO055 init failed: {result}")
                time.sleep(2.0)
                return
        except Exception as e:
            print(f"ERROR: BNO055 init exception: {e}")
            time.sleep(2.0)
            return

    print("All systems ready. Starting navigation.")
    state = STATE_FORWARD


def handle_forward():
    global state

    data = get_sensor_data()
    if data is None:
        time.sleep(0.05)
        return

    dist, stat, signal, sigma = data

    if not is_path_clear(dist, stat):
        print(f"Obstacle detected at ≤{OBSTACLE_MM}mm. Stopping.")
        drive("stop")
        state = STATE_OBSTACLE
        return

    drive("forward")
    time.sleep(0.05)


def handle_obstacle():
    global state, scan_start_hdg, scan_degrees, clear_heading

    print(f"Backing up for {BACKUP_MS}ms...")
    drive("reverse")
    time.sleep(BACKUP_MS / 1000.0)
    drive("stop")
    time.sleep(0.1)

    scan_start_hdg = get_heading()
    scan_degrees   = 0.0
    clear_heading  = None
    print(f"Starting scan from heading {scan_start_hdg:.1f}°")
    state = STATE_SCANNING


def handle_scanning():
    global state, scan_degrees, clear_heading

    # Rotate CW one step
    drive("rotate_cw")
    time.sleep(TURN_SETTLE_MS / 1000.0)
    drive("stop")
    time.sleep(0.05)

    # Read current heading
    current_hdg = get_heading()
    if current_hdg < 0:
        print("ERROR: IMU read failed during scan.")
        time.sleep(0.1)
        return

    # Calculate how far we've rotated
    rotated = (current_hdg - scan_start_hdg + 360.0) % 360.0
    scan_degrees = rotated
    print(f"Scan: {scan_degrees:.1f}° rotated, heading {current_hdg:.1f}°")

    # Read sensor
    data = get_sensor_data()
    if data is not None:
        dist, stat, signal, sigma = data
        if is_path_clear(dist, stat):
            clear_heading = current_hdg
            print(f"Clear path found at heading {clear_heading:.1f}°")
            state = STATE_RECOVERING
            return

    # Full 360° scanned with no clear path
    if scan_degrees >= 355.0:
        print("Full 360° scanned — no clear path found. Waiting.")
        drive("stop")
        state = STATE_FULL_BLOCK


def handle_recovering():
    global state, clear_heading

    current_hdg = get_heading()
    if current_hdg < 0:
        print("ERROR: IMU read failed during recovery.")
        time.sleep(0.1)
        return

    diff = heading_diff(clear_heading, current_hdg)
    print(f"Recovering: target {clear_heading:.1f}°, current {current_hdg:.1f}°, diff {diff:.1f}°")

    if abs(diff) <= 5.0:
        # On heading — go forward
        drive("stop")
        time.sleep(0.1)
        print("On heading. Resuming forward.")
        clear_heading = None
        state = STATE_FORWARD
        return

    # Still turning — rotate toward target
    if diff > 0:
        drive("rotate_cw")
    else:
        drive("rotate_ccw")
    time.sleep(TURN_SETTLE_MS / 1000.0)
    drive("stop")
    time.sleep(0.05)


def handle_full_block():
    """No clear path found. Re-scan periodically."""
    global state, scan_start_hdg, scan_degrees, clear_heading

    time.sleep(2.0)
    print("Retrying scan...")
    scan_start_hdg = get_heading()
    scan_degrees   = 0.0
    clear_heading  = None
    state = STATE_SCANNING


# ── Main loop ─────────────────────────────────────────────────────────────────

def loop():
    global state

    if state == STATE_INIT:
        handle_init()
    elif state == STATE_FORWARD:
        handle_forward()
    elif state == STATE_OBSTACLE:
        handle_obstacle()
    elif state == STATE_SCANNING:
        handle_scanning()
    elif state == STATE_RECOVERING:
        handle_recovering()
    elif state == STATE_FULL_BLOCK:
        handle_full_block()


App.run(user_loop=loop)
