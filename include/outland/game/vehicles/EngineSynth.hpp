#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

// Engine sound. The engine itself is real recordings (assets/audio/engines): a few seamless loops
// for each kind of engine (a four, a V8, a diesel), from idle up the revs, played back pitched to
// the revs and crossfaded from one to the next as they climb; the throttle makes it louder and
// brighter. Without the recordings (or in tests) it is synthesised instead, and what rides on top
// is synthesised either way: the turbo, the supercharger, pops, the jake brake, the air brakes,
// the intake under load, the road and the tyres.
//
// The synthesis follows the engine exactly. A four-stroke fires each cylinder once every two turns of the crank. Each
// firing blows a pulse of hot gas into the exhaust: a smooth pressure pulse as wide as the
// exhaust valve's blowdown (about a sixth of a turn of the crank, so it narrows as the revs rise
// and the note brightens). The pulses run down the pipe, which rings at its own lengths (the
// header and the tailpipe reflect them back), and out through the muffler, which takes the edge
// off (less for a sport exhaust, hardly at all for straight pipes). The intake roars under load;
// every firing differs a little. The pitch of the note is the firing rate, so it follows the revs;
// the revs follow the gearbox (an automatic: it shifts up as the revs climb and down as they
// fall). Uneven firing makes a V8's lope; diesels clatter and knock; turbos whistle and blow off;
// straight pipes pop on the overrun; a big rig's engine brake barks and its air brakes sigh when
// it stops. Wind, road rumble and tyre squeal ride on top.
namespace outland::game::vehicles {

struct EngineProfile {
    int cylinders{4};
    float idle{850}, redline{6500};
    int gears{5};
    bool diesel{false};
    bool cvt{false};          // no gears: the revs ride the throttle (a side-by-side's belt)
    float lope{0};            // uneven firing, 0..1 (a cross-plane V8's burble)
    float body_hz{110};       // the exhaust's ring at idle
    float rasp{.5F};          // the hiss at the top, 0..1
    float turbo{0};           // whistle and blow-off, 0..1
    float whine{0};           // a supercharger's whine, 0..1
    float gain{1};            // how loud (the exhaust)
    bool pops{false};         // crackles and bangs on the overrun
    float pipe{2.6F};         // metres of exhaust from the manifold to the tailpipe (the pipe's ring)
    float muffler{900};       // Hz: where the muffler starts taking the edge off
    bool jake{false};         // an engine (compression-release) brake: barks on the overrun
    bool air_brakes{false};   // sighs when it comes to a stop
};

// The voice for a vehicle type (game::vehicles definition id) with its fitted engine and exhaust
// (game::autos::engines and exhausts indices).
EngineProfile engine_profile(std::string_view definition, int engine = 0, int exhaust = 0);

// A recorded engine loop: mono samples at the synth's rate, and the firing rate it was recorded at.
struct EngineLoop {
    std::vector<float> pcm;
    float hz{50};
};

// The recorded loops for a vehicle (names in assets/audio/engines, low revs to high), and each
// one's firing rate.
std::vector<std::string_view> engine_loops(std::string_view definition, int engine = 0, int exhaust = 0);
float engine_loop_hz(std::string_view name);

// The automatic gearbox: from road speed and throttle to the revs.
class Gearbox {
public:
    // `speed`: road speed as a share of the vehicle's top speed; `throttle`: -1..1 (back is the brake).
    float update(float dt, float speed, float throttle, const EngineProfile& profile);
    [[nodiscard]] int gear() const { return gear_; }
    [[nodiscard]] float rpm() const { return rpm_; }
    [[nodiscard]] bool shifting() const { return shift_ > 0; }
    void reset(const EngineProfile& profile) { gear_ = 1; rpm_ = profile.idle; shift_ = 0; }

private:
    int gear_{1};
    float rpm_{0}, shift_{0};
};

class EngineSynth {
public:
    struct Params {
        bool running{false};
        float rpm{0};
        float load{0};        // 0..1: how hard it's working (the throttle, in gear)
        float speed{0};       // m/s on the road (wind and rumble)
        float squeal{0};      // 0..1: tyres sliding
        float volume{1};
        bool limiter{false};  // bouncing off the rev limiter
        float brake{0};       // 0..1: on the brakes (a big rig's engine brake bites harder)
    };
    explicit EngineSynth(int sample_rate = 22050) : rate_(static_cast<float>(sample_rate)) {}
    void set_profile(const EngineProfile& profile);
    void set(const Params& params) { params_ = params; }
    // Play these recordings (low revs to high; null or empty to synthesise). Not owned: they must
    // outlive the synth's use of them.
    void set_loops(const std::vector<const EngineLoop*>& loops);
    [[nodiscard]] bool recorded() const { return !loops_.empty(); }
    [[nodiscard]] const EngineProfile& profile() const { return profile_; }
    // Mono samples, -1..1.
    void render(float* out, int frames);
    void render(short* out, int frames);
    [[nodiscard]] std::uint64_t firings() const { return firings_; }
    [[nodiscard]] int pops() const { return pops_; }
    [[nodiscard]] float jake() const { return jake_; }      // how hard the engine brake is barking, 0..1
    [[nodiscard]] int sighs() const { return sighs_; }      // air brake releases so far

private:
    struct Pulse { float at{0}, length{1}, strength{0}; };
    struct Band {
        float b0{0}, a1{0}, a2{0}, x1{0}, x2{0}, y1{0}, y2{0};
        void tune(float hz, float q, float rate);
        float run(float x);
    };
    float noise();
    float rate_;
    EngineProfile profile_{};
    Params params_{};
    std::array<float, 12> angle_{}, punch_{};   // each cylinder's firing point in the cycle, and its strength
    float phase_{0}, kick_{0}, burst_{0}, clatter_{0}, lowpass_{0}, spool_{0}, blow_{0}, whistle_{0}, whine_phase_{0};
    float rumble_{0}, wind_{0}, last_load_{0}, overrun_{0}, gain_{0};
    std::array<Pulse, 16> pulses_{};
    std::array<float, 2048> tail_{}, header_{};          // the pipe's travelling waves
    int tail_at_{0}, header_at_{0};
    float muffle1_{0}, muffle2_{0}, dc_in_{0}, dc_out_{0}, intake_{0}, jake_{0}, sigh_{0}, rolling_{0}, held_{0};
    int next_{0}, skip_{0}, pops_{0}, sighs_{0};
    std::uint64_t firings_{0};
    std::uint32_t seed_{0x9e3779b9U};
    Band body_, rasp_, crack_, squeal_, hiss_, roar_, air_;
    int retune_{0};
    std::vector<const EngineLoop*> loops_;
    std::array<double, 4> read_{};           // where each loop is playing
    float tone_{0}, gate_{1};
};

} // namespace outland::game::vehicles
