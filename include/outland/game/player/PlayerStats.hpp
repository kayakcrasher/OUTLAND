#pragma once
#include <cstdint>
#include <string>
#include <vector>

// The player's record on Verda, kept with their save: how far they have driven and walked, how
// fast they have gone, who they have taken down and how often they went down themselves, the
// arrests, the loads, the races, the money. Shown in the bag (STATS).
namespace outland::game::player {

struct StatLine {
    std::string label, value;
};

class PlayerStats {
public:
    // Distances (metres). A jump of more than `teleport` metres in one step (a respawn, a fast
    // travel, the drop) isn't travel and isn't counted.
    static constexpr float teleport = 25;
    void drove(float metres, float speed);   // speed: m/s, for the top speed
    void walked(float metres);
    void played(float seconds);
    void kill() { ++kills_; }
    void died() { ++deaths_; }
    void arrests(int booked) { arrests_ += booked > 0 ? booked : 0; }
    void busted() { ++busted_; }
    void hauled() { ++loads_; }
    void race_won() { ++races_; }
    // The wallet: what came in and what went out.
    void money(std::int64_t before, std::int64_t after);

    [[nodiscard]] double km_driven() const { return driven_ / 1000; }
    [[nodiscard]] double km_walked() const { return walked_ / 1000; }
    [[nodiscard]] float top_kmh() const { return top_ * 3.6F; }
    [[nodiscard]] int kills() const { return kills_; }
    [[nodiscard]] int deaths() const { return deaths_; }
    [[nodiscard]] int arrests() const { return arrests_; }
    [[nodiscard]] int times_busted() const { return busted_; }
    [[nodiscard]] int loads() const { return loads_; }
    [[nodiscard]] int races() const { return races_; }
    [[nodiscard]] std::int64_t earned() const { return earned_; }
    [[nodiscard]] std::int64_t spent() const { return spent_; }
    [[nodiscard]] double hours_played() const { return seconds_ / 3600; }

    // For the bag: every stat with its value ("KM DRIVEN", "12.4").
    [[nodiscard]] std::vector<StatLine> lines() const;
    // "stats 1 driven=... walked=..." (unknown keys are skipped, missing ones start at zero).
    [[nodiscard]] std::string serialise() const;
    bool parse(const std::string& text);

private:
    double driven_{0}, walked_{0}, seconds_{0};
    float top_{0};
    int kills_{0}, deaths_{0}, arrests_{0}, busted_{0}, loads_{0}, races_{0};
    std::int64_t earned_{0}, spent_{0};
};

} // namespace outland::game::player
