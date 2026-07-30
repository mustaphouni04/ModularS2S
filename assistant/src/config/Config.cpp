#include "config/Config.hpp"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <stdexcept>

namespace assistant::config {

InputMode parse_input_mode(const std::string& value) {
    if (value == "typed") return InputMode::Typed;
    if (value == "voice") return InputMode::Voice;
    throw std::invalid_argument("input mode must be 'typed' or 'voice', got: " + value);
}

std::string to_string(InputMode mode) {
    return mode == InputMode::Typed ? "typed" : "voice";
}

namespace {

void overlay_yaml(Config& cfg, const YAML::Node& root) {
    if (auto n = root["llm"]) {
        if (n["endpoint"]) cfg.llm.endpoint = n["endpoint"].as<std::string>();
        if (n["model"]) cfg.llm.model = n["model"].as<std::string>();
        if (n["system_prompt"]) cfg.llm.system_prompt = n["system_prompt"].as<std::string>();
    }
    if (auto n = root["stt"]) {
        if (n["host"]) cfg.stt.host = n["host"].as<std::string>();
        if (n["port"]) cfg.stt.port = n["port"].as<int>();
        if (n["endpoint"]) {
            // allow "host:port" shorthand
            auto ep = n["endpoint"].as<std::string>();
            auto pos = ep.find(':');
            if (pos != std::string::npos) {
                cfg.stt.host = ep.substr(0, pos);
                cfg.stt.port = std::stoi(ep.substr(pos + 1));
            }
        }
        if (n["language"]) cfg.stt.language = n["language"].as<std::string>();
    }
    if (auto n = root["tts"]) {
        if (n["host"]) cfg.tts.host = n["host"].as<std::string>();
        if (n["port"]) cfg.tts.port = n["port"].as<int>();
        if (n["endpoint"]) {
            auto ep = n["endpoint"].as<std::string>();
            auto pos = ep.find(':');
            if (pos != std::string::npos) {
                cfg.tts.host = ep.substr(0, pos);
                cfg.tts.port = std::stoi(ep.substr(pos + 1));
            }
        }
        if (n["voice"]) cfg.tts.voice = n["voice"].as<std::string>();
    }
    if (auto n = root["audio"]) {
        if (n["sample_rate"]) cfg.audio.sample_rate = n["sample_rate"].as<int>();
        if (n["input_device"]) cfg.audio.input_device = n["input_device"].as<int>();
        if (n["output_device"]) cfg.audio.output_device = n["output_device"].as<int>();
        if (n["vad_rms_threshold"]) cfg.audio.vad_rms_threshold = n["vad_rms_threshold"].as<double>();
        if (n["vad_trailing_silence_ms"])
            cfg.audio.vad_trailing_silence_ms = n["vad_trailing_silence_ms"].as<int>();
        if (n["allow_barge_in"]) cfg.audio.allow_barge_in = n["allow_barge_in"].as<bool>();
    }
    if (auto n = root["input"]) {
        if (n["mode"]) cfg.input.mode = parse_input_mode(n["mode"].as<std::string>());
    }
}

}  // namespace

Config Config::load(const std::string& path) {
    Config cfg{};
    if (!path.empty() && std::filesystem::exists(path)) {
        YAML::Node root = YAML::LoadFile(path);
        overlay_yaml(cfg, root);
    }
    return cfg;
}

Config Config::from_args(int argc, char** argv) {
    std::string config_path;
    // First pass: find --config so file values load before CLI overrides win.
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    Config cfg = Config::load(config_path);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            cfg.input.mode = parse_input_mode(argv[++i]);
        } else if (arg == "--model" && i + 1 < argc) {
            cfg.llm.model = argv[++i];
        } else if (arg == "--llm-endpoint" && i + 1 < argc) {
            cfg.llm.endpoint = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            ++i;  // already consumed above
        }
    }

    return cfg;
}

}  // namespace assistant::config
