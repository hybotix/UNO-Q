#!/usr/bin/env python3
import sys
import subprocess
import os

LAST_APP_FILE = os.path.expanduser("~/.last_app")

def get_app_path(app_name):
    if app_name.startswith("/") or app_name.startswith("~") or app_name.startswith("."):
        return app_name
    return os.path.expanduser(f"~/Arduino/{app_name}")

def save_last_app(app_name):
    with open(LAST_APP_FILE, "w") as f:
        f.write(app_name)

def load_last_app():
    if os.path.exists(LAST_APP_FILE):
        with open(LAST_APP_FILE, "r") as f:
            return f.read().strip()
    return None

def main():
    if len(sys.argv) < 2:
        app_name = load_last_app()
        if not app_name:
            print("Usage: logs <app_name>")
            print("Example: logs matrix-app")
            sys.exit(1)
        print(f"Using last app: {app_name}")
    else:
        app_name = sys.argv[1]

    save_last_app(app_name)
    app_path = get_app_path(app_name)
    subprocess.run(["arduino-app-cli", "app", "logs", app_path])

if __name__ == "__main__":
    main()
