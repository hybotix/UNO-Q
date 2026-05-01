# ei-c
Hybrid RobotiX — HybX Development System

## Purpose
Edge Impulse data collector for the VL53L5CX 8x8 ToF depth map sensor.
Collects labeled 8x8 distance frames and writes them to a CSV file in
Edge Impulse format for training a spatial classification model.

## Classes
- UP — object in upper half of FOV
- DOWN — object in lower half of FOV
- LEFT — object in left half of FOV
- RIGHT — object in right half of FOV
- CENTER — object in center of FOV

## Usage
```
# On the UNO Q — collect labeled frames
start ei-c UP
start ei-c DOWN
start ei-c LEFT
start ei-c RIGHT
start ei-c CENTER
mon

# Output file
~/data/ei-c/ei-c_<LABEL>_<TIMESTAMP>.csv
```

## Visualization
Copy CSV files off the UNO Q and run visualize.py on your Mac:
```
scp arduino@uno-q.local:~/data/ei-c/*.csv ./data/
python3 visualize.py data/ei-c_UP_20260501.csv
```

## Hardware
- SparkFun VL53L5CX large breakout (Wire1)
- 8x8 resolution, fixed forward-facing mount

## Orientation
- Row 0 = top of FOV, Row 7 = bottom of FOV
- Column 0 = robot left, Column 7 = robot right
- Visualization applies np.fliplr() to show view from behind sensor

## Notes
- sketch.ino and sketch.yaml are identical to monitor — same Bridge functions
- Only main.py differs — data collection instead of display
