#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Texas Hold'em at La Verda Mano's table in Verda City: the player against three Krimuloj,
// fixed limit (blinds 5/10, bets 10 before the turn and 20 after, four bets a street), side
// pots for short stacks. The Krimuloj play by estimated equity (Monte Carlo against the hands
// still in) and pot odds, with the odd bluff. Pure game logic: no drawing, no input.
namespace outland::game::casino {

struct Card {
    int rank{2};  // 2..14 (ace high)
    int suit{0};  // 0 spades, 1 hearts, 2 diamonds, 3 clubs
    bool operator==(const Card&) const = default;
};
std::string rank_name(int rank); // "2".."10", "J", "Q", "K", "A"

// A comparable strength for the best five of up to seven cards: higher wins, equal splits.
std::uint32_t evaluate(std::span<const Card> cards);
int hand_category(std::uint32_t value); // 0 high card .. 8 straight flush
const char* category_name(int category);

enum class Street : std::uint8_t { Preflop, Flop, Turn, River, Showdown };
enum class Action : std::uint8_t { Fold, Call, Raise }; // Call with nothing to call is a check

struct Seat {
    std::string name;
    int stack{0};
    std::array<Card, 2> hole{};
    bool human{false}, folded{false}, all_in{false}, acted{false}, out{false};
    int bet{0};    // this street
    int total{0};  // this hand
    int won{0};    // last showdown
    std::string last; // last action, for the table
};

class Poker {
public:
    static constexpr int small_blind = 5, big_blind = 10, small_bet = 10, big_bet = 20, cap = 4;
    static constexpr int crew_stack = 600; // a Krimulo who goes broke buys back in

    // The player (seat 0) sits down with `buy_in` against `opponents` (names).
    void sit(int buy_in, const std::vector<std::string>& opponents, std::uint64_t seed);
    // Shuffles, posts the blinds and deals. False if the player has no chips.
    bool deal();
    // The player's move (only when it is their turn).
    bool act(Action action);
    // The Krimuloj think (one action per `think` seconds) and the streets advance.
    void update(float dt, float think = .8F);

    [[nodiscard]] bool hand_running() const { return running_; }
    [[nodiscard]] bool human_turn() const { return running_ && to_act_ == 0 && street_ != Street::Showdown; }
    [[nodiscard]] int to_call(int seat) const;
    [[nodiscard]] int raise_size() const { return street_ >= Street::Turn ? big_bet : small_bet; }
    [[nodiscard]] bool can_raise(int seat) const;
    [[nodiscard]] Street street() const { return street_; }
    [[nodiscard]] const std::vector<Seat>& seats() const { return seats_; }
    [[nodiscard]] const std::vector<Card>& board() const { return board_; }
    [[nodiscard]] int pot() const;
    [[nodiscard]] int live_seats() const { return live_count(); }
    [[nodiscard]] int dealer() const { return dealer_; }
    [[nodiscard]] int to_act() const { return to_act_; }
    [[nodiscard]] const std::string& result() const { return result_; }
    [[nodiscard]] int stack() const { return seats_.empty() ? 0 : seats_[0].stack; }
    [[nodiscard]] int hands() const { return hands_; }
    // Chips the player leaves with (their stack); the table is empty afterwards.
    int cash_out();
    // AI equity estimate for `hole` on `board` against `opponents` random hands (exposed for tests).
    float equity(const std::array<Card, 2>& hole, const std::vector<Card>& board, int opponents, int trials);
    // Tests: stack the deck (drawn from the back).
    void set_deck(std::vector<Card> deck) { forced_deck_ = std::move(deck); }

private:
    float random();
    Card draw();
    int next_active(int from) const;   // the next seat still able to act
    int live_count() const;            // seats not folded
    void begin_street();
    void post(int seat, int amount);
    void advance(int actor);
    bool round_done() const;
    void showdown();
    void ai_act(int seat);
    void apply(int seat, Action action);

    std::vector<Seat> seats_;
    std::vector<Card> deck_, board_, forced_deck_;
    Street street_{Street::Preflop};
    int dealer_{0}, to_act_{-1}, current_bet_{0}, raises_{0}, hands_{0};
    bool running_{false};
    float think_clock_{0};
    std::string result_;
    std::uint64_t rng_{0x2545f4914f6cdd1dULL};
};

} // namespace outland::game::casino
