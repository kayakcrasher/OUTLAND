// LAN play: the message format round-trips, and a host and two clients on this machine find each
// other, gather in a lobby, start together and pass play messages through the host.
#include "outland/net/LanSession.hpp"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <thread>
using namespace outland::net;
namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
double now() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return duration<double>(steady_clock::now() - start).count();
}
// Poll everyone until `done` (or give up after `seconds`).
bool settle(std::initializer_list<LanSession*> sessions, const std::function<bool()>& done, double seconds = 4) {
    const double until = now() + seconds;
    while (now() < until) {
        for (auto* s : sessions) s->poll(now());
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}
template <class T> bool inbox_has(std::vector<Inbound>& inbox, Msg type, T& out, std::uint8_t* from = nullptr) {
    for (const auto& m : inbox)
        if (m.header.type == type && decode(m.payload, out)) {if (from) *from = m.header.from; return true;}
    return false;
}
}

int main() {
    // The format.
    {
        PlayerState state{12.5F, {1, 2, 3}, 1.25F, -.2F, {4, 0, 5}, 97, 2, StateAlive | StateDriving, 0, 7, {10, 1, 20}, 90, 12};
        PlayerState back;
        check(decode(encode(state), back) && back.vehicle == 7 && back.car.z == 20 && back.health == 97 && back.flags == (StateAlive | StateDriving),
              "a player's state round-trips");
        state.vehicle = -1;
        check(decode(encode(state), back) && back.vehicle == -1 && encode(state).size() < encode(PlayerState{.vehicle = 3}).size(), "no car, fewer bytes");
        BotSnapshot snapshot{3.5F, {}, {}};
        for (std::uint16_t i = 1; i <= 99; ++i) snapshot.bots.push_back({i, {float(i), 0, -float(i)}, 45, 100, BotMoving});
        snapshot.cars.push_back({4, {1, 2, 3}, 180, 9});
        BotSnapshot read_back;
        const auto bytes = encode(snapshot);
        check(decode(bytes, read_back) && read_back.bots.size() == 99 && read_back.bots[98].feet.x == 99 && read_back.cars[0].index == 4,
              "ninety-nine bots and a car round-trip (" + std::to_string(bytes.size()) + " bytes)");
        check(bytes.size() < 2400, "a bot snapshot fits a couple of packets");
        Hit hit{-5, 22.5F, {1, 1, 1}, 1};
        Hit hit_back;
        check(decode(encode(hit), hit_back) && hit_back.attacker == -5 && hit_back.amount == 22.5F, "a hit round-trips");
        auto truncated = encode(hit);
        truncated.pop_back();
        check(!decode(truncated, hit_back), "a short message is refused");
        Header header{Msg::Taken, 3, everyone, 77};
        const auto datagram = pack(header, encode(Taken{"5:village/2", 0}));
        Header got;
        std::size_t at = 0;
        check(unpack(datagram, got, at) && got.type == Msg::Taken && got.from == 3 && got.match == 77, "the header round-trips");
        auto stray = datagram;
        stray[0] ^= 1;
        check(!unpack(stray, got, at), "someone else's packets are ignored");
    }
    if (!LanSession::supported()) {std::cout << "[SKIP] no sockets here\n"; return failures ? 1 : 0;}

    // A host, one client that browses for it and one that joins straight away.
    LanSession host, bee, cee;
    check(host.host("HOST", 1, 1, now()), "host a game");
    check(bee.browse(), "browse for games");
    if (!settle({&host, &bee}, [&] { return !bee.hosts().empty(); })) {
        std::cout << "[SKIP] no loopback broadcast here: " << bee.status() << '\n';
        return failures ? 1 : 0;
    }
    const auto info = bee.hosts().front();
    check(info.name == "HOST" && info.beacon.mode == 1 && info.beacon.open, "the beacon names the game");
    settle({&host, &bee}, [] { return false; }, 2.2);
    check(bee.hosts().size() == 1, "one game listed once (heard by broadcast and loopback)");
    check(bee.join(info, "BEE", now()), "join");
    check(cee.join(info, "CEE", now()), "join without browsing");
    check(settle({&host, &bee, &cee}, [&] { return bee.role() == LanSession::Role::Client && cee.role() == LanSession::Role::Client &&
                                                   host.roster().players.size() == 3 && bee.roster().players.size() == 3 && cee.roster().players.size() == 3; }),
          "everyone in the lobby");
    check(bee.slot() != cee.slot() && bee.slot() > 0 && cee.slot() > 0, "each client has its own slot");
    check(cee.name_of(bee.slot()) == "BEE" && bee.name_of(0) == "HOST", "names in the roster");
    host.set_lobby(3, 2);
    check(settle({&host, &bee, &cee}, [&] { return bee.roster().mode == 3 && cee.roster().level == 2; }), "the host's choice reaches the lobby");

    host.start_match(424242);
    const auto host_start = host.take_start();
    std::optional<Start> bee_start, cee_start;
    check(settle({&host, &bee, &cee}, [&] {
        if (!bee_start) bee_start = bee.take_start();
        if (!cee_start) cee_start = cee.take_start();
        return bee_start && cee_start; }), "the start reaches everyone");
    check(host_start && bee_start && cee_start && host_start->seed == 424242 && bee_start->seed == 424242 && cee_start->mode == 3,
          "one seed and one mode for all");
    check(host.in_match() && bee.in_match() && cee.in_match(), "everyone in the match");

    // Play: a state to everyone goes through the host to the other client; a hit only to its target.
    bee.send(Msg::State, PlayerState{1, {5, 0, 5}, 0, 0, {}, 125, 0, StateAlive});
    cee.send(Msg::Hit, Hit{static_cast<std::int32_t>(cee.slot()), 30, {0, 1, 0}, 2}, bee.slot());
    std::vector<Inbound> at_host, at_bee, at_cee;
    settle({&host, &bee, &cee}, [&] {
        for (auto& m : host.take_inbox()) at_host.push_back(std::move(m));
        for (auto& m : bee.take_inbox()) at_bee.push_back(std::move(m));
        for (auto& m : cee.take_inbox()) at_cee.push_back(std::move(m));
        PlayerState s; Hit h;
        return inbox_has(at_cee, Msg::State, s) && inbox_has(at_bee, Msg::Hit, h); }, 2);
    PlayerState seen;
    std::uint8_t from = 255;
    check(inbox_has(at_host, Msg::State, seen, &from) && from == bee.slot() && seen.feet.x == 5, "the host sees the client");
    check(inbox_has(at_cee, Msg::State, seen, &from) && from == bee.slot(), "the other client sees it, relayed");
    Hit taken;
    check(inbox_has(at_bee, Msg::Hit, taken) && taken.amount == 30, "the hit reaches its target");
    check(!inbox_has(at_host, Msg::Hit, taken) && !inbox_has(at_cee, Msg::Hit, taken), "...and no one else");

    // Leaving.
    const auto cee_slot = cee.slot();
    cee.stop();
    std::vector<Inbound> left;
    check(settle({&host, &bee}, [&] {
        for (auto& m : host.take_inbox()) left.push_back(std::move(m));
        return bee.roster().players.size() == 2 && std::any_of(left.begin(), left.end(), [&](const Inbound& m) {
            return m.header.type == Msg::Leave && m.header.from == cee_slot; }); }), "a client leaves");
    host.stop();
    check(settle({&bee}, [&] { return bee.role() != LanSession::Role::Client; }), "the host leaves, and the client knows");
    if (failures) return 1;
    std::cout << "[PASS] LAN play\n";
    return 0;
}
