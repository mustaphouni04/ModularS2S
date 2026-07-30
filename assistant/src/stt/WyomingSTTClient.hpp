#pragma once

#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

namespace assistant::stt {

// Client for a wyoming-whisper STT server (e.g. dustynv/wyoming-whisper on
// the Jetson, GPU-accelerated faster-whisper). Endpointing (deciding when
// the user has stopped talking) is the caller's job - VoiceInputSource's
// VAD decides that and hands over one complete utterance's worth of audio
// per call, matching how wyoming-whisper expects audio-start/-chunk/-stop.
class WyomingSTTClient {
public:
    WyomingSTTClient(std::string host, int port, std::string language);

    // Sends the full utterance and returns the recognized transcript.
    // pcm holds signed 16-bit little-endian mono samples at `sample_rate`.
    // Throws std::runtime_error on connection failure. Returns "" if
    // cancelled via stop_token before a transcript arrives.
    std::string transcribe(const std::vector<int16_t>& pcm, int sample_rate,
                            std::stop_token stop_token);

private:
    std::string host_;
    int port_;
    std::string language_;
};

}  // namespace assistant::stt
