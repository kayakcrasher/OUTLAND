#pragma once

#include <raylib.h>
#include <array>
#include <string>
#include "outland/world/VerdaRegion.hpp"
#include "outland/game/combat/WeaponSystem.hpp"

namespace outland::engine::audio {

class EnvironmentAudio {
public:
    explicit EnvironmentAudio(const std::string& asset_directory);
    ~EnvironmentAudio();
    EnvironmentAudio(const EnvironmentAudio&) = delete;
    EnvironmentAudio& operator=(const EnvironmentAudio&) = delete;

    void update(Vector3 position, bool grounded, bool active, const world::VerdaRegion& region);
    void set_volume(float volume);
    void toggle_mute();
    void play_combat(game::combat::WeaponId weapon, const game::combat::WeaponEvents& events);
    [[nodiscard]] bool muted() const { return muted_; }
    [[nodiscard]] bool ready() const { return ready_; }
    [[nodiscard]] float volume() const { return volume_; }

private:
    Music wind_{};
    std::array<std::array<Sound, 3>, 3> steps_{};
    std::array<std::array<Sound, 3>, 2> gunshots_{};
    Sound reload_{}, hit_{}, dry_{};
    unsigned int gunshot_index_{0};
    bool owns_device_{false};
    bool ready_{false};
    bool muted_{false};
    bool have_position_{false};
    bool was_grounded_{false};
    Vector3 previous_{};
    float stride_distance_{0.0F};
    float volume_{0.65F};
    unsigned int step_number_{0};
};

} // namespace outland::engine::audio
