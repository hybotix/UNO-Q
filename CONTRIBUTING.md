# Contributing to UNO-Q
## Hybrid RobotiX

First — thank you. Every sketch, app, fix, and idea that comes in makes
this repo more useful for everyone building on the Arduino UNO Q platform.

*"I. WILL. NEVER. GIVE. UP. OR. SURRENDER."*

---

## The Philosophy

This repo contains the Arduino sketches and Python apps that run on the
UNO Q. The goal is clean, well-structured, reproducible code that works
reliably in the HybX Development System environment. Every contribution
should move in that direction — cleaner, more reliable, more capable.

We are not at a 1.x release yet. This is the time to get things right.
If something is wrong, fix it properly.

---

## What We Welcome

- **New sketches and apps** — new sensor integrations, new robot behaviors,
  new capabilities
- **Bug fixes** — if something is broken, tell us or fix it
- **Improvements to existing apps** — better sensor handling, better Bridge
  integration, better Python logic
- **New entries in KNOWN_ISSUES.md** — discovered vendor or hardware bugs
  belong there with a link to the upstream issue
- **Documentation** — if something is unclear, fix it
- **Ideas and discussion** — open an issue, start a conversation

No contribution is too small. A comment fix is welcome. A one-line
improvement is welcome. A completely new app is welcome.

---

## What We Do Not Accept

- Arduino sketches that do not use the Bridge architecture
- Python apps that bypass `arduino.app_utils`
- Library management outside of `libs` — never install or reference
  libraries directly; always use `libs install` and `libs use`
- Hardcoded paths or credentials of any kind
- Manual edits to `sketch.yaml` library sections — `libs` owns that
- Code that does not follow the standards below

---

## Project Structure

Each project lives under `Arduino/UNO-Q/<project-name>/` and follows
this scaffold:

```
<project-name>/
  app.yaml          — App metadata (name, icon, description)
  sketch/
    sketch.ino      — MCU code (Arduino Bridge template)
    sketch.yaml     — Library dependencies (managed by libs — never edit manually)
  python/
    main.py         — Python controller
    requirements.txt — Python dependencies
```

Use `project new arduino <n>` from the HybX Development System to
create a correctly scaffolded project.

---

## Arduino Sketch Standards

### Bridge Architecture

- Always register `Bridge.provide()` calls **before** `setup()` — this is
  a hard platform requirement, not a style choice
- Keep `loop()` empty when using Bridge — the Bridge drives execution
- Initialize all sensors in `setup()`

### Sensor Code Completeness

- All sensor code must be fully present: include, instance, function,
  and `Bridge.provide()` active
- Only `begin()` calls are commented out for sensors not yet physically
  connected — everything else stays active
- Unconnected sensors are commented out, never deleted

### QWIIC / Stemma QT

- The QWIIC connector is on I2C bus 1, not bus 0
- All QWIIC/Stemma QT sensors **must** use `&Wire1`
- This is an undocumented platform behavior — do not use the default Wire

### Library Management

- Never manually edit `sketch.yaml` library sections
- `sketch.yaml` is owned exclusively by `libs` in the HybX Development System
- To add a library to a project: `libs install <n>` then `libs use <project> <n>`
- To remove a library: `libs unuse <project> <n>` then `libs remove <n>`

### Code Style

- Clear, readable variable names
- Comments on anything non-obvious
- One sensor or concern per file where practical

---

## Python App Standards

### Bridge Integration

- Use `Bridge.call()` to read values registered on the MCU side
- Use `App.run(user_loop=loop)` as the entry point
- Keep the loop function focused — one responsibility per loop

### Dependencies

- List all pip dependencies in `requirements.txt`
- Do not import packages not in `requirements.txt`
- `arduino.app_utils` is Docker-injected at runtime — do not add it to
  `requirements.txt`

### Code Style

- Python 3, PEP 8 compliant
- Column-aligned assignments are the project style — keep them
- Clear, readable variable names
- Docstrings on all functions
- No hardcoded paths — use `os.path.expanduser()` or config

---

## Naming Conventions

- **Hybrid RobotiX** — capital H, R, X — always
- **My Chairiet** — title case, always (portmanteau of CHAIR and CHARIOT)
- **HybX** — Discord tag, always exactly this
- **"essentially the same"** is never acceptable when accessibility
  needs are specified in writing

---

## Commit Messages

- First line: `app/sketch: short description` (50 chars or less)
- Body: explain what changed and why, not just what
- Reference sensor datasheets or vendor issues where relevant

---

## How to Contribute

1. Fork the repo
2. Create a branch: `git checkout -b my-feature`
3. Make your changes following the standards above
4. Test on a real UNO Q using the HybX Development System
5. Open a pull request with a clear description of what changed and why

Pull requests are welcome against any part of the project. All pull
requests will be reviewed and merged at the sole discretion of the
project maintainer.

---

## Reporting Issues

Open a GitHub issue. Include:

- What you were trying to do
- What command you ran or what sketch you uploaded
- What output or behavior you got
- What you expected instead
- Board firmware version if relevant

Vendor and hardware bugs belong in `docs/KNOWN_ISSUES.md` with a link
to the upstream issue if one exists.

---

## Conduct

Be constructive. Be direct. Be respectful. We are all here to build
something good. Criticism of code is welcome and expected — criticism
of people is not.

---

## Questions

Open an issue tagged `question`. No question is too basic.

---

*Hybrid RobotiX — San Diego*
