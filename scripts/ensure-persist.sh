#!/usr/bin/env bash
# ensure-persist.sh — Ensure persistent storage is available for dubsiren
#
# Installed to /usr/local/lib/dubsiren/ (root-owned) by setup.sh.
# Called as ExecStartPre=+ in the dubsiren systemd service.  Tries multiple
# strategies to make INSTALL_DIR/data a durable, writable mount.  Exits 0
# even on total failure so the siren still starts (the C++ code handles
# volatile storage gracefully with warnings).
#
# Strategies (tried in order):
#   1. Data dir is already a mount point (systemd unit or previous run)
#   2. Mount root device rw at /mnt/persist, bind-mount data from there
#   3. Find the overlay lower-layer mount and bind-mount data from it
#   4. All failed — exit 0 anyway so dubsiren starts with volatile storage

set -u

INSTALL_DIR="${1:?Usage: ensure-persist.sh <install-dir>}"
DATA_DIR="${INSTALL_DIR}/data"
PERSIST_MNT="/mnt/persist"
LOG_TAG="ensure-persist"

log()  { echo "[${LOG_TAG}] $*"; }
warn() { echo "[${LOG_TAG}] WARNING: $*" >&2; }

# --- Helper: is $1 a mount point? -------------------------------------------
is_mount_point() {
    local dev parent_dev
    dev=$(stat -c '%d' "$1" 2>/dev/null) || return 1
    parent_dev=$(stat -c '%d' "$1/.." 2>/dev/null) || return 1
    [[ "$dev" != "$parent_dev" ]]
}

# --- Helper: is root an overlay filesystem? ----------------------------------
is_overlay_root() {
    awk '$2 == "/" && $3 == "overlay" { found=1 } END { exit !found }' /proc/mounts
}

# --- Helper: mkdir that rejects symlinks (prevents symlink-to-/etc attacks) --
safe_mkdir() {
    if [[ -L "$1" ]]; then
        warn "Security: $1 is a symlink — refusing to mount"
        return 1
    fi
    mkdir -p "$1"
}

# --- Helper: fix ownership of data dirs on persist mount --------------------
# When this script creates directories, they're owned by root.  The dubsiren
# service runs as a regular user who cannot write to root-owned 755 dirs.
# Derive the correct owner from the install dir on the persist mount.
fix_data_ownership() {
    local persist_data="$1"
    local persist_install="${PERSIST_MNT}${INSTALL_DIR}"
    local dir_owner=""

    if [[ -d "$persist_install" ]]; then
        dir_owner=$(stat -c '%U:%G' "$persist_install" 2>/dev/null)
    fi
    if [[ -z "$dir_owner" || "$dir_owner" == "root:root" ]]; then
        # Try parent directory (e.g., /mnt/persist/home/pi)
        dir_owner=$(stat -c '%U:%G' "$(dirname "$persist_install")" 2>/dev/null)
    fi

    if [[ -n "$dir_owner" && "$dir_owner" != "root:root" ]]; then
        chown -R "$dir_owner" "$persist_data" 2>/dev/null || true
        log "Set ownership of ${persist_data} to ${dir_owner}"
    else
        # Last resort: make world-writable so the app can write regardless
        chmod -R a+rwX "$persist_data" || true
        warn "Could not determine app user — made ${persist_data} world-writable"
    fi
}

# --- Strategy 1: Already mounted? -------------------------------------------
if is_mount_point "${DATA_DIR}"; then
    # Check if the mount is actually writable
    if touch "${DATA_DIR}/.rw-test" 2>/dev/null; then
        rm -f "${DATA_DIR}/.rw-test"
        log "Data dir already mounted (rw) at ${DATA_DIR} — done."
        exit 0
    fi

    # Data dir is mounted but read-only.  The overlayroot package forces the
    # root block device ro, so both /mnt/persist and the bind mount inherit ro.
    # Remount /mnt/persist rw first, then remount the bind mount.
    log "Data dir mounted at ${DATA_DIR} but read-only — fixing..."

    if is_mount_point "${PERSIST_MNT}"; then
        if mount -o remount,rw "${PERSIST_MNT}" 2>/dev/null; then
            log "Remounted ${PERSIST_MNT} as read-write."
        else
            warn "Failed to remount ${PERSIST_MNT} rw."
        fi
    fi

    if mount -o remount,rw "${DATA_DIR}" 2>/dev/null; then
        log "Remounted ${DATA_DIR} as read-write — persistent storage ready."
        exit 0
    fi

    # Remount failed — unmount the ro bind and fall through to try again
    warn "Could not remount ${DATA_DIR} rw — unmounting to retry..."
    umount "${DATA_DIR}" 2>/dev/null || true
fi

# If root is not overlay, no special handling needed — data dir is on rw disk
if ! is_overlay_root; then
    log "Not on overlay FS — data dir is on normal rw filesystem."
    exit 0
fi

log "Overlay FS detected — attempting to set up persistent storage..."

# --- Strategy 2: Mount root device rw at /mnt/persist -----------------------
# Find the root block device using an exhaustive detection chain.
# Different Pi models, OS versions, and boot methods present the root device
# differently.  We try every available method to ensure reliability.
ROOT_DEV=""
ROOT_FSTYPE=""

# --- Detection 1: /proc/cmdline root= parameter (authoritative) ------------
# The kernel was told which device to boot from — this is the most reliable
# source and works across all Pi models and OS versions.
CMDLINE_ROOT=$(sed -n 's/.*\broot=\(\S\+\).*/\1/p' /proc/cmdline)
if [[ -n "$CMDLINE_ROOT" ]]; then
    case "$CMDLINE_ROOT" in
        PARTUUID=*|UUID=*)
            ROOT_DEV=$(blkid -t "$CMDLINE_ROOT" -o device 2>/dev/null | head -1)
            ;;
        /dev/*)
            ROOT_DEV="$CMDLINE_ROOT"
            ;;
    esac
    if [[ -n "$ROOT_DEV" && -b "$ROOT_DEV" ]]; then
        ROOT_FSTYPE=$(blkid -s TYPE -o value "$ROOT_DEV" 2>/dev/null)
        ROOT_FSTYPE="${ROOT_FSTYPE:-ext4}"
        log "Detection 1: root device from /proc/cmdline: ${ROOT_DEV} (${ROOT_FSTYPE})"
    else
        warn "Detection 1: /proc/cmdline root=${CMDLINE_ROOT} could not be resolved"
        ROOT_DEV=""
    fi
fi

# --- Detection 2: overlay lowerdir= mount option ---------------------------
# The overlay mount knows its own lower layer.  Find the device behind it.
if [[ -z "$ROOT_DEV" ]]; then
    LOWER_MP=$(grep ' / overlay ' /proc/mounts | sed -n 's/.*lowerdir=\([^,:]*\).*/\1/p' | head -1)
    if [[ -n "$LOWER_MP" && -d "$LOWER_MP" ]]; then
        ROOT_DEV=$(findmnt -n -o SOURCE "$LOWER_MP" 2>/dev/null | head -1)
        if [[ -n "$ROOT_DEV" && -b "$ROOT_DEV" ]]; then
            ROOT_FSTYPE=$(blkid -s TYPE -o value "$ROOT_DEV" 2>/dev/null)
            ROOT_FSTYPE="${ROOT_FSTYPE:-ext4}"
            log "Detection 2: root device from overlay lowerdir: ${ROOT_DEV} (${ROOT_FSTYPE})"
        else
            ROOT_DEV=""
        fi
    fi
fi

# --- Detection 3: known overlay lower-layer mount points --------------------
if [[ -z "$ROOT_DEV" ]]; then
    for candidate in /media/root-ro /overlay/lower /mnt/lower /mnt/base; do
        if [[ -d "$candidate" ]]; then
            ROOT_DEV=$(findmnt -n -o SOURCE "$candidate" 2>/dev/null | head -1)
            if [[ -n "$ROOT_DEV" && -b "$ROOT_DEV" ]]; then
                ROOT_FSTYPE=$(blkid -s TYPE -o value "$ROOT_DEV" 2>/dev/null)
                ROOT_FSTYPE="${ROOT_FSTYPE:-ext4}"
                log "Detection 3: root device from ${candidate}: ${ROOT_DEV} (${ROOT_FSTYPE})"
                break
            fi
            ROOT_DEV=""
        fi
    done
fi

# --- Detection 4: /proc/mounts scan (improved) -----------------------------
# Skip vfat/boot partitions; prefer mounts at overlay-like paths.
if [[ -z "$ROOT_DEV" ]]; then
    while IFS=' ' read -r dev mp fstype _rest; do
        # Skip virtual/pseudo and boot filesystems
        if [[ "$fstype" =~ ^(overlay|tmpfs|devtmpfs|proc|sysfs|devpts|cgroup2?|fuse|vfat|fat32) ]]; then
            continue
        fi
        if [[ "$dev" != /dev/* ]]; then
            continue
        fi
        if [[ "$dev" == *mmcblk* || "$dev" == *nvme* || "$dev" == *sd* ]]; then
            ROOT_DEV="$dev"
            ROOT_FSTYPE="$fstype"
            if [[ "$mp" == */lower* || "$mp" == */ro* || "$mp" == */base* ]]; then
                break
            fi
        fi
    done < /proc/mounts
    if [[ -n "$ROOT_DEV" ]]; then
        log "Detection 4: root device from /proc/mounts scan: ${ROOT_DEV} (${ROOT_FSTYPE})"
    fi
fi

# --- Detection 5: common device names (last resort) ------------------------
if [[ -z "$ROOT_DEV" ]]; then
    for candidate in /dev/mmcblk0p2 /dev/sda2 /dev/nvme0n1p2 /dev/sda1; do
        if [[ -b "$candidate" ]]; then
            ROOT_DEV="$candidate"
            ROOT_FSTYPE=$(blkid -s TYPE -o value "$ROOT_DEV" 2>/dev/null)
            ROOT_FSTYPE="${ROOT_FSTYPE:-ext4}"
            log "Detection 5: trying common device ${ROOT_DEV} (${ROOT_FSTYPE})"
            break
        fi
    done
fi

if [[ -n "$ROOT_DEV" ]]; then
    log "Found root device: ${ROOT_DEV} (${ROOT_FSTYPE})"

    # Try to mount it rw at /mnt/persist (may already be mounted by systemd unit)
    if is_mount_point "${PERSIST_MNT}"; then
        log "Persist mount already active at ${PERSIST_MNT}."
        # The systemd unit or overlayroot may have mounted it read-only.
        # Remount rw so we can actually write presets.
        if ! touch "${PERSIST_MNT}/.rw-test" 2>/dev/null; then
            log "Persist mount is read-only — remounting rw..."
            if mount -o remount,rw "${PERSIST_MNT}" 2>/dev/null; then
                log "Remounted ${PERSIST_MNT} as read-write."
            else
                warn "Failed to remount ${PERSIST_MNT} rw."
            fi
        else
            rm -f "${PERSIST_MNT}/.rw-test"
        fi
    else
        mkdir -p "${PERSIST_MNT}"
        for attempt in 1 2 3; do
            if mount -t "${ROOT_FSTYPE}" -o rw,noatime "${ROOT_DEV}" "${PERSIST_MNT}" 2>/dev/null; then
                log "Mounted ${ROOT_DEV} rw at ${PERSIST_MNT} (attempt ${attempt})."
                break
            fi
            warn "Mount attempt ${attempt}/3 failed — retrying..."
            sleep 1
        done
    fi

    # Now try to bind-mount the data directory
    if is_mount_point "${PERSIST_MNT}" && [[ -d "${PERSIST_MNT}${DATA_DIR}" ]]; then
        # Ensure correct ownership on existing dirs (may be root-owned from
        # a previous run or a different setup)
        fix_data_ownership "${PERSIST_MNT}${DATA_DIR}"
        if safe_mkdir "${DATA_DIR}"; then
            if mount --bind "${PERSIST_MNT}${DATA_DIR}" "${DATA_DIR}"; then
                log "Bind-mounted ${PERSIST_MNT}${DATA_DIR} → ${DATA_DIR} — persistent storage ready."
                exit 0
            fi
            warn "Bind mount failed."
        fi
    elif is_mount_point "${PERSIST_MNT}"; then
        # Data dir doesn't exist on persist mount yet — create it
        mkdir -p "${PERSIST_MNT}${DATA_DIR}/mp3s" "${PERSIST_MNT}${DATA_DIR}/presets"
        fix_data_ownership "${PERSIST_MNT}${DATA_DIR}"
        if safe_mkdir "${DATA_DIR}"; then
            if mount --bind "${PERSIST_MNT}${DATA_DIR}" "${DATA_DIR}"; then
                log "Created and bind-mounted ${PERSIST_MNT}${DATA_DIR} → ${DATA_DIR} — persistent storage ready."
                exit 0
            fi
            warn "Bind mount failed after creating directories."
        fi
    fi
fi

# --- Strategy 3: Find overlay lower-layer mount and bind from it ------------
log "Strategy 2 failed — looking for overlay lower layer..."

LOWER_PATH=""
# Check common raspi-config overlay lower-layer paths
for candidate in /overlay/lower /media/root-ro /mnt/lower; do
    if [[ -d "${candidate}${DATA_DIR}" ]]; then
        LOWER_PATH="${candidate}"
        break
    fi
done

# Also parse overlay mount options for lowerdir=
if [[ -z "$LOWER_PATH" ]]; then
    LOWER_PATH=$(awk '$2 == "/" && $3 == "overlay" {
        n = split($4, opts, ",")
        for (i = 1; i <= n; i++) {
            if (opts[i] ~ /^lowerdir=/) {
                sub(/^lowerdir=/, "", opts[i])
                # lowerdir can have multiple colon-separated paths; take first
                split(opts[i], parts, ":")
                print parts[1]
                exit
            }
        }
    }' /proc/mounts)
fi

if [[ -n "$LOWER_PATH" && -d "${LOWER_PATH}${DATA_DIR}" ]]; then
    log "Found overlay lower layer at ${LOWER_PATH}"

    # Do NOT remount the lower layer rw — that would undermine overlay FS
    # protection.  Bind-mount as-is and test if it happens to be writable.
    if safe_mkdir "${DATA_DIR}"; then
        if mount --bind "${LOWER_PATH}${DATA_DIR}" "${DATA_DIR}"; then
            # Check if the bind mount is actually writable
            if touch "${DATA_DIR}/.persist-test" 2>/dev/null; then
                rm -f "${DATA_DIR}/.persist-test"
                log "Bind-mounted from overlay lower layer — persistent storage ready."
                exit 0
            else
                warn "Lower-layer bind mount is read-only — presets won't persist."
                umount "${DATA_DIR}" 2>/dev/null || true
            fi
        fi
    fi
fi

# --- All strategies failed ---------------------------------------------------
warn "Could not set up persistent storage."
warn "Dubsiren will start with volatile storage — presets will be lost on power cycle."
warn "Check: systemctl status mnt-persist.mount"
exit 0
