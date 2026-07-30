#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace assistant::util {

// One Wyoming protocol event: a JSON header line, optionally followed by a
// raw binary payload whose length is carried in the header as
// "payload_length". See https://github.com/rhasspy/wyoming
struct WyomingEvent {
    std::string type;
    nlohmann::json data = nlohmann::json::object();
    std::vector<uint8_t> payload;
    bool has_payload = false;

    static WyomingEvent with_payload(std::string type, nlohmann::json data,
                                      std::vector<uint8_t> payload) {
        WyomingEvent e;
        e.type = std::move(type);
        e.data = std::move(data);
        e.payload = std::move(payload);
        e.has_payload = true;
        return e;
    }

    static WyomingEvent simple(std::string type, nlohmann::json data = nlohmann::json::object()) {
        WyomingEvent e;
        e.type = std::move(type);
        e.data = std::move(data);
        return e;
    }
};

// Blocking, synchronous TCP client speaking the Wyoming wire format.
// Intended to be used from a single dedicated worker thread per call
// (one connection is opened per STT/TTS request), so no internal locking.
class WyomingConnection {
public:
    WyomingConnection() = default;
    ~WyomingConnection();

    WyomingConnection(const WyomingConnection&) = delete;
    WyomingConnection& operator=(const WyomingConnection&) = delete;

    // Throws std::runtime_error on failure.
    void connect(const std::string& host, int port);

    void write_event(const WyomingEvent& event);

    // Throws std::runtime_error on disconnect/parse error.
    WyomingEvent read_event();

    void close();

    // Safe to call from another thread while a read_event()/write_event()
    // call is blocked on this connection; unblocks the pending recv/send
    // so the owning thread can observe cancellation and unwind. Does not
    // close the file descriptor itself (close()/destructor still required).
    void request_shutdown();

    bool is_open() const { return fd_.load() >= 0; }

private:
    void write_all(const uint8_t* data, size_t len);
    void read_exact(uint8_t* data, size_t len);
    std::string read_line();

    std::atomic<int> fd_{-1};
};

}  // namespace assistant::util
