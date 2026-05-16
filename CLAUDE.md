# Hybrid RobotiX — UNO-Q Coding Standards
## CLAUDE.md

This file defines the coding standards and conventions for all code in the
UNO-Q repository. These standards apply to all Arduino sketches (`.ino`) and
all Python app files (`.py`). All code written or modified must follow these
standards without exception.

---

## General Principles

- All changes must be recorded in CHANGELOG.md at the root of the repo

- Do NOT make assumptions about ANYTHING!

- Make all changes to repos first!

- **All sensors are equal.** There is no such thing as a primary sensor.
  Process every sensor independently. Never skip a loop iteration because one
  sensor is unavailable. Display `***` with the correct units for any reading
  that is unavailable.

- **Degrade gracefully.** Always display something meaningful. A missing
  reading is shown as `***<unit>` (e.g. `***°F`, `***%`, `*** ppm`).

- **No silent failures.** Every error condition must be reported — print it,
  log it, or show it on the display. Never silently skip or swallow errors.

- **All identifiers must be `snake_case`.** No `camelCase` anywhere — not in
  variable names, function names, or constants. Constants use `UPPER_SNAKE_CASE`.

---

## Naming Conventions

```python
# Python — all snake_case
temp_c        = 22.5
humidity_str  = "***%"
scroll_x      = 0

def compass_point(heading):
    pass

SCROLLING_ENABLED = True
CALIBRATION_FILE  = "~/.scd30-calibrated"
```

```cpp
// Arduino — all snake_case
float temp_c       = 0.0;
bool  init_done    = false;
int   scroll_x     = 0;

void scroll_tick() {}

#define SCROLLING_ENABLED true
#define SCROLL_SPEED_MS   125
```

---

## If / Else / Elif Rules

### Two paths — always use positive condition first

When a block has both an `if` body AND an `else` body, the positive condition
comes first.

```python
# CORRECT
if temp_c:
    temp_f   = (temp_c * 9.0 / 5.0) + 32.0
    temp_str = f"{fmt(temp_f)}°F({fmt(temp_c)}°C)"
else:
    temp_str = "***°F(***°C)"

# WRONG
if not temp_c:
    temp_str = "***°F(***°C)"
else:
    temp_f   = (temp_c * 9.0 / 5.0) + 32.0
    temp_str = f"{fmt(temp_f)}°F({fmt(temp_c)}°C)"
```

```cpp
// CORRECT
if (SCROLLING_ENABLED) {
    scroll_tick();
}

// WRONG
if (!SCROLLING_ENABLED) {
    return;
}
scroll_tick();
```

### One path, positive body — use positive condition

When there is only one path and the body is the "happy" case, use the positive
condition.

```python
# CORRECT
if spectral:
    blue = spectral["F3_450nm"]
    ...

# WRONG
if spectral is None:
    return
blue = spectral["F3_450nm"]
```

### One path, negative guard — use negative condition directly

When there is only one path and it is a guard clause (bail out, skip, error),
use the negative condition directly. Do NOT write `if x: pass else: guard`.

```python
# CORRECT
if not started:
    time.sleep(5)
    started = True

if not sensor_ready:
    print("Sensor not ready")
    return

# WRONG
if started:
    pass
else:
    time.sleep(5)
    started = True
```

```cpp
// CORRECT
if (!sensor.begin()) {
    init_failed = true;
    init_done   = true;
}

// WRONG
if (sensor.begin()) {
    init_done = true;
} else {
    init_failed = true;
    init_done   = true;
}
```

### Never use `is None` or `is not None`

Use simple truthiness checks instead.

```python
# CORRECT
if temp_c:
    ...

if not csv_file:
    ...

humidity_str = f"{fmt(humidity)}%" if humidity else "***%"

# WRONG
if temp_c is not None:
    ...

if csv_file is None:
    ...

humidity_str = f"{fmt(humidity)}%" if humidity is not None else "***%"
```

### No single-line if statements

Always expand to a full block.

```python
# CORRECT
if result:
    started = True

# WRONG
if result: started = True
```

```cpp
// CORRECT
if (x > 0) {
    return x;
}

// WRONG
if (x > 0) return x;
```

---

## Return Patterns

### Use `result = None` with a single return at the end

When a function builds and returns a value, assign to `result` and return once.

```python
# CORRECT
def parse_apds9999(data):
    result = None

    if data and data != "0,0,0,0,0,0":
        values = data.split(",")

        if len(values) == 6:
            result = {
                "proximity": int(values[0]),
                "lux":       float(values[1]),
                "r":         int(values[2]),
                "g":         int(values[3]),
                "b":         int(values[4]),
                "ir":        int(values[5]),
            }

    return result

# WRONG
def parse_apds9999(data):
    if not data or data == "0,0,0,0,0,0":
        return None
    values = data.split(",")
    if len(values) != 6:
        return None
    return {
        "proximity": int(values[0]),
        ...
    }
```

### Simple returns — no else needed after a return

```python
# CORRECT
def fmt(value, decimals=1):
    if round(value, decimals) == int(value):
        return str(int(value))
    return f"{value:.{decimals}f}"

# WRONG
def fmt(value, decimals=1):
    if round(value, decimals) == int(value):
        return str(int(value))
    else:
        return f"{value:.{decimals}f}"
```

---

## Blank Line Rules

### Python

- **Blank line before every `def`** (at any indent level)
- **Blank line after every `if`, `for`, `while`, `try` block** when the next
  statement is at the same or lower indent level
- **No blank line immediately after a `:` block opener**
- **No blank line immediately before the end of a block**
- **No trailing blank lines inside dict or list literals**
- **No more than one consecutive blank line anywhere**
- **Blank line before `App.run()`** at end of file

```python
# CORRECT
def loop():
    result = Bridge.call("get_scd30_data")

    if result:
        co2, temp, humidity = result.split(",")
        print("CO2: " + co2 + " ppm")

    time.sleep(2)

App.run(user_loop=loop)

# WRONG
def loop():

    result = Bridge.call("get_scd30_data")
    if result:

        co2, temp, humidity = result.split(",")
        print("CO2: " + co2 + " ppm")
    time.sleep(2)
App.run(user_loop=loop)
```

### Arduino (C++)

- **Blank line before every function definition**
- **Blank line after every function closing brace** (at indent level 0)
- **No blank line immediately after `{`**
- **No blank line immediately before `}`**
- **No more than one consecutive blank line anywhere**

```cpp
// CORRECT
void scroll_tick() {
    if (SCROLLING_ENABLED) {
        last_scroll_ms = millis();
        matrix.beginDraw();
    }
}

void setup() {
    Wire1.begin();
}

// WRONG
void scroll_tick() {

    if (SCROLLING_ENABLED) {

        last_scroll_ms = millis();
        matrix.beginDraw();

    }

}
void setup() {
    Wire1.begin();
}
```

---

## Indentation

- **Python:** 4 spaces per level. No tabs. Ever.
- **Arduino:** 4 spaces per level. No tabs. Ever.
- **Indentation must be perfectly consistent.** Mixed indentation is never
  acceptable.

```python
# CORRECT
if spectral:
    blue  = spectral["F3_450nm"]
    green = spectral["F5_515nm"]

    if SCROLLING_ENABLED:
        Bridge.call("set_matrix_msg", msg)
        time.sleep(scroll_duration(msg))

# WRONG
if spectral:
  blue  = spectral["F3_450nm"]    # 2 spaces — wrong
  green = spectral["F5_515nm"]
  if SCROLLING_ENABLED:
      Bridge.call("set_matrix_msg", msg)    # mixed indent — wrong
```

---

## Column Alignment

Align assignment operators and dict values vertically when declaring groups of
related variables or dict entries.

```python
# CORRECT
temp_f       = (temp_c * 9.0 / 5.0) + 32.0
humidity_str = f"{fmt(humidity)}%"
co2_str      = f"{co2:.0f} ppm"

result = {
    "proximity": int(values[0]),
    "lux":       float(values[1]),
    "r":         int(values[2]),
    "g":         int(values[3]),
}

# WRONG
temp_f = (temp_c * 9.0 / 5.0) + 32.0
humidity_str = f"{fmt(humidity)}%"
co2_str = f"{co2:.0f} ppm"
```

```cpp
// CORRECT
static bool init_done    = false;
static bool init_failed  = false;
static int  scroll_x     = 0;

// WRONG
static bool init_done = false;
static bool init_failed = false;
static int scroll_x = 0;
```

---

## Sensor Data Handling

- **Never exit a loop because one sensor is unavailable.** Process all sensors
  independently.
- **Use `***` with units for unavailable readings:**
  - Temperature: `***°F(***°C)`
  - Humidity: `***%`
  - CO2: `*** ppm`
  - Heading: `H***° ***`
  - Pitch: `P***°`
  - Roll: `R***°`
- **Parse functions return `None` for bad data** — callers check truthiness.
- **Scroll functions skip silently if data is `None`** — `if spectral:` etc.

```python
# CORRECT
if temp_c:
    temp_f   = (temp_c * 9.0 / 5.0) + 32.0
    temp_str = f"{fmt(temp_f)}°F({fmt(temp_c)}°C)"
else:
    temp_str = "***°F(***°C)"

humidity_str = f"{fmt(humidity)}%" if humidity else "***%"
co2_str      = f"{co2:.0f} ppm"   if co2      else "*** ppm"
```

---

## Arduino-Specific Rules

- **Always use `Wire1`** for QWIIC/Stemma QT sensors — the QWIIC connector is
  on I2C bus 1, not bus 0.
- **`Bridge.provide()` calls must come before `setup()`.**
- **`Bridge.begin()` must be the last call in `setup()`.**
- **All sensor state variables declared `static`** at file scope.

```cpp
// CORRECT
static bool init_done   = false;
static bool init_failed = false;

String get_sensor_status() { ... }

void begin_sensor() {
    Wire1.begin();
    sensor.begin(Wire1);
}

void setup() {
    Bridge.provide("get_sensor_status", get_sensor_status);
    Bridge.provide("begin_sensor",      begin_sensor);
    Bridge.begin();
}
```

---

## Python-Specific Rules

- **All imports at the top of the file.**
- **Constants defined at module level** after imports, before any `def`.
- **`App.run(user_loop=loop)` is always the last line** with a blank line before it.
- **No `__pycache__` or `.pyc` files in the repo** — covered by `.gitignore`.

---

## Git Commit Messages

Commit messages use a short prefix:

| Prefix | Use |
|--------|-----|
| `feat:` | New feature or new app |
| `fix:` | Bug fix |
| `style:` | Formatting, naming, no logic change |
| `docs:` | Documentation only |
| `chore:` | Maintenance (gitignore, cleanup, etc.) |
| `refactor:` | Code restructure, no behavior change |

---

*Hybrid RobotiX — San Diego, CA*
