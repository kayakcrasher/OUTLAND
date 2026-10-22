#include "outland/net/LanProtocol.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace outland::net {

void Writer::f32(float v) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    u32(bits);
}
void Writer::str(const std::string& s) {
    const auto n = std::min<std::size_t>(s.size(), 255);
    u8(static_cast<std::uint8_t>(n));
    bytes.insert(bytes.end(), s.begin(), s.begin() + static_cast<std::ptrdiff_t>(n));
}

std::uint8_t Reader::u8() { return need(1) ? data_[at_++] : 0; }
std::uint16_t Reader::u16() {
    if (!need(2)) return 0;
    const auto v = static_cast<std::uint16_t>(data_[at_] | (data_[at_ + 1] << 8));
    at_ += 2;
    return v;
}
std::uint32_t Reader::u32() {
    if (!need(4)) return 0;
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(data_[at_ + static_cast<std::size_t>(i)]) << (8 * i);
    at_ += 4;
    return v;
}
std::uint64_t Reader::u64() {
    const std::uint64_t low = u32(), high = u32();
    return low | (high << 32);
}
float Reader::f32() {
    const std::uint32_t bits = u32();
    float v = 0;
    std::memcpy(&v, &bits, 4);
    if (!std::isfinite(v)) {ok_ = false; return 0;}
    return v;
}
std::string Reader::str() {
    const std::size_t n = u8();
    if (!need(n)) return {};
    std::string s(reinterpret_cast<const char*>(data_ + at_), n);
    at_ += n;
    return s;
}

std::vector<std::uint8_t> pack(const Header& header, const std::vector<std::uint8_t>& payload) {
    Writer w;
    w.u32(magic); w.u8(version); w.u8(static_cast<std::uint8_t>(header.type));
    w.u8(header.from); w.u8(header.to); w.u32(header.match);
    w.bytes.insert(w.bytes.end(), payload.begin(), payload.end());
    return std::move(w.bytes);
}

bool unpack(const std::vector<std::uint8_t>& datagram, Header& header, std::size_t& payload_at) {
    Reader r(datagram.data(), datagram.size());
    if (r.u32() != magic || r.u8() != version) return false;
    const auto type = r.u8();
    if (type < static_cast<std::uint8_t>(Msg::Beacon) || type > static_cast<std::uint8_t>(Msg::Grenade)) return false;
    header.type = static_cast<Msg>(type);
    header.from = r.u8(); header.to = r.u8(); header.match = r.u32();
    if (!r.ok()) return false;
    payload_at = 12;
    return true;
}

const char* sides_name(Sides sides) {
    switch (sides) {
        case Sides::Squad: return "ONE SQUAD";
        case Sides::Teams: return "TWO TEAMS";
        case Sides::Free: break;
    }
    return "EVERYONE FOR THEMSELVES";
}
const char* team_name(std::uint8_t team) { return team % 2 == 0 ? "TEAM A" : "TEAM B"; }

void write(Writer& w, const Beacon& m) { w.str(m.host); w.u8(m.mode); w.u8(m.level); w.u8(m.players); w.u16(m.port); w.u8(m.open); w.u8(m.sides); }
bool read(Reader& r, Beacon& m) {
    m.host = r.str(); m.mode = r.u8(); m.level = r.u8(); m.players = r.u8(); m.port = r.u16(); m.open = r.u8() != 0; m.sides = r.u8();
    return r.ok() && m.sides < sides_count;
}

void write(Writer& w, const Roster& m) {
    w.u8(m.mode); w.u8(m.level); w.u8(static_cast<std::uint8_t>(m.players.size()));
    for (const auto& p : m.players) {w.u8(p.slot); w.str(p.name);}
    w.u8(m.sides);
}
bool read(Reader& r, Roster& m) {
    m.mode = r.u8(); m.level = r.u8();
    const int n = r.u8();
    if (n > max_players) return false;
    m.players.resize(static_cast<std::size_t>(n));
    for (auto& p : m.players) {p.slot = r.u8(); p.name = r.str();}
    m.sides = r.u8();
    return r.ok() && m.sides < sides_count;
}

void write(Writer& w, const Start& m) { w.u8(m.mode); w.u8(m.level); w.u64(m.seed); w.u8(m.sides); }
bool read(Reader& r, Start& m) { m.mode = r.u8(); m.level = r.u8(); m.seed = r.u64(); m.sides = r.u8(); return r.ok() && m.sides < sides_count; }

void write(Writer& w, const PlayerState& m) {
    w.f32(m.time); w.v3(m.feet); w.f32(m.yaw); w.f32(m.pitch); w.v3(m.velocity); w.f32(m.health);
    w.u8(m.weapon); w.u8(m.flags); w.u8(m.dive); w.u16(static_cast<std::uint16_t>(m.vehicle));
    if (m.vehicle >= 0) {w.v3(m.car); w.f32(m.car_yaw); w.f32(m.car_speed);}
}
bool read(Reader& r, PlayerState& m) {
    m.time = r.f32(); m.feet = r.v3(); m.yaw = r.f32(); m.pitch = r.f32(); m.velocity = r.v3(); m.health = r.f32();
    m.weapon = r.u8(); m.flags = r.u8(); m.dive = r.u8(); m.vehicle = static_cast<std::int16_t>(r.u16());
    if (m.vehicle >= 0) {m.car = r.v3(); m.car_yaw = r.f32(); m.car_speed = r.f32();}
    return r.ok();
}

void write(Writer& w, const Hit& m) { w.i32(m.attacker); w.f32(m.amount); w.v3(m.from); w.u8(m.weapon); }
bool read(Reader& r, Hit& m) { m.attacker = r.i32(); m.amount = r.f32(); m.from = r.v3(); m.weapon = r.u8(); return r.ok() && m.amount >= 0 && m.amount < 1000; }

void write(Writer& w, const Died& m) { w.i32(m.killer); }
bool read(Reader& r, Died& m) { m.killer = r.i32(); return r.ok(); }

void write(Writer& w, const Taken& m) { w.str(m.pickup); w.u16(m.remaining); }
bool read(Reader& r, Taken& m) { m.pickup = r.str(); m.remaining = r.u16(); return r.ok(); }

void write(Writer& w, const Dropped& m) { w.str(m.pickup); w.str(m.item); w.u16(m.quantity); w.u16(m.loaded); w.v3(m.position); }
bool read(Reader& r, Dropped& m) { m.pickup = r.str(); m.item = r.str(); m.quantity = r.u16(); m.loaded = r.u16(); m.position = r.v3(); return r.ok(); }

void write(Writer& w, const BotSnapshot& m) {
    w.f32(m.elapsed);
    w.u16(static_cast<std::uint16_t>(m.bots.size()));
    for (const auto& b : m.bots) {w.u16(b.id); w.v3(b.feet); w.f32(b.yaw); w.u8(b.health); w.u8(b.flags);}
    w.u16(static_cast<std::uint16_t>(m.cars.size()));
    for (const auto& c : m.cars) {w.u16(c.index); w.v3(c.position); w.f32(c.yaw); w.f32(c.speed);}
}
bool read(Reader& r, BotSnapshot& m) {
    m.elapsed = r.f32();
    const int bots = r.u16();
    if (bots > 512) return false;
    m.bots.resize(static_cast<std::size_t>(bots));
    for (auto& b : m.bots) {b.id = r.u16(); b.feet = r.v3(); b.yaw = r.f32(); b.health = r.u8(); b.flags = r.u8();}
    const int cars = r.u16();
    if (cars > 512) return false;
    m.cars.resize(static_cast<std::size_t>(cars));
    for (auto& c : m.cars) {c.index = r.u16(); c.position = r.v3(); c.yaw = r.f32(); c.speed = r.f32();}
    return r.ok();
}

void write(Writer& w, const BotHit& m) { w.u16(m.bot); w.f32(m.amount); w.v3(m.from); }
bool read(Reader& r, BotHit& m) { m.bot = r.u16(); m.amount = r.f32(); m.from = r.v3(); return r.ok() && m.amount >= 0 && m.amount < 1000; }

void write(Writer& w, const KillCredit& m) { w.i32(m.bot); }
bool read(Reader& r, KillCredit& m) { m.bot = r.i32(); return r.ok(); }

void write(Writer& w, const Feed& m) { w.str(m.text); }
bool read(Reader& r, Feed& m) { m.text = r.str(); return r.ok(); }

void write(Writer& w, const Grenade& m) { w.v3(m.origin); w.v3(m.velocity); w.u8(m.rocket); }
bool read(Reader& r, Grenade& m) { m.origin = r.v3(); m.velocity = r.v3(); m.rocket = r.u8(); return r.ok(); }

}
