#include "stt/WyomingSTTClient.hpp"

#include "util/WyomingProtocol.hpp"

#include <spdlog/spdlog.h>

#include <cstring>

namespace assistant::stt {

using assistant::util::WyomingConnection;
using assistant::util::WyomingEvent;

namespace {
constexpr size_t kChunkFrames = 1024;  // frames per audio-chunk event
}

WyomingSTTClient::WyomingSTTClient(std::string host, int port, std::string language)
    : host_(std::move(host)), port_(port), language_(std::move(language)) {}

std::string WyomingSTTClient::transcribe(const std::vector<int16_t>& pcm, int sample_rate,
                                          std::stop_token stop_token) {
    if (stop_token.stop_requested() || pcm.empty()) return "";

    WyomingConnection conn;
    conn.connect(host_, port_);
    std::stop_callback cancel_on_stop(stop_token, [&conn] { conn.request_shutdown(); });

    try {
        nlohmann::json start_data;
        start_data["rate"] = sample_rate;
        start_data["width"] = 2;
        start_data["channels"] = 1;
        if (!language_.empty()) start_data["language"] = language_;
        conn.write_event(WyomingEvent::simple("audio-start", start_data));

        nlohmann::json chunk_data;
        chunk_data["rate"] = sample_rate;
        chunk_data["width"] = 2;
        chunk_data["channels"] = 1;

        for (size_t offset = 0; offset < pcm.size(); offset += kChunkFrames) {
            if (stop_token.stop_requested()) return "";

            size_t count = std::min(kChunkFrames, pcm.size() - offset);
            std::vector<uint8_t> payload(count * sizeof(int16_t));
            std::memcpy(payload.data(), pcm.data() + offset, payload.size());

            conn.write_event(WyomingEvent::with_payload("audio-chunk", chunk_data, payload));
        }

        conn.write_event(WyomingEvent::simple("audio-stop"));

        while (true) {
            WyomingEvent event = conn.read_event();
            if (event.type == "transcript") {
                std::string text;
                if (event.data.contains("text")) {
                    text = event.data["text"].get<std::string>();
                }
                conn.close();
                return text;
            }
            spdlog::debug("wyoming-stt: ignoring event type '{}'", event.type);
        }
    } catch (const std::exception& e) {
        if (stop_token.stop_requested()) {
            spdlog::info("wyoming-stt: transcription cancelled");
            return "";
        }
        throw;
    }
}

}  // namespace assistant::stt
