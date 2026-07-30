#include "assistant/TypedInputSource.hpp"

#include <iostream>

namespace assistant::core {

TypedInputSource::~TypedInputSource() { stop(); }

void TypedInputSource::start(std::stop_token stop_token,
                              std::function<void(std::string)> on_utterance) {
    reader_thread_ = std::thread(&TypedInputSource::read_loop, this, stop_token,
                                  std::move(on_utterance));
}

void TypedInputSource::stop() {
    if (reader_thread_.joinable()) {
        // std::getline blocks on stdin and can't be woken by the stop_token
        // alone; on shutdown we detach rather than hang the process waiting
        // for one more Enter keypress that may never come.
        reader_thread_.detach();
    }
}

void TypedInputSource::read_loop(std::stop_token stop_token,
                                  std::function<void(std::string)> on_utterance) {
    while (!stop_token.stop_requested()) {
        std::cout << "\n> " << std::flush;

        std::string line;
        if (!std::getline(std::cin, line)) {
            on_utterance("");  // EOF (Ctrl-D) ends the session
            return;
        }

        if (stop_token.stop_requested()) return;

        if (line == "/quit" || line == "/exit") {
            on_utterance("");
            return;
        }

        if (!line.empty()) {
            on_utterance(line);
        }
    }
}

}  // namespace assistant::core
