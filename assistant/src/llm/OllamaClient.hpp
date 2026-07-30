#pragma once

#include <functional>
#include <stop_token>
#include <string>
#include <vector>

namespace assistant::llm {

struct ChatMessage {
    std::string role;  // "system" | "user" | "assistant"
    std::string content;
};

// Streaming HTTP client for Ollama's POST /api/chat (stream:true).
// One instance can be reused across turns; each call to chat_stream()
// blocks the calling thread until the response completes or is cancelled
// via the given stop_token, invoking on_token for every incremental
// content delta as it arrives off the wire (never buffering the full reply).
class OllamaClient {
public:
    OllamaClient(std::string endpoint, std::string model);

    // Returns the full accumulated assistant response text.
    // Throws std::runtime_error on HTTP/connection failure.
    std::string chat_stream(const std::vector<ChatMessage>& messages,
                             const std::function<void(const std::string& token_delta)>& on_token,
                             std::stop_token stop_token);

private:
    std::string endpoint_;
    std::string model_;
};

}  // namespace assistant::llm
