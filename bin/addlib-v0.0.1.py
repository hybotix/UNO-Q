#!/usr/bin/env python3
import sys
import subprocess

def usage():
    print("Usage: addlib <command> <library name>")
    print("Commands:")
    print("  search <name>   - Search for a library")
    print("  install <name>  - Install a library")
    print("  list            - List installed libraries")
    print("  upgrade         - Upgrade all installed libraries")
    print("")
    print("Examples:")
    print("  addlib search \"Adafruit SCD30\"")
    print("  addlib install \"Adafruit SCD30\"")
    print("  addlib list")
    print("  addlib upgrade")

def main():
    if len(sys.argv) < 2:
        usage()
        sys.exit(1)

    command = sys.argv[1]

    if command == "list":
        subprocess.run(["arduino-cli", "lib", "list"])
    elif command == "upgrade":
        subprocess.run(["arduino-cli", "lib", "upgrade"])
    elif command in ("search", "install"):
        if len(sys.argv) < 3:
            print(f"Error: '{command}' requires a library name")
            usage()
            sys.exit(1)
        lib_name = sys.argv[2]
        subprocess.run(["arduino-cli", "lib", command, lib_name])
    else:
        print(f"Error: Unknown command '{command}'")
        usage()
        sys.exit(1)

if __name__ == "__main__":
    main()
