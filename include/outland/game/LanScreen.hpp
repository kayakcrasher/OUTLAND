#pragma once
#include "outland/net/LanSession.hpp"
#include <raylib.h>
#include <string>
#include <unordered_set>

// LAN PLAY from the home screen: host a game or join one found on the network, then the lobby,
// where the host picks the mode, the bots and the sides (everyone for themselves, one squad, two
// teams) and starts the match for everyone.
namespace outland::game {

class LanScreen {
public:
    enum class Kind { None, Back, Host, Join, CycleMode, CycleLevel, CycleSides, Start, Leave };
    struct Action { Kind kind{Kind::None}; int index{-1}; };
    static constexpr int listed = 5;   // games shown at once

    Action update(int width, int height, const net::LanSession& lan);
    void draw(int width, int height, const net::LanSession& lan, const std::string& name) const;

    static Rectangle host_button(int width, int height);
    static Rectangle game_button(int index, int width, int height);
    static Rectangle back_button(int width, int height);
    static Rectangle mode_button(int width, int height);
    static Rectangle level_button(int width, int height);
    static Rectangle sides_button(int width, int height);
    static Rectangle start_button(int width, int height);

private:
    std::unordered_set<int> previous_;
};

}
