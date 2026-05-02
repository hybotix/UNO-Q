# VL53L5CX — Notes and Observations
## Hybrid RobotiX — get-data

> *Hybrid RobotiX designs and creates intelligent technologies that empower people facing physical and accessibility challenges to live more independently and achieve more on their own terms.*

*"I. WILL. NEVER. GIVE. UP. OR. SURRENDER."*

---

## About the Sensor

The VL53L5CX is a state of the art, Time-of-Flight (ToF), multizone ranging sensor enhancing the ST FlightSense product family. Housed in a miniature reflowable package, it integrates a SPAD array, physical infrared filters, and diffractive optical elements (DOE) to achieve the best ranging performance in various ambient lighting conditions with a range of cover glass materials.

**Key specifications:**
- 8x8 multizone ranging (64 simultaneous distance measurements)
- 65° diagonal field of view
- Up to 4m range
- Up to 15Hz in 8x8 mode, 60Hz in 4x4 mode
- SPAD array with DOE for uniform zone coverage
- Designed for varying ambient lighting conditions

---

## Raw Data

The sensor provides four data streams per frame:

- **Distance (mm)** — the actual measured distance for each zone
- **Signal (kcps/SPAD)** — signal strength, kilocounts per second per SPAD. Higher values indicate stronger reflections.
- **Sigma (mm)** — ranging uncertainty. Lower sigma means a more confident measurement.
- **Target Status** — True/False validity per zone. Status codes 5 or 9 indicate valid readings.

---

## Data Visualization Philosophy

The data is displayed as two 2D arrays in the sensor's own coordinate frame, flipped on the Y (column) axis. This is the only way to truly understand what the sensor is seeing — you must see it from the sensor's perspective, not the viewer's perspective.

Most visualization tools abstract away the raw numbers with heat maps and point clouds. These tools embed assumptions about what matters and make it harder to build real intuition about the sensor's behavior. Raw numbers in the correct spatial orientation are what understanding requires.

**Zone layout (8x8, physical orientation):**

```
Orientation: SparkFun logo at TOP, lens facing FORWARD.
Left/right defined from BEHIND the sensor looking forward.

[0][0] = upper-left   [0][7] = upper-right   ← row 0 = TOP of FOV
[4][0] = mid-left     [4][7] = mid-right
[7][0] = lower-left   [7][7] = lower-right   ← row 7 = BOTTOM of FOV

col 0 = LEFT    col 7 = RIGHT
col 3-4 = CENTER
```

After Y-axis flip (column mirror), the display matches what the sensor sees looking forward — left is left, right is right.

---

## Confidence Values

Confidence is a computed value combining signal strength and sigma:

```python
sig_score = min(signal / SIGNAL_MAX, 1.0)
sma_score = max(0.0, 1.0 - sigma / SIGMA_MAX)
confidence = (sig_score * 0.6 + sma_score * 0.4) * 99.99
```

Constants used:
- `SIGNAL_MAX = 8000.0` kcps/SPAD
- `SIGMA_MAX  = 30.0` mm

**Observation:** Initial confidence values of 35-42% suggest that real-world signal values are well below the 8000 kcps/SPAD maximum. The SIGNAL_MAX constant may need tuning based on actual measurements from the monitor.

---

## Classification Strategy

### Goal

Train a model to classify where in the sensor's field of view an object is located:

| Label | Meaning |
|-------|---------|
| `center` | Object centered in the FOV |
| `left` | Object in the left portion of the FOV |
| `right` | Object in the right portion of the FOV |
| `up` | Object in the upper portion of the FOV |
| `down` | Object in the lower portion of the FOV |

### Data Collection Protocol

- **Sensor moves, object stays fixed** — rotating the sensor left moves the object's apparent position right in the depth map
- **One label per collection run** — the label describes the deliberate orientation for that run
- **Varying distance within runs** — teaches the model that labels are about spatial position, not absolute distance
- **Multiple materials** — surface variation makes the model robust to different reflectance properties
- **250-500 frames per run** — natural hand tremor adds useful variation
- **Multiple files per label** — Edge Impulse merges them automatically on upload

### Centroid Analysis

The spatial position of the object is determined by a proximity-weighted centroid of the distance matrix:

```
weight = 1 / distance   (closer = heavier weight)
centroid_x = weighted average of column indices
centroid_y = weighted average of row indices
```

Dead zone (2.5 - 4.5 on each axis) maps to center. Outside that, the dominant axis deviation determines the label.

---

## Environment Mapping Potential

The VL53L5CX is capable of far more than classification. The 8x8 depth map is essentially a low-resolution 3D point cloud — 64 distance measurements in a known angular arrangement at up to 15Hz. Combined with IMU data (BNO055, LSM6DSOX) for odometry and orientation, this is the foundation for basic SLAM (Simultaneous Localization and Mapping).

For a small indoor robot, 64 zones at 15Hz is sufficient to detect walls, doorways, and obstacles. High resolution is not required for this use case.

---

## Texture Detection

The signal and sigma data contain surface texture information. Different materials at the same distance will return different signal strengths and sigma values. This is a future area of exploration — the sensor may be capable of surface classification in addition to distance measurement.

---

## Portability

The `hybx_vl53l5cx` library uses a Zephyr-native I2C platform layer targeting the UNO Q's i2c4 peripheral. Porting to other platforms (e.g. Arduino Portenta H7) would require a new platform layer using that platform's Wire implementation. The ST ULD itself is portable C.

The primary porting challenge is the ~86KB firmware upload — on the UNO Q this required DMA-enabled I2C. Other platforms with more capable I2C hardware may handle it without special configuration.

---

## Tools Built

| Tool | Description |
|------|-------------|
| `main-v1.0.2.py` | Interactive data collector — prompts for label and frame count, writes Edge Impulse-compatible CSV |
| `monitor-v1.0.1.py` | Live raw data monitor — displays all four matrices continuously, Y-axis flipped |
| `visualizer-v1.0.4.py` | Per-frame labeling tool — centroid-based direction suggestion, forward/backward navigation, atomic save |

---

## Key Lessons Learned

- **Ctrl+C without clean shutdown** leaves the sensor in a bad state requiring power cycle. Always call `end_sensor()` Bridge function before exit.
- **`Wire1.begin()` must be called BEFORE `Bridge.begin()`** — reversing this hangs the MCU permanently.
- **Firmware upload is Linux-triggered** — `begin_sensor()` is a Bridge function called from Python, not from `setup()`, to avoid starving the Bridge UART transport during the ~30s upload.
- **The sensor does not require power cycling between sketch reflashes** — as long as `end_sensor()` is called cleanly before the Python app exits.

---

*Hybrid RobotiX — San Diego, CA*
