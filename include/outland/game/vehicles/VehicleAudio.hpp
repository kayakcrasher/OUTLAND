#pragma once
#include "outland/game/vehicles/EngineSynth.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include <array>
#include <map>
#include <mutex>
#include <string>
#include <vector>
namespace outland::game::vehicles {
// The vehicles' sound: the engine (game::vehicles::EngineSynth - real recordings looped to the
// revs, synthesised effects on top - for the player's vehicle and the nearest other one on the
// move), and the recorded starter, ignition off, horn, doors and parking brake.
class VehicleAudio {
  public:
    // The engines play from an audio callback: the device asks for each block as it needs it (so a
    // slow or stalled frame never starves it), and the synth renders it then, from what the game
    // last told it. 1024 frames a block: ~46 ms.
    static constexpr int sample_rate = 22050, chunk = 1024;
    static constexpr float fade_out = .18F;   // seconds: an engine stopping (getting out, pausing) fades, never cuts
    static constexpr float hearing = 40;   // metres: another vehicle's engine is heard this close
    ~VehicleAudio();
    // `listener`: where the ears are (the camera); defaults to the player's vehicle.
    void update(const VehicleSystem &system, const world::VerdaRegion &region, float dt,
                bool paused, float volume = 1, const Vector3 *listener = nullptr);
    void stop();
    // The player's engine (what it is doing; for tests and the HUD).
    [[nodiscard]] const EngineSynth &engine() const { return voices_[0].synth; }
    [[nodiscard]] const Gearbox &gearbox() const { return voices_[0].box; }
    [[nodiscard]] const std::string &passing() const { return voices_[1].vehicle; }

  private:
    struct Voice {
        AudioStream stream{};
        EngineSynth synth{sample_rate};   // (rendered on the audio thread: touched only under `lock`)
        std::mutex lock;
        Gearbox box;
        std::string vehicle, profile;
        bool playing{false};
        float stopping{0};                // seconds left of its fade out (0: not fading)
    };
    bool initialized_{false}, parking_{false};
    float volume_{1};
    std::array<Voice, 2> voices_{};   // the player's vehicle; the nearest other one on the move
    std::array<Sound, 6> sounds_{};   // start, stop, horn, door open, door close, parking brake
    std::map<std::string, EngineLoop, std::less<>> loops_;   // assets/audio/engines, by name
    float door_timer_{0};
    void initialize();
    void play(std::size_t index);
    void tune(Voice &voice, const world::WorldAsset &asset);
    void feed(Voice &voice, const EngineSynth::Params &params, float level);
    void silence(Voice &voice);   // fade it out (it stops when the fade is done)
    void halt(Voice &voice);      // stop it now
    void tick(Voice &voice, float dt);
    static void render(int voice, void *buffer, unsigned int frames);
    static VehicleAudio *live_;   // the one whose engines the audio callbacks render
};
} // namespace outland::game::vehicles
