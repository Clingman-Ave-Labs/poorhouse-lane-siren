// web_server.cpp — Always-on HTTP server for device configuration
//
// Uses cpp-httplib (single-header) to serve the web UI and REST API.
// Runs in its own thread.  Accessible at poorhouse.local/config on the
// local network, or via captive portal when AP mode is active.

#include "web_server.h"
#include "web_ui.h"
#include "siren_log.h"

#include <httplib.h>

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <memory>
#include <unistd.h>
#include <sys/wait.h>

static std::unique_ptr<httplib::Server> g_server;
static std::thread g_thread;
static std::atomic<bool> g_running{false};
static web_server::Callbacks g_callbacks;  // persistent copy of callbacks
static pid_t g_mdns_pid = 0;  // avahi-publish child for poorhouse.local

// ─── mDNS hostname alias ────────────────────────────────────────────
//
// Publishes "poorhouse.local" via avahi regardless of the system hostname.
// This lets users always reach the config portal at poorhouse.local/config
// even if Pi Imager set the hostname to something else (e.g. poorhouse34).
//
// The system hostname remains unchanged, so SSH to poorhouse34.local
// continues to work.

static std::string get_local_ip()
{
    // Get the IP of the default route interface
    FILE* fp = popen("hostname -I 2>/dev/null | awk '{print $1}'", "r");
    if (!fp) return "";
    char buf[64] = {};
    if (fgets(buf, sizeof(buf), fp)) {
        // Strip trailing newline
        for (char* p = buf; *p; p++) {
            if (*p == '\n' || *p == '\r') { *p = '\0'; break; }
        }
    }
    pclose(fp);
    return buf;
}

// Check if poorhouse.local already resolves to our IP via avahi
static bool mdns_already_resolves(const std::string& our_ip)
{
    // Quick probe: does poorhouse.local already point at us?
    FILE* fp = popen("avahi-resolve-host-name -4 poorhouse.local 2>/dev/null", "r");
    if (!fp) return false;
    char buf[256] = {};
    if (fgets(buf, sizeof(buf), fp)) {
        // Output format: "poorhouse.local\t192.168.x.x\n"
        if (strstr(buf, our_ip.c_str())) {
            pclose(fp);
            return true;
        }
    }
    pclose(fp);
    return false;
}

// Write our IP into /etc/avahi/hosts (static mDNS alias — survives reboots,
// no collision with avahi-daemon since it's the daemon itself serving it).
static bool write_avahi_hosts(const std::string& ip)
{
    static const char* const HOSTS_FILE = "/etc/avahi/hosts";
    static const char* const ALIAS      = "poorhouse.local";

    // Read existing file, replace or append our line
    std::string contents;
    bool found = false;
    FILE* rf = fopen(HOSTS_FILE, "r");
    if (rf) {
        char line[512];
        while (fgets(line, sizeof(line), rf)) {
            if (strstr(line, ALIAS)) {
                // Replace existing entry with our current IP
                contents += ip + "\t" + ALIAS + "\n";
                found = true;
            } else {
                contents += line;
            }
        }
        fclose(rf);
    }
    if (!found) {
        contents += ip + "\t" + ALIAS + "\n";
    }

    FILE* wf = fopen(HOSTS_FILE, "w");
    if (!wf) {
        fprintf(stderr, "WEB: Cannot write %s (not root?) — "
                "falling back to avahi-publish\n", HOSTS_FILE);
        return false;
    }
    fputs(contents.c_str(), wf);
    fclose(wf);

    printf("WEB: Wrote %s -> %s in %s\n", ALIAS, ip.c_str(), HOSTS_FILE);
    return true;
}

static void start_mdns_alias()
{
    // Check if system hostname is already exactly "poorhouse" — no alias needed
    // (hostnames like "poorhouse35" still need the alias so the canonical
    //  poorhouse.local address works)
    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));
    if (strncmp(hostname, "poorhouse", 9) == 0 &&
        (hostname[9] == '\0' || hostname[9] == '.')) {
        printf("WEB: Hostname is already '%s' — mDNS alias not needed\n", hostname);
        return;
    }

    std::string ip = get_local_ip();
    if (ip.empty()) {
        fprintf(stderr, "WEB: Could not determine local IP — skipping mDNS alias\n");
        return;
    }

    // If poorhouse.local already resolves to our IP, nothing to do
    if (mdns_already_resolves(ip)) {
        printf("WEB: poorhouse.local already resolves to %s — alias OK\n", ip.c_str());
        return;
    }

    // Strategy 1: write /etc/avahi/hosts (reliable, no collision issues)
    if (write_avahi_hosts(ip)) {
        // Give avahi-daemon a moment to pick up the change
        usleep(500000);
        if (mdns_already_resolves(ip)) {
            printf("WEB: mDNS alias active via /etc/avahi/hosts\n");
            return;
        }
        // Poke avahi-daemon to re-read the hosts file
        FILE* fp = popen("systemctl reload avahi-daemon 2>/dev/null || "
                         "killall -HUP avahi-daemon 2>/dev/null", "r");
        if (fp) pclose(fp);
        usleep(500000);
        if (mdns_already_resolves(ip)) {
            printf("WEB: mDNS alias active via /etc/avahi/hosts (after reload)\n");
            return;
        }
    }

    // Strategy 2: fall back to avahi-publish (original approach)
    printf("WEB: Falling back to avahi-publish for poorhouse.local -> %s\n", ip.c_str());

    pid_t pid = fork();
    if (pid == 0) {
        // Child — run avahi-publish-host-name (blocks until killed)
        freopen("/dev/null", "w", stdout);
        execlp("avahi-publish-host-name", "avahi-publish-host-name",
               "poorhouse.local", ip.c_str(), nullptr);
        execlp("avahi-publish", "avahi-publish", "-a",
               "poorhouse.local", ip.c_str(), nullptr);
        _exit(127);
    } else if (pid > 0) {
        g_mdns_pid = pid;
        usleep(200000);
        int status = 0;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == 0) {
            printf("WEB: mDNS alias active via avahi-publish (pid %d)\n", pid);
        } else {
            fprintf(stderr, "WEB: avahi-publish failed — "
                    "poorhouse.local alias unavailable\n");
            fprintf(stderr, "WEB: Device reachable at %s.local or %s\n",
                    hostname, ip.c_str());
            g_mdns_pid = 0;
        }
    }
}

static void stop_mdns_alias()
{
    if (g_mdns_pid > 0) {
        kill(g_mdns_pid, SIGTERM);
        int status;
        waitpid(g_mdns_pid, &status, 0);
        g_mdns_pid = 0;
    }
    // Note: we leave /etc/avahi/hosts intact on stop — the alias should
    // persist across siren restarts so the device remains reachable.
}

// ─── Captive portal detection responses ─────────────────────────────
//
// Different OS vendors probe specific URLs to detect captive portals.
// We respond with redirects or minimal content to trigger the portal.

// Captive portal redirect — only active during AP mode
static void captive_redirect(const httplib::Request&, httplib::Response& res)
{
    if (g_callbacks.is_ap_active && g_callbacks.is_ap_active()) {
        res.status = 302;
        res.set_header("Location", "http://192.168.4.1/");
    } else {
        res.status = 404;
    }
}

static void setup_captive_portal(httplib::Server& svr)
{
    svr.Get("/generate_204",      captive_redirect);   // Android
    svr.Get("/gen_204",           captive_redirect);   // Android alt
    svr.Get("/hotspot-detect.html", captive_redirect); // Apple
    svr.Get("/connecttest.txt",   captive_redirect);   // Windows
    svr.Get("/ncsi.txt",          captive_redirect);   // Windows NCSI
    svr.Get("/success.txt",       captive_redirect);   // Firefox
}

// ─── JSON helpers ───────────────────────────────────────────────────

static void json_response(httplib::Response& res, const std::string& json)
{
    res.set_content(json, "application/json");
}

static void json_ok(httplib::Response& res, const std::string& msg = "ok")
{
    res.set_content("{\"status\":\"" + msg + "\"}", "application/json");
}

static void json_error(httplib::Response& res, const std::string& msg, int status = 400)
{
    res.status = status;
    res.set_content("{\"error\":\"" + msg + "\"}", "application/json");
}

// ─── Setup API routes ───────────────────────────────────────────────

static void setup_api(httplib::Server& svr)
{
    // Main page — accessible at / (AP mode) or /config (network mode)
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web_ui::INDEX_HTML, "text/html");
    });
    svr.Get("/config", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web_ui::INDEX_HTML, "text/html");
    });

    // ── Preset endpoints ────────────────────────────────────────────

    svr.Get("/api/presets", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_all_presets) {
            json_response(res, g_callbacks.get_all_presets());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Get("/api/presets/current", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_preset_state) {
            json_response(res, g_callbacks.get_preset_state());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/presets/apply", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.apply_preset) { json_error(res, "not implemented", 501); return; }
        auto category = req.get_param_value("category");
        auto index_str = req.get_param_value("index");
        int index = 0;
        try { index = std::stoi(index_str); } catch (...) {}
        if (g_callbacks.apply_preset(category, index)) {
            json_ok(res);
        } else {
            json_error(res, "failed to apply preset");
        }
    });

    svr.Post("/api/presets/save", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.save_to_slot) { json_error(res, "not implemented", 501); return; }
        auto slot_str = req.get_param_value("slot");
        auto name = req.get_param_value("name");
        int slot = 0;
        try { slot = std::stoi(slot_str); } catch (...) {}
        if (g_callbacks.save_to_slot(slot, name)) {
            json_ok(res, "saved");
        } else {
            json_error(res, "failed to save");
        }
    });

    svr.Post("/api/presets/swap", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.swap_slots) { json_error(res, "not implemented", 501); return; }
        int a = 0, b = 0;
        try { a = std::stoi(req.get_param_value("a")); } catch (...) {}
        try { b = std::stoi(req.get_param_value("b")); } catch (...) {}
        if (g_callbacks.swap_slots(a, b)) {
            json_ok(res, "swapped");
        } else {
            json_error(res, "failed to swap");
        }
    });

    svr.Post("/api/presets/load-to-slot", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.load_to_slot) { json_error(res, "not implemented", 501); return; }
        int slot = 0, index = 0;
        try { slot = std::stoi(req.get_param_value("slot")); } catch (...) {}
        try { index = std::stoi(req.get_param_value("index")); } catch (...) {}
        auto category = req.get_param_value("category");
        if (g_callbacks.load_to_slot(slot, category, index)) {
            json_ok(res, "loaded");
        } else {
            json_error(res, "failed to load preset to slot");
        }
    });

    // ── Bank slot operations ───────────────────────────────────────

    svr.Post("/api/presets/bank/load", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.load_to_bank_slot) { json_error(res, "not implemented", 501); return; }
        auto bank = req.get_param_value("bank");
        int slot = 0, index = 0;
        try { slot = std::stoi(req.get_param_value("slot")); } catch (...) {}
        try { index = std::stoi(req.get_param_value("index")); } catch (...) {}
        auto category = req.get_param_value("category");
        if (g_callbacks.load_to_bank_slot(bank, slot, category, index)) {
            json_ok(res, "loaded");
        } else {
            json_error(res, "failed to load preset to bank slot");
        }
    });

    svr.Post("/api/presets/bank/save", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.save_to_bank_slot) { json_error(res, "not implemented", 501); return; }
        auto bank = req.get_param_value("bank");
        int slot = 0;
        try { slot = std::stoi(req.get_param_value("slot")); } catch (...) {}
        auto name = req.get_param_value("name");
        if (g_callbacks.save_to_bank_slot(bank, slot, name)) {
            json_ok(res, "saved");
        } else {
            json_error(res, "failed to save to bank slot");
        }
    });

    // ── Siren options ───────────────────────────────────────────────

    svr.Get("/api/siren/options", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_siren_options) {
            json_response(res, g_callbacks.get_siren_options());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/siren/options", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.set_siren_options) { json_error(res, "not implemented", 501); return; }
        if (g_callbacks.set_siren_options(req.body)) {
            json_ok(res, "applied");
        } else {
            json_error(res, "failed to apply options");
        }
    });

    // ── Preview control ─────────────────────────────────────────────

    svr.Post("/api/preview/start", [](const httplib::Request& req, httplib::Response& res) {
        if (g_callbacks.preview_start) {
            g_callbacks.preview_start(req.body);
            json_ok(res, "previewing");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/preview/stop", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.preview_stop) {
            g_callbacks.preview_stop();
            json_ok(res, "stopped");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── Live DSP parameter control ──────────────────────────────────

    svr.Get("/api/dsp/state", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_dsp_state) {
            json_response(res, g_callbacks.get_dsp_state());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/dsp/param", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.set_dsp_param) { json_error(res, "not implemented", 501); return; }
        auto name = req.get_param_value("name");
        auto value_str = req.get_param_value("value");
        if (name.empty() || value_str.empty()) {
            json_error(res, "name and value required"); return;
        }
        float value = 0;
        try { value = std::stof(value_str); } catch (...) {
            json_error(res, "invalid value"); return;
        }
        if (g_callbacks.set_dsp_param(name, value)) {
            json_ok(res, "set");
        } else {
            json_error(res, "unknown parameter");
        }
    });

    svr.Post("/api/dsp/reset", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.reset_defaults) {
            g_callbacks.reset_defaults();
            json_ok(res, "reset");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── System info & control ───────────────────────────────────────

    svr.Get("/api/system/info", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_system_info) {
            json_response(res, g_callbacks.get_system_info());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/system/reboot", [](const httplib::Request&, httplib::Response& res) {
        if (!g_callbacks.reboot_system) { json_error(res, "not implemented", 501); return; }
        json_ok(res, "rebooting");
        std::thread([]() {
            usleep(500000);
            g_callbacks.reboot_system();
        }).detach();
    });

    svr.Get("/api/system/log", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_system_log) {
            json_response(res, g_callbacks.get_system_log());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/system/restart", [](const httplib::Request&, httplib::Response& res) {
        if (!g_callbacks.restart_service) { json_error(res, "not implemented", 501); return; }
        json_ok(res, "restarting");
        std::thread([]() {
            usleep(500000);
            g_callbacks.restart_service();
        }).detach();
    });

    // ── Encoder mapping ─────────────────────────────────────────────

    svr.Get("/api/encoders/map", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_encoder_map) {
            json_response(res, g_callbacks.get_encoder_map());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/encoders/map", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.set_encoder_map) { json_error(res, "not implemented", 501); return; }
        if (g_callbacks.set_encoder_map(req.body)) {
            json_ok(res, "saved");
        } else {
            json_error(res, "failed to save mapping");
        }
    });

    svr.Get("/api/encoders/sensitivity", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_encoder_sensitivity) {
            json_response(res, g_callbacks.get_encoder_sensitivity());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/encoders/sensitivity", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.set_encoder_sensitivity) { json_error(res, "not implemented", 501); return; }
        if (g_callbacks.set_encoder_sensitivity(req.body)) {
            json_ok(res, "saved");
        } else {
            json_error(res, "failed to save sensitivity");
        }
    });

    // ── WiFi operations ─────────────────────────────────────────────

    svr.Get("/api/wifi/scan", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.wifi_scan) {
            json_response(res, g_callbacks.wifi_scan());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/wifi/connect", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.wifi_connect) { json_error(res, "not implemented", 501); return; }
        auto ssid = req.get_param_value("ssid");
        auto password = req.get_param_value("password");
        if (g_callbacks.wifi_connect(ssid, password)) {
            json_ok(res, "credentials saved");
        } else {
            json_error(res, "failed to save credentials");
        }
    });

    svr.Get("/api/wifi/status", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.wifi_status) {
            json_response(res, g_callbacks.wifi_status());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/wifi/test", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.wifi_test) { json_error(res, "not implemented", 501); return; }
        auto ssid = req.get_param_value("ssid");
        auto password = req.get_param_value("password");
        if (ssid.empty()) { json_error(res, "ssid required"); return; }
        if (g_callbacks.wifi_test(ssid, password)) {
            json_ok(res, "test started");
        } else {
            json_error(res, "test already in progress");
        }
    });

    svr.Get("/api/wifi/test-result", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.wifi_test_result) {
            json_response(res, g_callbacks.wifi_test_result());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── Update operations ───────────────────────────────────────────

    svr.Get("/api/update/branches", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.update_branches) {
            json_response(res, g_callbacks.update_branches());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/update/check", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.update_check) { json_error(res, "not implemented", 501); return; }
        auto branch = req.get_param_value("branch");
        if (branch.empty()) branch = "main";
        json_response(res, g_callbacks.update_check(branch));
    });

    svr.Post("/api/update/install", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.update_install) { json_error(res, "not implemented", 501); return; }
        auto branch = req.get_param_value("branch");
        if (branch.empty()) branch = "main";
        if (g_callbacks.update_install(branch)) {
            json_ok(res, "update started");
        } else {
            json_error(res, "already running");
        }
    });

    svr.Get("/api/update/status", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.update_status) {
            json_response(res, g_callbacks.update_status());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Get("/api/update/log", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.update_log) {
            json_response(res, g_callbacks.update_log());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── Shell ────────────────────────────────────────────────────────

    svr.Post("/api/system/shell", [](const httplib::Request& req, httplib::Response& res) {
        // Parse command from JSON body
        std::string cmd;
        auto j = req.body;
        // Minimal JSON parse: extract "cmd" value
        auto pos = j.find("\"cmd\"");
        if (pos == std::string::npos) { json_error(res, "missing cmd"); return; }
        auto colon = j.find(':', pos);
        auto q1 = j.find('"', colon + 1);
        auto q2 = j.find('"', q1 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos) { json_error(res, "bad cmd"); return; }
        cmd = j.substr(q1 + 1, q2 - q1 - 1);
        // Unescape basic JSON escapes
        for (size_t i = 0; i < cmd.size(); i++) {
            if (cmd[i] == '\\' && i + 1 < cmd.size()) {
                if (cmd[i+1] == 'n') { cmd.replace(i, 2, "\n"); }
                else if (cmd[i+1] == 't') { cmd.replace(i, 2, "\t"); }
                else if (cmd[i+1] == '"') { cmd.replace(i, 2, "\""); }
                else if (cmd[i+1] == '\\') { cmd.replace(i, 2, "\\"); }
            }
        }

        slog("SHELL: %s", cmd.c_str());

        // Run command, capture stdout+stderr, with timeout
        std::string full_cmd = cmd + " 2>&1";
        FILE* fp = popen(full_cmd.c_str(), "r");
        if (!fp) { json_error(res, "popen failed"); return; }

        std::string output;
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            output += buf;
        }
        int status = pclose(fp);
        int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

        // Build JSON response — escape output for JSON
        std::string escaped;
        for (char c : output) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\r') escaped += "\\r";
            else if (c == '\t') escaped += "\\t";
            else if ((unsigned char)c < 0x20) { char h[8]; snprintf(h, sizeof(h), "\\u%04x", c); escaped += h; }
            else escaped += c;
        }

        std::string json = "{\"output\":\"" + escaped + "\",\"exit\":" + std::to_string(exit_code) + "}";
        res.set_content(json, "application/json");
    });

    // ── Backup/restore ──────────────────────────────────────────────

    svr.Get("/api/backup", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.backup_create) {
            auto data = g_callbacks.backup_create();
            res.set_header("Content-Disposition",
                           "attachment; filename=\"dubsiren-backup.json\"");
            res.set_content(data, "application/json");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/restore", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.backup_restore) { json_error(res, "not implemented", 501); return; }
        if (g_callbacks.backup_restore(req.body)) {
            json_ok(res, "restored");
        } else {
            json_error(res, "restore failed");
        }
    });

    // ── Exit AP mode ────────────────────────────────────────────────

    svr.Post("/api/exit", [](const httplib::Request&, httplib::Response& res) {
        json_ok(res, "exiting AP mode");
        if (g_callbacks.exit_ap) {
            // Schedule exit after response is sent
            std::thread([]() {
                usleep(500000);  // 500ms delay so response reaches client
                g_callbacks.exit_ap();
            }).detach();
        }
    });
}

// ─── Public interface ───────────────────────────────────────────────

bool web_server::start(int port, const Callbacks& cb)
{
    if (g_running) return true;

    // Clean up any previous failed attempt (thread must be joined before reassignment)
    if (g_thread.joinable()) {
        g_thread.join();
    }
    g_server.reset();

    g_callbacks = cb;  // persistent copy — outlives this function
    g_server = std::make_unique<httplib::Server>();

    setup_captive_portal(*g_server);
    setup_api(*g_server);

    g_running = true;
    g_thread = std::thread([port]() {
        slog("WEB: Server starting on port %d", port);
        if (!g_server->listen("0.0.0.0", port)) {
            slog("WEB: Failed to start server on port %d", port);
        }
        g_running = false;
        slog("WEB: Server stopped");
    });

    // Wait briefly for server to start
    usleep(100000);

    // Publish poorhouse.local mDNS alias (if hostname differs)
    if (g_running) {
        start_mdns_alias();
    }

    return g_running;
}

void web_server::stop()
{
    if (!g_running) return;

    slog("WEB: Stopping server");
    stop_mdns_alias();
    if (g_server) {
        g_server->stop();
    }

    if (g_thread.joinable()) {
        g_thread.join();
    }

    g_server.reset();
    g_callbacks = {};
    g_running = false;
}

bool web_server::is_running()
{
    return g_running;
}
