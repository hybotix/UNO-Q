#!/usr/bin/env python3
import sys
import os
import subprocess

EXTRA_HOSTS = [
    "pimqtt.local:192.168.1.117",
    "pimqtt:192.168.1.117",
]

def patch_compose(app_path):
    compose_file = os.path.join(app_path, ".cache", "app-compose.yaml")
    if not os.path.exists(compose_file):
        return
    with open(compose_file, "r") as f:
        content = f.read()
    for host in EXTRA_HOSTS:
        entry = "    - " + host + "\n"
        if host not in content:
            content = content.replace(
                "    extra_hosts:\n",
                "    extra_hosts:\n" + entry
            )
    with open(compose_file, "w") as f:
        f.write(content)
    print("Patched compose file with extra_hosts")

def main():
    if len(sys.argv) < 2:
        print("Usage: restart <app_path>")
        sys.exit(1)

    app_path = sys.argv[1]

    subprocess.run(["arduino-app-cli", "app", "stop", app_path])
    subprocess.run(["arduino-app-cli", "app", "start", app_path])
    patch_compose(app_path)
    subprocess.run(["docker", "compose", "-f",
                   os.path.join(app_path, ".cache", "app-compose.yaml"),
                   "up", "-d", "--force-recreate"])
    subprocess.run(["arduino-app-cli", "app", "logs", app_path])

if __name__ == "__main__":
    main()
