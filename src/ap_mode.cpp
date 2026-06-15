// ap_mode.cpp — WiFi Access Point management
//
// Manages the lifecycle of hostapd + dnsmasq to create a WiFi AP
// with captive portal DNS redirection.  All DNS queries resolve to
// the device IP (192.168.4.1) so connected clients get redirected
// to the configuration portal automatically.
//
// Strategy:
//   1. Create virtual uap0 interface for concurrent STA+AP (BCM43436 supports
//      this). wlan0 stays connected to the home network while uap0 runs the
//      config portal.  Channel is inherited from the STA connection.
//   2. If virtual interface creation fails, fall back to using wlan0 directly
//      (drops WiFi but works everywhere).

#include "ap_mode.h"
#include "siren_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>

static const char* AP_IP        = "192.168.4.1";
static const char* AP_PHY_IFACE = "wlan0";     // physical interface
static const char* HOSTAPD_CONF = "/tmp/dubsiren_hostapd.conf";
static const char* HOSTAPD_PID  = "/tmp/dubsiren_hostapd.pid";
static const char* DNSMASQ_CONF = "/tmp/dubsiren_dnsmasq.conf";
static const char* DNSMASQ_PID  = "/tmp/dubsiren_dnsmasq.pid";

static bool  g_active       = false;
static std::string g_ssid;
static const char* AP_VIRT_IFACE = "uap0"; // virtual AP interface name
static std::string g_iface;   // actual interface used (uap0 or wlan0)
static bool g_using_wlan0 = false;  // true if we fell back to wlan0

// "sudo " when not root, "" when root — prefixed to privileged commands
static const char* SUDO = (geteuid() == 0) ? "" : "sudo ";

// ─── Read MAC address from sysfs ────────────────────────────────────

static std::string read_mac()
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", AP_PHY_IFACE);

    std::ifstream f(path);
    if (!f.is_open()) return "00:00:00:00:00:00";

    std::string mac;
    std::getline(f, mac);
    return mac;
}

std::string ap_mode::get_mac_suffix()
{
    std::string mac = read_mac();
    // MAC format: "aa:bb:cc:dd:ee:ff" — take last 4 hex chars (ee + ff, no colons)
    std::string suffix;
    for (char c : mac) {
        if (c != ':') suffix += c;
    }
    // Last 4 characters, uppercase
    if (suffix.size() >= 4) {
        suffix = suffix.substr(suffix.size() - 4);
        for (auto& c : suffix) c = static_cast<char>(toupper(c));
    } else {
        suffix = "0000";
    }
    return suffix;
}

// ─── Write config files ─────────────────────────────────────────────

static int get_sta_channel()
{
    // Read the current channel from wlan0's STA connection so the AP
    // uses the same channel (required for concurrent STA+AP).
    FILE* pipe = popen("iw dev wlan0 info 2>/dev/null | awk '/channel/{print $2}'", "r");
    if (!pipe) return 0;
    char buf[32] = {};
    if (fgets(buf, sizeof(buf), pipe)) {
        pclose(pipe);
        int ch = atoi(buf);
        return (ch >= 1 && ch <= 14) ? ch : 0;
    }
    pclose(pipe);
    return 0;
}

static bool write_hostapd_conf(const std::string& ssid, const char* iface,
                                bool concurrent)
{
    FILE* f = fopen(HOSTAPD_CONF, "w");
    if (!f) {
        slog("AP: Failed to write %s", HOSTAPD_CONF);
        return false;
    }

    // In concurrent mode, match the STA channel (required by brcmfmac).
    // In standalone mode, default to channel 6.
    int channel = concurrent ? get_sta_channel() : 6;
    if (channel == 0) channel = 6;

    fprintf(f,
        "interface=%s\n"
        "driver=nl80211\n"
        "ssid=%s\n"
        "hw_mode=g\n"
        "channel=%d\n"
        "wmm_enabled=0\n"
        "macaddr_acl=0\n"
        "auth_algs=1\n"
        "ignore_broadcast_ssid=0\n"
        "wpa=0\n",
        iface, ssid.c_str(), channel);

    slog("AP: hostapd config — iface=%s channel=%d concurrent=%s",
         iface, channel, concurrent ? "yes" : "no");

    fclose(f);
    return true;
}

static bool write_dnsmasq_conf(const char* iface)
{
    FILE* f = fopen(DNSMASQ_CONF, "w");
    if (!f) {
        slog("AP: Failed to write %s", DNSMASQ_CONF);
        return false;
    }

    fprintf(f,
        "interface=%s\n"
        "bind-interfaces\n"
        "listen-address=%s\n"
        "dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h\n"
        "address=/#/%s\n"            // captive portal: all DNS → device
        "no-resolv\n"
        "no-poll\n"
        "log-queries\n"
        "log-dhcp\n",
        iface, AP_IP, AP_IP);

    fclose(f);
    return true;
}

// ─── Interface setup helpers ─────────────────────────────────────────

// Create virtual uap0 interface for concurrent STA+AP
static bool try_virtual_ap()
{
    char cmd[512];

    // Remove stale virtual interface from previous run
    snprintf(cmd, sizeof(cmd), "%siw dev %s del 2>/dev/null", SUDO, AP_VIRT_IFACE);
    system(cmd);
    usleep(300000);

    // Kill our own stale hostapd if it holds the phy lock (PID-file based)
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, HOSTAPD_PID);
    system(cmd);
    usleep(200000);

    // Try creating the virtual AP interface
    snprintf(cmd, sizeof(cmd), "%siw dev %s interface add %s type __ap",
             SUDO, AP_PHY_IFACE, AP_VIRT_IFACE);
    int ret = system(cmd);
    if (ret != 0) {
        slog("AP: 'iw dev %s interface add %s' failed (exit %d)",
             AP_PHY_IFACE, AP_VIRT_IFACE, ret);
        return false;
    }

    // Tell NetworkManager to ignore the virtual interface
    snprintf(cmd, sizeof(cmd), "%snmcli device set %s managed no 2>/dev/null",
             SUDO, AP_VIRT_IFACE);
    system(cmd);
    usleep(200000);

    // Bring interface up first (required before hostapd on brcmfmac)
    snprintf(cmd, sizeof(cmd), "%sip link set %s up", SUDO, AP_VIRT_IFACE);
    system(cmd);
    usleep(200000);

    // Configure IP
    snprintf(cmd, sizeof(cmd), "%sip addr add %s/24 dev %s", SUDO, AP_IP, AP_VIRT_IFACE);
    system(cmd);

    // Verify uap0 exists (don't check for UP — brcmfmac keeps virtual
    // interfaces in DOWN/DORMANT state until hostapd starts on them)
    snprintf(cmd, sizeof(cmd), "ip link show %s >/dev/null 2>&1", AP_VIRT_IFACE);
    ret = system(cmd);
    if (ret != 0) {
        slog("AP: %s interface not found after creation", AP_VIRT_IFACE);
        return false;
    }

    // Log the current state for debugging
    snprintf(cmd, sizeof(cmd), "ip link show %s 2>/dev/null | head -1", AP_VIRT_IFACE);
    system(cmd);

    return true;
}

// Fall back to using wlan0 directly — disconnects from WiFi
static bool setup_wlan0_ap()
{
    slog("AP: Using wlan0 directly (WiFi will disconnect)");

    // Tell NetworkManager to release wlan0 (don't stop wpa_supplicant
    // service directly — NM manages it via D-Bus and gets confused)
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%snmcli device disconnect wlan0 2>/dev/null", SUDO);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%snmcli device set wlan0 managed no 2>/dev/null", SUDO);
    system(cmd);
    usleep(500000);

    // Flush existing IP and configure AP IP
    snprintf(cmd, sizeof(cmd), "%sip addr flush dev wlan0", SUDO);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%sip addr add %s/24 dev wlan0", SUDO, AP_IP);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%sip link set wlan0 up", SUDO);
    system(cmd);
    usleep(200000);

    return true;
}

// Restore wlan0 to managed mode after AP shutdown
static void restore_wlan0()
{
    slog("AP: Restoring wlan0 to managed mode");

    char cmd[256];
    int ret;

    // Flush the AP IP address
    snprintf(cmd, sizeof(cmd), "%sip addr flush dev wlan0 2>/dev/null", SUDO);
    system(cmd);

    // Hand wlan0 back to NetworkManager
    snprintf(cmd, sizeof(cmd), "%snmcli device set wlan0 managed yes 2>/dev/null", SUDO);
    ret = system(cmd);
    slog("AP: nmcli set managed yes → %d", ret);

    // Give NM a moment to detect the device
    usleep(1000000);

    // Ask NM to auto-connect (picks the best known network)
    snprintf(cmd, sizeof(cmd), "%snmcli device connect wlan0 2>/dev/null", SUDO);
    ret = system(cmd);
    slog("AP: nmcli device connect → %d", ret);

    // Poll for WiFi association (up to 15s)
    for (int i = 0; i < 15; i++) {
        usleep(1000000);
        FILE* pipe = popen("iwgetid -r wlan0 2>/dev/null", "r");
        if (pipe) {
            char buf[128] = {};
            if (fgets(buf, sizeof(buf), pipe)) {
                // Strip newline
                for (char* p = buf; *p; p++)
                    if (*p == '\n' || *p == '\r') { *p = 0; break; }
                if (buf[0]) {
                    slog("AP: WiFi reconnected to '%s' after %ds", buf, i + 1);
                    pclose(pipe);

                    // Log the IP address
                    FILE* ip_pipe = popen("ip -4 addr show wlan0 2>/dev/null | "
                                          "awk '/inet /{print $2}' | cut -d/ -f1", "r");
                    if (ip_pipe) {
                        char ipbuf[64] = {};
                        if (fgets(ipbuf, sizeof(ipbuf), ip_pipe))
                            slog("AP: wlan0 IP: %s", ipbuf);
                        pclose(ip_pipe);
                    }
                    return;
                }
            }
            pclose(pipe);
        }
    }

    slog("AP: WARNING — WiFi did not reconnect within 15s");
}

// ─── Public interface ───────────────────────────────────────────────

bool ap_mode::start_ap()
{
    if (g_active) return true;

    std::string suffix = get_mac_suffix();
    g_ssid = "Poorhouse-Siren-Config-" + suffix;

    // 1. Clean up any previous AP state
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, HOSTAPD_PID);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, DNSMASQ_PID);
    system(cmd);
    usleep(500000);

    // 2. Try virtual uap0 first (concurrent STA+AP), fall back to wlan0
    bool uap0_ok = try_virtual_ap();
    if (uap0_ok) {
        g_iface = AP_VIRT_IFACE;
        g_using_wlan0 = false;
        slog("AP: Trying virtual interface %s (WiFi stays connected)", AP_VIRT_IFACE);
    }

    if (uap0_ok) {
        // Try starting hostapd on uap0
        if (!write_hostapd_conf(g_ssid, AP_VIRT_IFACE, true)) { uap0_ok = false; }
        if (uap0_ok) {
            if (!write_dnsmasq_conf(AP_VIRT_IFACE)) { uap0_ok = false; }
        }
        if (uap0_ok) {
            slog("AP: Starting '%s' on %s", g_ssid.c_str(), AP_VIRT_IFACE);
            snprintf(cmd, sizeof(cmd), "%shostapd %s -B -P %s",
                     SUDO, HOSTAPD_CONF, HOSTAPD_PID);
            int ret = system(cmd);
            if (ret != 0) {
                slog("AP: hostapd failed on %s (exit %d) — will try wlan0",
                     AP_VIRT_IFACE, ret);
                // Clean up uap0
                snprintf(cmd, sizeof(cmd), "%siw dev %s del 2>/dev/null",
                         SUDO, AP_VIRT_IFACE);
                system(cmd);
                uap0_ok = false;
            }
        }
    }

    if (!uap0_ok) {
        slog("AP: Virtual %s unavailable — falling back to wlan0", AP_VIRT_IFACE);
        if (!setup_wlan0_ap()) {
            slog("AP: Failed to configure wlan0 for AP mode");
            return false;
        }
        g_iface = "wlan0";
        g_using_wlan0 = true;

        if (!write_hostapd_conf(g_ssid, "wlan0", false)) return false;
        if (!write_dnsmasq_conf("wlan0")) return false;

        slog("AP: Starting '%s' on wlan0", g_ssid.c_str());
        snprintf(cmd, sizeof(cmd), "%shostapd %s -B -P %s",
                 SUDO, HOSTAPD_CONF, HOSTAPD_PID);
        int ret = system(cmd);
        if (ret != 0) {
            slog("AP: Failed to start hostapd on wlan0 (exit %d)", ret);
            restore_wlan0();
            return false;
        }
    }

    usleep(1000000);  // 1s — let hostapd fully settle

    // Start dnsmasq for DHCP + captive portal DNS
    snprintf(cmd, sizeof(cmd), "%sdnsmasq -C %s --pid-file=%s",
             SUDO, DNSMASQ_CONF, DNSMASQ_PID);
    int ret = system(cmd);
    if (ret != 0) {
        slog("AP: Failed to start dnsmasq (exit %d)", ret);
        snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, HOSTAPD_PID);
        system(cmd);
        if (g_using_wlan0) restore_wlan0();
        return false;
    }
    g_active = true;
    slog("AP: Access point active — SSID: %s  IP: %s (concurrent=%s)",
         g_ssid.c_str(), AP_IP, g_using_wlan0 ? "no" : "yes");
    return true;
}

void ap_mode::stop_ap()
{
    if (!g_active) return;

    slog("AP: Stopping access point (iface=%s, wlan0_mode=%s)",
         g_iface.c_str(), g_using_wlan0 ? "yes" : "no");

    char cmd[256];
    int ret;

    // Kill hostapd using PID file first, then fallback to killall
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, HOSTAPD_PID);
    ret = system(cmd);
    if (ret != 0) {
        slog("AP: pkill -F hostapd failed (ret=%d), trying killall", ret);
        snprintf(cmd, sizeof(cmd), "%skillall hostapd 2>/dev/null", SUDO);
        system(cmd);
    }

    // Kill dnsmasq using PID file first, then fallback to killall
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, DNSMASQ_PID);
    ret = system(cmd);
    if (ret != 0) {
        slog("AP: pkill -F dnsmasq failed (ret=%d), trying killall", ret);
        snprintf(cmd, sizeof(cmd), "%skillall dnsmasq 2>/dev/null", SUDO);
        system(cmd);
    }

    usleep(500000);

    // Verify hostapd is actually dead
    ret = system("pgrep hostapd >/dev/null 2>&1");
    if (ret == 0) {
        slog("AP: hostapd still running after kill — force killing");
        snprintf(cmd, sizeof(cmd), "%skillall -9 hostapd 2>/dev/null", SUDO);
        system(cmd);
        usleep(200000);
    }

    // Clean up config and PID files
    unlink(HOSTAPD_CONF);
    unlink(DNSMASQ_CONF);
    unlink(HOSTAPD_PID);
    unlink(DNSMASQ_PID);

    if (g_using_wlan0) {
        // Restore wlan0 to managed mode so it reconnects to WiFi
        restore_wlan0();
    } else {
        // Remove the virtual AP interface
        slog("AP: Removing virtual interface %s", AP_VIRT_IFACE);
        snprintf(cmd, sizeof(cmd), "%siw dev %s del", SUDO, AP_VIRT_IFACE);
        system(cmd);
    }

    g_active = false;
    g_using_wlan0 = false;
    slog("AP: Access point stopped — STA mode restored");
}

bool ap_mode::is_active()
{
    return g_active;
}

std::string ap_mode::get_ssid()
{
    return g_ssid;
}

const char* ap_mode::get_ip()
{
    return AP_IP;
}

bool ap_mode::is_concurrent()
{
    return g_active && !g_using_wlan0;
}
