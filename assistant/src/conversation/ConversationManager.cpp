#include "conversation/ConversationManager.hpp"

namespace assistant::conversation {

ConversationManager::ConversationManager(std::string system_prompt, std::size_t max_turns)
    : system_prompt_(std::move(system_prompt)), max_turns_(max_turns) {}

void ConversationManager::add_user_message(const std::string& text) {
    history_.push_back({"user", text});
    trim();
}

void ConversationManager::add_assistant_message(const std::string& text) {
    history_.push_back({"assistant", text});
    trim();
}

std::vector<llm::ChatMessage> ConversationManager::build_messages() const {
    std::vector<llm::ChatMessage> messages;
    messages.reserve(history_.size() + 1);
    if (!system_prompt_.empty()) {
        messages.push_back({"system", system_prompt_});
    }
    messages.insert(messages.end(), history_.begin(), history_.end());
    return messages;
}

void ConversationManager::reset() { history_.clear(); }

void ConversationManager::trim() {
    // Keep the most recent max_turns_ user+assistant messages (a "turn" is
    // one user + one assistant message, so cap at 2*max_turns_ entries).
    const std::size_t cap = max_turns_ * 2;
    if (history_.size() > cap) {
        history_.erase(history_.begin(), history_.begin() + (history_.size() - cap));
    }
}

}  // namespace assistant::conversation
