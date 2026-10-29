#pragma once
#include "outland/world/roads/RoadGraph.hpp"
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::vehicles { class VehicleSystem; }

// Street racing (Explore): at night the island's young drivers meet by the Marnafto station on
// the ring road - three cars with their underglow on, and Fulmo, who runs it. Talk to him (USE) and
// put 1,000 kr in the pot; line your car up at the start; three, two, one, and it's a quarter of
// the ring flat out, through the flares, to the finish. First across takes the pot (everyone's
// stake). The cars run on autopilot (game::ai::steer_along), quicker when they fall behind and
// easing off when they are far ahead, so a race is close. Leave your car or the road and you've
// forfeited.
namespace outland::game::racing {

enum class Stage : std::uint8_t { Closed, Open, Offered, Lining, Countdown, Racing, Done };
enum class Result : std::uint8_t { None, Won, Lost, Forfeit };

struct Racer {
    std::string vehicle;          // runtime vehicle id
    std::string name;             // "Mira 'Ŝtormo' Kovač"
    float pace{40};               // m/s on the straight, before the catch-up
    Vector3 parked{};             // its place at the meet
    float parked_yaw{0};
    // Racing: its own line round (each racer keeps a lane).
    world::roads::Route route;
    float progress{0}, stuck{0}, reverse{0};
    int attempts{0};
    bool finished{false};
    float finish_time{0};
};

struct Frame {
    float dt{0};
    int hour{12};
    bool driving{false};          // the player is at the wheel
    Vector3 car{};                // the player's car (or feet)
    float speed{0};               // the player's car, m/s
    std::int64_t money{0};
};

class StreetRace {
public:
    static constexpr int bet = 1000;
    static constexpr float lane = 4.5F;            // metres right of the ring's centre line (clear of the barrier)
    static constexpr float checkpoint_radius = 30;
    static constexpr float countdown_seconds = 3;
    static constexpr float talk_range = 3.5F;
    static bool night(int hour) { return hour >= 21 || hour < 3; }

    // The ring road's centre line (a loop) and the meet: the track runs from the ring point nearest
    // the meet a quarter of the way round. Returns false without a ring.
    bool setup(const std::vector<Vector3>& ring, Vector3 meet);
    [[nodiscard]] bool ready() const { return track_.complete; }
    void set_racers(std::vector<Racer> racers) { racers_ = std::move(racers); }
    [[nodiscard]] std::vector<Racer>& racers() { return racers_; }
    [[nodiscard]] const std::vector<Racer>& racers() const { return racers_; }
    [[nodiscard]] Vector3 meet() const { return meet_; }
    [[nodiscard]] Vector3 organiser() const { return organiser_; }
    [[nodiscard]] const world::roads::Route& track() const { return track_; }
    [[nodiscard]] Vector3 start() const;
    [[nodiscard]] float start_yaw() const;
    [[nodiscard]] Vector3 finish() const;
    [[nodiscard]] float length() const { return finish_at_; }   // start to finish (the track runs on past it)
    [[nodiscard]] const std::vector<float>& checkpoints() const { return checkpoints_; }   // distances along the track
    // The grid: where racer `i` lines up (behind the line, in the two lanes).
    [[nodiscard]] Vector3 grid(int i, float* yaw = nullptr) const;

    // USE by Fulmo: the first time he names the stakes; the second (with the money) the bet is on
    // and the player lines up. Returns what he says; `paid` is set when the stake was taken.
    std::string talk(std::int64_t money, bool& paid);
    // Each frame. Moves the racers (the game spawned their cars at the meet).
    void update(const Frame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region);

    [[nodiscard]] Stage stage() const { return stage_; }
    [[nodiscard]] float countdown() const { return countdown_; }
    [[nodiscard]] int position() const;            // the player's place, 1-based (racing)
    [[nodiscard]] int field() const { return static_cast<int>(racers_.size()) + 1; }
    [[nodiscard]] int next_checkpoint() const { return next_cp_; }
    [[nodiscard]] float player_progress() const { return player_progress_; }
    [[nodiscard]] Vector3 target() const;          // where to head: the grid, the next flare, the finish
    // Once: how the race ended for the player, and the pot (won) or the stake (lost).
    Result take_result() { const Result r = result_; result_ = Result::None; return r; }
    [[nodiscard]] std::int64_t pot() const { return bet * field(); }
    [[nodiscard]] const std::string& winner() const { return winner_; }

private:
    void reset_field(vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    world::roads::Route track_;
    std::vector<float> checkpoints_;
    std::vector<Racer> racers_;
    Vector3 meet_{}, organiser_{};
    Stage stage_{Stage::Closed};
    Result result_{Result::None};
    float countdown_{0}, clock_{0}, away_{0}, done_for_{0}, player_progress_{0}, finish_at_{0};
    int next_cp_{0};
    bool player_finished_{false};
    std::string winner_;
};

} // namespace outland::game::racing
