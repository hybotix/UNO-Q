#include <Arduino_RouterBridge.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_MotorShield.h>
#include <utility/imumaths.h>
#include <Wire.h>

Adafruit_SCD30 scd30;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);
Adafruit_MotorShield shield = Adafruit_MotorShield();

Adafruit_DCMotor *motorFL = shield.getMotor(1);
Adafruit_DCMotor *motorFR = shield.getMotor(2);
Adafruit_DCMotor *motorRL = shield.getMotor(3);
Adafruit_DCMotor *motorRR = shield.getMotor(4);

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

void set_motor(int motor_id, int speed) {
    switch (motor_id) {
        case 1: setMotor(motorFL, speed); break;
        case 2: setMotor(motorFR, speed); break;
        case 3: setMotor(motorRL, speed); break;
        case 4: setMotor(motorRR, speed); break;
    }
}

void stop_motors() {
    motorFL->run(RELEASE);
    motorFR->run(RELEASE);
    motorRL->run(RELEASE);
    motorRR->run(RELEASE);
}

void mecanum_move(int x, int y, int r) {
    int fl = y + x + r;
    int fr = y - x - r;
    int rl = y - x + r;
    int rr = y + x - r;

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

String get_scd_data() {
    if (scd30.dataReady()) {
        scd30.read();
        return String(scd30.CO2) + "," +
               String(scd30.temperature) + "," +
               String(scd30.relative_humidity);
    }
    return "0,0,0";
}

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

void setup() {
    Bridge.begin();
    shield.begin();
    while (!scd30.begin(0x61, &Wire1)) {
        delay(100);
    }
    while (!bno.begin()) {
        delay(100);
    }
    bno.setExtCrystalUse(true);

    Bridge.provide("get_scd_data", get_scd_data);
    Bridge.provide("get_bno_data", get_bno_data);
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

void loop() {
}
