#include "outland/net/LanSession.hpp"
#include <algorithm>
#include <random>

namespace outland::net {

std::string LanSession::name_of(std::uint8_t slot) const {
    for (const auto& p : roster_.players) if (p.slot == slot) return p.name;
    return "PLAYER " + std::to_string(slot + 1);
}

Peer* LanSession::peer(std::uint8_t slot) {
    for (auto& p : peers_) if (p.slot == slot) return &p;
    return nullptr;
}

bool LanSession::host(const std::string& name, std::uint8_t mode, std::uint8_t level, double now) {
    stop();
    if (!socket_.open(0)) {status_ = "Could not open a network socket"; return false;}
    role_ = Role::Host;
    slot_ = 0;
    name_ = name;
    std::random_device device;
    match_ = (static_cast<std::uint32_t>(device()) << 1) | 1U;
    roster_ = {mode, level, {{0, name}}};
    next_beacon_ = now;
    status_ = "Hosting - waiting for players";
    return true;
}

bool LanSession::browse() {
    stop();
    if (!discovery_.open(discovery_port, true)) {status_ = "Could not listen for games on this network"; return false;}
    role_ = Role::Browsing;
    status_ = "Looking for games on this network...";
    return true;
}

bool LanSession::join(const HostInfo& host, const std::string& name, double now) {
    if (!socket_.is_open() && !socket_.open(0)) {status_ = "Could not open a network socket"; return false;}
    host_address_ = host.address;
    match_ = 0;
    name_ = name;
    host_heard_ = now;
    Writer w;
    w.str(name);
    send_to(host_address_, Msg::Join, w.bytes, 0, 0);
    joining_ = true;
    next_join_ = now + 1;
    status_ = "Joining " + host.name + "...";
    return true;
}

void LanSession::stop() {
    if (role_ == Role::Host)
        for (const auto& p : peers_) send_to(p.address, Msg::Leave, {}, slot_, p.slot);
    if (role_ == Role::Client) send_to(host_address_, Msg::Leave, {}, slot_, 0);
    socket_.close();
    discovery_.close();
    role_ = Role::Off;
    peers_.clear(); hosts_.clear(); inbox_.clear();
    roster_ = {};
    start_.reset();
    in_match_ = false;
    joining_ = false;
    slot_ = 0;
    match_ = 0;
}

void LanSession::set_lobby(std::uint8_t mode, std::uint8_t level) {
    if (role_ != Role::Host || in_match_) return;
    roster_.mode = mode;
    roster_.level = level;
    broadcast_roster();
}

void LanSession::start_match(std::uint64_t seed) {
    if (role_ != Role::Host) return;
    const Start start{roster_.mode, roster_.level, seed};
    for (const auto& p : peers_) send_to(p.address, Msg::Start, encode(start), 0, p.slot);
    start_ = start;
    in_match_ = true;
}

void LanSession::send_to(const Address& to, Msg type, const std::vector<std::uint8_t>& payload, std::uint8_t from, std::uint8_t target) {
    socket_.send(to, pack({type, from, target, match_}, payload));
}

void LanSession::send_raw(Msg type, const std::vector<std::uint8_t>& payload, std::uint8_t to) {
    if (role_ == Role::Host) {
        for (const auto& p : peers_) if (to == everyone || to == p.slot) send_to(p.address, type, payload, 0, to);
    } else if (role_ == Role::Client && match_ != 0) {
        send_to(host_address_, type, payload, slot_, to);
    }
}

void LanSession::broadcast_roster() {
    const auto payload = encode(roster_);
    for (const auto& p : peers_) send_to(p.address, Msg::Roster, payload, 0, p.slot);
}

void LanSession::poll(double now) {
    std::vector<std::uint8_t> datagram;
    Address from;
    // Beacons from hosts, while browsing.
    while (discovery_.receive(datagram, from)) {
        Header header;
        std::size_t at = 0;
        if (!unpack(datagram, header, at) || header.type != Msg::Beacon) continue;
        Beacon beacon;
        Reader r(datagram.data() + at, datagram.size() - at);
        if (!read(r, beacon)) continue;
        const Address game{from.ip, beacon.port};
        // A game on this machine is heard twice (broadcast and loopback): one entry, the same port and name.
        const auto local = [](const Address& a) { return (a.ip >> 24) == 127; };
        auto found = std::find_if(hosts_.begin(), hosts_.end(), [&](const HostInfo& h) {
            return h.address == game || (h.address.port == game.port && h.name == beacon.host && (local(h.address) || local(game)));
        });
        if (found == hosts_.end()) {hosts_.push_back({beacon.host, game, beacon, now}); continue;}
        found->beacon = beacon; found->name = beacon.host; found->heard = now;
    }
    std::erase_if(hosts_, [&](const HostInfo& h) { return now - h.heard > host_forget; });
    while (socket_.receive(datagram, from)) {
        Header header;
        std::size_t at = 0;
        if (!unpack(datagram, header, at)) continue;
        handle(header, datagram, at, from, now);
    }
    if (role_ == Role::Host) {
        if (now >= next_beacon_) {
            next_beacon_ = now + beacon_every;
            const Beacon beacon{name_, roster_.mode, roster_.level, static_cast<std::uint8_t>(peers_.size() + 1), socket_.port(),
                                !in_match_ && peers_.size() + 1 < max_players};
            const auto bytes = pack({Msg::Beacon, 0, everyone, match_}, encode(beacon));
            socket_.send(Address::broadcast(discovery_port), bytes);
            if (announce_loopback) socket_.send(Address::loopback(discovery_port), bytes);
        }
        if (now >= next_ping_) {
            next_ping_ = now + ping_every;
            for (const auto& p : peers_) send_to(p.address, Msg::Ping, {}, 0, p.slot);
        }
        const auto before = peers_.size();
        std::vector<std::uint8_t> gone;
        for (const auto& p : peers_) if (now - p.heard > timeout) gone.push_back(p.slot);
        std::erase_if(peers_, [&](const Peer& p) { return now - p.heard > timeout; });
        if (peers_.size() != before) {
            std::erase_if(roster_.players, [&](const RosterEntry& e) { return std::find(gone.begin(), gone.end(), e.slot) != gone.end(); });
            for (const auto slot : gone) inbox_.push_back({{Msg::Leave, slot, everyone, match_}, {}});
            broadcast_roster();
        }
    } else if (joining_ && role_ != Role::Client && now >= next_join_) {
        // The join (or its welcome) was lost: ask again, for a while.
        next_join_ = now + 1;
        if (now - host_heard_ > timeout) {joining_ = false; status_ = "The host did not answer";}
        else {Writer w; w.str(name_); send_to(host_address_, Msg::Join, w.bytes, 0, 0);}
    } else if (role_ == Role::Client) {
        if (now >= next_ping_) {
            next_ping_ = now + ping_every;
            send_to(host_address_, Msg::Ping, {}, slot_, 0);
        }
        if (now - host_heard_ > timeout) {
            status_ = "Lost the host";
            const bool was = in_match_;
            socket_.close();
            role_ = discovery_.is_open() ? Role::Browsing : Role::Off;
            match_ = 0;
            roster_ = {};
            if (was) inbox_.push_back({{Msg::Leave, 0, everyone, 0}, {}});
            in_match_ = false;
        }
    }
}

void LanSession::handle(const Header& header, const std::vector<std::uint8_t>& datagram, std::size_t at, const Address& from, double now) {
    const std::vector<std::uint8_t> payload(datagram.begin() + static_cast<std::ptrdiff_t>(at), datagram.end());
    if (role_ == Role::Host) {
        if (header.type == Msg::Join) {
            Reader r(payload.data(), payload.size());
            std::string name = r.str();
            if (name.empty()) name = "PLAYER";
            Peer* known = nullptr;
            for (auto& p : peers_) if (p.address == from) known = &p;
            if (!known) {
                if (in_match_ || peers_.size() + 1 >= static_cast<std::size_t>(max_players)) {
                    Writer w; w.str(in_match_ ? "The match has started" : "The game is full");
                    socket_.send(from, pack({Msg::Reject, 0, everyone, match_}, w.bytes));
                    return;
                }
                std::uint8_t slot = 1;
                while (peer(slot)) ++slot;
                peers_.push_back({slot, name, from, now});
                roster_.players.push_back({slot, name});
                known = &peers_.back();
            }
            known->heard = now;
            Writer w; w.u8(known->slot);
            send_to(from, Msg::Welcome, w.bytes, 0, known->slot);
            broadcast_roster();
            return;
        }
        if (header.match != match_) return;
        Peer* sender = peer(header.from);
        if (!sender || !(sender->address == from)) return;
        sender->heard = now;
        switch (header.type) {
        case Msg::Ping: return;
        case Msg::Leave: {
            const auto slot = sender->slot;
            std::erase_if(peers_, [&](const Peer& p) { return p.slot == slot; });
            std::erase_if(roster_.players, [&](const RosterEntry& e) { return e.slot == slot; });
            inbox_.push_back({{Msg::Leave, slot, everyone, match_}, {}});
            broadcast_roster();
            return;
        }
        default: break;
        }
        // Play messages: for the host, and relayed to the others they are for.
        if (header.to == everyone || header.to == 0) inbox_.push_back({header, payload});
        for (const auto& p : peers_)
            if (p.slot != header.from && (header.to == everyone || header.to == p.slot)) send_to(p.address, header.type, payload, header.from, header.to);
        return;
    }
    // A client (or one still browsing, mid-join).
    if (!(from == host_address_)) return;
    host_heard_ = now;
    switch (header.type) {
    case Msg::Welcome: {
        Reader r(payload.data(), payload.size());
        slot_ = r.u8();
        match_ = header.match;
        role_ = Role::Client;
        joining_ = false;
        status_ = "In the lobby - waiting for the host to start";
        return;
    }
    case Msg::Reject: {
        Reader r(payload.data(), payload.size());
        status_ = "Could not join: " + r.str();
        joining_ = false;
        return;
    }
    default: break;
    }
    if (header.match != match_ || match_ == 0) return;
    switch (header.type) {
    case Msg::Roster: {
        Roster roster;
        if (decode(payload, roster)) roster_ = std::move(roster);
        return;
    }
    case Msg::Start: {
        Start start;
        if (decode(payload, start)) {start_ = start; in_match_ = true;}
        return;
    }
    case Msg::Leave:
        if (header.from == 0) {
            status_ = "The host left";
            const bool was = in_match_;
            socket_.close();
            role_ = discovery_.is_open() ? Role::Browsing : Role::Off;
            match_ = 0;
            roster_ = {};
            in_match_ = false;
            if (was) inbox_.push_back({header, {}});
        }
        return;
    case Msg::Ping: return;
    default:
        inbox_.push_back({header, payload});
    }
}

}
