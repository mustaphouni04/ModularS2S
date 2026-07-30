#include "assistant/VoiceInputSource.hpp"

#include <spdlog/spdlog.h>

namespace assistant::core {

VoiceInputSource::VoiceInputSource(const config::Config& cfg)
    : audio_input_(cfg.audio.sample_rate, cfg.audio.vad_rms_threshold,
                   cfg.audio.vad_trailing_silence_ms, cfg.audio.input_device),
      stt_client_(cfg.stt.host, cfg.stt.port, cfg.stt.language),
      sample_rate_(cfg.audio.sample_rate) {}

VoiceInputSource::~VoiceInputSource() { stop(); }

void VoiceInputSource::set_activity_probe(std::function<bool()> should_listen) {
    should_listen_ = std::move(should_listen);
}

void VoiceInputSource::start(std::stop_token stop_token,
                              std::function<void(std::string)> on_utterance) {
    audio_input_.start();
    capture_thread_ = std::thread(&VoiceInputSource::capture_loop, this, stop_token,
                                   std::move(on_utterance));
}

void VoiceInputSource::stop() {
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }
    audio_input_.stop();
}

void VoiceInputSource::capture_loop(std::stop_token stop_token,
                                     std::function<void(std::string)> on_utterance) {
    spdlog::info("voice input: listening (say something)");

    while (!stop_token.stop_requested()) {
        std::vector<int16_t> pcm = audio_input_.capture_utterance(stop_token, should_listen_);
        if (pcm.empty() || stop_token.stop_requested()) continue;

        spdlog::debug("voice input: captured {} samples, transcribing", pcm.size());

        std::string text;
        try {
            text = stt_client_.transcribe(pcm, sample_rate_, stop_token);
        } catch (const std::exception& e) {
            spdlog::warn("voice input: transcription failed: {}", e.what());
            continue;
        }

        if (!text.empty()) {
            on_utterance(text);
        }
    }
}

}  // namespace assistant::core
