#pragma once

#include "util/WyomingProtocol.hpp"

#include <cstdint>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

namespace assistant::tts {

struct AudioFormat {
    int rate = 22050;
    int width = 2;  // bytes per sample
    int channels = 1;
};

// Client for a wyoming-piper TTS server (e.g. dustynv/wyoming-piper on the
// Jetson, GPU-accelerated). Opens one connection per synthesize() call.
class WyomingTTSClient {
public:
    WyomingTTSClient(std::string host, int port, std::string voice);

    // Synthesizes `text` and streams PCM audio out via on_audio_chunk as it
    // arrives from the server (never waits for the whole utterance).
    // on_format is invoked once, before the first chunk. Blocks until the
    // server sends audio-stop, the connection closes, or stop_token fires.
    void synthesize(const std::string& text,
                     const std::function<void(const AudioFormat&)>& on_format,
                     const std::function<void(const std::vector<uint8_t>&)>& on_audio_chunk,
                     std::stop_token stop_token);

private:
    std::string host_;
    int port_;
    std::string voice_;
};

}  // namespace assistant::tts
