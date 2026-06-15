#!/usr/bin/env bash
set -euo pipefail

# =============================================================================
# Poorhouse Lane Siren V4 — Setup Script
# Raspberry Pi Zero 2W Dub Siren Installer
# =============================================================================

# --- Color helpers -----------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()    { echo -e "${BLUE}[INFO]${NC}  $1"; }
success() { echo -e "${GREEN}[OK]${NC}    $1"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $1"; }
error()   { echo -e "${RED}[ERROR]${NC} $1"; }

# --- Global variables --------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
REPO_URL="https://github.com/parkredding/poorhouse-lane-v4.git"
REPO_BRANCH="${REPO_BRANCH:-main}"
IS_PI_ZERO=false
BOOT_CHANGED=false
# Inherit from env if set (survives clone_if_needed re-exec)
ENABLE_AUTOSTART="${ENABLE_AUTOSTART:-}"
ENABLE_KIOSK="${ENABLE_KIOSK:-}"

# --- check_root() ------------------------------------------------------------
check_root() {
    if [[ "$EUID" -ne 0 ]]; then
        error "This script must be run as root (sudo bash setup.sh)"
        exit 1
    fi

    echo ""
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}  Poorhouse Lane Siren V4 — Setup${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""

    # Detect real user (the one who ran sudo)
    REAL_USER="${SUDO_USER:-pi}"
    REAL_HOME=$(eval echo "~${REAL_USER}")
    INSTALL_DIR="${REAL_HOME}/dubsiren"

    info "Installing for user: ${REAL_USER}"
    info "Install directory:   ${INSTALL_DIR}"
    echo ""
}

# --- ask_options() -----------------------------------------------------------
ask_options() {
    # Skip prompts if already configured (re-exec from clone_if_needed)
    if [[ -n "$ENABLE_AUTOSTART" && -n "$ENABLE_KIOSK" ]]; then
        info "Autostart: ${ENABLE_AUTOSTART}"
        info "Kiosk mode: ${ENABLE_KIOSK}"
        return 0
    fi

    echo -e "${BLUE}Configuration Options${NC}"
    echo ""

    # Read from /dev/tty so prompts work even when piped (curl | bash)
    # Autostart prompt — default Y
    read -rp "  Start dubsiren automatically on boot? [Y/n]: " autostart_choice < /dev/tty
    if [[ "$autostart_choice" =~ ^[Nn]$ ]]; then
        ENABLE_AUTOSTART=false
        info "Autostart: disabled"
    else
        ENABLE_AUTOSTART=true
        info "Autostart: enabled"
    fi

    echo ""

    # Kiosk mode prompt — default N
    echo -e "  Kiosk mode turns this Pi into a dedicated dub siren appliance:"
    echo -e "    ${YELLOW}•${NC} Console auto-login (no password prompt)"
    echo -e "    ${YELLOW}•${NC} CLI-only boot (disables desktop if installed)"
    echo -e "    ${YELLOW}•${NC} HDMI output disabled to save power"
    echo -e "    ${YELLOW}•${NC} Screen blanking disabled"
    echo -e "    ${YELLOW}•${NC} Read-only filesystem (protects SD card from corruption)"
    echo -e "    ${YELLOW}•${NC} Writable data/ directory preserved for presets & mp3s"
    echo ""
    read -rp "  Enable kiosk mode? [y/N]: " kiosk_choice < /dev/tty
    if [[ "$kiosk_choice" =~ ^[Yy]$ ]]; then
        ENABLE_KIOSK=true
        info "Kiosk mode: enabled"
    else
        ENABLE_KIOSK=false
        info "Kiosk mode: disabled"
    fi

    echo ""

    # Export so clone_if_needed re-exec preserves choices
    export ENABLE_AUTOSTART ENABLE_KIOSK
}

# --- detect_pi() -------------------------------------------------------------
detect_pi() {
    if [[ -f /proc/device-tree/model ]]; then
        local model
        model=$(tr -d '\0' < /proc/device-tree/model)
        info "Detected: ${model}"
        if [[ "$model" == *"Zero"* ]]; then
            IS_PI_ZERO=true
        fi
    else
        warn "Not running on a Raspberry Pi."
        warn "Continuing anyway for development purposes."
    fi
}

# --- ensure_git() ------------------------------------------------------------
ensure_git() {
    if command -v git &>/dev/null; then
        return 0
    fi
    info "Git not found. Installing..."
    apt-get update -qq
    apt-get install -y git
    success "Git installed."
}

# --- clone_if_needed() -------------------------------------------------------
clone_if_needed() {
    # If CMakeLists.txt exists alongside this script, we're inside the repo
    if [[ -f "${SCRIPT_DIR}/CMakeLists.txt" ]]; then
        return 0
    fi

    info "Repository not found locally. Cloning to ${INSTALL_DIR}..."
    git clone -b "${REPO_BRANCH}" "${REPO_URL}" "${INSTALL_DIR}"
    chown -R "${REAL_USER}:${REAL_USER}" "${INSTALL_DIR}"
    info "Re-running setup from cloned repo..."
    exec bash "${INSTALL_DIR}/setup.sh"
}

# --- setup_swap() ------------------------------------------------------------
setup_swap() {
    if [[ "$IS_PI_ZERO" != true ]]; then
        info "Not a Pi Zero — skipping swap setup."
        return 0
    fi

    if [[ -f /swapfile ]] && grep -q '/swapfile' /proc/swaps 2>/dev/null; then
        success "Swap file already active."
        return 0
    fi

    info "Creating 1GB swap file (essential for Pi Zero builds)..."

    # Disable dphys-swapfile if present to avoid conflicts
    if systemctl is-active --quiet dphys-swapfile 2>/dev/null; then
        systemctl stop dphys-swapfile
        systemctl disable dphys-swapfile
    fi

    dd if=/dev/zero of=/swapfile bs=1M count=1024 status=progress
    chmod 600 /swapfile
    mkswap /swapfile
    swapon /swapfile

    # Make persistent across reboots
    if ! grep -q '/swapfile' /etc/fstab; then
        echo '/swapfile none swap sw 0 0' >> /etc/fstab
    fi

    success "1GB swap file created and activated."
}

# --- install_deps() ----------------------------------------------------------
install_deps() {
    info "Installing build dependencies..."
    apt-get update -qq
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        libasound2-dev \
        libgpiod-dev \
        gpiod \
        hostapd \
        dnsmasq \
        avahi-daemon \
        avahi-utils
    # Disable hostapd/dnsmasq at boot — AP mode starts them on demand
    systemctl disable hostapd 2>/dev/null || true
    systemctl stop hostapd 2>/dev/null || true
    systemctl disable dnsmasq 2>/dev/null || true
    systemctl stop dnsmasq 2>/dev/null || true
    # Enable avahi for mDNS — the siren process publishes poorhouse.local
    # as an mDNS alias at runtime via avahi-publish, so the system hostname
    # set by Pi Imager (e.g. poorhouse34) is preserved for SSH access.
    systemctl enable avahi-daemon 2>/dev/null || true
    systemctl start avahi-daemon 2>/dev/null || true
    success "Dependencies installed."
}

# --- build_rpi_ws281x() ------------------------------------------------------
build_rpi_ws281x() {
    local have_lib=false have_hdr=false
    [[ -f /usr/local/lib/libws2811.so ]] || [[ -f /usr/local/lib/libws2811.a ]] && have_lib=true
    [[ -f /usr/local/include/ws2811.h ]] && have_hdr=true

    if $have_lib && $have_hdr; then
        success "rpi_ws281x already installed."
        return 0
    fi

    info "Building rpi_ws281x from source..."
    local build_dir
    build_dir=$(mktemp -d)

    git clone https://github.com/jgarff/rpi_ws281x.git "${build_dir}/rpi_ws281x"
    mkdir -p "${build_dir}/rpi_ws281x/build"
    cd "${build_dir}/rpi_ws281x/build"
    cmake -DBUILD_SHARED=ON -DBUILD_TEST=OFF ..

    # Use fewer jobs on Pi Zero to avoid OOM
    if [[ "$IS_PI_ZERO" == true ]]; then
        make -j2
    else
        make -j"$(nproc)"
    fi

    make install
    ldconfig

    # Ensure headers are findable at /usr/local/include/ (not just a subdirectory).
    # Some cmake versions install to /usr/local/include/ws2811/ — copy them up.
    for hdr in ws2811.h rpihw.h pwm.h clk.h dma.h gpio.h pcm.h; do
        if [[ ! -f "/usr/local/include/${hdr}" ]]; then
            if [[ -f "/usr/local/include/ws2811/${hdr}" ]]; then
                cp "/usr/local/include/ws2811/${hdr}" /usr/local/include/
                info "Copied ${hdr} from ws2811/ subdirectory"
            elif [[ -f "${build_dir}/rpi_ws281x/${hdr}" ]]; then
                cp "${build_dir}/rpi_ws281x/${hdr}" /usr/local/include/
                info "Installed missing header: ${hdr}"
            fi
        fi
    done

    # Clean up
    rm -rf "${build_dir}"
    cd "${SCRIPT_DIR}"

    success "rpi_ws281x installed."
}

# --- configure_boot() --------------------------------------------------------
configure_boot() {
    local boot_config=""
    if [[ -e /boot/firmware/config.txt ]]; then
        boot_config="/boot/firmware/config.txt"
    elif [[ -e /boot/config.txt ]]; then
        boot_config="/boot/config.txt"
    else
        warn "No boot config found. Skipping boot configuration."
        warn "If you're not on a Raspberry Pi, this is expected."
        return 0
    fi

    info "Configuring ${boot_config} for I2S DAC..."
    BOOT_CHANGED=false

    # Enable I2S
    if grep -q '^dtparam=i2s=on' "${boot_config}"; then
        success "I2S already enabled."
    elif grep -q '^#\s*dtparam=i2s=on' "${boot_config}"; then
        # Backup before first change
        if [[ "$BOOT_CHANGED" == false ]]; then
            cp "${boot_config}" "${boot_config}.bak.$(date +%Y%m%d%H%M%S)"
        fi
        sed -i 's/^#\s*dtparam=i2s=on/dtparam=i2s=on/' "${boot_config}"
        BOOT_CHANGED=true
        info "Uncommented dtparam=i2s=on"
    else
        if [[ "$BOOT_CHANGED" == false ]]; then
            cp "${boot_config}" "${boot_config}.bak.$(date +%Y%m%d%H%M%S)"
        fi
        echo 'dtparam=i2s=on' >> "${boot_config}"
        BOOT_CHANGED=true
        info "Added dtparam=i2s=on"
    fi

    # Add HiFiBerry DAC overlay
    if grep -q '^dtoverlay=hifiberry-dac' "${boot_config}"; then
        success "HiFiBerry DAC overlay already configured."
    else
        if [[ "$BOOT_CHANGED" == false ]]; then
            cp "${boot_config}" "${boot_config}.bak.$(date +%Y%m%d%H%M%S)"
        fi
        echo 'dtoverlay=hifiberry-dac' >> "${boot_config}"
        BOOT_CHANGED=true
        info "Added dtoverlay=hifiberry-dac"
    fi

    # Disable onboard audio
    if grep -q '^dtparam=audio=on' "${boot_config}"; then
        if [[ "$BOOT_CHANGED" == false ]]; then
            cp "${boot_config}" "${boot_config}.bak.$(date +%Y%m%d%H%M%S)"
        fi
        sed -i 's/^dtparam=audio=on/#dtparam=audio=on/' "${boot_config}"
        BOOT_CHANGED=true
        info "Commented out dtparam=audio=on (disabled onboard audio)"
    else
        success "Onboard audio already disabled or not present."
    fi

    if [[ "$BOOT_CHANGED" == true ]]; then
        warn "Boot configuration changed. A reboot is required."
    else
        success "Boot configuration already up to date."
    fi
}

# --- configure_alsa() --------------------------------------------------------
configure_alsa() {
    info "Writing ALSA configuration for PCM5102 DAC..."

    cat > /etc/asound.conf << 'ALSA_EOF'
pcm.!default {
    type hw
    card 0
}

ctl.!default {
    type hw
    card 0
}
ALSA_EOF

    success "ALSA config written to /etc/asound.conf"
}

# --- create_data_dirs() ------------------------------------------------------
create_data_dirs() {
    mkdir -p "${INSTALL_DIR}/data/mp3s"
    mkdir -p "${INSTALL_DIR}/data/presets"
    mkdir -p "${INSTALL_DIR}/data/config"
    chown -R "${REAL_USER}:${REAL_USER}" "${INSTALL_DIR}/data"
    success "Data directories created at ${INSTALL_DIR}/data/{mp3s,presets,config}"
}

# --- install_service() -------------------------------------------------------
install_service() {
    info "Installing systemd service..."

    # Install the persistent-storage helper to a root-owned location so that
    # the unprivileged service user cannot modify a script that runs as root.
    install -D -m 755 -o root -g root \
        "${SCRIPT_DIR}/scripts/ensure-persist.sh" \
        /usr/local/lib/dubsiren/ensure-persist.sh
    success "ensure-persist.sh installed to /usr/local/lib/dubsiren/"

    local data_mount_unit
    data_mount_unit="$(systemd-escape --path "${INSTALL_DIR}/data").mount"

    cat > /etc/systemd/system/dubsiren.service << EOF
[Unit]
Description=Poorhouse Lane Dub Siren V4
Wants=sound.target
After=sound.target ${data_mount_unit}
Wants=${data_mount_unit}

[Service]
Type=simple
ExecStartPre=+/usr/local/lib/dubsiren/ensure-persist.sh ${INSTALL_DIR}
ExecStart=${INSTALL_DIR}/build/dubsiren
WorkingDirectory=${INSTALL_DIR}
Restart=on-failure
RestartSec=3
StartLimitIntervalSec=0
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=80
Nice=-20
LimitRTPRIO=99
LimitMEMLOCK=infinity
User=root
Environment=HOME=${REAL_HOME}

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload

    # Service runs as root but repo is owned by the install user — git's
    # safe.directory check would block all git operations (OTA updates).
    git config --global --get safe.directory "${INSTALL_DIR}" >/dev/null 2>&1 || \
        git config --global --add safe.directory "${INSTALL_DIR}"
    success "git safe.directory configured for root"

    if [[ "$ENABLE_AUTOSTART" == true ]]; then
        systemctl enable dubsiren.service
        success "dubsiren.service installed and enabled."
    else
        systemctl disable dubsiren.service 2>/dev/null || true
        success "dubsiren.service installed (not enabled)."
        info "To enable later: sudo systemctl enable --now dubsiren.service"
    fi
}

# --- configure_kiosk() -------------------------------------------------------
configure_kiosk() {
    if [[ "$ENABLE_KIOSK" != true ]]; then
        return 0
    fi

    info "Configuring kiosk mode..."

    # --- 1. Console auto-login -----------------------------------------------
    if command -v raspi-config &>/dev/null; then
        raspi-config nonint do_boot_behaviour B2
        success "Console auto-login enabled (raspi-config)."
    else
        # Manual getty override for auto-login
        local getty_dir="/etc/systemd/system/getty@tty1.service.d"
        mkdir -p "${getty_dir}"
        cat > "${getty_dir}/autologin.conf" << EOF
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin ${REAL_USER} --noclear %I \$TERM
EOF
        success "Console auto-login enabled (getty override)."
    fi

    # --- 2. CLI-only boot (disable desktop) ----------------------------------
    if systemctl get-default 2>/dev/null | grep -q 'graphical'; then
        systemctl set-default multi-user.target
        BOOT_CHANGED=true
        success "Boot target set to multi-user (CLI only)."
    else
        success "Already booting to CLI."
    fi

    # --- 3. Disable HDMI output ----------------------------------------------
    cat > /etc/systemd/system/hdmi-off.service << 'EOF'
[Unit]
Description=Disable HDMI output to save power
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/usr/bin/tvservice -o
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

    success "HDMI-off service created."

    # --- 4. Disable screen blanking ------------------------------------------
    local cmdline_file=""
    if [[ -e /boot/firmware/cmdline.txt ]]; then
        cmdline_file="/boot/firmware/cmdline.txt"
    elif [[ -e /boot/cmdline.txt ]]; then
        cmdline_file="/boot/cmdline.txt"
    fi

    if [[ -n "$cmdline_file" ]]; then
        if ! grep -q 'consoleblank=0' "$cmdline_file"; then
            cp "${cmdline_file}" "${cmdline_file}.bak.$(date +%Y%m%d%H%M%S)"
            # cmdline.txt must be a single line — append to it
            sed -i 's/$/ consoleblank=0/' "$cmdline_file"
            BOOT_CHANGED=true
            success "Screen blanking disabled (consoleblank=0)."
        else
            success "Screen blanking already disabled."
        fi
    else
        warn "No cmdline.txt found — skipping screen blanking config."
    fi

    # --- 5. Read-only filesystem (overlay FS) with writable data dir ----------
    if command -v raspi-config &>/dev/null; then
        # Set up writable data directory BEFORE enabling overlay.
        # After overlay, root is read-only. We mount the root block device
        # at /mnt/persist (read-write) and bind-mount the data dir from it.
        local root_dev root_fstype
        root_dev=$(findmnt -n -o SOURCE /)
        root_fstype=$(findmnt -n -o FSTYPE /)

        # Generate systemd-compatible mount unit names
        local persist_unit="mnt-persist.mount"
        local data_mount_path
        data_mount_path=$(systemd-escape --path "${INSTALL_DIR}/data")
        local data_unit="${data_mount_path}.mount"

        # Mount unit: root device read-write at /mnt/persist
        mkdir -p /mnt/persist
        cat > "/etc/systemd/system/${persist_unit}" << EOF
[Unit]
Description=Writable mount of root device for persistent data
After=local-fs-pre.target
Before=local-fs.target

[Mount]
What=${root_dev}
Where=/mnt/persist
Type=${root_fstype}
Options=rw,noatime

[Install]
WantedBy=local-fs.target
EOF

        # Bind mount: /mnt/persist/.../data -> INSTALL_DIR/data
        cat > "/etc/systemd/system/${data_unit}" << EOF
[Unit]
Description=Bind mount persistent dubsiren data
After=${persist_unit}
Requires=${persist_unit}

[Mount]
What=/mnt/persist${INSTALL_DIR}/data
Where=${INSTALL_DIR}/data
Type=none
Options=bind,nofail

[Install]
WantedBy=local-fs.target
EOF

        success "Persist mount units created."
    else
        warn "raspi-config not found — skipping overlay filesystem."
        warn "Install raspi-config or manually configure overlayFS for SD protection."
    fi

    # --- 6. Reload systemd and enable all kiosk units ------------------------
    systemctl daemon-reload
    systemctl enable hdmi-off.service
    success "HDMI output will be disabled on boot."

    if command -v raspi-config &>/dev/null; then
        systemctl enable "${persist_unit}" "${data_unit}"
        success "Writable data directory configured at ${INSTALL_DIR}/data/"

        raspi-config nonint enable_overlayfs
        BOOT_CHANGED=true
        success "Read-only overlay filesystem enabled."
        warn "To make changes later, disable overlay with:"
        warn "  sudo raspi-config nonint disable_overlayfs && sudo reboot"
    fi

    echo ""
    success "Kiosk mode configured."
}

# --- build_project() ---------------------------------------------------------
build_project() {
    local build_dir="${SCRIPT_DIR}/build"
    local make_jobs
    if [[ "$IS_PI_ZERO" == true ]]; then
        make_jobs=2
    else
        make_jobs="$(nproc)"
    fi

    if [[ -f "${build_dir}/CMakeCache.txt" ]]; then
        info "Existing build detected — updating..."
        cd "${build_dir}"
        make -j"${make_jobs}"
    else
        info "No existing build found — configuring and building..."
        mkdir -p "${build_dir}"
        cd "${build_dir}"
        cmake ..
        make -j"${make_jobs}"
    fi

    cd "${SCRIPT_DIR}"
    chown -R "${REAL_USER}:${REAL_USER}" "${build_dir}"
    success "Build complete: ${build_dir}/dubsiren"
}

# --- print_summary() ---------------------------------------------------------
print_summary() {
    echo ""
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}  Setup Complete!${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""
    echo "  Install dir:  ${INSTALL_DIR}"
    echo "  Binary:       ${SCRIPT_DIR}/build/dubsiren"
    echo "  Data folder:  ${INSTALL_DIR}/data/"
    echo "    MP3s:       ${INSTALL_DIR}/data/mp3s/"
    echo "    Presets:    ${INSTALL_DIR}/data/presets/"
    echo "    Config:     ${INSTALL_DIR}/data/config/"
    echo ""

    if [[ "$ENABLE_AUTOSTART" == true ]]; then
        echo "  Autostart:    enabled (dubsiren.service)"
    else
        echo "  Autostart:    disabled"
        echo "                Enable later: sudo systemctl enable --now dubsiren.service"
    fi

    if [[ "$ENABLE_KIOSK" == true ]]; then
        echo "  Kiosk mode:   enabled (auto-login, CLI-only, HDMI off, read-only FS)"
        echo "  Writable:     ${INSTALL_DIR}/data/ persists through overlay FS"
        echo "                Undo overlay: sudo raspi-config nonint disable_overlayfs"
    else
        echo "  Kiosk mode:   disabled"
    fi

    echo ""

    if [[ "$BOOT_CHANGED" == true ]]; then
        warn "Boot configuration was modified — a reboot is required!"
        echo ""
        read -rp "Reboot now? [Y/n]: " reboot_choice < /dev/tty
        if [[ "$reboot_choice" =~ ^[Nn]$ ]]; then
            warn "Remember to reboot before using the dub siren!"
        else
            info "Rebooting..."
            reboot
        fi
    fi
}

# =============================================================================
# Main
# =============================================================================
check_root
detect_pi
ask_options
ensure_git
clone_if_needed
setup_swap
install_deps
build_rpi_ws281x
configure_boot
configure_alsa
create_data_dirs
install_service
configure_kiosk
build_project
print_summary
