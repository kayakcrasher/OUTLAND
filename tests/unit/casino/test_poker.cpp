// The Green Hand's poker: hand ranking, blinds, betting, folding out, showdown and side pots.
#include "outland/game/casino/Poker.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace outland::game::casino;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::uint32_t hand(std::vector<Card> cards) { return evaluate(cards); }
}

int main() {
    // Rankings.
    const auto royal = hand({{14, 0}, {13, 0}, {12, 0}, {11, 0}, {10, 0}, {2, 1}, {3, 2}});
    const auto wheel = hand({{14, 1}, {2, 0}, {3, 2}, {4, 3}, {5, 0}, {9, 1}, {13, 2}});
    const auto quads = hand({{9, 0}, {9, 1}, {9, 2}, {9, 3}, {2, 0}, {13, 1}, {4, 2}});
    const auto boat = hand({{8, 0}, {8, 1}, {8, 2}, {4, 3}, {4, 0}, {4, 1}, {2, 2}});
    const auto flush = hand({{2, 1}, {7, 1}, {9, 1}, {11, 1}, {13, 1}, {14, 0}, {14, 2}});
    const auto trips = hand({{7, 0}, {7, 1}, {7, 2}, {12, 3}, {3, 0}, {2, 1}, {9, 2}});
    const auto two_pair = hand({{12, 0}, {12, 1}, {5, 2}, {5, 3}, {14, 0}, {3, 1}, {2, 2}});
    const auto pair = hand({{10, 0}, {10, 1}, {14, 2}, {8, 3}, {6, 0}, {3, 1}, {2, 2}});
    const auto high = hand({{14, 0}, {12, 1}, {9, 2}, {7, 3}, {5, 0}, {3, 1}, {2, 2}});
    check(hand_category(royal) == 8 && hand_category(quads) == 7 && hand_category(boat) == 6 && hand_category(flush) == 5, "top categories");
    check(hand_category(wheel) == 4 && hand_category(trips) == 3 && hand_category(two_pair) == 2 && hand_category(pair) == 1 && hand_category(high) == 0, "lower categories");
    check(royal > quads && quads > boat && boat > flush && flush > wheel && wheel > trips && trips > two_pair && two_pair > pair && pair > high, "order");
    check(hand({{14, 0}, {14, 1}, {13, 2}, {7, 3}, {5, 0}}) > hand({{14, 2}, {14, 3}, {12, 2}, {7, 1}, {5, 1}}), "kickers decide");
    check(hand({{14, 0}, {14, 1}, {13, 2}, {7, 3}, {5, 0}}) == hand({{14, 2}, {14, 3}, {13, 1}, {7, 1}, {5, 1}}), "equal hands split");
    check(hand({{6, 0}, {7, 1}, {8, 2}, {9, 3}, {10, 0}}) > wheel, "six-high straight beats the wheel");
    check(std::string(category_name(6)) == "Full House" && rank_name(12) == "Q", "names");

    // Equity: aces are strong, seven-deuce is not.
    Poker poker;
    poker.sit(300, {"VITO", "BRUNO", "LA KALVA"}, 5);
    const float aces = poker.equity({Card{14, 0}, Card{14, 1}}, {}, 1, 600);
    const float junk = poker.equity({Card{7, 0}, Card{2, 1}}, {}, 1, 600);
    std::cout << "equity heads-up: AA " << aces << ", 72o " << junk << '\n';
    check(aces > .75F && junk < .42F, "equity estimates");

    // A hand: blinds are posted, the player folds, the crew plays it out and someone wins.
    check(poker.deal(), "deal");
    check(poker.pot() == Poker::small_blind + Poker::big_blind, "blinds in the pot");
    check(poker.board().empty() && poker.street() == Street::Preflop, "preflop");
    bool folded = false;
    for (int i = 0; i < 400 && poker.hand_running(); ++i) {
        if (poker.human_turn()) { poker.act(Action::Fold); folded = poker.seats()[0].folded || folded; }
        else poker.update(1, .5F);
    }
    (void)folded;
    check(!poker.hand_running() && !poker.result().empty(), "the hand finishes: " + poker.result());
    int chips = 0;
    for (const auto& s : poker.seats()) chips += s.stack;
    check(chips == 300 + 3 * Poker::crew_stack, "no chips appear or vanish");

    // Many hands with the player always calling: chips are conserved and the game keeps going.
    for (int hand_no = 0; hand_no < 60 && poker.stack() > 0; ++hand_no) {
        check(poker.deal(), "deal again");
        for (int i = 0; i < 2000 && poker.hand_running(); ++i) {
            if (poker.human_turn()) poker.act(Action::Call);
            else poker.update(1, .5F);
        }
        check(!poker.hand_running(), "every hand ends");
        int total = 0;
        for (const auto& s : poker.seats()) total += s.stack;
        check(total >= 300 + 3 * Poker::crew_stack, "chips conserved (rebuys only add)");
        check(poker.board().size() == 5 || poker.result().find(" with ") == std::string::npos, "a showdown shows five cards");
    }

    // Side pot: the player all in short; the rest is fought over by the others.
    Poker side;
    side.sit(15, {"VITO", "BRUNO", "LA KALVA"}, 9);
    check(side.deal(), "short-stack deal");
    for (int i = 0; i < 2000 && side.hand_running(); ++i) {
        if (side.human_turn()) side.act(Action::Raise);
        else side.update(1, .5F);
    }
    int total = 0;
    for (const auto& s : side.seats()) total += s.stack;
    check(total == 15 + 3 * Poker::crew_stack, "side pots conserve chips");
    check(side.seats()[0].won <= 60, "the short stack can win at most what it matched: " + std::to_string(side.seats()[0].won));
    const int left = side.stack();
    check(side.cash_out() == left && side.seats().empty(), "cash out empties the table");
    std::cout << "poker tests passed\n";
    return 0;
}
