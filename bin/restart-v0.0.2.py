#!/usr/bin/env python3
import sys
import subprocess
import os

def main():
    if len(sys.argv) < 2:
        print("Usage: restart <app_name>")
        print("Example: restart securesmars")
        sys.exit(1)

    app_name = sys.argv[1]

    # If full path given, use as-is. Otherwise prepend ~/Arduino/
    if app_name.startswith("/") or app_name.startswith("~") or app_name.startswith("."):
        app_path = app_name
    else:
        app_path = os.path.expanduser(f"~/Arduino/{app_name}")

    subprocess.run(["arduino-app-cli", "app", "stop", app_path])
    subprocess.run(["arduino-app-cli", "app", "start", app_path])
    subprocess.run(["arduino-app-cli", "app", "logs", app_path])

if __name__ == "__main__":
    main()
