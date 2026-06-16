#!/usr/bin/env bash
# update-service.sh — Ensure the systemd service file matches the repo version
#
# Called by the OTA updater after build+deploy.  Patches the service file
# in-place so that fixes to User=, capabilities, etc. are applied without
# needing a full reinstall.
#
# Usage:  sudo scripts/update-service.sh [install_dir]
#         (run from the repo root or pass install_dir explicitly)

set -eu

INSTALL_DIR="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
SERVICE=/etc/systemd/system/dubsiren.service
CHANGED=false

if [[ ! -f "$SERVICE" ]]; then
    echo "Service file not found — skipping (not installed via setup.sh?)"
    exit 0
fi

# --- Ensure service runs as root (required for /dev/mem LED access) ---------
if grep -q '^User=' "$SERVICE" && ! grep -q '^User=root' "$SERVICE"; then
    sed -i 's/^User=.*/User=root/' "$SERVICE"
    echo "Patched User= → root"
    CHANGED=true
fi

# --- Ensure ExecStart points to current install dir -------------------------
EXPECTED_EXEC="ExecStart=${INSTALL_DIR}/build/dubsiren"
if ! grep -qF "$EXPECTED_EXEC" "$SERVICE"; then
    sed -i "s|^ExecStart=.*|${EXPECTED_EXEC}|" "$SERVICE"
    echo "Patched ExecStart → ${INSTALL_DIR}/build/dubsiren"
    CHANGED=true
fi

# --- Ensure WorkingDirectory matches ----------------------------------------
EXPECTED_WD="WorkingDirectory=${INSTALL_DIR}"
if ! grep -qF "$EXPECTED_WD" "$SERVICE"; then
    sed -i "s|^WorkingDirectory=.*|${EXPECTED_WD}|" "$SERVICE"
    echo "Patched WorkingDirectory → ${INSTALL_DIR}"
    CHANGED=true
fi

# --- Ensure git safe.directory for root (service runs as root) ---------------
# Without this, all git operations (OTA updates, branch listing) fail with
# "detected dubious ownership" because the repo is owned by a non-root user.
if ! git config --global --get safe.directory "$INSTALL_DIR" >/dev/null 2>&1; then
    git config --global --add safe.directory "$INSTALL_DIR"
    echo "Added git safe.directory for $INSTALL_DIR"
    CHANGED=true
fi

# --- Ensure OTA origin points at the current (Clingman Ave Labs) repo --------
# Devices first installed from the old repo still have its URL as origin, so
# OTA updates would keep pulling from there.  Pin origin to the canonical repo.
# Idempotent: only rewrites when the URL differs.
EXPECTED_ORIGIN="https://github.com/Clingman-Ave-Labs/poorhouse-lane-siren.git"
CURRENT_ORIGIN="$(git -C "$INSTALL_DIR" -c safe.directory="$INSTALL_DIR" remote get-url origin 2>/dev/null || true)"
if [[ -n "$CURRENT_ORIGIN" && "$CURRENT_ORIGIN" != "$EXPECTED_ORIGIN" ]]; then
    git -C "$INSTALL_DIR" -c safe.directory="$INSTALL_DIR" remote set-url origin "$EXPECTED_ORIGIN"
    echo "Repointed origin → ${EXPECTED_ORIGIN}"
    CHANGED=true
fi

# --- Reload systemd if anything changed -------------------------------------
if $CHANGED; then
    systemctl daemon-reload
    echo "systemd reloaded."
else
    echo "Service file already up to date."
fi
