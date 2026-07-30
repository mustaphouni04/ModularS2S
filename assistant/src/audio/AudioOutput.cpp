#include "audio/AudioOutput.hpp"

#include "util/PortAudioGuard.hpp"

#include <portaudio.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace assistant::audio {

namespace {
assistant::util::PortAudioGuard g_pa_guard_instance_holder [[maybe_unused]];
}

AudioOutput::AudioOutput(int sample_rate, int channels, int device_index)
    : sample_rate_(sample_rate), channels_(channels), device_index_(device_index) {}

AudioOutput::~AudioOutput() { stop(); }

void AudioOutput::open_stream_locked() {
    close_stream_locked();

    PaStreamParameters output_params{};
    output_params.device =
        device_index_ >= 0 ? device_index_ : Pa_GetDefaultOutputDevice();
    if (output_params.device == paNoDevice) {
        throw std::runtime_error("audio: no default output device available");
    }
    output_params.channelCount = channels_;
    output_params.sampleFormat = paInt16;
    output_params.suggestedLatency =
        Pa_GetDeviceInfo(output_params.device)->defaultLowOutputLatency;
    output_params.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&stream_, nullptr, &output_params, sample_rate_,
                                 paFramesPerBufferUnspecified, paNoFlag, nullptr, nullptr);
    if (err != paNoError) {
        throw std::runtime_error(std::string("audio: Pa_OpenStream failed: ") +
                                  Pa_GetErrorText(err));
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        throw std::runtime_error(std::string("audio: Pa_StartStream failed: ") +
                                  Pa_GetErrorText(err));
    }
}

void AudioOutput::close_stream_locked() {
    if (stream_) {
        Pa_AbortStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
}

void AudioOutput::configure(int sample_rate, int channels) {
    if (sample_rate == sample_rate_ && channels == channels_ && stream_ != nullptr) {
        return;
    }
    sample_rate_ = sample_rate;
    channels_ = channels;

    std::lock_guard<std::mutex> lock(stream_mutex_);
    if (running_.load()) {
        open_stream_locked();
    }
}

void AudioOutput::start() {
    if (running_.exchange(true)) return;

    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        open_stream_locked();
    }

    writer_thread_ = std::thread(&AudioOutput::writer_loop, this);
}

void AudioOutput::stop() {
    if (!running_.exchange(false)) return;

    queue_cv_.notify_all();
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }

    std::lock_guard<std::mutex> lock(stream_mutex_);
    close_stream_locked();
}

void AudioOutput::enqueue(const std::vector<uint8_t>& pcm) {
    if (pcm.empty()) return;
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_.push_back(pcm);
    queue_cv_.notify_one();
}

void AudioOutput::flush() {
    flush_requested_.store(true);
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.clear();
    }
    queue_cv_.notify_all();

    // Discard whatever PortAudio has already buffered internally so
    // playback stops audibly right away, then bring the stream back up
    // for the next utterance. If the writer thread is mid-Pa_WriteStream()
    // this waits for that single call to return first (bounded by one
    // buffer's worth of audio, typically well under the low-latency
    // setting used in open_stream_locked()).
    std::lock_guard<std::mutex> lock(stream_mutex_);
    if (stream_) {
        open_stream_locked();
    }
}

void AudioOutput::wait_until_drained() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this] { return queue_.empty() && !playing_; });
}

bool AudioOutput::is_active() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return playing_ || !queue_.empty();
}

void AudioOutput::writer_loop() {
    while (running_.load()) {
        std::vector<uint8_t> chunk;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !queue_.empty() || !running_.load(); });
            if (!running_.load()) break;
            if (queue_.empty()) continue;
            chunk = std::move(queue_.front());
            queue_.pop_front();
            playing_ = true;
        }

        if (flush_requested_.exchange(false)) {
            // A flush arrived between us popping and writing; drop this
            // chunk too, it belongs to the interrupted utterance.
            std::lock_guard<std::mutex> lock(queue_mutex_);
            playing_ = false;
            queue_cv_.notify_all();
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(stream_mutex_);
            if (stream_) {
                size_t frame_size = static_cast<size_t>(channels_) * sizeof(int16_t);
                unsigned long frames =
                    static_cast<unsigned long>(chunk.size() / frame_size);
                if (frames > 0) {
                    PaError err = Pa_WriteStream(stream_, chunk.data(), frames);
                    if (err != paNoError && err != paOutputUnderflowed) {
                        spdlog::warn("audio: Pa_WriteStream failed: {}", Pa_GetErrorText(err));
                    }
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (queue_.empty()) {
                playing_ = false;
                queue_cv_.notify_all();
            }
        }
    }
}

}  // namespace assistant::audio
