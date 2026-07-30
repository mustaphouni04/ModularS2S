#include "config/Config.hpp"
#include "test_util.hpp"

#include <cstdio>
#include <fstream>

using assistant::config::Config;
using assistant::config::InputMode;

namespace {

void test_defaults() {
    Config cfg = Config::load("");
    CHECK_EQ(cfg.llm.endpoint, std::string("http://localhost:11434"));
    CHECK(cfg.input.mode == InputMode::Typed);
}

void test_yaml_overlay() {
    const char* path = "/tmp/modular_s2s_test_config.yaml";
    {
        std::ofstream f(path);
        f << "llm:\n"
             "  model: llama3.2:3b\n"
             "stt:\n"
             "  endpoint: 127.0.0.1:10300\n"
             "tts:\n"
             "  endpoint: 127.0.0.1:10200\n"
             "input:\n"
             "  mode: voice\n";
    }

    Config cfg = Config::load(path);
    CHECK_EQ(cfg.llm.model, std::string("llama3.2:3b"));
    CHECK_EQ(cfg.stt.host, std::string("127.0.0.1"));
    CHECK_EQ(cfg.stt.port, 10300);
    CHECK_EQ(cfg.tts.port, 10200);
    CHECK(cfg.input.mode == InputMode::Voice);

    std::remove(path);
}

void test_cli_overrides_config_file() {
    const char* argv[] = {"assistant", "--mode", "voice", "--model", "qwen3:4b"};
    Config cfg = Config::from_args(5, const_cast<char**>(argv));
    CHECK(cfg.input.mode == InputMode::Voice);
    CHECK_EQ(cfg.llm.model, std::string("qwen3:4b"));
}

}  // namespace

int main() {
    test_defaults();
    test_yaml_overlay();
    test_cli_overrides_config_file();

    if (g_failures > 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all config checks passed\n";
    return 0;
}
