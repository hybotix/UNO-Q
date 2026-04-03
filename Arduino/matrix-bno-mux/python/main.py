from arduino.app_utils import *
import time
import os

# Scroll timing constants — must match sketch
PIXELS_PER_CHAR = 6
MS_PER_PIXEL    = 125

# Calibration flag file — lives in $HOME on the UNO Q
# $HOME is bind-mounted into the container by the start command
CALIBRATION_FILE = os.path.expanduser("~/.scd30-calibrated")

started = False

def compass_point(heading):
    """Return 8-point compass direction for a heading in degrees."""
    points = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
    index = int((heading + 22.5) / 45) % 8
    return points[index]

def scroll_duration(msg):
    """Calculate how long the message takes to scroll once in seconds."""
    return len(msg) * PIXELS_PER_CHAR * MS_PER_PIXEL / 1000

def calibrate():
    """
    Calibrate SCD30 temperature offset using SHT45 as reference.
    Only runs if ~/.scd30-calibrated does not exist.
    Creates the flag file on success to prevent future recalibration.
    """
    if os.path.exists(CALIBRATION_FILE):
        print("SCD30: calibration file found — skipping calibration")
        return

    print("SCD30: calibrating temperature offset using SHT45 reference...")
    cal_msg = " Calibrating SCD-30... "
    Bridge.call("set_matrix_msg", cal_msg)
    time.sleep(scroll_duration(cal_msg))
    result = Bridge.call("calibrate_scd30")
    print(f"SCD30: calibration result: {result}")

    if result and result.startswith("offset:"):
        with open(CALIBRATION_FILE, "w") as f:
            f.write(result)
        print(f"SCD30: calibration complete — {result}")
    elif result == "skipped":
        print("SCD30: offset out of bounds — calibration skipped")
    else:
        print("SCD30: calibration failed")

def parse_as7343(data):
    """
    Parse AS7343 14-channel spectral data.
    Returns dict of channel readings, or None if data unavailable.
    Channels: F1(405nm), F2(425nm), F3(450nm), F4(475nm), F5(515nm),
              F6(555nm), F7(590nm), F8(630nm), F9(680nm), F10(910nm),
              F11(940nm), F12(1000nm), CLEAR, NIR
    """
    if not data or data == "0,0,0,0,0,0,0,0,0,0,0,0,0,0":
        return None
    values = [int(v) for v in data.split(",")]
    if len(values) != 14:
        return None
    return {
        "F1_405nm":   values[0],
        "F2_425nm":   values[1],
        "F3_450nm":   values[2],
        "F4_475nm":   values[3],
        "F5_515nm":   values[4],
        "F6_555nm":   values[5],
        "F7_590nm":   values[6],
        "F8_630nm":   values[7],
        "F9_680nm":   values[8],
        "F10_910nm":  values[9],
        "F11_940nm":  values[10],
        "F12_1000nm": values[11],
        "CLEAR":      values[12],
        "NIR":        values[13],
    }

def scroll_as7343(spectral):
    """
    Scroll AS7343 spectral data as two messages:
    - Visible: key visible channels (blue, green, red, clear)
    - NIR:     near-infrared channels
    Call this from loop() when AS7343 is connected.
    """
    if spectral is None:
        return

    # Message 3a — visible spectrum highlights
    blue  = spectral["F3_450nm"]
    green = spectral["F5_515nm"]
    red   = spectral["F8_630nm"]
    clear = spectral["CLEAR"]
    print(f"Visible: B={blue} G={green} R={red} Clear={clear}")
    msg3a = f" B:{blue} G:{green} R:{red} Clr:{clear} "
    Bridge.call("set_matrix_msg", msg3a)
    time.sleep(scroll_duration(msg3a))

    # Message 3b — NIR channels
    nir910  = spectral["F10_910nm"]
    nir940  = spectral["F11_940nm"]
    nir1000 = spectral["F12_1000nm"]
    nir     = spectral["NIR"]
    print(f"NIR: 910={nir910} 940={nir940} 1000={nir1000} NIR={nir}")
    msg3b = f" 910:{nir910} 940:{nir940} NIR:{nir} "
    Bridge.call("set_matrix_msg", msg3b)
    time.sleep(scroll_duration(msg3b))


def parse_apds9999(data):
    """
    Parse APDS9999 proximity, lux and RGB color data.
    Returns dict or None if data unavailable.
    Fields: proximity, lux, r, g, b, ir
    """
    if not data or data == "0,0,0,0,0,0":
        return None
    values = data.split(",")
    if len(values) != 6:
        return None
    return {
        "proximity": int(values[0]),
        "lux":       float(values[1]),
        "r":         int(values[2]),
        "g":         int(values[3]),
        "b":         int(values[4]),
        "ir":        int(values[5]),
    }

def scroll_apds9999(apds):
    """
    Scroll APDS9999 data as one message: proximity, lux, and RGB.
    Call this from loop() when APDS9999 is connected.
    """
    if apds is None:
        return
    proximity = apds["proximity"]
    lux       = apds["lux"]
    r         = apds["r"]
    g         = apds["g"]
    b         = apds["b"]
    print(f"Prox:{proximity} Lux:{lux:.1f} R:{r} G:{g} B:{b}")
    msg = f" Prox:{proximity} Lux:{lux:.1f} R:{r} G:{g} B:{b} "
    Bridge.call("set_matrix_msg", msg)
    time.sleep(scroll_duration(msg))


def parse_sgp41(data):
    """
    Parse SGP41 VOC and NOx raw signals.
    Returns dict or None if data unavailable.
    Fields: voc_raw, nox_raw
    Use Sensirion algorithm to convert raw to index values.
    """
    if not data or data == "0,0":
        return None
    values = data.split(",")
    if len(values) != 2:
        return None
    return {
        "voc_raw": int(values[0]),
        "nox_raw": int(values[1]),
    }

def scroll_sgp41(sgp):
    """
    Scroll SGP41 VOC and NOx raw signal data.
    Call this from loop() when SGP41 is connected.
    """
    if sgp is None:
        return
    voc = sgp["voc_raw"]
    nox = sgp["nox_raw"]
    print(f"VOC:{voc} NOx:{nox}")
    msg = f" VOC:{voc} NOx:{nox} "
    Bridge.call("set_matrix_msg", msg)
    time.sleep(scroll_duration(msg))

def loop():
    global started

    if not started:
        time.sleep(5)
        calibrate()
        # Wait for valid SCD30 data before starting scroll
        while True:
            scd_check = Bridge.call("get_scd_data")
            if scd_check and scd_check != "0,0,0":
                break
            time.sleep(1)
        started = True

    scd_data  = Bridge.call("get_scd_data")
    sht_data  = Bridge.call("get_sht45_data")
    bno_data  = Bridge.call("get_bno_data")
    # as7343_data = Bridge.call("get_as7343_data")  # Uncomment when AS7343 connected
    # apds9999_data = Bridge.call("get_apds9999_data")  # Uncomment when APDS9999 connected
    # sgp41_data = Bridge.call("get_sgp41_data")  # Uncomment when SGP41 connected

    co2      = None
    temp_c   = None
    humidity = None
    heading  = None
    pitch    = None
    roll     = None
    spectral = None
    apds     = None
    sgp      = None

    if scd_data and scd_data != "0,0,0":
        co2 = float(scd_data.split(",")[0])

    if sht_data and sht_data != "0,0":
        parts    = sht_data.split(",")
        temp_c   = float(parts[0])
        humidity = float(parts[1])

    if bno_data:
        values  = bno_data.split(",")
        heading = float(values[0])
        pitch   = float(values[1])
        roll    = float(values[2])

    # spectral = parse_as7343(as7343_data)  # Uncomment when AS7343 connected
    # apds = parse_apds9999(apds9999_data)  # Uncomment when APDS9999 connected
    # sgp = parse_sgp41(sgp41_data)  # Uncomment when SGP41 connected

    # Skip loop iteration if primary sensor data not ready
    if temp_c is None or co2 is None:
        time.sleep(1)
        return

    # Message 1 — environmental data (always first)
    temp_f = (temp_c * 9.0 / 5.0) + 32.0
    print(f"{temp_f:.1f}\u00b0F ({temp_c:.1f}\u00b0C)  {humidity:.1f}%  {co2:.1f} ppm")
    msg1 = f" {temp_f:.1f}\u00b0F({temp_c:.1f}\u00b0C) {humidity:.1f}% {co2:.1f}ppm "
    Bridge.call("set_matrix_msg", msg1)
    time.sleep(scroll_duration(msg1))

    # Message 2 — orientation data
    if heading is not None:
        cp = compass_point(heading)
        print(f"H{heading:.1f}\u00b0 {cp}  P{pitch:.1f}\u00b0  R{roll:.1f}\u00b0")
        msg2 = f" H{heading:.1f}\u00b0 {cp} P{pitch:.1f}\u00b0 R{roll:.1f}\u00b0 "
        Bridge.call("set_matrix_msg", msg2)
        time.sleep(scroll_duration(msg2))

    # Message 3 — AS7343 spectral data
    # Uncomment when AS7343 is connected:
    # scroll_as7343(spectral)
    # scroll_apds9999(apds)
    # scroll_sgp41(sgp)

App.run(user_loop=loop)
