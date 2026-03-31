#!/usr/bin/env python3
import sys
import subprocess

def main():
    if len(sys.argv) < 2:
        print("Usage: stop <app_path>")
        sys.exit(1)

    app_path = sys.argv[1]

    subprocess.run(["arduino-app-cli", "app", "stop", app_path])

if __name__ == "__main__":
    main()
