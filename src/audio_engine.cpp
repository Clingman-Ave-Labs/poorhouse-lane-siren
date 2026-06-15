// audio_engine.cpp — ALSA PCM playback engine
//
// Spawns a dedicated writer thread that:
//   1. Calls the user callback to fill a mono float buffer
//   2. Converts to interleaved stereo S16_LE
//   3. Writes to the ALSA PCM device
//
// Recovers automatically from xruns (underruns).

#include "audio_engine.h"

#include <cstdio>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#ifdef HAS_ALSA
#include <alsa/asoundlib.h>
#endif

static constexpr int CHANNELS    = 2;
static constexpr int PERIOD_SIZE = 256;  // frames per period

// ─── Private implementation ─────────────────────────────────────────

struct AudioEngine::Impl {
    AudioCallback callback;
    int           sampleRate = 48000;
    std::string   device     = "hw:0,0";

    std::atomic<bool> running{false};
    std::thread       thread;

    std::vector<float>   floatBuf;   // mono float from callback
    std::vector<int16_t> pcmBuf;     // interleaved stereo S16

#ifdef HAS_ALSA
    snd_pcm_t *pcm = nullptr;
#endif
};

// ─── Constructor / destructor ───────────────────────────────────────

AudioEngine::AudioEngine()  = default;
AudioEngine::~AudioEngine() { shutdown(); }

int AudioEngine::sampleRate() const
{
    return pimpl_ ? pimpl_->sampleRate : 48000;
}

// ─── init ───────────────────────────────────────────────────────────

bool AudioEngine::init(const std::string& device, int sampleRate,
                       AudioCallback cb)
{
    pimpl_ = std::make_unique<Impl>();
    pimpl_->callback   = std::move(cb);
    pimpl_->sampleRate = sampleRate;
    pimpl_->device     = device;
    pimpl_->floatBuf.resize(PERIOD_SIZE);
    pimpl_->pcmBuf.resize(PERIOD_SIZE * CHANNELS);

#ifdef HAS_ALSA
    int err;

    // Retry opening the ALSA device — at boot the I2S DAC module may
    // still be loading when the service starts.
    static constexpr int  MAX_RETRIES = 10;
    static constexpr auto RETRY_DELAY = std::chrono::seconds(1);

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
        err = snd_pcm_open(&pimpl_->pcm, device.c_str(),
                           SND_PCM_STREAM_PLAYBACK, 0);
        if (err == 0) break;

        fprintf(stderr, "ALSA: cannot open '%s': %s (attempt %d/%d)\n",
                device.c_str(), snd_strerror(err), attempt, MAX_RETRIES);

        if (attempt < MAX_RETRIES)
            std::this_thread::sleep_for(RETRY_DELAY);
    }
    if (err < 0) return false;

    // ── HW params ──────────────────────────────────────────────────
    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pimpl_->pcm, hw);

    snd_pcm_hw_params_set_access(pimpl_->pcm, hw,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pimpl_->pcm, hw,
                                 SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pimpl_->pcm, hw, CHANNELS);

    unsigned rate = static_cast<unsigned>(sampleRate);
    snd_pcm_hw_params_set_rate_near(pimpl_->pcm, hw, &rate, nullptr);
    pimpl_->sampleRate = static_cast<int>(rate);

    snd_pcm_uframes_t period = PERIOD_SIZE;
    snd_pcm_hw_params_set_period_size_near(pimpl_->pcm, hw,
                                           &period, nullptr);

    snd_pcm_uframes_t buffer = period * 4;
    snd_pcm_hw_params_set_buffer_size_near(pimpl_->pcm, hw, &buffer);

    err = snd_pcm_hw_params(pimpl_->pcm, hw);
    if (err < 0) {
        fprintf(stderr, "ALSA: cannot set hw params: %s\n",
                snd_strerror(err));
        return false;
    }

    // ── SW params — start after one period ─────────────────────────
    snd_pcm_sw_params_t *sw;
    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(pimpl_->pcm, sw);
    snd_pcm_sw_params_set_start_threshold(pimpl_->pcm, sw, period);
    snd_pcm_sw_params(pimpl_->pcm, sw);

    printf("ALSA ready  device=%s  rate=%u  period=%lu  buffer=%lu\n",
           device.c_str(), rate,
           static_cast<unsigned long>(period),
           static_cast<unsigned long>(buffer));
    return true;

#else
    fprintf(stderr, "NOTE: built without ALSA — no audio output\n");
    return true;
#endif
}

// ─── start / stop ───────────────────────────────────────────────────

void AudioEngine::start()
{
    if (!pimpl_ || pimpl_->running) return;
    pimpl_->running = true;

#ifdef HAS_ALSA
    pimpl_->thread = std::thread([this] {
        auto *d = pimpl_.get();

        while (d->running) {
            // 1. Fill mono float buffer
            d->callback(d->floatBuf.data(), PERIOD_SIZE);

            // 2. Convert mono float → interleaved stereo S16
            for (int i = 0; i < PERIOD_SIZE; i++) {
                float s = std::clamp(d->floatBuf[i], -1.0f, 1.0f);
                auto pcm = static_cast<int16_t>(s * 32767.0f);
                d->pcmBuf[i * 2]     = pcm;   // L
                d->pcmBuf[i * 2 + 1] = pcm;   // R
            }

            // 3. Write to ALSA
            snd_pcm_sframes_t written =
                snd_pcm_writei(d->pcm, d->pcmBuf.data(), PERIOD_SIZE);

            if (written < 0)
                snd_pcm_recover(d->pcm, static_cast<int>(written), 1);
        }
    });
#endif
}

void AudioEngine::stop()
{
    if (!pimpl_) return;
    pimpl_->running = false;
    if (pimpl_->thread.joinable())
        pimpl_->thread.join();

#ifdef HAS_ALSA
    if (pimpl_->pcm)
        snd_pcm_drop(pimpl_->pcm);
#endif
}

// ─── shutdown ───────────────────────────────────────────────────────

void AudioEngine::shutdown()
{
    if (!pimpl_) return;
    stop();

#ifdef HAS_ALSA
    if (pimpl_->pcm) {
        snd_pcm_close(pimpl_->pcm);
        pimpl_->pcm = nullptr;
    }
#endif

    pimpl_.reset();
}
