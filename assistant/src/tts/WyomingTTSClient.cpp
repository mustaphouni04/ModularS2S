#include "tts/WyomingTTSClient.hpp"

#include <spdlog/spdlog.h>

namespace assistant::tts {

using assistant::util::WyomingConnection;
using assistant::util::WyomingEvent;

WyomingTTSClient::WyomingTTSClient(std::string host, int port, std::string voice)
    : host_(std::move(host)), port_(port), voice_(std::move(voice)) {}

void WyomingTTSClient::synthesize(
    const std::string& text, const std::function<void(const AudioFormat&)>& on_format,
    const std::function<void(const std::vector<uint8_t>&)>& on_audio_chunk,
    std::stop_token stop_token) {
    if (stop_token.stop_requested()) return;

    WyomingConnection conn;
    conn.connect(host_, port_);

    // Unblock any in-flight recv()/send() the moment cancellation is
    // requested, since std::stop_token alone can't interrupt blocking I/O.
    std::stop_callback cancel_on_stop(stop_token, [&conn] { conn.request_shutdown(); });

    nlohmann::json data;
    data["text"] = text;
    if (!voice_.empty()) {
        data["voice"] = {{"name", voice_}};
    }
    conn.write_event(WyomingEvent::simple("synthesize", data));

    bool format_sent = false;
    AudioFormat format;

    try {
        while (true) {
            WyomingEvent event = conn.read_event();

            if (event.type == "audio-start") {
                if (event.data.contains("rate")) format.rate = event.data["rate"].get<int>();
                if (event.data.contains("width")) format.width = event.data["width"].get<int>();
                if (event.data.contains("channels"))
                    format.channels = event.data["channels"].get<int>();
                on_format(format);
                format_sent = true;
            } else if (event.type == "audio-chunk") {
                if (!format_sent) {
                    // Server skipped audio-start; fall back to defaults.
                    on_format(format);
                    format_sent = true;
                }
                if (event.has_payload && !event.payload.empty()) {
                    on_audio_chunk(event.payload);
                }
            } else if (event.type == "audio-stop") {
                break;
            } else {
                spdlog::debug("wyoming-tts: ignoring event type '{}'", event.type);
            }
        }
    } catch (const std::exception& e) {
        if (stop_token.stop_requested()) {
            spdlog::info("wyoming-tts: synthesis cancelled");
        } else {
            throw;
        }
    }

    conn.close();
}

}  // namespace assistant::tts
