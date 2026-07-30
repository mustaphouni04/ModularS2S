#pragma once

#include <cstdint>
#include <functional>
#include <stop_token>
#include <vector>

typedef void PaStream;  // matches AudioOutput.hpp / portaudio.h

namespace assistant::audio {

// Microphone capture with a simple energy-based (RMS) voice-activity
// detector: waits for speech to start, accumulates samples, and returns
// the utterance once trailing silence exceeds the configured timeout.
// No acoustic echo cancellation - see Config::allow_barge_in.
class AudioInput {
public:
    AudioInput(int sample_rate, double vad_rms_threshold, int trailing_silence_ms,
               int device_index = -1);
    ~AudioInput();

    AudioInput(const AudioInput&) = delete;
    AudioInput& operator=(const AudioInput&) = delete;

    void start();
    void stop();

    // Blocks until one full utterance is captured, `stop_token` fires, or
    // should_listen() (polled only while no speech has started yet) returns
    // false and then stays false for the whole waiting period. Returns an
    // empty vector if interrupted before any speech was detected.
    std::vector<int16_t> capture_utterance(std::stop_token stop_token,
                                            const std::function<bool()>& should_listen);

private:
    int sample_rate_;
    double vad_rms_threshold_;
    int trailing_silence_ms_;
    int device_index_;
    int chunk_frames_;

    PaStream* stream_ = nullptr;
};

}  // namespace assistant::audio
