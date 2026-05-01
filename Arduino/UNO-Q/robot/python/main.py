"""
Robot Python Application
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

Mecanum-wheel robot with VL53L5CX ToF obstacle detection and
BNO055 IMU-guided scan-and-recover navigation.

Architecture:
  All navigation logic runs here in Python. The Arduino sketch exposes
  hardware control via Bridge functions — Python calls them and makes
  all decisions. The sketch loop stays minimal (just sensor.poll()).

State machine:
  INIT        -- initialize VL53L5CX then BNO055 sequentially
  FORWARD     -- drive forward, check sensor every loop for obstacles
  OBSTACLE    -- stop, backup ~5cm, snapshot heading, enter SCANNING
  SCANNING    -- rotate CW in SCAN_STEP_DEG increments tracked by BNO055,
                 read sensor at each step, look for a clear path
  RECOVERING  -- turn to the clear heading found during scan
  FULL_BLOCK  -- no clear heading after 360° — wait and retry

"Clear path" definition:
  All zones in CENTER_COLS (3 and 4) across FORWARD_ROWS (0-4) must be:
    - Valid (target_status == T)
    - Distance > OBSTACLE_MM

Tunable constants (top of file):
  OBSTACLE_MM     -- react distance in mm (default 100)
  BACKUP_MS       -- reverse duration in ms (~5cm at half speed)
  SCAN_STEP_DEG   -- heading increment per scan step in degrees (default 10)
  TURN_SETTLE_MS  -- ms to wait after rotate before reading BNO055 heading
  CENTER_COLS     -- sensor columns defining the forward path (default 3, 4)
  FORWARD_ROWS    -- sensor rows to check for obstacles (default 0-4)

Future changes:
  - BACKUP_MS: replace with encoder-based distance when odometry is added
  - SCANNING: replace rotate_cw with pan servo commands when pan/tilt is added
"""

from arduino.app_utils import *
import time

# ── Tunable constants ─────────────────────────────────────────────────────────
RESOLUTION    = "8x8"       # VL53L5CX resolution — "4x4" or "8x8"
WIDTH         = 8           # grid width matching RESOLUTION

OBSTACLE_MM   = 100         # stop and react if any center zone ≤ this (mm)
BACKUP_MS     = 800         # ms to reverse before scanning (~5cm at speed 128)
                            # TODO: replace with encoder distance when available

SCAN_STEP_DEG   = 10        # degrees to rotate per scan step
TURN_SETTLE_MS  = 300       # ms to wait after rotate_cw before reading heading
                            # allows robot to settle before IMU reading

CENTER_COLS   = (3, 4)      # columns 3 and 4 = center of forward path
FORWARD_ROWS  = range(0, 5) # rows 0-4: upper and mid-field zones
                            # rows 5-7 (floor zone) excluded from obstacle check

# Confidence scoring constants for VL53L5CX at 400kHz, 8x8
SIGNAL_MAX = 8000.0         # kcps/SPAD — typical max for good surface returns
SIGMA_MAX  = 30.0           # mm — sigma above this threshold = poor reading

# ── State names ───────────────────────────────────────────────────────────────
STATE_INIT       = "INIT"
STATE_FORWARD    = "FORWARD"
STATE_OBSTACLE   = "OBSTACLE"
STATE_SCANNING   = "SCANNING"
STATE_RECOVERING = "RECOVERING"
STATE_FULL_BLOCK = "FULL_BLOCK"

# ── VL53L5CX error step name map ──────────────────────────────────────────────
# Maps ULD step numbers to human-readable function names for error messages
ERROR_STEPS = {
    "0": "none",
    "1": "vl53l5cx_init",
    "2": "vl53l5cx_set_resolution",
    "3": "vl53l5cx_set_ranging_frequency_hz",
    "4": "vl53l5cx_start_ranging",
    "5": "vl53l5cx_stop_ranging",
    "6": "vl53l5cx_check_data_ready",
    "7": "vl53l5cx_get_ranging_data",
}

# ── Navigation state variables ────────────────────────────────────────────────
state           = STATE_INIT  # current state machine state
sensor_ready    = False       # True once VL53L5CX is initialized and ranging
imu_ready       = False       # True once BNO055 is initialized
scan_start_hdg  = 0.0         # BNO055 heading at the start of a scan
scan_degrees    = 0.0         # total degrees rotated during current scan
clear_heading   = None        # heading where a clear path was found (or None)

# ── Sensor data helpers ───────────────────────────────────────────────────────

def parse_int_matrix(data: str) -> list:
    """
    Parse a semicolon-separated string of comma-separated integers
    into a 2D list. Used for distance, signal, and sigma matrices.
    Example: "100,200;300,400" → [[100, 200], [300, 400]]
    """
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def parse_bool_matrix(data: str) -> list:
    """
    Parse a semicolon-separated string of T/F values into a 2D bool list.
    "T" = valid target (ST status 5 or 9), "F" = invalid.
    Example: "T,F;F,T" → [[True, False], [False, True]]
    """
    return [[v == "T" for v in row.split(",")] for row in data.split(";")]


def format_error(status: str) -> str:
    """
    Format a "error:step:code" or "init_failed:step:code" string from
    the sketch into a human-readable message using ERROR_STEPS.
    """
    parts = status.split(":")
    if len(parts) >= 3:
        step_name = ERROR_STEPS.get(parts[1], f"step_{parts[1]}")
        return f"{parts[0]}: {step_name} (ULD code {parts[2]})"
    return status


def get_sensor_data():
    """
    Fetch all four VL53L5CX data matrices from the sketch via Bridge.
    Returns (dist, stat, signal, sigma) tuple, or None on any error.

    All four arrays are 2D lists indexed as [row][col]:
      dist   -- int16  distance in mm
      stat   -- bool   True = valid reading
      signal -- uint32 signal strength (kcps/SPAD)
      sigma  -- uint16 range sigma (mm)
    """
    try:
        dist_raw   = Bridge.call("get_distance_data")
        stat_raw   = Bridge.call("get_target_status")
        signal_raw = Bridge.call("get_signal_data")
        sigma_raw  = Bridge.call("get_sigma_data")
    except Exception as e:
        print(f"ERROR: sensor read failed: {e}")
        return None

    # "0" means no frame ready yet — not an error, just wait
    if "0" in (dist_raw, stat_raw, signal_raw, sigma_raw):
        return None

    # Sensor fault — log and return None
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


def is_path_clear(dist: list, stat: list) -> bool:
    """
    Returns True if the forward path is clear.

    Checks all zones in CENTER_COLS across FORWARD_ROWS:
      - stat[row][col] must be True (valid reading)
      - dist[row][col] must be > OBSTACLE_MM

    If any zone fails either check, the path is considered blocked.
    Floor zones (rows 5-7) are excluded to avoid false positives from
    the ground plane.
    """
    for row in FORWARD_ROWS:
        for col in CENTER_COLS:
            if not stat[row][col]:
                # Invalid reading in center path — treat as blocked
                return False
            if dist[row][col] <= OBSTACLE_MM:
                # Object within react distance
                return False
    return True


def get_heading() -> float:
    """
    Read the absolute heading from the BNO055 via Bridge.
    Returns a float 0.0-360.0, or -1.0 on error.
    0° = magnetic north, increases clockwise.
    """
    try:
        raw = Bridge.call("get_heading")
        return float(raw)
    except Exception:
        return -1.0  # signals IMU read failure to caller


def heading_diff(target: float, current: float) -> float:
    """
    Compute the shortest signed angular difference: target - current.
    Result is in the range (-180, 180]:
      positive = need to rotate CW to reach target
      negative = need to rotate CCW to reach target
    """
    diff = (target - current + 360.0) % 360.0
    if diff > 180.0:
        diff -= 360.0
    return diff


def drive(command: str):
    """
    Send a drive command to the sketch via Bridge.
    Valid commands: "forward", "reverse", "left", "right",
                    "rotate_cw", "rotate_ccw", "stop"
    Logs an error if the sketch returns anything other than "ok".
    """
    try:
        result = Bridge.call("drive", command)
        if result != "ok":
            print(f"ERROR: drive({command}) returned: {result}")
    except Exception as e:
        print(f"ERROR: drive({command}) failed: {e}")


# ── State handlers ────────────────────────────────────────────────────────────

def handle_init():
    """
    STATE_INIT — initialize all hardware before starting navigation.

    Sequence:
      1. Initialize VL53L5CX (begin_sensor — may take up to 120s for firmware upload)
      2. Set resolution to RESOLUTION
      3. Initialize BNO055 (begin_imu)
      4. Transition to STATE_FORWARD

    Any failure causes a retry delay and returns without changing state.
    """
    global sensor_ready, imu_ready, state

    # Step 1: VL53L5CX initialization
    if not sensor_ready:
        print("Initializing VL53L5CX...")
        try:
            # begin_sensor() blocks during firmware upload — use long timeout
            result = Bridge.call("begin_sensor", timeout=120)
            if result.startswith("init_failed"):
                print("ERROR: VL53L5CX init failed: " + format_error(result))
                time.sleep(5.0)  # back off before retry
                return
            if result == "ready":
                # Set resolution and confirm
                res = Bridge.call("set_resolution", RESOLUTION)
                print(f"VL53L5CX ready. Resolution: {res}")
                sensor_ready = True
            else:
                print("ERROR: unexpected sensor status: " + result)
                time.sleep(2.0)
                return
        except Exception as e:
            print(f"ERROR: VL53L5CX init exception: {e}")
            time.sleep(2.0)
            return

    # Step 2: BNO055 initialization
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

    # Both systems ready — start driving
    print("All systems ready. Starting navigation.")
    state = STATE_FORWARD


def handle_forward():
    """
    STATE_FORWARD — drive forward and continuously check for obstacles.

    Reads the VL53L5CX every loop iteration. If is_path_clear() returns
    False, stops immediately and transitions to STATE_OBSTACLE.
    If no frame is available yet, waits 50ms and returns.
    """
    global state

    data = get_sensor_data()
    if data is None:
        # No frame ready — wait and retry
        time.sleep(0.05)
        return

    dist, stat, signal, sigma = data

    if not is_path_clear(dist, stat):
        # Obstacle within OBSTACLE_MM — stop and enter recovery
        print(f"Obstacle detected at ≤{OBSTACLE_MM}mm. Stopping.")
        drive("stop")
        state = STATE_OBSTACLE
        return

    # Path is clear — keep driving forward
    drive("forward")
    time.sleep(0.05)  # 50ms loop rate for forward driving


def handle_obstacle():
    """
    STATE_OBSTACLE — stop, back up ~5cm, then begin a scan.

    Backup is currently time-based (BACKUP_MS at driveSpeed=128).
    TODO: replace with encoder-based distance measurement when
    motor encoders are wired and integrated.

    After backup, snapshots the current BNO055 heading as the scan
    origin, resets scan state, and transitions to STATE_SCANNING.
    """
    global state, scan_start_hdg, scan_degrees, clear_heading

    # Reverse for BACKUP_MS to create clearance before scanning
    print(f"Backing up for {BACKUP_MS}ms...")
    drive("reverse")
    time.sleep(BACKUP_MS / 1000.0)
    drive("stop")
    time.sleep(0.1)  # brief settle before reading heading

    # Snapshot current heading as scan origin (0° reference)
    scan_start_hdg = get_heading()
    scan_degrees   = 0.0    # reset rotation accumulator
    clear_heading  = None   # no clear heading found yet

    print(f"Starting scan from heading {scan_start_hdg:.1f}°")
    state = STATE_SCANNING


def handle_scanning():
    """
    STATE_SCANNING — rotate CW in SCAN_STEP_DEG increments, checking
    for a clear forward path at each step using the BNO055 for precision.

    Process per call:
      1. Rotate CW briefly, wait TURN_SETTLE_MS for robot to settle
      2. Read current heading from BNO055
      3. Compute total degrees rotated from scan origin
      4. Read VL53L5CX — check if forward path is clear
      5. If clear → record heading, transition to STATE_RECOVERING
      6. If 360° scanned with no clear path → STATE_FULL_BLOCK

    Future: when pan/tilt platform is added, this handler will pan
    the sensor rather than rotating the entire robot.
    """
    global state, scan_degrees, clear_heading

    # Rotate CW by approximately one step and let robot settle
    drive("rotate_cw")
    time.sleep(TURN_SETTLE_MS / 1000.0)
    drive("stop")
    time.sleep(0.05)  # additional settle before IMU read

    # Read heading and calculate total rotation from scan origin
    current_hdg = get_heading()
    if current_hdg < 0:
        # IMU read failed — skip this step and retry
        print("ERROR: IMU read failed during scan.")
        time.sleep(0.1)
        return

    # Total rotation is the clockwise difference from scan start
    rotated = (current_hdg - scan_start_hdg + 360.0) % 360.0
    scan_degrees = rotated
    print(f"Scan: {scan_degrees:.1f}° rotated, heading {current_hdg:.1f}°")

    # Check if the sensor sees a clear path at this heading
    data = get_sensor_data()
    if data is not None:
        dist, stat, signal, sigma = data
        if is_path_clear(dist, stat):
            # Found a clear heading — store it and transition to recovery
            clear_heading = current_hdg
            print(f"Clear path found at heading {clear_heading:.1f}°")
            state = STATE_RECOVERING
            return

    # Check for full 360° rotation — 355° allows for IMU jitter near 360°
    if scan_degrees >= 355.0:
        print("Full 360° scanned — no clear path found. Waiting.")
        drive("stop")
        state = STATE_FULL_BLOCK


def handle_recovering():
    """
    STATE_RECOVERING — turn the robot to face the clear heading found
    during the scan, then resume forward navigation.

    Uses BNO055 heading to determine turn direction and completion.
    Turns CW if target is clockwise from current, CCW if counter-clockwise.
    Transitions to STATE_FORWARD once within 5° of the target heading.
    """
    global state, clear_heading

    current_hdg = get_heading()
    if current_hdg < 0:
        # IMU read failed — stay in this state and retry next loop
        print("ERROR: IMU read failed during recovery.")
        time.sleep(0.1)
        return

    # Signed angular difference — positive = rotate CW, negative = CCW
    diff = heading_diff(clear_heading, current_hdg)
    print(f"Recovering: target {clear_heading:.1f}°, "
          f"current {current_hdg:.1f}°, diff {diff:.1f}°")

    if abs(diff) <= 5.0:
        # Within 5° tolerance — on heading, resume forward
        drive("stop")
        time.sleep(0.1)  # brief pause before resuming
        print("On heading. Resuming forward.")
        clear_heading = None  # clear the saved heading
        state = STATE_FORWARD
        return

    # Still off heading — rotate toward target
    if diff > 0:
        drive("rotate_cw")   # need to rotate clockwise
    else:
        drive("rotate_ccw")  # need to rotate counter-clockwise

    time.sleep(TURN_SETTLE_MS / 1000.0)
    drive("stop")
    time.sleep(0.05)  # settle before next heading read


def handle_full_block():
    """
    STATE_FULL_BLOCK — no clear path was found after a full 360° scan.

    Waits 2 seconds, then re-initializes the scan state and retries
    a full scan from the current heading. The robot stays stopped
    while waiting.
    """
    global state, scan_start_hdg, scan_degrees, clear_heading

    print("No clear path found. Waiting 2s before retrying scan...")
    time.sleep(2.0)

    # Re-snapshot current heading as new scan origin and retry
    scan_start_hdg = get_heading()
    scan_degrees   = 0.0
    clear_heading  = None
    print(f"Retrying scan from heading {scan_start_hdg:.1f}°")
    state = STATE_SCANNING


# ── Main loop ─────────────────────────────────────────────────────────────────

def loop():
    """
    Main loop — called repeatedly by App.run().
    Dispatches to the appropriate state handler each iteration.
    All timing and blocking is handled within each handler.
    """
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
    else:
        # Unknown state — should never happen
        print(f"ERROR: unknown state '{state}' — resetting to INIT")
        state = STATE_INIT


App.run(user_loop=loop)
