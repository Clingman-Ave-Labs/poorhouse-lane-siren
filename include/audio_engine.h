#pragma once

#include <functional>
#include <memory>
#include <string>

// Callback signature: fill `buffer` with `frames` mono float samples [–1,+1]
using AudioCallback = std::function<void(float* buffer, int frames)>;

// ─── ALSA playback engine ───────────────────────────────────────────
//
// Opens a PCM device, spawns a writer thread, and calls the user
// callback to fill each period.  Mono float output is duplicated to
// stereo S16_LE for the DAC.
//
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool init(const std::string& device, int sampleRate, AudioCallback cb);
    void start();
    void stop();
    void shutdown();

    int sampleRate() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};
