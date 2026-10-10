#pragma once
#include <raylib.h>
#include <cstdint>
#include <vector>

// One island-wide record of what can be heard. Anything that makes noise (guns, running feet,
// engines, horns, impacts) emits an event; anything that listens (combat bots, residents, NPCs,
// later zombies and police) reads the events it has not seen yet and decides for itself whether
// it was in earshot. Nobody learns a position they could not have heard.
namespace outland::game::sound {

enum class SoundKind : std::uint8_t { Gunshot, Footstep, Engine, Horn, Impact, Shout };
const char* sound_kind_name(SoundKind kind);

// Sources: 0 is the player, 1.. are combat bots, negative values are anonymous (world, NPCs).
struct SoundEvent {
    SoundKind kind{SoundKind::Impact};
    Vector3 position{};
    float radius{0};     // metres at which a normal listener can still just hear it
    int source{-1};
    double time{0};
    std::uint64_t serial{0};
};

// Typical audible ranges, metres.
namespace radius {
inline constexpr float rifle = 300, pistol = 180, sprint = 14, run = 9, engine = 40, horn = 120, impact = 25, shout = 45;
}

class SoundBus {
public:
    // Events older than this are dropped; listeners poll at least this often.
    static constexpr double lifetime = 1.0;

    std::uint64_t emit(SoundKind kind, Vector3 position, float radius, int source, double time);
    void advance(double now);   // expire old events
    void clear();

    // Events newer than `after` (a serial from a previous call, 0 for everything retained).
    [[nodiscard]] const std::vector<SoundEvent>& events() const { return events_; }
    [[nodiscard]] std::uint64_t latest() const { return serial_; }
    // True if a listener at `listener` with `hearing` (1 = normal) hears `event`.
    [[nodiscard]] static bool audible(const SoundEvent& event, Vector3 listener, float hearing = 1);
    // How loud it arrives: 1 at the source, 0 at the edge of its radius.
    [[nodiscard]] static float loudness(const SoundEvent& event, Vector3 listener, float hearing = 1);

    // Call `f(event)` for each event newer than `after` that the listener can hear.
    template <class F>
    void heard(Vector3 listener, float hearing, std::uint64_t after, F&& f) const {
        for (const auto& event : events_)
            if (event.serial > after && audible(event, listener, hearing)) f(event);
    }

private:
    std::vector<SoundEvent> events_;
    std::uint64_t serial_{0};
};
}
