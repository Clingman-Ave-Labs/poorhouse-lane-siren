#pragma once

// Thread-safe in-memory ring buffer logger for troubleshooting via the web UI.
// Usage:  siren_log("WEB: Server started on port %d", port);
// Writes to both stdout and a ring buffer accessible via /api/system/log.

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

namespace siren_log {

static constexpr int MAX_ENTRIES = 200;
static constexpr int MAX_LINE = 256;

struct Entry {
    char timestamp[20];  // "HH:MM:SS"
    char message[MAX_LINE];
};

inline std::mutex& mutex() { static std::mutex m; return m; }
inline Entry* entries() { static Entry e[MAX_ENTRIES] = {}; return e; }
inline int& head() { static int h = 0; return h; }
inline int& count() { static int c = 0; return c; }

inline void log(const char* fmt, ...) {
    char msg[MAX_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    // Strip trailing newline for clean display
    size_t len = strlen(msg);
    while (len > 0 && (msg[len-1] == '\n' || msg[len-1] == '\r'))
        msg[--len] = '\0';

    // Timestamp
    char ts[20];
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    snprintf(ts, sizeof(ts), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Also print to stdout
    printf("[%s] %s\n", ts, msg);

    // Store in ring buffer
    std::lock_guard<std::mutex> lk(mutex());
    Entry& e = entries()[head()];
    strncpy(e.timestamp, ts, sizeof(e.timestamp));
    strncpy(e.message, msg, sizeof(e.message));
    e.message[MAX_LINE - 1] = '\0';
    head() = (head() + 1) % MAX_ENTRIES;
    if (count() < MAX_ENTRIES) count()++;
}

// Return log as JSON array (newest last)
inline std::string to_json() {
    std::lock_guard<std::mutex> lk(mutex());
    // Pre-allocate: ~80 bytes per entry avg
    std::string json;
    json.reserve(static_cast<size_t>(count()) * 80 + 2);
    json += '[';
    int n = count();
    int start = (head() - n + MAX_ENTRIES) % MAX_ENTRIES;
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ',';
        const Entry& e = entries()[(start + i) % MAX_ENTRIES];
        json += "{\"t\":\"";
        json += e.timestamp;
        json += "\",\"m\":\"";
        for (const char* p = e.message; *p; p++) {
            switch (*p) {
                case '"':  json += "\\\""; break;
                case '\\': json += "\\\\"; break;
                case '\b': json += "\\b";  break;
                case '\f': json += "\\f";  break;
                case '\n': json += "\\n";  break;
                case '\r': json += "\\r";  break;
                case '\t': json += "\\t";  break;
                default:   json += *p;     break;
            }
        }
        json += "\"}";
    }
    json += ']';
    return json;
}

} // namespace siren_log

#define slog(...) siren_log::log(__VA_ARGS__)
