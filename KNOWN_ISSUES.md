# Known Issues

## Docker Network Isolation — Local Hostname Resolution Fails

**Status:** Open — waiting for Arduino to fix  
**Filed:** [arduino-app-cli GitHub issue](https://github.com/arduino/arduino-app-cli/issues)  
**Affects:** All apps that connect to local network services by hostname

### Problem

Python apps managed by `arduino-app-cli` run inside Docker containers using an isolated network (`arduino-<appname>_default`). This prevents apps from resolving local network hostnames including:

- mDNS hostnames (`*.local`) such as `pimqtt.local`
- Hostnames defined in the host's `/etc/hosts`
- Any hostname resolvable on the local network but not via public DNS

The host's `/etc/hosts` is not inherited by the container, and mDNS multicast does not pass through Docker's network isolation.

### Symptoms

App logs show:

```
socket.gaierror: [Errno -2] Name or service not known
```

### What Was Tried

- Adding `extra_hosts` to `app.yaml` — silently ignored, not passed to generated compose file
- Adding hostname to `/etc/hosts` on the UNO Q — not inherited by Docker container
- The generated `.cache/app-compose.yaml` only contains the default `msgpack-rpc-router:host-gateway` entry

### Impact on SecureSMARS

SecureSMARS cannot connect to the Mosquitto MQTT broker on `pimqtt.local` via the standard `restart` workflow. The connection works when running the Docker container manually with the `extra_hosts` entry added to `.cache/app-compose.yaml`, but this is overwritten every time the app starts.

### Workaround (Manual — Not Recommended)

1. Start the app normally with `start <app_path>`
2. Manually edit `.cache/app-compose.yaml` and add to `extra_hosts`:
   ```yaml
   - pimqtt.local:192.168.1.117
   - pimqtt:192.168.1.117
   ```
3. Run: `docker compose -f <app_path>/.cache/app-compose.yaml up -d --force-recreate`

This workaround is not sustainable as the compose file is regenerated on every app start.

### Required Fix

Arduino needs to implement one of the following:

1. Support `extra_hosts` in `app.yaml` and pass them through to the generated compose file
2. Use `network_mode: host` for the container
3. Document a supported way to configure Docker networking for apps
