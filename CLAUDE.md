# CLAUDE.md

## Fresh Install (one-liner)

Run on a fresh Raspberry Pi to install everything. Prompts for autostart and kiosk mode:

```
curl -fsSL https://raw.githubusercontent.com/parkredding/poorhouse-lane-v4/main/setup.sh | sudo bash
```

## Testing on Raspberry Pi

After SSH'ing into the Raspberry Pi, run this one-liner to stop any running instance, fetch the newest branch, check it out, build, deploy, and run:

```
sudo systemctl stop dubsiren.service 2>/dev/null; sudo killall dubsiren 2>/dev/null; sleep 1; cd ~/dubsiren && git fetch origin && BRANCH=$(git for-each-ref --sort=-committerdate 'refs/remotes/origin/claude/*' --format='%(refname:lstrip=3)' | head -1) && git checkout "$BRANCH" && git pull && mkdir -p build && cd build && rm -f CMakeCache.txt && cmake .. && make -j$(nproc) && sudo ../scripts/deploy-to-persist.sh && sudo ./dubsiren
```

The `deploy-to-persist.sh` step copies the binary to the real disk so it persists across reboots on overlay FS (kiosk mode). On non-overlay systems it's a no-op.

## Siren Health Check

Run on the Raspberry Pi to check installation status, kiosk/regular mode, and whether user presets are persistent:

```
echo "=== Dub Siren Health Check ==="; echo "-- Install --"; (test -f /home/pi/dubsiren/build/dubsiren && echo "Binary: OK" || echo "Binary: MISSING"); (systemctl is-enabled dubsiren.service 2>/dev/null && echo "Autostart: ON" || echo "Autostart: OFF"); (systemctl is-active dubsiren.service 2>/dev/null && echo "Service: RUNNING" || echo "Service: STOPPED"); echo "-- Mode --"; (awk '$2=="/" && $3=="overlay"{found=1} END{if(found) print "Kiosk mode: YES (overlay FS)"; else print "Kiosk mode: NO (regular)"}' /proc/mounts); echo "-- Preset Persistence --"; (mountpoint -q /home/pi/dubsiren/data 2>/dev/null && echo "Data bind-mount: OK (persistent)" || (mountpoint -q /mnt/persist 2>/dev/null && echo "Persist mount: OK (/mnt/persist)" || echo "Persist mount: NOT FOUND")); PFILE=/home/pi/dubsiren/data/presets/user_presets.txt; (test -f "$PFILE" && echo "Preset file: EXISTS ($PFILE)" || echo "Preset file: MISSING"); (touch "$PFILE.healthcheck" 2>/dev/null && echo "Preset dir: WRITABLE (presets will persist)" || echo "Preset dir: READ-ONLY (presets will be LOST on reboot)"); rm -f "$PFILE.healthcheck" 2>/dev/null
```

Sample output on a healthy kiosk install:
```
=== Dub Siren Health Check ===
-- Install --
Binary: OK
Autostart: ON
Service: RUNNING
-- Mode --
Kiosk mode: YES (overlay FS)
-- Preset Persistence --
Data bind-mount: OK (persistent)
Preset file: EXISTS (/home/pi/dubsiren/data/presets/user_presets.txt)
Preset dir: WRITABLE (presets will persist)
```
