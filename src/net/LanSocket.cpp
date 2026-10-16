#include "outland/net/LanSocket.hpp"

#if !defined(__EMSCRIPTEN__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace outland::net {

std::string Address::text() const {
    return std::to_string((ip >> 24) & 255) + "." + std::to_string((ip >> 16) & 255) + "." +
           std::to_string((ip >> 8) & 255) + "." + std::to_string(ip & 255) + ":" + std::to_string(port);
}

LanSocket& LanSocket::operator=(LanSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_; port_ = other.port_;
        other.fd_ = -1;
    }
    return *this;
}

#if defined(__EMSCRIPTEN__)
bool LanSocket::supported() { return false; }
bool LanSocket::open(std::uint16_t, bool) { return false; }
void LanSocket::close() { fd_ = -1; }
bool LanSocket::send(const Address&, const std::vector<std::uint8_t>&) const { return false; }
bool LanSocket::receive(std::vector<std::uint8_t>&, Address&) const { return false; }
#else
bool LanSocket::supported() { return true; }

bool LanSocket::open(std::uint16_t port, bool shared) {
    close();
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;
    const int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof on);
    if (shared) {
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
#ifdef SO_REUSEPORT
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof on);
#endif
    }
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof local) != 0) {::close(fd); return false;}
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    socklen_t length = sizeof local;
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&local), &length);
    fd_ = fd;
    port_ = ntohs(local.sin_port);
    return true;
}

void LanSocket::close() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    port_ = 0;
}

bool LanSocket::send(const Address& to, const std::vector<std::uint8_t>& bytes) const {
    if (fd_ < 0 || bytes.empty()) return false;
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(to.ip);
    remote.sin_port = htons(to.port);
    return ::sendto(fd_, bytes.data(), bytes.size(), 0, reinterpret_cast<sockaddr*>(&remote), sizeof remote) ==
           static_cast<ssize_t>(bytes.size());
}

bool LanSocket::receive(std::vector<std::uint8_t>& bytes, Address& from) const {
    if (fd_ < 0) return false;
    bytes.resize(65536);
    sockaddr_in remote{};
    socklen_t length = sizeof remote;
    const auto got = ::recvfrom(fd_, bytes.data(), bytes.size(), 0, reinterpret_cast<sockaddr*>(&remote), &length);
    if (got <= 0) {bytes.clear(); return false;}
    bytes.resize(static_cast<std::size_t>(got));
    from = {ntohl(remote.sin_addr.s_addr), ntohs(remote.sin_port)};
    return true;
}
#endif

}
