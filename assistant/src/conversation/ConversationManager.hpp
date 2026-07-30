#pragma once

#include "llm/OllamaClient.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace assistant::conversation {

// Owns the system prompt and rolling user/assistant history, and hands back
// the message list OllamaClient needs for the next turn. Trims from the
// oldest non-system messages once the history grows past max_turns.
class ConversationManager {
public:
    explicit ConversationManager(std::string system_prompt, std::size_t max_turns = 12);

    void add_user_message(const std::string& text);
    void add_assistant_message(const std::string& text);

    // Messages to send to OllamaClient::chat_stream for the next turn,
    // system prompt first.
    std::vector<llm::ChatMessage> build_messages() const;

    void reset();

private:
    void trim();

    std::string system_prompt_;
    std::size_t max_turns_;
    std::vector<llm::ChatMessage> history_;  // user/assistant only
};

}  // namespace assistant::conversation
