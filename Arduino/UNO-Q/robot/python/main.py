"""
HybX UNO Q Robot — Python Application
Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>

An autonomous Mecanum-wheel robot built on the Arduino UNO Q and the
HybX Development System. This file contains all navigation logic, running
on the Linux side and commanding the sketch via the Arduino RouterBridge.

The robot uses a SparkFun VL53L5CX 8x8 ToF sensor for forward obstacle
detection, an Adafruit BNO055 IMU for absolute heading, and an Adafruit
Motor Shield V2 driving four Mecanum wheels in an X-pattern configuration
for full holonomic motion (forward, reverse, strafe, and rotate).

Architecture:
  All navigation logic runs here on the Linux side. The Arduino sketch
  handles hardware access only — sensor polling, motor PWM, IMU reads.
  Python calls Bridge functions to command the hardware and read data.

State machine:
  INIT        -- initialize VL53L5CX then BNO055 sequentially
  FORWARD     -- drive forward, check sensor every loop iteration
  OBSTACLE    -- obstacle detected: stop, back up, begin scan
  SCANNING    -- rotate CW in SCAN_STEP_DEG increments, check for clear path
  RECOVERING  -- turn to the clear heading found during scan
  FULL_BLOCK  -- no clear path found after full 360° — wait and retry

"Clear path" definition:
  All zones in CENTER_COLS (columns 3 and 4) across FORWARD_ROWS (rows 0-4)
  must have valid target_status AND distance > OBSTACLE_MM.

VL53L5CX zone layout (8x8, physically verified):
  Orientation: SparkFun logo at TOP, lens facing FORWARD.
  Left/right defined from BEHIND the sensor looking forward
  (same direction the robot travels).

  [0][0] upper-left    [0][7] upper-right   row 0 = TOP of FOV
  [4][3] center-left   [4][4] center-right  obstacle detection zone
  [7][0] lower-left    [7][7] lower-right   row 7 = BOTTOM of FOV

  col 0 = robot LEFT    col 7 = robot RIGHT
  col 3-4 = robot CENTER (forward path)

  Verified by hand test on SparkFun large VL53L5CX breakout (SEN-18642).

Tunable constants (top of file):
  OBSTACLE_MM    -- distance threshold to trigger obstacle response (mm)
  BACKUP_MS      -- reverse duration on obstacle detect (ms, ~5cm at speed 128)
  SCAN_STEP_DEG  -- BNO055 heading increment per scan step (degrees)
  TURN_SETTLE_MS -- wait after each rotate command before reading heading (ms)
  CENTER_COLS    -- sensor columns that define the forward path
  FORWARD_ROWS   -- sensor rows checked for obstacle detection

Future: When encoder odometry is added, replace the time-based backup in
handle_obstacle() with a distance-based reverse. The state machine and
all other logic remain unchanged.

Future: When the pan/tilt servo platform is added, replace the whole-robot
rotation in handle_scanning() with pan servo sweeps. The scan state and
clear-path detection logic remain unchanged.
"""

from arduino.app_utils import *
import time

# ── Tunable constants ─────────────────────────────────────────────────────────
RESOLUTION     = "8x8"        # VL53L5CX resolution: "4x4" or "8x8"
WIDTH          = 8             # grid width matching RESOLUTION
OBSTACLE_MM    = 100           # react if any forward center zone <= this (mm)
BACKUP_MS      = 800           # reverse duration after obstacle detect (ms)
                               # tune so robot moves ~5cm at driveSpeed 128
SCAN_STEP_DEG  = 10            # degrees to rotate per scan increment
TURN_SETTLE_MS = 300           # ms to pause after rotate before reading heading
                               # increase if heading reads are unstable
CENTER_COLS    = (3, 4)        # sensor columns defining the forward path
                               # columns 3 and 4 = center of 8x8 FOV
FORWARD_ROWS   = range(0, 5)   # rows 0-4: upper half + center of FOV
                               # rows 5-7 (floor proximity) excluded

# ── VL53L5CX confidence scaling ───────────────────────────────────────────────
# Used for future confidence display — not used in navigation decisions yet.
SIGNAL_MAX = 8000.0   # kcps/SPAD — typical max for good returns at 400kHz
SIGMA_MAX  = 30.0     # mm — anything above this indicates poor measurement

# ── Error step name lookup ────────────────────────────────────────────────────
# Maps hybx_last_error_step codes to human-readable ULD function names.
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

# ── State constants ───────────────────────────────────────────────────────────
STATE_INIT       = "INIT"
STATE_FORWARD    = "FORWARD"
STATE_OBSTACLE   = "OBSTACLE"
STATE_SCANNING   = "SCANNING"
STATE_RECOVERING = "RECOVERING"
STATE_FULL_BLOCK = "FULL_BLOCK"

# ── Runtime state ─────────────────────────────────────────────────────────────
state          = STATE_INIT   # current navigation state
sensor_ready   = False        # True once VL53L5CX is initialized and ranging
imu_ready      = False        # True once BNO055 is initialized
scan_start_hdg = 0.0          # BNO055 heading at the start of a scan
scan_degrees   = 0.0          # total degrees rotated since scan began
clear_heading  = None         # heading where a clear path was found


# ── Sensor data helpers ───────────────────────────────────────────────────────

def parse_int_matrix(data: str) -> list:
    """
    Parse a semicolon-row, comma-column string into a 2D integer list.
    Example: "100,200;300,400" -> [[100, 200], [300, 400]]
    """
    return [[int(v) for v in row.split(",")] for row in data.split(";")]


def parse_bool_matrix(data: str) -> list:
    """
    Parse a semicolon-row, comma-column T/F string into a 2D boolean list.
    "T" -> True (valid reading), anything else -> False.
    Example: "T,F;T,T" -> [[True, False], [True, True]]
    """
    return [[v == "T" for v in row.split(",")] for row in data.split(";")]


def format_error(status: str) -> str:
    """
    Convert an "init_failed:step:code" or "error:step:code" string into
    a human-readable message using the ERROR_STEPS lookup table.
    """
    parts = status.split(":")

    if len(parts) >= 3:
        step_name = ERROR_STEPS.get(parts[1], f"step_{parts[1]}")
        return f"{parts[0]}: {step_name} (ULD code {parts[2]})"

    return status


def get_sensor_data():
    """
    Fetch all four sensor arrays from the sketch via Bridge calls.

    Returns a tuple (dist, stat, signal, sigma) where each is a 2D list,
    or None if the sensor is not ready or a read/parse error occurs.

    dist   -- [[int]]  distance in mm per zone
    stat   -- [[bool]] True = valid reading (status 5 or 9)
    signal -- [[int]]  signal_per_spad (kcps/SPAD)
    sigma  -- [[int]]  range_sigma_mm
    """
    try:
        dist_raw   = Bridge.call("get_distance_data")
        stat_raw   = Bridge.call("get_target_status")
        signal_raw = Bridge.call("get_signal_data")
        sigma_raw  = Bridge.call("get_sigma_data")
    except Exception as e:
        print(f"ERROR: sensor read failed: {e}")
        return None

    # "0" means sensor not ready yet — not an error, just no data yet
    if "0" in (dist_raw, stat_raw, signal_raw, sigma_raw):
        return None

    # "error:" prefix means a runtime ranging failure
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
    Return True if the forward path is clear of obstacles.

    Checks all zones in CENTER_COLS across FORWARD_ROWS.
    A zone fails if:
      - stat[row][col] is False (invalid reading), OR
      - dist[row][col] <= OBSTACLE_MM (obstacle within threshold)

    Both conditions must pass for all checked zones to return True.
    Floor rows (5-7) are excluded — they naturally read close distances.
    """
    for row in FORWARD_ROWS:
        for col in CENTER_COLS:
            if not stat[row][col]:
                # Invalid reading — treat as blocked (fail safe)
                return False

            if dist[row][col] <= OBSTACLE_MM:
                # Obstacle within threshold distance
                return False

    return True


def get_heading() -> float:
    """
    Read the BNO055 Euler heading via Bridge call.

    Returns heading in degrees (0.0-360.0), or -1.0 on error.
    0 degrees = direction the robot faced when powered on / calibrated.

    NOTE: When pan/tilt is added, absolute look direction =
    pan_angle + get_heading(). This function stays unchanged.
    """
    try:
        raw = Bridge.call("get_heading")
        return float(raw)
    except Exception as e:
        print(f"ERROR: get_heading() failed: {e}")
        return -1.0


def heading_diff(target: float, current: float) -> float:
    """
    Compute the shortest signed angular difference: target - current.
    Result is in range (-180, 180].
    Positive = rotate CW, Negative = rotate CCW.

    Example: target=350, current=10 -> diff=-20 (CCW is shorter)
    """
    diff = (target - current + 360.0) % 360.0

    if diff > 180.0:
        diff -= 360.0

    return diff


def drive(command: str):
    """
    Send a drive command to the sketch via Bridge call.
    Valid commands: "forward", "reverse", "left", "right",
                   "rotate_cw", "rotate_ccw", "stop"
    Prints an error if the sketch returns anything other than "ok".
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
    STATE_INIT — initialize hardware sequentially before navigation starts.

    Step 1: Initialize VL53L5CX (firmware upload, ~10 seconds).
    Step 2: Initialize BNO055.

    Transitions to STATE_FORWARD once both are ready.
    Retries with a delay on any failure.
    """
    global sensor_ready, imu_ready, state

    # Step 1: Initialize VL53L5CX if not already ready
    if not sensor_ready:
        print("Initializing VL53L5CX...")
        try:
            # begin_sensor() blocks during firmware upload — use long timeout
            result = Bridge.call("begin_sensor", timeout=120)

            if result.startswith("init_failed"):
                print("ERROR: VL53L5CX init failed: " + format_error(result))
                time.sleep(5.0)
                return

            if result in ("ready", "already_started"):
                res = Bridge.call("set_resolution", RESOLUTION)
                print(f"VL53L5CX ready. Resolution: {res}")
                sensor_ready = True
            else:
                print("ERROR: unexpected response: " + result)
                time.sleep(2.0)
                return

        except Exception as e:
            print(f"ERROR: VL53L5CX init exception: {e}")
            time.sleep(2.0)
            return

    # Step 2: Initialize BNO055 once sensor is ready
    if not imu_ready:
        print("Initializing BNO055...")
        try:
            result = Bridge.call("begin_imu")

            if result in ("ready", "already_started"):
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

    # Both initialized — begin navigation
    print("All systems ready. Starting navigation.")
    state = STATE_FORWARD


def handle_forward():
    """
    STATE_FORWARD — drive forward and continuously check for obstacles.

    Reads sensor every loop iteration (~50ms with sleep).
    Transitions to STATE_OBSTACLE if any forward center zone <= OBSTACLE_MM.
    Continues driving forward if path is clear.
    """
    global state

    data = get_sensor_data()

    if data is None:
        # Sensor not ready or read failed — stop motors until data returns
        drive("stop")
        time.sleep(0.05)
        return

    dist, stat, signal, sigma = data

    if not is_path_clear(dist, stat):
        # Obstacle detected — stop immediately before backing up
        print(f"Obstacle detected at <={OBSTACLE_MM}mm. Stopping.")
        drive("stop")
        state = STATE_OBSTACLE
        return

    # Path is clear — keep driving forward
    drive("forward")
    time.sleep(0.05)  # ~20Hz sensor poll rate


def handle_obstacle():
    """
    STATE_OBSTACLE — respond to a detected obstacle.

    1. Reverse for BACKUP_MS milliseconds to create clearance (~5cm).
    2. Stop and settle briefly.
    3. Snapshot current BNO055 heading as the scan reference.
    4. Transition to STATE_SCANNING.

    NOTE: Replace time-based reverse with distance-based reverse when
    encoder odometry is available. The scan setup below stays unchanged.
    """
    global state, scan_start_hdg, scan_degrees, clear_heading

    # Back up to create clearance before scanning
    print(f"Backing up for {BACKUP_MS}ms...")
    drive("reverse")
    time.sleep(BACKUP_MS / 1000.0)
    drive("stop")
    time.sleep(0.1)  # brief settle before reading heading

    # Record heading at scan start — scan measures rotation from this point
    hdg = get_heading()

    if hdg < 0:
        print("WARNING: IMU read failed before scan start — using 0.0 degrees as reference.")
        hdg = 0.0

    scan_start_hdg = hdg
    scan_degrees   = 0.0
    clear_heading  = None
    print(f"Starting scan from heading {scan_start_hdg:.1f} degrees")
    state = STATE_SCANNING


def handle_scanning():
    """
    STATE_SCANNING — rotate CW in SCAN_STEP_DEG increments and check
    each heading for a clear forward path using the BNO055 for precision.

    Each iteration:
      1. Rotate CW briefly, then stop and wait TURN_SETTLE_MS for heading
         to stabilize before reading.
      2. Read current BNO055 heading and compute total rotation so far.
      3. Read sensor — if path is clear, record heading and recover.
      4. If 360 degrees scanned with no clear path, enter STATE_FULL_BLOCK.

    NOTE: When pan/tilt is added, replace rotate_cw with a pan servo
    increment. scan_start_hdg becomes pan_start_angle. The clear-path
    detection logic below stays unchanged.
    """
    global state, scan_degrees, clear_heading

    # Rotate one step CW, then stop and let heading settle
    drive("rotate_cw")
    time.sleep(TURN_SETTLE_MS / 1000.0)
    drive("stop")
    time.sleep(0.05)  # additional settle before IMU read

    # Read current heading and compute total rotation from scan start
    current_hdg = get_heading()

    if current_hdg < 0:
        # Stop motors while IMU is unavailable — do not continue rotating blind
        drive("stop")
        print("ERROR: IMU read failed during scan — stopping motors, retrying.")
        time.sleep(0.1)
        return

    # Total rotation = how far we've gone from the scan start heading.
    # Modulo 360 handles the 359->0 degree wraparound correctly.
    scan_degrees = (current_hdg - scan_start_hdg + 360.0) % 360.0
    print(f"Scan: {scan_degrees:.1f} degrees rotated, heading {current_hdg:.1f}")

    # Check sensor at this heading for a clear path
    data = get_sensor_data()

    if data is None:
        # Sensor read failed at this heading — log and continue scanning
        print(f"WARNING: sensor read failed at {scan_degrees:.1f} degrees — skipping this heading.")
    else:
        dist, stat, signal, sigma = data

        if is_path_clear(dist, stat):
            # Clear path found — record heading and start recovery turn
            clear_heading = current_hdg
            print(f"Clear path found at heading {clear_heading:.1f} degrees")
            state = STATE_RECOVERING
            return

    # Full 360 degrees scanned with no clear heading found
    if scan_degrees >= 355.0:
        print("Full 360 degrees scanned — no clear path found. Waiting.")
        drive("stop")
        state = STATE_FULL_BLOCK


def handle_recovering():
    """
    STATE_RECOVERING — turn to the clear heading found during scanning.

    Each iteration:
      1. Read current BNO055 heading.
      2. Compute shortest-path angular difference to clear_heading.
      3. If within 5 degree tolerance — stop, resume forward.
      4. Otherwise rotate CW or CCW toward target heading.

    The 5 degree tolerance prevents oscillation around the target heading.
    """
    global state, clear_heading

    current_hdg = get_heading()

    if current_hdg < 0:
        # Stop motors while IMU is unavailable — do not continue turning blind
        drive("stop")
        print("ERROR: IMU read failed during recovery — stopping motors, retrying.")
        time.sleep(0.1)
        return

    # Compute signed shortest-path difference to target heading
    diff = heading_diff(clear_heading, current_hdg)
    print(f"Recovering: target {clear_heading:.1f}, "
          f"current {current_hdg:.1f}, diff {diff:.1f} degrees")

    if abs(diff) <= 5.0:
        # Within tolerance — on heading, resume forward navigation
        drive("stop")
        time.sleep(0.1)
        print("On heading. Resuming forward.")
        clear_heading = None
        state = STATE_FORWARD
        return

    # Not yet on heading — rotate toward target.
    # Positive diff = CW is shorter, Negative diff = CCW is shorter.
    if diff > 0:
        drive("rotate_cw")
    else:
        drive("rotate_ccw")

    time.sleep(TURN_SETTLE_MS / 1000.0)
    drive("stop")
    time.sleep(0.05)  # settle before next heading read


def handle_full_block():
    """
    STATE_FULL_BLOCK — no clear path was found after a full 360 degree scan.

    Wait 2 seconds, then restart a fresh scan from the current heading.
    This handles temporary blockages (e.g. a person walking past).
    """
    global state, scan_start_hdg, scan_degrees, clear_heading

    time.sleep(2.0)  # pause before retrying — environment may have changed
    print("Retrying scan after full block...")

    # Read heading for scan reference — if IMU fails, use 0.0 as fallback
    hdg = get_heading()

    if hdg < 0:
        print("WARNING: IMU read failed before retry scan — using 0.0 degrees as reference.")
        hdg = 0.0

    # Reset scan state and start fresh from current heading
    scan_start_hdg = hdg
    scan_degrees   = 0.0
    clear_heading  = None
    state = STATE_SCANNING


# ── Main loop ─────────────────────────────────────────────────────────────────

def loop():
    """
    Main loop — called repeatedly by App.run().
    Dispatches to the appropriate state handler each iteration.
    State transitions are handled inside each handler function.
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


App.run(user_loop=loop)
