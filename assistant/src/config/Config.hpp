#pragma once

#include <string>

namespace assistant::config {

enum class InputMode { Typed, Voice };

struct LlmConfig {
    std::string endpoint = "http://localhost:11434";
    std::string model = "qwen3:4b";
    std::string system_prompt = "You are a helpful, concise voice assistant.";
};

struct SttConfig {
    std::string host = "127.0.0.1";
    int port = 10300;
    std::string language = "en";
};

struct TtsConfig {
    std::string host = "127.0.0.1";
    int port = 10200;
    std::string voice = "en_US-lessac-medium";
};

struct AudioConfig {
    int sample_rate = 16000;
    int input_device = -1;   // -1 = default
    int output_device = -1;  // -1 = default
    // VAD tuning
    double vad_rms_threshold = 0.02;
    int vad_trailing_silence_ms = 700;
    // Without acoustic echo cancellation, letting the mic listen for a new
    // utterance while the speaker is actively playing the assistant's own
    // reply risks self-triggering. Off by default; voice mode simply mutes
    // VAD detection while audio is playing. Turn on only if your speaker/mic
    // setup (e.g. a headset, or a speaker far from the mic) doesn't feed
    // back into the mic.
    bool allow_barge_in = false;
};

struct InputConfig {
    InputMode mode = InputMode::Typed;
};

struct Config {
    LlmConfig llm;
    SttConfig stt;
    TtsConfig tts;
    AudioConfig audio;
    InputConfig input;

    // Loads defaults, then overlays a YAML file if `path` is non-empty and exists.
    static Config load(const std::string& path);

    // Applies `--mode typed|voice` / `--config <path>` / `--model <name>` style overrides.
    // Returns the (possibly re-loaded) config with CLI overrides applied.
    static Config from_args(int argc, char** argv);
};

InputMode parse_input_mode(const std::string& value);
std::string to_string(InputMode mode);

}  // namespace assistant::config
