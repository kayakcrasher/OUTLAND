#pragma once
#include "outland/net/LanProtocol.hpp"
#include "outland/net/LanSocket.hpp"
#include <cstdint>
#include <optional>
#include <utility>
#include <string>
#include <vector>

// LAN play: one game hosts (slot 0) and announces itself on the local network; others browse,
// join, wait in the lobby and start together when the host does. In the match every message goes
// through the host, which relays it to whoever it is for. Desktop and Android; not the browser.
namespace outland::net {

struct HostInfo {
    std::string name;
    Address address;       // the host's game socket
    Beacon beacon;
    double heard{0};
};

struct Peer {
    std::uint8_t slot{0};
    std::string name;
    Address address;
    double heard{0};
};

struct Inbound {
    Header header;
    std::vector<std::uint8_t> payload;
};

class LanSession {
public:
    enum class Role { Off, Browsing, Host, Client };
    static constexpr double beacon_every = 1.0, ping_every = 1.0, timeout = 8.0, host_forget = 4.0;

    LanSession() = default;
    LanSession(const LanSession&) = delete;
    LanSession& operator=(const LanSession&) = delete;
    ~LanSession() { stop(); }

    static bool supported() { return LanSocket::supported(); }
    // Host a game, announced as `name` (the lobby's mode and bot level can change until it starts).
    bool host(const std::string& name, std::uint8_t mode, std::uint8_t level, double now);
    // Look for games on the network.
    bool browse();
    bool join(const HostInfo& host, const std::string& name, double now);
    // Tell the others and close.
    void stop();
    // Receive and route everything waiting; beacons, pings, timeouts. Call every frame.
    void poll(double now);

    // The host's lobby.
    void set_lobby(std::uint8_t mode, std::uint8_t level);
    // The host starts the match: everyone (the host too) gets it from take_start().
    void start_match(std::uint64_t seed);
    std::optional<Start> take_start() { auto s = start_; start_.reset(); return s; }

    // In play: a message to everyone (or one slot), relayed by the host.
    template <class T> void send(Msg type, const T& message, std::uint8_t to = everyone) { send_raw(type, encode(message), to); }
    void send_raw(Msg type, const std::vector<std::uint8_t>& payload, std::uint8_t to = everyone);
    std::vector<Inbound> take_inbox() { return std::exchange(inbox_, {}); }

    [[nodiscard]] Role role() const { return role_; }
    [[nodiscard]] bool active() const { return role_ == Role::Host || role_ == Role::Client; }
    [[nodiscard]] bool hosting() const { return role_ == Role::Host; }
    [[nodiscard]] bool in_match() const { return in_match_; }
    void end_match() { in_match_ = false; }
    [[nodiscard]] std::uint8_t slot() const { return slot_; }
    [[nodiscard]] std::uint32_t match() const { return match_; }
    [[nodiscard]] const std::vector<HostInfo>& hosts() const { return hosts_; }
    [[nodiscard]] const Roster& roster() const { return roster_; }
    [[nodiscard]] std::string name_of(std::uint8_t slot) const;
    [[nodiscard]] const std::string& status() const { return status_; }
    [[nodiscard]] std::uint16_t port() const { return socket_.port(); }
    // For tests on one machine: also announce to the loopback address.
    bool announce_loopback{true};

private:
    void send_to(const Address& to, Msg type, const std::vector<std::uint8_t>& payload, std::uint8_t from, std::uint8_t target);
    void handle(const Header& header, const std::vector<std::uint8_t>& payload, std::size_t at, const Address& from, double now);
    void broadcast_roster();
    Peer* peer(std::uint8_t slot);

    Role role_{Role::Off};
    LanSocket socket_, discovery_;
    std::uint8_t slot_{0};
    std::uint32_t match_{0};
    std::string name_;
    Roster roster_;
    std::vector<Peer> peers_;          // the host's clients
    Address host_address_;
    double host_heard_{0}, next_beacon_{0}, next_ping_{0};
    std::vector<HostInfo> hosts_;
    std::optional<Start> start_;
    bool in_match_{false};
    bool joining_{false};
    double next_join_{0};
    std::vector<Inbound> inbox_;
    std::string status_;
};

}
