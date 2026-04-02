#!/usr/bin/env python3
import sys
import subprocess
import os
import shutil

LAST_APP_FILE = os.path.expanduser("~/.last_app")

def get_app_path(app_name):
    if app_name.startswith("/") or app_name.startswith("~") or app_name.startswith("."):
        return app_name
    return os.path.expanduser(f"~/Arduino/{app_name}")

def load_last_app():
    if os.path.exists(LAST_APP_FILE):
        with open(LAST_APP_FILE, "r") as f:
            return f.read().strip()
    return None

def main():
    os.system("clear")

    if len(sys.argv) < 2:
        app_name = load_last_app()
        if not app_name:
            print("Usage: cache <app_name>")
            print("Example: cache matrix-app")
            sys.exit(1)
        print(f"Using last app: {app_name}")
    else:
        app_name = sys.argv[1]

    app_path = get_app_path(app_name)
    cache_path = os.path.join(app_path, ".cache")

    # Stop the app first
    subprocess.run(["arduino-app-cli", "app", "stop", app_path])

    # Remove Docker container and image for this app
    app_id = os.path.basename(app_path)
    container_name = f"arduino-{app_id}-main-1"
    subprocess.run(["docker", "rm", "-f", container_name], capture_output=True)
    subprocess.run(["docker", "rmi", "-f", f"arduino-{app_id}-main"], capture_output=True)
    print(f"Removed Docker container and image for: {app_id}")

    # Remove the cache
    if os.path.exists(cache_path):
        shutil.rmtree(cache_path)
        print(f"Cleared cache: {cache_path}")
    else:
        print(f"No cache found at: {cache_path}")

    # Restart the app
    subprocess.run(["arduino-app-cli", "app", "start", app_path])
    subprocess.run(["arduino-app-cli", "app", "logs", app_path])

if __name__ == "__main__":
    main()
