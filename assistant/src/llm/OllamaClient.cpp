#include "llm/OllamaClient.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace assistant::llm {

OllamaClient::OllamaClient(std::string endpoint, std::string model)
    : endpoint_(std::move(endpoint)), model_(std::move(model)) {}

std::string OllamaClient::chat_stream(
    const std::vector<ChatMessage>& messages,
    const std::function<void(const std::string& token_delta)>& on_token,
    std::stop_token stop_token) {
    httplib::Client cli(endpoint_);
    // Generation of a full reply can legitimately take a while on-device;
    // the read timeout only needs to cover the gap between two chunks.
    cli.set_read_timeout(120, 0);
    cli.set_write_timeout(30, 0);

    nlohmann::json body;
    body["model"] = model_;
    body["stream"] = true;
    body["messages"] = nlohmann::json::array();
    for (const auto& m : messages) {
        body["messages"].push_back({{"role", m.role}, {"content", m.content}});
    }

    std::string full_response;
    std::string line_buffer;
    bool cancelled = false;

    auto content_receiver = [&](const char* data, size_t len) {
        if (stop_token.stop_requested()) {
            cancelled = true;
            return false;
        }

        line_buffer.append(data, len);
        size_t pos;
        while ((pos = line_buffer.find('\n')) != std::string::npos) {
            std::string line = line_buffer.substr(0, pos);
            line_buffer.erase(0, pos + 1);
            if (line.empty()) continue;

            try {
                auto j = nlohmann::json::parse(line);
                if (j.contains("message") && j["message"].contains("content")) {
                    std::string delta = j["message"]["content"].get<std::string>();
                    if (!delta.empty()) {
                        full_response += delta;
                        on_token(delta);
                    }
                }
                if (j.contains("error")) {
                    throw std::runtime_error("ollama error: " + j["error"].get<std::string>());
                }
            } catch (const nlohmann::json::parse_error& e) {
                spdlog::warn("ollama: failed to parse stream line: {}", e.what());
            }

            if (stop_token.stop_requested()) {
                cancelled = true;
                return false;
            }
        }
        return true;
    };

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/chat";
    req.headers = {{"Content-Type", "application/json"}};
    req.body = body.dump();
    req.content_receiver = [&content_receiver](const char* data, size_t len, uint64_t /*offset*/,
                                                uint64_t /*total_length*/) {
        return content_receiver(data, len);
    };

    auto res = cli.send(req);

    if (cancelled) {
        spdlog::info("ollama: stream cancelled");
        return full_response;
    }

    if (!res) {
        throw std::runtime_error("ollama: request failed: " +
                                  httplib::to_string(res.error()));
    }
    if (res->status != 200) {
        throw std::runtime_error("ollama: HTTP " + std::to_string(res->status) + ": " + res->body);
    }

    return full_response;
}

}  // namespace assistant::llm
