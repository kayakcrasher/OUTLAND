#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

// The LAN game's messages: a small binary format (little-endian) over UDP. Every datagram starts
// with a header; the host (slot 0) relays everything clients send to whoever it is for.
namespace outland::net {

inline constexpr std::uint32_t magic = 0x4E414C4FU;   // "OLAN"
inline constexpr std::uint8_t version = 1;
inline constexpr std::uint16_t discovery_port = 47821;
inline constexpr int max_players = 8;
inline constexpr std::uint8_t everyone = 255;

enum class Msg : std::uint8_t {
    Beacon = 1, Join, Welcome, Reject, Roster, Leave, Start, Ping,
    State, Hit, Died, Taken, Dropped, BotSnapshot, BotHit, KillCredit, Feed, Grenade,
};

struct Header {
    Msg type{};
    std::uint8_t from{0};        // the sender's slot (0: the host)
    std::uint8_t to{everyone};   // a slot, or everyone
    std::uint32_t match{0};      // the host's game; strays from another game are dropped
};

class Writer {
public:
    void u8(std::uint8_t v) { bytes.push_back(v); }
    void u16(std::uint16_t v) { for (int i = 0; i < 2; ++i) bytes.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
    void u32(std::uint32_t v) { for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
    void u64(std::uint64_t v) { for (int i = 0; i < 8; ++i) bytes.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
    void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
    void f32(float v);
    void v3(Vector3 v) { f32(v.x); f32(v.y); f32(v.z); }
    void str(const std::string& s);   // up to 255 bytes
    std::vector<std::uint8_t> bytes;
};

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
    std::uint8_t u8();
    std::uint16_t u16();
    std::uint32_t u32();
    std::uint64_t u64();
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    float f32();
    Vector3 v3() { const float x = f32(), y = f32(), z = f32(); return {x, y, z}; }
    std::string str();
    [[nodiscard]] bool ok() const { return ok_; }
    [[nodiscard]] bool done() const { return at_ == size_; }
private:
    bool need(std::size_t n) { if (at_ + n > size_) ok_ = false; return ok_; }
    const std::uint8_t* data_;
    std::size_t size_, at_{0};
    bool ok_{true};
};

std::vector<std::uint8_t> pack(const Header& header, const std::vector<std::uint8_t>& payload);
// The header, and a reader over the payload; false for anything not ours.
bool unpack(const std::vector<std::uint8_t>& datagram, Header& header, std::size_t& payload_at);

// ---------------------------------------------------------------- lobby
struct Beacon { std::string host; std::uint8_t mode{0}, level{1}, players{1}; std::uint16_t port{0}; bool open{true}; };
struct RosterEntry { std::uint8_t slot{0}; std::string name; };
struct Roster { std::uint8_t mode{0}, level{1}; std::vector<RosterEntry> players; };
struct Start { std::uint8_t mode{0}, level{1}; std::uint64_t seed{0}; };

// ---------------------------------------------------------------- play
// A player as everyone else sees them, sent ~20 times a second.
struct PlayerState {
    float time{0};
    Vector3 feet{};
    float yaw{0}, pitch{0};       // radians, the player's look
    Vector3 velocity{};
    float health{0};
    std::uint8_t weapon{0};
    std::uint8_t flags{0};        // see State* below
    std::uint8_t dive{0};         // aircraft::DiveStage while dropping
    std::int16_t vehicle{-1};     // index in VehicleSystem::vehicles() when driving
    Vector3 car{};
    float car_yaw{0}, car_speed{0};
};
inline constexpr std::uint8_t StateAlive = 1, StateFiring = 2, StateAiming = 4, StateCrouch = 8, StateDriving = 16,
                               StateGrounded = 32, StateCanopy = 64, StateDropping = 128;
struct Hit { std::int32_t attacker{0}; float amount{0}; Vector3 from{}; std::uint8_t weapon{0}; }; // attacker: slot, or -bot id
struct Died { std::int32_t killer{-1}; };        // the sender died; killer as in Hit (or -1: the zone, a fall...)
struct Taken { std::string pickup; std::uint16_t remaining{0}; };
struct Dropped { std::string pickup, item; std::uint16_t quantity{1}, loaded{0}; Vector3 position{}; };
struct BotState { std::uint16_t id{0}; Vector3 feet{}; float yaw{0}; std::uint8_t health{0}, flags{0}; };
inline constexpr std::uint8_t BotMoving = 1, BotMounted = 2, BotRunning = 4, BotFiring = 8;
struct CarState { std::uint16_t index{0}; Vector3 position{}; float yaw{0}, speed{0}; };
struct BotSnapshot { float elapsed{0}; std::vector<BotState> bots; std::vector<CarState> cars; };
struct BotHit { std::uint16_t bot{0}; float amount{0}; Vector3 from{}; };
struct KillCredit { std::int32_t bot{0}; };
struct Feed { std::string text; };
struct Grenade { Vector3 origin{}, velocity{}; };

void write(Writer&, const Beacon&);      bool read(Reader&, Beacon&);
void write(Writer&, const Roster&);      bool read(Reader&, Roster&);
void write(Writer&, const Start&);       bool read(Reader&, Start&);
void write(Writer&, const PlayerState&); bool read(Reader&, PlayerState&);
void write(Writer&, const Hit&);         bool read(Reader&, Hit&);
void write(Writer&, const Died&);        bool read(Reader&, Died&);
void write(Writer&, const Taken&);       bool read(Reader&, Taken&);
void write(Writer&, const Dropped&);     bool read(Reader&, Dropped&);
void write(Writer&, const BotSnapshot&); bool read(Reader&, BotSnapshot&);
void write(Writer&, const BotHit&);      bool read(Reader&, BotHit&);
void write(Writer&, const KillCredit&);  bool read(Reader&, KillCredit&);
void write(Writer&, const Feed&);        bool read(Reader&, Feed&);
void write(Writer&, const Grenade&);     bool read(Reader&, Grenade&);

template <class T> std::vector<std::uint8_t> encode(const T& message) {
    Writer w;
    write(w, message);
    return std::move(w.bytes);
}
template <class T> bool decode(const std::vector<std::uint8_t>& payload, T& message) {
    Reader r(payload.data(), payload.size());
    return read(r, message) && r.ok() && r.done();
}

}
