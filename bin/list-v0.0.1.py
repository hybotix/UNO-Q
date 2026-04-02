#!/usr/bin/env python3
import subprocess

def main():
    subprocess.run(["arduino-app-cli", "app", "list"])

if __name__ == "__main__":
    main()
