#pragma once

#include <functional>
#include <string>

// ─── Web configuration server ──────────────────────────────────────
//
// Always-on HTTP server for device configuration.  Accessible at
// poorhouse.local/config on the local network, or via captive portal
// in AP mode.  Provides REST API endpoints for presets, WiFi, etc.

namespace web_server {

// Callbacks for interacting with the siren state
struct Callbacks {
    // Get current siren state as JSON
    std::function<std::string()> get_preset_state;

    // Get all presets (user + library) as JSON
    std::function<std::string()> get_all_presets;

    // Apply a preset by category and index
    std::function<bool(const std::string& category, int index)> apply_preset;

    // Save current state to user slot with a name
    std::function<bool(int slot, const std::string& name)> save_to_slot;

    // Swap two user preset slots
    std::function<bool(int a, int b)> swap_slots;

    // Load a library/factory preset into a user slot
    std::function<bool(int slot, const std::string& category, int index)> load_to_slot;

    // Load a library/factory preset into any bank's slot
    std::function<bool(const std::string& bank, int slot,
                       const std::string& category, int index)> load_to_bank_slot;

    // Save current DSP state to any bank's slot
    std::function<bool(const std::string& bank, int slot,
                       const std::string& name)> save_to_bank_slot;

    // Get/set siren options as JSON
    std::function<std::string()> get_siren_options;
    std::function<bool(const std::string& json)> set_siren_options;

    // Preview control
    std::function<void(const std::string& preset_json)> preview_start;
    std::function<void()> preview_stop;

    // Live DSP parameter control
    std::function<std::string()> get_dsp_state;
    std::function<bool(const std::string& name, float value)> set_dsp_param;
    std::function<bool()> reset_defaults;

    // System info & control
    std::function<std::string()> get_system_info;
    std::function<bool()> reboot_system;
    std::function<bool()> restart_service;

    // Encoder mapping
    std::function<std::string()> get_encoder_map;
    std::function<bool(const std::string& json)> set_encoder_map;

    // Encoder sensitivity
    std::function<std::string()> get_encoder_sensitivity;
    std::function<bool(const std::string& body)> set_encoder_sensitivity;

    // WiFi operations
    std::function<std::string()> wifi_scan;
    std::function<bool(const std::string& ssid, const std::string& password)> wifi_connect;
    std::function<std::string()> wifi_status;
    std::function<bool(const std::string& ssid, const std::string& password)> wifi_test;
    std::function<std::string()> wifi_test_result;

    // Update operations
    std::function<std::string(const std::string& branch)> update_check;
    std::function<bool(const std::string& branch)> update_install;
    std::function<std::string()> update_status;
    std::function<std::string()> update_branches;
    std::function<std::string()> update_log;

    // Backup/restore
    std::function<std::string()> backup_create;
    std::function<bool(const std::string& data)> backup_restore;

    // System log
    std::function<std::string()> get_system_log;

    // Exit AP mode
    std::function<void()> exit_ap;

    // Check if AP mode is currently active (for captive portal gating)
    std::function<bool()> is_ap_active;
};

// Start the web server on the given port (usually 80)
bool start(int port, const Callbacks& cb);

// Stop the web server
void stop();

// Check if server is running
bool is_running();

} // namespace web_server
