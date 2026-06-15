#pragma once

#include <string>

// ─── Access Point mode ─────────────────────────────────────────────
//
// Turns the Raspberry Pi into a WiFi access point with a captive
// portal for device configuration.  Uses hostapd + dnsmasq.
//
// SSID format: Poorhouse-Siren-Config-XXXX (last 4 chars of MAC)

namespace ap_mode {

// Get the last 4 hex characters of the WiFi MAC address (uppercase)
std::string get_mac_suffix();

// Start the access point: configure interface, launch hostapd + dnsmasq
// Returns true on success
bool start_ap();

// Stop the access point: kill hostapd + dnsmasq, restore networking
void stop_ap();

// Check if AP mode is currently active
bool is_active();

// Get the full SSID being broadcast
std::string get_ssid();

// Get the AP IP address (always 192.168.4.1)
const char* get_ip();

// True if running concurrent STA+AP (uap0) — wlan0 is free for WiFi tests
bool is_concurrent();

} // namespace ap_mode
