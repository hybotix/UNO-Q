/**
 * SecureSMARS Sketch
 * Hybrid RobotiX - Dale Weber (N7PKT)
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
Adafruit_DCMotor *motorFL = shield.getMotor(1); // Front Left
Adafruit_DCMotor *motorFR = shield.getMotor(2); // Front Right
Adafruit_DCMotor *motorRL = shield.getMotor(3); // Rear Left
Adafruit_DCMotor *motorRR = shield.getMotor(4); // Rear Right

// ── Last known sensor values for LED matrix display ──────────────────────────
float lastCO2 = 0;
float lastTempC = 0;
float lastHumidity = 0;

// ── Motor control helpers ─────────────────────────────────────────────────────

/**
 * Set a single motor speed and direction.
 * speed: -255 (full reverse) to +255 (full forward), 0 = stop
 */
void setMotor(Adafruit_DCMotor *motor, int speed) {
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
        case 1: setMotor(motorFL, speed); break;
        case 2: setMotor(motorFR, speed); break;
        case 3: setMotor(motorRL, speed); break;
        case 4: setMotor(motorRR, speed); break;
    }
}

/**
 * Stop and release all four motors.
 */
void stop_motors() {
    motorFL->run(RELEASE);
    motorFR->run(RELEASE);
    motorRL->run(RELEASE);
    motorRR->run(RELEASE);
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
    int maxVal = max(max(abs(fl), abs(fr)), max(abs(rl), abs(rr)));
    if (maxVal > 255) {
        fl = fl * 255 / maxVal;
        fr = fr * 255 / maxVal;
        rl = rl * 255 / maxVal;
        rr = rr * 255 / maxVal;
    }

    setMotor(motorFL, fl);
    setMotor(motorFR, fr);
    setMotor(motorRL, rl);
    setMotor(motorRR, rr);
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
 * Also updates lastCO2, lastTempC, lastHumidity for LED matrix display.
 * Returns "0,0,0" if data not ready.
 */
String get_scd_data() {
    if (scd30.dataReady()) {
        scd30.read();
        lastCO2 = scd30.CO2;
        lastTempC = scd30.temperature;
        lastHumidity = scd30.relative_humidity;
        return String(lastCO2) + "," +
               String(lastTempC) + "," +
               String(lastHumidity);
    }
    return "0,0,0";
}

/**
 * Read all BNO055 data and return as CSV string.
 * Order: heading, pitch, roll, gyro xyz, linear accel xyz,
 *        gravity xyz, mag xyz, accel xyz, quat wxyz,
 *        cal sys/gyro/accel/mag, temperature
 */
String get_bno_data() {
    sensors_event_t orientationData, angVelocityData, linearAccelData, gravityData, magData, accelData;
    bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
    bno.getEvent(&angVelocityData, Adafruit_BNO055::VECTOR_GYROSCOPE);
    bno.getEvent(&linearAccelData, Adafruit_BNO055::VECTOR_LINEARACCEL);
    bno.getEvent(&gravityData, Adafruit_BNO055::VECTOR_GRAVITY);
    bno.getEvent(&magData, Adafruit_BNO055::VECTOR_MAGNETOMETER);
    bno.getEvent(&accelData, Adafruit_BNO055::VECTOR_ACCELEROMETER);

    imu::Quaternion quat = bno.getQuat();

    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);

    int8_t temp = bno.getTemp();

    return String(orientationData.orientation.x) + "," +
           String(orientationData.orientation.y) + "," +
           String(orientationData.orientation.z) + "," +
           String(angVelocityData.gyro.x) + "," +
           String(angVelocityData.gyro.y) + "," +
           String(angVelocityData.gyro.z) + "," +
           String(linearAccelData.acceleration.x) + "," +
           String(linearAccelData.acceleration.y) + "," +
           String(linearAccelData.acceleration.z) + "," +
           String(gravityData.acceleration.x) + "," +
           String(gravityData.acceleration.y) + "," +
           String(gravityData.acceleration.z) + "," +
           String(magData.magnetic.x) + "," +
           String(magData.magnetic.y) + "," +
           String(magData.magnetic.z) + "," +
           String(accelData.acceleration.x) + "," +
           String(accelData.acceleration.y) + "," +
           String(accelData.acceleration.z) + "," +
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
void scrollMatrix() {
    float tempF = (lastTempC * 9.0 / 5.0) + 32.0;

    String msg = String((int)round(tempF)) + "F " +
                 String((int)round(lastTempC)) + "C " +
                 String((int)round(lastCO2)) + "ppm " +
                 String((int)round(lastHumidity)) + "% ";

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

    // Initialize BNO055 on QWIIC bus (Wire1), use external crystal
    while (!bno.begin()) {
        delay(100);
    }
    bno.setExtCrystalUse(true);

    // Register sensor functions with Bridge
    Bridge.provide("get_scd_data", get_scd_data);
    Bridge.provide("get_bno_data", get_bno_data);

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
        lastCO2 = scd30.CO2;
        lastTempC = scd30.temperature;
        lastHumidity = scd30.relative_humidity;
    }

    // Scroll current readings on LED matrix
    scrollMatrix();
}
