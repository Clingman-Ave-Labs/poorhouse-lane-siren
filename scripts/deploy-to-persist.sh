#!/usr/bin/env bash
# deploy-to-persist.sh — Copy built binary to persistent storage on overlay FS
#
# On kiosk mode (overlay FS), builds happen in volatile RAM and are lost on
# reboot.  This script copies the binary and helper scripts through /mnt/persist
# to the real disk so they survive power cycles.
#
# On non-overlay systems this is a no-op.
#
# Usage:  sudo scripts/deploy-to-persist.sh
#         (run from the repo root after building)

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PERSIST_MNT="/mnt/persist"

# --- Helpers (same as ensure-persist.sh) ------------------------------------
is_overlay_root() {
    awk '$2 == "/" && $3 == "overlay" { found=1 } END { exit !found }' /proc/mounts
}

is_mount_point() {
    local dev parent_dev
    dev=$(stat -c '%d' "$1" 2>/dev/null) || return 1
    parent_dev=$(stat -c '%d' "$1/.." 2>/dev/null) || return 1
    [[ "$dev" != "$parent_dev" ]]
}

# --- Skip on non-overlay systems -------------------------------------------
if ! is_overlay_root; then
    echo "Not on overlay FS — no deploy needed (binary is already on rw disk)."
    exit 0
fi

echo "Overlay FS detected — deploying to persistent storage..."

# --- Ensure /mnt/persist is mounted rw -------------------------------------
if is_mount_point "$PERSIST_MNT"; then
    # May be mounted ro by overlayroot — remount rw if needed
    if ! touch "${PERSIST_MNT}/.rw-test" 2>/dev/null; then
        echo "Persist mount is read-only — remounting rw..."
        mount -o remount,rw "$PERSIST_MNT"
    else
        rm -f "${PERSIST_MNT}/.rw-test"
    fi
elif ! is_mount_point "$PERSIST_MNT"; then
    CMDLINE_ROOT=$(sed -n 's/.*\broot=\(\S\+\).*/\1/p' /proc/cmdline)
    ROOT_DEV=""
    case "$CMDLINE_ROOT" in
        PARTUUID=*|UUID=*)
            ROOT_DEV=$(blkid -t "$CMDLINE_ROOT" -o device 2>/dev/null | head -1)
            ;;
        /dev/*)
            ROOT_DEV="$CMDLINE_ROOT"
            ;;
    esac
    if [[ -z "$ROOT_DEV" || ! -b "$ROOT_DEV" ]]; then
        echo "ERROR: Cannot determine root device to mount $PERSIST_MNT" >&2
        exit 1
    fi
    ROOT_FSTYPE=$(blkid -s TYPE -o value "$ROOT_DEV" 2>/dev/null)
    ROOT_FSTYPE="${ROOT_FSTYPE:-ext4}"
    mkdir -p "$PERSIST_MNT"
    mount -t "$ROOT_FSTYPE" -o rw,noatime "$ROOT_DEV" "$PERSIST_MNT"
    echo "Mounted $ROOT_DEV at $PERSIST_MNT"
fi

# --- Verify binary exists --------------------------------------------------
if [[ ! -f "${SCRIPT_DIR}/build/dubsiren" ]]; then
    echo "ERROR: ${SCRIPT_DIR}/build/dubsiren not found — build first" >&2
    exit 1
fi

# --- Copy binary ------------------------------------------------------------
DEST="${PERSIST_MNT}${SCRIPT_DIR}/build"
mkdir -p "$DEST"
cp "${SCRIPT_DIR}/build/dubsiren" "$DEST/dubsiren"
echo "Deployed binary → ${DEST}/dubsiren"

# --- Copy helper scripts ----------------------------------------------------
SCRIPT_DEST="${PERSIST_MNT}/usr/local/lib/dubsiren"
mkdir -p "$SCRIPT_DEST"
cp "${SCRIPT_DIR}/scripts/ensure-persist.sh" "$SCRIPT_DEST/ensure-persist.sh"
chmod 755 "$SCRIPT_DEST/ensure-persist.sh"
echo "Deployed ensure-persist.sh → ${SCRIPT_DEST}/"

# Also deploy update-service.sh to the persistent repo so OTA can call it
PERSIST_SCRIPTS="${PERSIST_MNT}${SCRIPT_DIR}/scripts"
mkdir -p "$PERSIST_SCRIPTS"
for s in update-service.sh deploy-to-persist.sh ensure-persist.sh; do
    if [[ -f "${SCRIPT_DIR}/scripts/${s}" ]]; then
        cp "${SCRIPT_DIR}/scripts/${s}" "${PERSIST_SCRIPTS}/${s}"
        chmod 755 "${PERSIST_SCRIPTS}/${s}"
    fi
done
echo "Deployed scripts → ${PERSIST_SCRIPTS}/"

# --- Sync source tree to persistent disk ------------------------------------
# Update the git repo on the real disk so that after reboot the overlay
# lower layer matches the version we just built.  Without this, the repo
# would revert to the pre-update state on every reboot.
PERSIST_REPO="${PERSIST_MNT}${SCRIPT_DIR}"
if [[ -d "${PERSIST_REPO}/.git" ]]; then
    echo "Syncing source tree to persistent disk..."
    # Sync git objects so the persistent repo has the same history
    rsync -a --delete "${SCRIPT_DIR}/.git/" "${PERSIST_REPO}/.git/"
    # Checkout the working tree to match (but preserve data/ dir)
    (cd "${PERSIST_REPO}" && git checkout -f HEAD -- . 2>/dev/null) || true
    echo "Source tree synced."
else
    echo "Note: No git repo on persistent disk — source not synced."
fi

# --- Preserve user data (safety measure) ------------------------------------
# The data/ directory is bind-mounted from persistent storage, so it's already
# durable.  Just verify it exists on the persist mount.
PERSIST_DATA="${PERSIST_MNT}${SCRIPT_DIR}/data"
if [[ ! -d "${PERSIST_DATA}/presets" ]]; then
    mkdir -p "${PERSIST_DATA}/presets" "${PERSIST_DATA}/config"
    echo "Created data directories on persistent storage."
fi

sync
echo "Deploy complete — changes will survive reboot."
