#include "audio/AudioInput.hpp"

#include "util/PortAudioGuard.hpp"

#include <portaudio.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <stdexcept>

namespace assistant::audio {

namespace {
assistant::util::PortAudioGuard g_pa_guard_instance_holder [[maybe_unused]];

constexpr int kFrameDurationMs = 20;
constexpr int kMaxUtteranceMs = 30000;  // safety cap

double rms(const std::vector<int16_t>& frame) {
    if (frame.empty()) return 0.0;
    double sum_sq = 0.0;
    for (int16_t s : frame) {
        double normalized = s / 32768.0;
        sum_sq += normalized * normalized;
    }
    return std::sqrt(sum_sq / frame.size());
}

}  // namespace

AudioInput::AudioInput(int sample_rate, double vad_rms_threshold, int trailing_silence_ms,
                        int device_index)
    : sample_rate_(sample_rate),
      vad_rms_threshold_(vad_rms_threshold),
      trailing_silence_ms_(trailing_silence_ms),
      device_index_(device_index),
      chunk_frames_(sample_rate * kFrameDurationMs / 1000) {}

AudioInput::~AudioInput() { stop(); }

void AudioInput::start() {
    if (stream_) return;

    PaStreamParameters input_params{};
    input_params.device = device_index_ >= 0 ? device_index_ : Pa_GetDefaultInputDevice();
    if (input_params.device == paNoDevice) {
        throw std::runtime_error("audio: no default input device available");
    }
    input_params.channelCount = 1;
    input_params.sampleFormat = paInt16;
    input_params.suggestedLatency = Pa_GetDeviceInfo(input_params.device)->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&stream_, &input_params, nullptr, sample_rate_, chunk_frames_,
                                 paNoFlag, nullptr, nullptr);
    if (err != paNoError) {
        throw std::runtime_error(std::string("audio: Pa_OpenStream (input) failed: ") +
                                  Pa_GetErrorText(err));
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        throw std::runtime_error(std::string("audio: Pa_StartStream (input) failed: ") +
                                  Pa_GetErrorText(err));
    }
}

void AudioInput::stop() {
    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
}

std::vector<int16_t> AudioInput::capture_utterance(std::stop_token stop_token,
                                                     const std::function<bool()>& should_listen) {
    if (!stream_) start();

    std::vector<int16_t> utterance;
    std::vector<int16_t> chunk(chunk_frames_);

    bool speech_started = false;
    int silence_ms = 0;
    int total_ms = 0;

    while (!stop_token.stop_requested()) {
        PaError err = Pa_ReadStream(stream_, chunk.data(), chunk_frames_);
        if (err != paNoError && err != paInputOverflowed) {
            spdlog::warn("audio: Pa_ReadStream failed: {}", Pa_GetErrorText(err));
            break;
        }

        if (!speech_started) {
            // Even while muted (should_listen()==false) we must keep
            // draining the hardware buffer, we just discard the audio.
            if (should_listen && !should_listen()) {
                continue;
            }

            if (rms(chunk) >= vad_rms_threshold_) {
                speech_started = true;
                utterance.insert(utterance.end(), chunk.begin(), chunk.end());
                silence_ms = 0;
                total_ms += kFrameDurationMs;
            }
            continue;
        }

        utterance.insert(utterance.end(), chunk.begin(), chunk.end());
        total_ms += kFrameDurationMs;

        if (rms(chunk) >= vad_rms_threshold_) {
            silence_ms = 0;
        } else {
            silence_ms += kFrameDurationMs;
            if (silence_ms >= trailing_silence_ms_) {
                break;
            }
        }

        if (total_ms >= kMaxUtteranceMs) {
            spdlog::warn("audio: utterance hit max length ({} ms), cutting off", kMaxUtteranceMs);
            break;
        }
    }

    return utterance;
}

}  // namespace assistant::audio
