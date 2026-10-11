#pragma once
#include "outland/game/combat/WeaponDefinition.hpp"
#include <raylib.h>
#include <array>
#include <string>
#include <vector>

namespace outland::engine::audio {

// How a shot (or blast) somewhere in the world reaches the listener: louder and brighter close
// in, muffled (the distant mix) past `distant_from`, panned toward its side of the head, and
// late by the speed of sound. Pure, so it is tested without an audio device.
struct ShotMix {
    float volume{0};   // 0..1 before the master volume
    float pan{0};      // -1 left .. 1 right (raylib 6)
    float delay{0};    // seconds
    bool distant{false};
    bool audible{false};
};
ShotMix mix_shot(Vector3 source, Vector3 listener, Vector3 forward, float hearing_radius, float loudness = 1);

// Synthesized gun audio (tools/generate_weapon_audio.py): the player's own reports, other
// shooters' reports placed in the world, grenade blasts, pump and bolt actions, shell casings
// and the kill confirmation. Many voices per sample, so automatic fire and its tails overlap.
class GunAudio {
public:
    static constexpr float speed_of_sound = 343;
    explicit GunAudio(const std::string& directory);
    ~GunAudio();
    GunAudio(const GunAudio&) = delete;
    GunAudio& operator=(const GunAudio&) = delete;

    void set_volume(float volume, bool muted);
    void player_shot(game::combat::WeaponId weapon);
    void world_shot(game::combat::WeaponId weapon, Vector3 source, Vector3 listener, Vector3 forward);
    void explosion(Vector3 at, Vector3 listener, Vector3 forward);
    void kill_confirm();
    void update(float dt); // plays what has arrived (sound travels; actions follow the shot)
    [[nodiscard]] bool ready() const { return ready_; }

private:
    static constexpr int voice_count = 6;
    struct Bank { std::vector<Sound> base; std::vector<std::array<Sound, voice_count>> voices; unsigned next{0}; };
    struct Pending { Bank* bank; float delay, volume, pan, pitch; };
    void load(Bank& bank, const std::string& path);
    void play(Bank& bank, float volume, float pan, float pitch);
    void unload(Bank& bank);
    float random();

    std::array<Bank, game::combat::weapon_count> close_{}, distant_{};
    Bank explosion_{}, explosion_distant_{}, pump_{}, bolt_{}, casing_{}, kill_{};
    std::vector<Pending> pending_;
    float volume_{.65F};
    bool muted_{false}, ready_{false};
    unsigned random_{0x2545f491U};
};

} // namespace outland::engine::audio
