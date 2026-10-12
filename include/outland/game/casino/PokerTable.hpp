#pragma once
#include "outland/game/casino/Poker.hpp"
#include <raylib.h>
#include <string>
#include <unordered_set>
#include <vector>

// The poker table on screen: the felt, the four seats, the board, the pot and the player's
// buttons (FOLD, CHECK/CALL, RAISE, DEAL, LEAVE; keys F, C, R, N or Enter, Esc or L). Taps and
// clicks like the shop's.
namespace outland::game::casino {

enum class PokerInput : unsigned char { None, Fold, Call, Raise, Deal, Leave };

class PokerTable {
public:
    void opened() { warm_ = true; }
    PokerInput input(const Poker& poker, int width, int height, bool focused);
    // `wallet`: kronoj outside the table; `house`: the gang that runs it.
    void draw(const Poker& poker, long long wallet, const std::string& house, int width, int height, float time) const;
    static Rectangle button(int index, int width, int height); // 0 fold, 1 call, 2 raise, 3 deal, 4 leave
    static void draw_card(Rectangle where, Card card, bool face_up);
private:
    std::vector<Vector2> clicks(bool focused);
    std::unordered_set<int> previous_;
    bool warm_{true};
};

} // namespace outland::game::casino
