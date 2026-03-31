#!/usr/bin/env python3

import sys
import os
import re
import subprocess

FQBN = "arduino:zephyr:unoq"

def compile_sketch(sketch_path):
    print("Compiling " + sketch_path + "...")
    result = subprocess.run(
        ["arduino-cli", "compile", "--fqbn", FQBN, sketch_path, "-v"],
        capture_output=True,
        text=True
    )
    print(result.stdout)
    if result.stderr:
        print(result.stderr)
    return result.returncode, result.stdout

def parse_libraries(output):
    libraries = []
    for line in output.splitlines():
        match = re.match(r'^(\S.*?)\s+(\d+\.\d+\.\d+)\s+/home/arduino/Arduino/libraries/', line)
        if match:
            name = match.group(1).strip()
            version = match.group(2).strip()
            libraries.append((name, version))
    return libraries

def generate_sketch_yaml(sketch_path, libraries):
    yaml_path = os.path.join(sketch_path, "sketch.yaml")
    print("Generating " + yaml_path + "...")
    with open(yaml_path, "w") as f:
        f.write("profiles:\n")
        f.write("  default:\n")
        f.write("    platforms:\n")
        f.write("      - platform: arduino:zephyr\n")
        if libraries:
            f.write("    libraries:\n")
            for name, version in libraries:
                f.write("      - " + name + " (" + version + ")\n")
        f.write("default_profile: default\n")

def upload_sketch(sketch_path):
    print("Uploading " + sketch_path + "...")
    result = subprocess.run(
        ["arduino-cli", "upload", "--profile", "default",
         "--fqbn", FQBN, sketch_path],
        capture_output=False
    )
    return result.returncode

def main():
    if len(sys.argv) < 2:
        print("Usage: build <sketch_path>")
        sys.exit(1)

    sketch_path = sys.argv[1]

    returncode, output = compile_sketch(sketch_path)
    if returncode != 0:
        print("Compile failed. Aborting upload.")
        sys.exit(1)

    libraries = parse_libraries(output)
    generate_sketch_yaml(sketch_path, libraries)

    if upload_sketch(sketch_path) != 0:
        print("Upload failed.")
        sys.exit(1)

    print("Done.")

if __name__ == "__main__":
    main()
