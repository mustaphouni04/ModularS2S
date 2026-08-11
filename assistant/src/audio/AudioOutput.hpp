#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

// Opaque PortAudio stream handle (avoids leaking <portaudio.h> into callers).
// Matches portaudio.h's own `typedef void PaStream;` exactly.
typedef void PaStream;

namespace assistant::audio {

// Queue-driven PCM playback on top of PortAudio's blocking write API.
// One background thread pulls buffers off the queue and writes them to the
// device; flush() drops everything immediately so a new user utterance can
// interrupt (barge-in) whatever the assistant is currently saying.
class AudioOutput {
public:
    AudioOutput(int sample_rate, int channels, int device_index = -1);
    ~AudioOutput();

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    void start();
    void stop();

    // Reconfigures sample rate / channels if they differ from the current
    // stream (e.g. TTS voice format only known after the first response);
    // reopens the underlying PortAudio stream when needed.
    void configure(int sample_rate, int channels);

    // pcm holds signed 16-bit little-endian samples.
    void enqueue(const std::vector<uint8_t>& pcm);

    // Immediately stops playback and discards all queued and in-flight
    // audio. Safe to call from any thread (used for barge-in).
    void flush();

    // Blocks until the queue empties and playback catches up naturally.
    void wait_until_drained();

    // True while there is queued or in-flight audio. Used by voice mode to
    // decide whether to mute the mic's VAD (see Config::allow_barge_in).
    bool is_active() const;

private:
    void writer_loop();
    void open_stream_locked();
    void close_stream_locked();

    int sample_rate_;
    int channels_;
    int device_index_;

    PaStream* stream_ = nullptr;
    std::mutex stream_mutex_;

    std::thread writer_thread_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<std::vector<uint8_t>> queue_;
    std::atomic<bool> running_{false};
    std::atomic<bool> flush_requested_{false};
    bool playing_ = false;  // true while queue non-empty or a write is in flight
};

}  // namespace assistant::audio
