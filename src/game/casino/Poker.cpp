#include "outland/game/casino/Poker.hpp"
#include <algorithm>
#include <initializer_list>

namespace outland::game::casino {

namespace {
// Highest card of a five-in-a-row in `mask` (bit r = rank r; the ace also plays low), or 0.
int straight_high(std::uint32_t mask) {
    if (mask & (1U << 14)) mask |= 1U << 1;
    for (int high = 14; high >= 5; --high) {
        const std::uint32_t run = 0x1FU << (high - 4);
        if ((mask & run) == run) return high;
    }
    return 0;
}
std::vector<int> top_ranks(std::uint32_t mask, int count, std::initializer_list<int> skip = {}) {
    std::vector<int> out;
    for (int r = 14; r >= 2 && static_cast<int>(out.size()) < count; --r)
        if ((mask >> r) & 1U && std::find(skip.begin(), skip.end(), r) == skip.end()) out.push_back(r);
    return out;
}
std::uint32_t pack(int category, const std::vector<int>& kickers) {
    std::uint32_t value = static_cast<std::uint32_t>(category) << 20;
    int shift = 16;
    for (const int k : kickers) { value |= static_cast<std::uint32_t>(k & 15) << shift; shift -= 4; if (shift < 0) break; }
    return value;
}
}

std::string rank_name(int rank) {
    switch (rank) {
        case 11: return "J";
        case 12: return "Q";
        case 13: return "K";
        case 14: return "A";
        default: return std::to_string(rank);
    }
}

std::uint32_t evaluate(std::span<const Card> cards) {
    std::array<int, 15> counts{};
    std::array<std::uint32_t, 4> suits{};
    std::array<int, 4> suit_count{};
    std::uint32_t mask = 0;
    for (const auto& c : cards) {
        if (c.rank < 2 || c.rank > 14 || c.suit < 0 || c.suit > 3) continue;
        ++counts[static_cast<std::size_t>(c.rank)];
        suits[static_cast<std::size_t>(c.suit)] |= 1U << c.rank;
        ++suit_count[static_cast<std::size_t>(c.suit)];
        mask |= 1U << c.rank;
    }
    int flush = -1;
    for (int s = 0; s < 4; ++s) {
        if (suit_count[static_cast<std::size_t>(s)] < 5) continue;
        flush = s;
        if (const int high = straight_high(suits[static_cast<std::size_t>(s)])) return pack(8, {high});
    }
    int quads = 0;
    std::vector<int> trips, pairs;
    for (int r = 14; r >= 2; --r) {
        const int n = counts[static_cast<std::size_t>(r)];
        if (n == 4) quads = r;
        else if (n == 3) trips.push_back(r);
        else if (n == 2) pairs.push_back(r);
    }
    if (quads) return pack(7, {quads, top_ranks(mask, 1, {quads}).at(0)});
    if (!trips.empty() && (trips.size() >= 2 || !pairs.empty()))
        return pack(6, {trips[0], std::max(trips.size() >= 2 ? trips[1] : 0, pairs.empty() ? 0 : pairs[0])});
    if (flush >= 0) return pack(5, top_ranks(suits[static_cast<std::size_t>(flush)], 5));
    if (const int high = straight_high(mask)) return pack(4, {high});
    if (!trips.empty()) {
        auto k = top_ranks(mask, 2, {trips[0]});
        return pack(3, {trips[0], k[0], k[1]});
    }
    if (pairs.size() >= 2) return pack(2, {pairs[0], pairs[1], top_ranks(mask, 1, {pairs[0], pairs[1]}).at(0)});
    if (pairs.size() == 1) {
        auto k = top_ranks(mask, 3, {pairs[0]});
        k.insert(k.begin(), pairs[0]);
        return pack(1, k);
    }
    return pack(0, top_ranks(mask, 5));
}

int hand_category(std::uint32_t value) { return static_cast<int>(value >> 20); }

const char* category_name(int category) {
    static constexpr const char* names[]{"High Card", "Pair", "Two Pair", "Three of a Kind", "Straight",
                                         "Flush", "Full House", "Four of a Kind", "Straight Flush"};
    return category >= 0 && category <= 8 ? names[category] : "?";
}

float Poker::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / static_cast<float>(1ULL << 24);
}

void Poker::sit(int buy_in, const std::vector<std::string>& opponents, std::uint64_t seed) {
    rng_ = (seed ^ 0x2545f4914f6cdd1dULL) | 1;
    seats_.clear();
    Seat you;
    you.name = "YOU";
    you.stack = std::max(0, buy_in);
    you.human = true;
    seats_.push_back(you);
    for (const auto& name : opponents) {
        Seat s;
        s.name = name;
        s.stack = crew_stack;
        seats_.push_back(s);
    }
    dealer_ = static_cast<int>(random() * static_cast<float>(seats_.size())) % static_cast<int>(seats_.size());
    running_ = false;
    street_ = Street::Showdown;
    hands_ = 0;
    result_.clear();
    board_.clear();
}

Card Poker::draw() {
    const Card c = deck_.back();
    deck_.pop_back();
    return c;
}

bool Poker::deal() {
    if (seats_.size() < 2 || seats_[0].stack <= 0) return false;
    for (auto& s : seats_) {
        if (!s.human && s.stack < big_blind) { s.stack = crew_stack; s.last = "BUYS IN"; } // the crew is never short
        s.folded = s.all_in = s.acted = false;
        s.bet = s.total = s.won = 0;
        s.last.clear();
    }
    if (!forced_deck_.empty()) {
        deck_ = forced_deck_;
        forced_deck_.clear();
    } else {
        deck_.clear();
        for (int suit = 0; suit < 4; ++suit) for (int rank = 2; rank <= 14; ++rank) deck_.push_back({rank, suit});
        for (std::size_t i = deck_.size() - 1; i > 0; --i)
            std::swap(deck_[i], deck_[static_cast<std::size_t>(random() * static_cast<float>(i + 1)) % (i + 1)]);
    }
    const int n = static_cast<int>(seats_.size());
    dealer_ = (dealer_ + 1) % n;
    board_.clear();
    for (int round = 0; round < 2; ++round)
        for (int i = 1; i <= n; ++i) seats_[static_cast<std::size_t>((dealer_ + i) % n)].hole[static_cast<std::size_t>(round)] = draw();
    street_ = Street::Preflop;
    const int sb = n == 2 ? dealer_ : (dealer_ + 1) % n, bb = (sb + 1) % n;
    post(sb, small_blind);
    post(bb, big_blind);
    seats_[static_cast<std::size_t>(sb)].last = "SMALL BLIND";
    seats_[static_cast<std::size_t>(bb)].last = "BIG BLIND";
    current_bet_ = big_blind;
    raises_ = 1;
    running_ = true;
    result_.clear();
    think_clock_ = 0;
    ++hands_;
    to_act_ = next_active(bb);
    if (to_act_ < 0 || round_done()) advance(bb); // everyone all in on the blinds
    return true;
}

void Poker::post(int seat, int amount) {
    auto& s = seats_[static_cast<std::size_t>(seat)];
    const int paid = std::clamp(amount, 0, s.stack);
    s.stack -= paid;
    s.bet += paid;
    s.total += paid;
    if (s.stack == 0) s.all_in = true;
}

int Poker::next_active(int from) const {
    const int n = static_cast<int>(seats_.size());
    for (int i = 1; i <= n; ++i) {
        const int s = (from + i) % n;
        if (!seats_[static_cast<std::size_t>(s)].folded && !seats_[static_cast<std::size_t>(s)].all_in) return s;
    }
    return -1;
}

int Poker::live_count() const {
    return static_cast<int>(std::count_if(seats_.begin(), seats_.end(), [](const Seat& s) { return !s.folded; }));
}

int Poker::to_call(int seat) const {
    if (seat < 0 || static_cast<std::size_t>(seat) >= seats_.size()) return 0;
    const auto& s = seats_[static_cast<std::size_t>(seat)];
    return std::min(s.stack, std::max(0, current_bet_ - s.bet));
}

bool Poker::can_raise(int seat) const {
    if (seat < 0 || static_cast<std::size_t>(seat) >= seats_.size()) return false;
    const auto& s = seats_[static_cast<std::size_t>(seat)];
    return raises_ < cap && s.stack > std::max(0, current_bet_ - s.bet);
}

int Poker::pot() const {
    int total = 0;
    for (const auto& s : seats_) total += s.total;
    return total;
}

bool Poker::round_done() const {
    int able = 0;
    for (const auto& s : seats_) {
        if (s.folded || s.all_in) continue;
        ++able;
        if (s.bet < current_bet_) return false;
    }
    if (able <= 1) return true; // nobody left to bet against
    for (const auto& s : seats_) if (!s.folded && !s.all_in && !s.acted) return false;
    return true;
}

void Poker::begin_street() {
    for (auto& s : seats_) { s.bet = 0; s.acted = false; }
    current_bet_ = 0;
    raises_ = 0;
    if (street_ == Street::Flop) for (int i = 0; i < 3; ++i) board_.push_back(draw());
    else if (street_ == Street::Turn || street_ == Street::River) board_.push_back(draw());
    to_act_ = next_active(dealer_);
}

void Poker::apply(int seat, Action action) {
    auto& s = seats_[static_cast<std::size_t>(seat)];
    const int owed = to_call(seat);
    if (action == Action::Raise && !can_raise(seat)) action = Action::Call;
    switch (action) {
        case Action::Fold:
            if (owed == 0) { s.last = "CHECK"; break; } // nobody folds for free
            s.folded = true;
            s.last = "FOLD";
            break;
        case Action::Call:
            post(seat, owed);
            s.last = owed == 0 ? "CHECK" : s.all_in ? "ALL IN" : "CALL " + std::to_string(owed);
            break;
        case Action::Raise: {
            post(seat, owed + raise_size());
            current_bet_ = std::max(current_bet_, s.bet);
            ++raises_;
            for (auto& other : seats_) other.acted = false;
            s.last = s.all_in ? "ALL IN" : (current_bet_ == raise_size() && street_ != Street::Preflop ? "BET " : "RAISE TO ") + std::to_string(s.bet);
            break;
        }
    }
    s.acted = true;
    think_clock_ = 0;
    // Everyone else folded: the last one standing takes the pot unseen.
    if (live_count() == 1) {
        const auto winner = std::find_if(seats_.begin(), seats_.end(), [](const Seat& x) { return !x.folded; });
        const int pot_total = pot();
        winner->stack += pot_total;
        winner->won = pot_total;
        result_ = (winner->human ? std::string("You win ") : winner->name + " wins ") + std::to_string(pot_total);
        street_ = Street::Showdown;
        running_ = false;
        to_act_ = -1;
        return;
    }
    advance(seat);
}

void Poker::advance(int actor) {
    if (!round_done()) { to_act_ = next_active(actor); return; }
    while (true) {
        if (street_ == Street::River) { showdown(); return; }
        street_ = static_cast<Street>(static_cast<int>(street_) + 1);
        begin_street();
        if (!round_done()) return;
    }
}

void Poker::showdown() {
    street_ = Street::Showdown;
    running_ = false;
    to_act_ = -1;
    while (board_.size() < 5 && !deck_.empty()) board_.push_back(draw());
    std::vector<std::uint32_t> value(seats_.size(), 0);
    for (std::size_t i = 0; i < seats_.size(); ++i) {
        if (seats_[i].folded) continue;
        std::vector<Card> cards(board_.begin(), board_.end());
        cards.push_back(seats_[i].hole[0]);
        cards.push_back(seats_[i].hole[1]);
        value[i] = evaluate(cards);
    }
    // Main pot and side pots by contribution level; each goes to the best hand that paid into it.
    std::vector<int> levels;
    for (const auto& s : seats_) if (s.total > 0) levels.push_back(s.total);
    std::sort(levels.begin(), levels.end());
    levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
    int previous = 0;
    for (const int level : levels) {
        int part = 0;
        for (const auto& s : seats_) part += std::min(s.total, level) - std::min(s.total, previous);
        previous = level;
        std::vector<std::size_t> eligible;
        for (std::size_t i = 0; i < seats_.size(); ++i) if (!seats_[i].folded && seats_[i].total >= level) eligible.push_back(i);
        if (eligible.empty()) { // folded money above every live stack: back to the live seat that paid most
            std::size_t most = 0;
            for (std::size_t i = 0; i < seats_.size(); ++i)
                if (!seats_[i].folded && (seats_[most].folded || seats_[i].total > seats_[most].total)) most = i;
            eligible.push_back(most);
        }
        std::uint32_t best = 0;
        for (const auto i : eligible) best = std::max(best, value[i]);
        std::vector<std::size_t> winners;
        for (const auto i : eligible) if (value[i] == best) winners.push_back(i);
        const int share = part / static_cast<int>(winners.size());
        for (const auto i : winners) { seats_[i].stack += share; seats_[i].won += share; }
        seats_[winners.front()].stack += part - share * static_cast<int>(winners.size());
        seats_[winners.front()].won += part - share * static_cast<int>(winners.size());
    }
    result_.clear();
    for (std::size_t i = 0; i < seats_.size(); ++i) {
        if (seats_[i].won <= 0 || seats_[i].folded) continue;
        if (!result_.empty()) result_ += ", ";
        result_ += (seats_[i].human ? std::string("You win ") : seats_[i].name + " wins ") + std::to_string(seats_[i].won) +
                   " with " + category_name(hand_category(value[i]));
    }
}

float Poker::equity(const std::array<Card, 2>& hole, const std::vector<Card>& board, int opponents, int trials) {
    opponents = std::clamp(opponents, 1, 8);
    std::vector<Card> rest;
    for (int suit = 0; suit < 4; ++suit)
        for (int rank = 2; rank <= 14; ++rank) {
            const Card c{rank, suit};
            if (c == hole[0] || c == hole[1] || std::find(board.begin(), board.end(), c) != board.end()) continue;
            rest.push_back(c);
        }
    const std::size_t need = static_cast<std::size_t>(opponents) * 2 + (5 - std::min<std::size_t>(5, board.size()));
    if (rest.size() < need || trials <= 0) return 0;
    float score = 0;
    std::vector<Card> mine, theirs;
    for (int t = 0; t < trials; ++t) {
        for (std::size_t i = 0; i < need; ++i)
            std::swap(rest[i], rest[i + static_cast<std::size_t>(random() * static_cast<float>(rest.size() - i)) % (rest.size() - i)]);
        std::size_t k = static_cast<std::size_t>(opponents) * 2;
        mine.assign(board.begin(), board.end());
        while (mine.size() < 5) mine.push_back(rest[k++]);
        const std::vector<Card> full_board = mine;
        mine.push_back(hole[0]);
        mine.push_back(hole[1]);
        const std::uint32_t me = evaluate(mine);
        int ties = 0;
        bool lost = false;
        for (int o = 0; o < opponents && !lost; ++o) {
            theirs = full_board;
            theirs.push_back(rest[static_cast<std::size_t>(o) * 2]);
            theirs.push_back(rest[static_cast<std::size_t>(o) * 2 + 1]);
            const std::uint32_t them = evaluate(theirs);
            if (them > me) lost = true;
            else if (them == me) ++ties;
        }
        if (!lost) score += 1.0F / static_cast<float>(ties + 1);
    }
    return score / static_cast<float>(trials);
}

void Poker::ai_act(int seat) {
    const auto& s = seats_[static_cast<std::size_t>(seat)];
    const int owed = to_call(seat);
    const float eq = equity(s.hole, board_, std::max(1, live_count() - 1), 160);
    const float odds = owed > 0 ? static_cast<float>(owed) / static_cast<float>(pot() + owed) : 0.0F;
    const float r = random();
    Action action;
    if (can_raise(seat) && (eq > .72F || (eq > .55F && r < .45F))) action = Action::Raise;
    else if (owed == 0) action = can_raise(seat) && r < .08F ? Action::Raise : Action::Call; // the odd bluff
    else if (eq + .04F > odds || r < .05F) action = Action::Call;
    else action = Action::Fold;
    apply(seat, action);
}

bool Poker::act(Action action) {
    if (!human_turn()) return false;
    apply(0, action);
    return true;
}

void Poker::update(float dt, float think) {
    if (!running_ || to_act_ <= 0) return;
    think_clock_ += std::max(0.0F, dt);
    if (think_clock_ >= think) ai_act(to_act_);
}

int Poker::cash_out() {
    const int chips = stack();
    seats_.clear();
    running_ = false;
    to_act_ = -1;
    return chips;
}

} // namespace outland::game::casino
