/**
 * SecureSMARS Sketch
 * Hybrid RobotiX
 *
 * Provides the following functionality via Arduino RouterBridge:
 *   - SCD30: CO2, temperature, humidity via QWIIC (Wire1)
 *   - BNO055: Full 9-DoF IMU data via QWIIC (Wire1)
 *   - Motor control: Mecanum drive via Adafruit Motor Shield V2
 *   - LED Matrix: Scrolling sensor data display
 *
 * MQTT topics (published by Python side):
 *   smars/scd  - SCD30 sensor data
 *   smars/bno  - BNO055 sensor data
 *   smars/cmd  - Motor commands (subscribed)
 *
 * Motor layout (Adafruit Motor Shield V2):
 *   M1 - Front Left
 *   M2 - Front Right
 *   M3 - Rear Left
 *   M4 - Rear Right
 *
 * Mecanum move(x, y, r):
 *   x = strafe  (-255 left,    +255 right)
 *   y = drive   (-255 back,    +255 forward)
 *   r = rotate  (-255 CCW,     +255 CW)
 *
 * LED Matrix scrolls continuously:
 *   Temperature F -> Temperature C -> CO2 ppm -> Humidity %
 */

#include <Arduino_RouterBridge.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_MotorShield.h>
#include <Arduino_LED_Matrix.h>
#include <ArduinoGraphics.h>
#include <utility/imumaths.h>
#include <Wire.h>

// ── Sensor instances ──────────────────────────────────────────────────────────
Adafruit_SCD30 scd30;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);

// ── Motor shield instance ─────────────────────────────────────────────────────
Adafruit_MotorShield shield = Adafruit_MotorShield();

// ── LED matrix instance ───────────────────────────────────────────────────────
ArduinoLEDMatrix matrix;

// ── Motor port assignments ────────────────────────────────────────────────────
Adafruit_DCMotor *motor_fl = shield.getMotor(1); // Front Left
Adafruit_DCMotor *motor_fr = shield.getMotor(2); // Front Right
Adafruit_DCMotor *motor_rl = shield.getMotor(3); // Rear Left
Adafruit_DCMotor *motor_rr = shield.getMotor(4); // Rear Right

// ── Last known sensor values for LED matrix display ──────────────────────────
float last_co2 = 0;
float last_temp_c = 0;
float last_humidity = 0;

// ── Motor control helpers ─────────────────────────────────────────────────────

/**
 * Set a single motor speed and direction.
 * speed: -255 (full reverse) to +255 (full forward), 0 = stop
 */
void set_motor(Adafruit_DCMotor *motor, int speed) {
    if (speed > 0) {
        motor->setSpeed(min(speed, 255));
        motor->run(FORWARD);
    } else if (speed < 0) {
        motor->setSpeed(min(-speed, 255));
        motor->run(BACKWARD);
    } else {
        motor->setSpeed(0);
        motor->run(RELEASE);
    }
}

/**
 * Set individual motor by ID (1-4).
 * motor_id: 1=FL, 2=FR, 3=RL, 4=RR
 */
void set_motor(int motor_id, int speed) {
    switch (motor_id) {
        case 1: set_motor(motor_fl, speed); break;
        case 2: set_motor(motor_fr, speed); break;
        case 3: set_motor(motor_rl, speed); break;
        case 4: set_motor(motor_rr, speed); break;
    }
}

/**
 * Stop and release all four motors.
 */
void stop_motors() {
    motor_fl->run(RELEASE);
    motor_fr->run(RELEASE);
    motor_rl->run(RELEASE);
    motor_rr->run(RELEASE);
}

/**
 * Mecanum omnidirectional drive.
 * x = strafe  (-255 left,    +255 right)
 * y = drive   (-255 back,    +255 forward)
 * r = rotate  (-255 CCW,     +255 CW)
 * Normalizes output to max 255 per motor.
 */
void mecanum_move(int x, int y, int r) {
    int fl = y + x + r;
    int fr = y - x - r;
    int rl = y - x + r;
    int rr = y + x - r;

    // Normalize if any motor exceeds max speed
    int max_val = max(max(abs(fl), abs(fr)), max(abs(rl), abs(rr)));
    if (max_val > 255) {
        fl = fl * 255 / max_val;
        fr = fr * 255 / max_val;
        rl = rl * 255 / max_val;
        rr = rr * 255 / max_val;
    }

    set_motor(motor_fl, fl);
    set_motor(motor_fr, fr);
    set_motor(motor_rl, rl);
    set_motor(motor_rr, rr);
}

// ── Bridge wrappers for motor commands ────────────────────────────────────────
void bridge_set_motor(int motor_id, int speed) { set_motor(motor_id, speed); }
void bridge_stop_motors() { stop_motors(); }
void bridge_move_forward(int speed) { mecanum_move(0, speed, 0); }
void bridge_move_backward(int speed) { mecanum_move(0, -speed, 0); }
void bridge_strafe_left(int speed) { mecanum_move(-speed, 0, 0); }
void bridge_strafe_right(int speed) { mecanum_move(speed, 0, 0); }
void bridge_rotate_cw(int speed) { mecanum_move(0, 0, speed); }
void bridge_rotate_ccw(int speed) { mecanum_move(0, 0, -speed); }
void bridge_move_diagonal_fl(int speed) { mecanum_move(-speed, speed, 0); }
void bridge_move_diagonal_fr(int speed) { mecanum_move(speed, speed, 0); }
void bridge_move_diagonal_rl(int speed) { mecanum_move(-speed, -speed, 0); }
void bridge_move_diagonal_rr(int speed) { mecanum_move(speed, -speed, 0); }
void bridge_mecanum_move(int x, int y, int r) { mecanum_move(x, y, r); }

// ── Sensor data functions ─────────────────────────────────────────────────────

/**
 * Read SCD30 and return CO2, temperature, humidity as CSV string.
 * Also updates last_co2, last_temp_c, last_humidity for LED matrix display.
 * Returns "0,0,0" if data not ready.
 */
String get_scd30_data() {
    if (scd30.dataReady()) {
        scd30.read();
        last_co2 = scd30.CO2;
        last_temp_c = scd30.temperature;
        last_humidity = scd30.relative_humidity;
        return String(last_co2) + "," +
               String(last_temp_c) + "," +
               String(last_humidity);
    }
    return "0,0,0";
}

/**
 * Read all BNO055 data and return as CSV string.
 * Order: heading, pitch, roll, gyro xyz, linear accel xyz,
 *        gravity xyz, mag xyz, accel xyz, quat wxyz,
 *        cal sys/gyro/accel/mag, temperature
 */
String get_bno055_data() {
    sensors_event_t orientation_data, ang_velocity_data, linear_accel_data, gravity_data, mag_data, accel_data;
    bno.getEvent(&orientation_data, Adafruit_BNO055::VECTOR_EULER);
    bno.getEvent(&ang_velocity_data, Adafruit_BNO055::VECTOR_GYROSCOPE);
    bno.getEvent(&linear_accel_data, Adafruit_BNO055::VECTOR_LINEARACCEL);
    bno.getEvent(&gravity_data, Adafruit_BNO055::VECTOR_GRAVITY);
    bno.getEvent(&mag_data, Adafruit_BNO055::VECTOR_MAGNETOMETER);
    bno.getEvent(&accel_data, Adafruit_BNO055::VECTOR_ACCELEROMETER);

    imu::Quaternion quat = bno.getQuat();

    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);

    int8_t temp = bno.getTemp();

    return String(orientation_data.orientation.x) + "," +
           String(orientation_data.orientation.y) + "," +
           String(orientation_data.orientation.z) + "," +
           String(ang_velocity_data.gyro.x) + "," +
           String(ang_velocity_data.gyro.y) + "," +
           String(ang_velocity_data.gyro.z) + "," +
           String(linear_accel_data.acceleration.x) + "," +
           String(linear_accel_data.acceleration.y) + "," +
           String(linear_accel_data.acceleration.z) + "," +
           String(gravity_data.acceleration.x) + "," +
           String(gravity_data.acceleration.y) + "," +
           String(gravity_data.acceleration.z) + "," +
           String(mag_data.magnetic.x) + "," +
           String(mag_data.magnetic.y) + "," +
           String(mag_data.magnetic.z) + "," +
           String(accel_data.acceleration.x) + "," +
           String(accel_data.acceleration.y) + "," +
           String(accel_data.acceleration.z) + "," +
           String(quat.w(), 4) + "," +
           String(quat.x(), 4) + "," +
           String(quat.y(), 4) + "," +
           String(quat.z(), 4) + "," +
           String(sys) + "," +
           String(gyro) + "," +
           String(accel) + "," +
           String(mag) + "," +
           String(temp);
}

// ── LED Matrix display ────────────────────────────────────────────────────────

/**
 * Scroll current sensor readings across the LED matrix.
 * Sequence: Temperature F -> Temperature C -> CO2 ppm -> Humidity %
 */
void scroll_matrix() {
    float temp_f = (last_temp_c * 9.0 / 5.0) + 32.0;

    String msg = String((int)round(temp_f)) + "F " +
                 String((int)round(last_temp_c)) + "C " +
                 String((int)round(last_co2)) + "ppm " +
                 String((int)round(last_humidity)) + "% ";

    matrix.beginDraw();
    matrix.stroke(0xFFFFFFFF);
    matrix.textScrollSpeed(50);
    matrix.textFont(Font_5x7);
    matrix.beginText(0, 1, 0xFFFFFF);
    matrix.println(msg);
    matrix.endText(SCROLL_LEFT);
    matrix.endDraw();
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Bridge.begin();
    matrix.begin();
    shield.begin();

    // Initialize SCD30 on QWIIC bus (Wire1)
    while (!scd30.begin(0x61, &Wire1)) {
        delay(100);
    }

    // Wait for first valid SCD30 reading before starting matrix scroll
    while (!scd30.dataReady()) {
        delay(100);
    }
    scd30.read();
    last_co2 = scd30.CO2;
    last_temp_c = scd30.temperature;
    last_humidity = scd30.relative_humidity;

    // Initialize BNO055 on QWIIC bus (Wire1), use external crystal
    while (!bno.begin()) {
        delay(100);
    }
    bno.setExtCrystalUse(true);

    // Register sensor functions with Bridge
    Bridge.provide("get_scd30_data", get_scd30_data);
    Bridge.provide("get_bno055_data", get_bno055_data);

    // Register motor control functions with Bridge
    Bridge.provide("set_motor", bridge_set_motor);
    Bridge.provide("stop_motors", bridge_stop_motors);
    Bridge.provide("move_forward", bridge_move_forward);
    Bridge.provide("move_backward", bridge_move_backward);
    Bridge.provide("strafe_left", bridge_strafe_left);
    Bridge.provide("strafe_right", bridge_strafe_right);
    Bridge.provide("rotate_cw", bridge_rotate_cw);
    Bridge.provide("rotate_ccw", bridge_rotate_ccw);
    Bridge.provide("move_diagonal_fl", bridge_move_diagonal_fl);
    Bridge.provide("move_diagonal_fr", bridge_move_diagonal_fr);
    Bridge.provide("move_diagonal_rl", bridge_move_diagonal_rl);
    Bridge.provide("move_diagonal_rr", bridge_move_diagonal_rr);
    Bridge.provide("mecanum_move", bridge_mecanum_move);
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    // Update sensor cache for LED matrix display
    if (scd30.dataReady()) {
        scd30.read();
        last_co2 = scd30.CO2;
        last_temp_c = scd30.temperature;
        last_humidity = scd30.relative_humidity;
    }

    // Scroll current readings on LED matrix
    scroll_matrix();
}
