#pragma once
#include "outland/game/sports/League.hpp"
#include <raylib.h>
#include <cstdint>
#include <vector>

// A ball game as it is played, for drawing: everyone on the field at a given minute - the pitch,
// the swing, the ball in play and who fields it, the throw, the runners going round - read off the
// game's plays (League::linescore), in a ballpark's own frame (world::ballpark: the plate at -Z,
// centre field +Z). Before first pitch the teams line up along the lines; after the last out the
// winners mob the mound and the losers trudge to their dugout.
namespace outland::game::sports {

enum class Motion : std::uint8_t {
    Stand,      // idle
    Ready,      // a fielder's crouch, glove out
    Squat,      // the catcher
    Stance,     // at the plate, the bat up
    Swing,      // phase 0..1: the bat round
    Pitch,      // phase 0..1: the wind-up, the leg kick, the arm over the top
    Throw,      // phase 0..1: a fielder's throw
    Catch,      // the glove up (a fly ball, a throw)
    Scoop,      // the glove down (a grounder)
    Run, Walk,  // on the move (speed says how fast)
    Cheer,      // arms up
    Sit,        // on the bench
};

enum class Role : std::uint8_t {
    Fielder,    // index: world::ballpark::fielders (0 pitcher .. 8 right field)
    Batter, Runner, OnDeck, Coach, Umpire, Bench,
};

struct Figure {
    Role role{Role::Fielder};
    int index{0};               // which fielder / runner (the base he started on) / bench seat
    bool home{false};           // the home club's (the umpire: neither)
    Vector3 at{};               // park frame
    float yaw{0};               // park frame, degrees; 0 faces +Z
    Motion motion{Motion::Stand};
    float phase{0};             // 0..1 through the motion
    float speed{0};             // m/s of real time, on the move
};

struct Bat { Vector3 grip{}, tip{}; };

struct Scene {
    std::vector<Figure> figures;
    std::vector<Bat> bats;
    bool ball{false};
    Vector3 ball_at{};
    int play{-1};               // the plate appearance under way (Linescore::plays), -1 none
};

// Everyone on the field `t` island minutes after first pitch; `seconds_per_minute` turns island
// minutes into real seconds (for how fast people move).
Scene scene(const Fixture& fixture, const Linescore& line, double t, float seconds_per_minute = 2);
// Whether the bat met the ball (a ball put in play) between t0 and t1.
bool crack(const Linescore& line, double t0, double t1);

} // namespace outland::game::sports
