#!/usr/bin/env python3
import sys
import subprocess
import os
import shutil
import time

LAST_APP_FILE = os.path.expanduser("~/.last_app")
HOME          = os.path.expanduser("~")

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

def nuke_docker(app_id):
    container_name = f"arduino-{app_id}-main-1"
    subprocess.run(["docker", "rm", "-f", container_name], capture_output=True)
    subprocess.run(["docker", "rmi", "-f", f"arduino-{app_id}-main"], capture_output=True)
    print(f"Removed Docker container and image for: {app_id}")

def clear_cache(app_path):
    cache_path = os.path.join(app_path, ".cache")
    if os.path.exists(cache_path):
        shutil.rmtree(cache_path)
        print(f"Cleared cache: {cache_path}")

def patch_compose(app_path):
    """
    Patch the generated app-compose.yaml to mount $HOME into the container.
    This allows Python apps to read/write files in $HOME (e.g. ~/.scd30-calibrated).
    The home directory is mounted at the same path inside the container.
    """
    compose_file = os.path.join(app_path, ".cache", "app-compose.yaml")

    # Wait for compose file to be generated
    for _ in range(30):
        if os.path.exists(compose_file):
            break
        time.sleep(0.5)

    if not os.path.exists(compose_file):
        print("WARNING: compose file not found — skipping $HOME mount patch")
        return

    home_mount = (
        f"    - type: bind\n"
        f"      source: {HOME}\n"
        f"      target: {HOME}\n"
    )

    with open(compose_file, "r") as f:
        content = f.read()

    if f"source: {HOME}" in content:
        print(f"$HOME already mounted in compose file")
        return

    # Insert after the first "volumes:" line
    content = content.replace("    volumes:\n", f"    volumes:\n{home_mount}", 1)

    with open(compose_file, "w") as f:
        f.write(content)

    print(f"Patched compose file: mounted {HOME} into container")

    # Restart the container with the patched compose file
    subprocess.run([
        "docker", "compose",
        "-f", compose_file,
        "up", "-d", "--force-recreate"
    ], capture_output=True)

def main():
    os.system("clear")
    print(f"=== start ===")

    if len(sys.argv) < 2:
        app_name = load_last_app()
        if not app_name:
            print("Usage: start <app_name>")
            print("Example: start matrix-app")
            sys.exit(1)
        print(f"Using last app: {app_name}")
    else:
        app_name = sys.argv[1]

    save_last_app(app_name)
    app_path = get_app_path(app_name)
    app_id = os.path.basename(app_path)

    nuke_docker(app_id)
    clear_cache(app_path)

    # Install newrepo as a bin command from the repo
    newrepo_src = os.path.expanduser("~/Repos/GitHub/hybotix/UNO-Q/scripts/newrepo.bash")
    newrepo_dst = os.path.expanduser("~/bin/newrepo")
    if os.path.exists(newrepo_src):
        import shutil
        shutil.copy2(newrepo_src, newrepo_dst)
        os.chmod(newrepo_dst, 0o755)
        print(f"Installed: newrepo -> ~/bin/newrepo")

    subprocess.run(["arduino-app-cli", "app", "start", app_path])
    patch_compose(app_path)

if __name__ == "__main__":
    main()
