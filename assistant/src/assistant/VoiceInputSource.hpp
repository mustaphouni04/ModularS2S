#pragma once

#include "assistant/InputSource.hpp"
#include "audio/AudioInput.hpp"
#include "config/Config.hpp"
#include "stt/WyomingSTTClient.hpp"

#include <thread>

namespace assistant::core {

// "Hands-free" input mode: continuously listens on the mic, uses
// AudioInput's energy-based VAD to detect a complete utterance, sends it to
// wyoming-whisper for transcription, and reports the transcript. No typing
// required.
class VoiceInputSource : public InputSource {
public:
    explicit VoiceInputSource(const config::Config& cfg);
    ~VoiceInputSource() override;

    void start(std::stop_token stop_token, std::function<void(std::string)> on_utterance) override;
    void stop() override;
    void set_activity_probe(std::function<bool()> should_listen) override;

private:
    void capture_loop(std::stop_token stop_token, std::function<void(std::string)> on_utterance);

    audio::AudioInput audio_input_;
    stt::WyomingSTTClient stt_client_;
    int sample_rate_;

    std::function<bool()> should_listen_;
    std::thread capture_thread_;
};

}  // namespace assistant::core
