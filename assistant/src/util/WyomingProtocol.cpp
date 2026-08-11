#include "util/WyomingProtocol.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <utility>

#include <cstring>
#include <stdexcept>

namespace assistant::util {

WyomingConnection::~WyomingConnection() { close(); }

void WyomingConnection::connect(const std::string& host, int port) {
    close();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    const std::string port_str = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
    if (rc != 0 || result == nullptr) {
        throw std::runtime_error("wyoming: failed to resolve " + host + ":" + port_str + " (" +
                                  gai_strerror(rc) + ")");
    }

    int fd = -1;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(result);

    if (fd < 0) {
        throw std::runtime_error("wyoming: failed to connect to " + host + ":" + port_str);
    }

    fd_.store(fd);
}

void WyomingConnection::close() {
    int fd = fd_.exchange(-1);
    if (fd >= 0) {
        ::close(fd);
    }
}

void WyomingConnection::request_shutdown() {
    int fd = fd_.load();
    if (fd >= 0) {
        ::shutdown(fd, SHUT_RDWR);
    }
}

void WyomingConnection::write_all(const uint8_t* data, size_t len) {
    int fd = fd_.load();
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, data + sent, len - sent, 0);
        if (n <= 0) {
            throw std::runtime_error("wyoming: connection write failed");
        }
        sent += static_cast<size_t>(n);
    }
}

void WyomingConnection::read_exact(uint8_t* data, size_t len) {
    int fd = fd_.load();
    size_t got = 0;
    while (got < len) {
        ssize_t n = ::recv(fd, data + got, len - got, 0);
        if (n <= 0) {
            throw std::runtime_error("wyoming: connection closed while reading payload");
        }
        got += static_cast<size_t>(n);
    }
}

std::string WyomingConnection::read_line() {
    int fd = fd_.load();
    std::string line;
    char c;
    while (true) {
        ssize_t n = ::recv(fd, &c, 1, 0);
        if (n <= 0) {
            throw std::runtime_error("wyoming: connection closed while reading header");
        }
        if (c == '\n') break;
        line.push_back(c);
    }
    return line;
}

void WyomingConnection::write_event(const WyomingEvent& event) {
    // The wire format is: a JSON header line, then (if data_length is set)
    // that many bytes of a *separate* JSON document holding "data", then
    // (if payload_length is set) that many raw payload bytes. "data" is
    // never embedded inline in the header itself - see read_event().
    std::string data_json;
    nlohmann::json header;
    header["type"] = event.type;
    if (!event.data.empty()) {
        data_json = event.data.dump();
        header["data_length"] = data_json.size();
    }
    if (event.has_payload) {
        header["payload_length"] = event.payload.size();
    }

    std::string header_line = header.dump() + "\n";
    write_all(reinterpret_cast<const uint8_t*>(header_line.data()), header_line.size());

    if (!data_json.empty()) {
        write_all(reinterpret_cast<const uint8_t*>(data_json.data()), data_json.size());
    }
    if (event.has_payload && !event.payload.empty()) {
        write_all(event.payload.data(), event.payload.size());
    }
}

WyomingEvent WyomingConnection::read_event() {
    std::string line = read_line();
    nlohmann::json header = nlohmann::json::parse(line);

    WyomingEvent event;
    event.type = header.at("type").get<std::string>();

    if (header.contains("data_length") && !header["data_length"].is_null()) {
        size_t data_len = header["data_length"].get<size_t>();
        if (data_len > 0) {
            std::string data_json(data_len, '\0');
            read_exact(reinterpret_cast<uint8_t*>(data_json.data()), data_len);
            event.data = nlohmann::json::parse(data_json);
        }
    } else if (header.contains("data") && !header["data"].is_null()) {
        // Some servers embed "data" directly in the header instead of as a
        // separate length-prefixed block; accept that form too.
        event.data = header["data"];
    }

    if (header.contains("payload_length") && !header["payload_length"].is_null()) {
        size_t len = header["payload_length"].get<size_t>();
        event.payload.resize(len);
        if (len > 0) {
            read_exact(event.payload.data(), len);
        }
        event.has_payload = true;
    }

    return event;
}

}  // namespace assistant::util
