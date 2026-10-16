#pragma once
#include <cstdint>
#include <string>
#include <vector>

// A non-blocking UDP socket for LAN play (desktop and Android; browsers have no UDP, so on the web
// build every call fails and `supported()` is false).
namespace outland::net {

struct Address {
    std::uint32_t ip{0};     // host byte order
    std::uint16_t port{0};
    bool operator==(const Address&) const = default;
    [[nodiscard]] std::string text() const;
    static Address loopback(std::uint16_t port) { return {0x7F000001U, port}; }
    static Address broadcast(std::uint16_t port) { return {0xFFFFFFFFU, port}; }
};

class LanSocket {
public:
    LanSocket() = default;
    LanSocket(const LanSocket&) = delete;
    LanSocket& operator=(const LanSocket&) = delete;
    LanSocket(LanSocket&& other) noexcept : fd_(other.fd_), port_(other.port_) { other.fd_ = -1; }
    LanSocket& operator=(LanSocket&& other) noexcept;
    ~LanSocket() { close(); }

    static bool supported();
    // Bind to `port` on every interface (0: any free port). `shared` lets several sockets on one
    // machine listen to the same port (the discovery port: each browsing game hears every beacon).
    bool open(std::uint16_t port, bool shared = false);
    void close();
    [[nodiscard]] bool is_open() const { return fd_ >= 0; }
    [[nodiscard]] std::uint16_t port() const { return port_; }

    bool send(const Address& to, const std::vector<std::uint8_t>& bytes) const;
    // One datagram if one is waiting (false when none is).
    bool receive(std::vector<std::uint8_t>& bytes, Address& from) const;

private:
    int fd_{-1};
    std::uint16_t port_{0};
};

}
