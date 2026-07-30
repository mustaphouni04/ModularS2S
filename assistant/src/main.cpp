#include "assistant/Assistant.hpp"
#include "assistant/TypedInputSource.hpp"
#include "assistant/VoiceInputSource.hpp"
#include "config/Config.hpp"

#include <spdlog/spdlog.h>

#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <stop_token>

namespace {

std::stop_source g_stop_source;

void handle_signal(int) { g_stop_source.request_stop(); }

void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [--mode typed|voice] [--config <path>] "
              << "[--model <name>] [--llm-endpoint <url>]\n"
              << "\n"
              << "  --mode typed   Hands-needed: type each turn, no microphone opened.\n"
              << "  --mode voice   Hands-free: mic + VAD + speech recognition.\n"
              << "  --config       YAML config file (defaults layered under CLI flags).\n";
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    assistant::config::Config cfg;
    try {
        cfg = assistant::config::Config::from_args(argc, argv);
    } catch (const std::exception& e) {
        spdlog::error("config error: {}", e.what());
        print_usage(argv[0]);
        return 1;
    }

    spdlog::info("ModularS2S assistant starting (mode={}, llm={} @ {})",
                 assistant::config::to_string(cfg.input.mode), cfg.llm.model, cfg.llm.endpoint);

    std::unique_ptr<assistant::core::InputSource> input_source;
    try {
        if (cfg.input.mode == assistant::config::InputMode::Typed) {
            input_source = std::make_unique<assistant::core::TypedInputSource>();
        } else {
            input_source = std::make_unique<assistant::core::VoiceInputSource>(cfg);
        }
    } catch (const std::exception& e) {
        spdlog::error("failed to initialize input source: {}", e.what());
        return 1;
    }

    try {
        assistant::core::Assistant assistant(cfg, std::move(input_source));
        assistant.run(g_stop_source.get_token());
    } catch (const std::exception& e) {
        spdlog::error("fatal error: {}", e.what());
        return 1;
    }

    spdlog::info("assistant: shut down cleanly");
    return 0;
}
