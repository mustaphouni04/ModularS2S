#include "assistant/Assistant.hpp"

#include <spdlog/spdlog.h>

namespace assistant::core {

Assistant::Assistant(config::Config cfg, std::unique_ptr<InputSource> input_source)
    : cfg_(std::move(cfg)),
      input_source_(std::move(input_source)),
      conversation_(cfg_.llm.system_prompt),
      ollama_(cfg_.llm.endpoint, cfg_.llm.model),
      tts_client_(cfg_.tts.host, cfg_.tts.port, cfg_.tts.voice),
      // Real output sample rate is corrected at runtime once the TTS
      // server's audio-start event reports its actual format (typically
      // 22050 Hz for Piper voices) - see AudioOutput::configure().
      audio_output_(cfg_.audio.sample_rate, /*channels=*/1, cfg_.audio.output_device) {}

Assistant::~Assistant() {
    running_.store(false);
    sentence_cv_.notify_all();
    if (tts_thread_.joinable()) tts_thread_.join();
    if (turn_thread_.joinable()) turn_thread_.join();
}

void Assistant::run(std::stop_token stop_token) {
    running_.store(true);
    audio_output_.start();
    tts_thread_ = std::thread(&Assistant::tts_worker_loop, this);

    std::stop_callback wake_on_session_stop(stop_token, [this] {
        std::lock_guard<std::mutex> lock(dispatch_mutex_);
        shutdown_requested_ = true;
        dispatch_cv_.notify_all();
    });

    spdlog::info("assistant: ready (mode={}, barge-in={})", config::to_string(cfg_.input.mode),
                 cfg_.audio.allow_barge_in);

    input_source_->set_activity_probe(
        [this] { return cfg_.audio.allow_barge_in || !audio_output_.is_active(); });
    input_source_->start(stop_token, [this](std::string text) { on_utterance(std::move(text)); });

    while (true) {
        std::string text;
        {
            std::unique_lock<std::mutex> lock(dispatch_mutex_);
            dispatch_cv_.wait(lock, [this] { return pending_text_.has_value() || shutdown_requested_; });
            if (shutdown_requested_ && !pending_text_.has_value()) break;
            text = std::move(*pending_text_);
            pending_text_.reset();
        }

        // Barge-in: a new utterance always preempts whatever the previous
        // turn left in flight (LLM stream, queued sentences, playback).
        current_turn_stop_.request_stop();
        {
            std::lock_guard<std::mutex> lock(sentence_mutex_);
            sentence_queue_.clear();
        }
        audio_output_.flush();
        if (turn_thread_.joinable()) turn_thread_.join();

        current_turn_stop_ = std::stop_source{};
        auto turn_token = current_turn_stop_.get_token();
        turn_thread_ = std::thread(&Assistant::run_turn, this, text, turn_token);
    }

    current_turn_stop_.request_stop();
    if (turn_thread_.joinable()) turn_thread_.join();

    input_source_->stop();

    running_.store(false);
    sentence_cv_.notify_all();
    if (tts_thread_.joinable()) tts_thread_.join();
    audio_output_.stop();
}

void Assistant::on_utterance(std::string text) {
    std::lock_guard<std::mutex> lock(dispatch_mutex_);
    if (text.empty()) {
        shutdown_requested_ = true;
    } else {
        pending_text_ = std::move(text);
    }
    dispatch_cv_.notify_all();
}

void Assistant::run_turn(const std::string& user_text, std::stop_token turn_stop_token) {
    conversation_.add_user_message(user_text);
    auto messages = conversation_.build_messages();
    sentence_buffer_.reset();

    auto on_token = [&](const std::string& delta) {
        auto sentences = sentence_buffer_.push(delta);
        for (auto& s : sentences) {
            std::lock_guard<std::mutex> lock(sentence_mutex_);
            sentence_queue_.push_back({s, turn_stop_token});
            sentence_cv_.notify_one();
        }
    };

    std::string full_response;
    try {
        full_response = ollama_.chat_stream(messages, on_token, turn_stop_token);
    } catch (const std::exception& e) {
        spdlog::error("assistant: LLM turn failed: {}", e.what());
        return;
    }

    if (!turn_stop_token.stop_requested()) {
        auto trailing = sentence_buffer_.flush();
        for (auto& s : trailing) {
            std::lock_guard<std::mutex> lock(sentence_mutex_);
            sentence_queue_.push_back({s, turn_stop_token});
            sentence_cv_.notify_one();
        }
    }

    if (!full_response.empty()) {
        conversation_.add_assistant_message(full_response);
    }
}

void Assistant::tts_worker_loop() {
    while (true) {
        QueuedSentence item;
        {
            std::unique_lock<std::mutex> lock(sentence_mutex_);
            sentence_cv_.wait(lock, [this] { return !sentence_queue_.empty() || !running_.load(); });
            if (sentence_queue_.empty()) {
                if (!running_.load()) break;
                continue;
            }
            item = std::move(sentence_queue_.front());
            sentence_queue_.pop_front();
        }

        if (item.turn_stop_token.stop_requested()) continue;
        speak_sentence(item.text, item.turn_stop_token);
    }
}

void Assistant::speak_sentence(const std::string& sentence, std::stop_token stop_token) {
    if (sentence.empty() || stop_token.stop_requested()) return;

    try {
        tts_client_.synthesize(
            sentence,
            [this](const tts::AudioFormat& fmt) { audio_output_.configure(fmt.rate, fmt.channels); },
            [this](const std::vector<uint8_t>& chunk) { audio_output_.enqueue(chunk); }, stop_token);
    } catch (const std::exception& e) {
        spdlog::warn("assistant: TTS failed for sentence '{}': {}", sentence, e.what());
    }
}

}  // namespace assistant::core
