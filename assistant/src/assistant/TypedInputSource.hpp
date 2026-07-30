#pragma once

#include "assistant/InputSource.hpp"

#include <thread>

namespace assistant::core {

// "Hands needed" input mode: reads one line of typed text per turn from
// stdin on a dedicated thread. The microphone is never opened in this mode.
class TypedInputSource : public InputSource {
public:
    ~TypedInputSource() override;

    void start(std::stop_token stop_token, std::function<void(std::string)> on_utterance) override;
    void stop() override;

private:
    void read_loop(std::stop_token stop_token, std::function<void(std::string)> on_utterance);

    std::thread reader_thread_;
};

}  // namespace assistant::core
