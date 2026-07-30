#pragma once

#include "assistant/InputSource.hpp"
#include "audio/AudioOutput.hpp"
#include "config/Config.hpp"
#include "conversation/ConversationManager.hpp"
#include "conversation/SentenceBuffer.hpp"
#include "llm/OllamaClient.hpp"
#include "tts/WyomingTTSClient.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace assistant::core {

// Main event loop. Owns the LLM/TTS clients, conversation memory and audio
// output; delegates "how does user input arrive" to an InputSource so the
// same loop drives both typed (hands-needed) and voice (hands-free) modes.
//
// Threading model:
//  - InputSource runs its own capture thread(s) and pushes recognized
//    utterances into this class via on_utterance().
//  - A dispatcher loop (the thread that calls run()) turns each utterance
//    into a turn, cancelling whatever the previous turn left in flight
//    first - this is what makes barge-in work: a new utterance always wins.
//  - Each turn runs on its own worker thread (turn_thread_) so the
//    dispatcher stays free to accept the next utterance immediately.
//  - A dedicated TTS worker thread drains completed sentences off a queue
//    and speaks them, so speech for sentence N can start while the LLM is
//    still generating sentence N+1.
class Assistant {
public:
    explicit Assistant(config::Config cfg, std::unique_ptr<InputSource> input_source);
    ~Assistant();

    // Blocks until the input source signals shutdown (EOF / /quit) or
    // stop_token fires.
    void run(std::stop_token stop_token);

    bool is_speaking() const { return audio_output_.is_active(); }
    bool allow_barge_in() const { return cfg_.audio.allow_barge_in; }

private:
    void on_utterance(std::string text);
    void run_turn(const std::string& user_text, std::stop_token turn_stop_token);
    void tts_worker_loop();
    void speak_sentence(const std::string& sentence, std::stop_token stop_token);

    config::Config cfg_;
    std::unique_ptr<InputSource> input_source_;

    conversation::ConversationManager conversation_;
    llm::OllamaClient ollama_;
    tts::WyomingTTSClient tts_client_;
    audio::AudioOutput audio_output_;
    conversation::SentenceBuffer sentence_buffer_;

    // Utterance -> turn dispatch
    std::mutex dispatch_mutex_;
    std::condition_variable dispatch_cv_;
    std::optional<std::string> pending_text_;
    bool shutdown_requested_ = false;
    std::stop_source current_turn_stop_;
    std::thread turn_thread_;

    // Sentence -> speech
    struct QueuedSentence {
        std::string text;
        std::stop_token turn_stop_token;
    };
    std::mutex sentence_mutex_;
    std::condition_variable sentence_cv_;
    std::deque<QueuedSentence> sentence_queue_;
    std::thread tts_thread_;
    std::atomic<bool> running_{false};
};

}  // namespace assistant::core
