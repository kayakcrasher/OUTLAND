#include "outland/game/campaign/Phone.hpp"
#include "outland/game/culture/Faith.hpp"
#include <algorithm>

namespace outland::game::campaign {

namespace {
constexpr const char* ron = "Uncle Ron";
constexpr const char* dario = "Uncle Dario";
} // namespace

bool Phone::said(const std::string& topic) const { return std::find(done_.begin(), done_.end(), topic) != done_.end(); }

std::optional<Call> Phone::choose(const PhoneContext& c) const {
    const auto fresh = [&](const char* topic) { return !said(topic); };
    // In order: what's most pressing first.
    if (!c.rumour.empty() && fresh("ron_rumour"))
        return Call{ron, "ron_rumour", {
            "Nevo... people are talking. " + c.rumour,
            "Tell me that wasn't you. ...No. Don't tell me. Just be careful out there."}, std::nullopt, ""};
    if (c.cop && fresh("dario_badge"))
        return Call{dario, "dario_badge", {
            "So. You wear their badge now. Ron must be proud.",
            "Family's family. But don't come to my chapel in that uniform, and don't come to my bar at all."}, std::nullopt, ""};
    if (c.feast_tomorrow >= 0 && c.feast_tomorrow < static_cast<int>(culture::feasts.size())) {
        const auto& feast = culture::feasts[static_cast<std::size_t>(c.feast_tomorrow)];
        const std::string topic = std::string("ron_feast_") + feast.name;
        if (!said(topic))
            return Call{ron, topic, {
                std::string("Tomorrow's ") + feast.name + "! " + feast.what + ".",
                "The whole island stops for it - even the gangs keep the peace. Come to the church with me; your aunt used to bake for it."},
                c.orthodox_church, "the Orthodox church"};
    }
    if (c.sunday && c.hour >= 6 && c.hour < 10 && !c.cop && fresh("dario_chapel"))
        return Call{dario, "dario_chapel", {
            "Nephew. Chapel at half nine, on the west boulevard. The boys should see your face.",
            "Wear something decent. And leave the gun in the car - it's Sunday."}, c.chapel, "the Baptist chapel"};
    if ((c.saturday && c.hour >= 17) || (c.sunday && c.hour < 9))
        if (fresh("ron_sunday"))
            return Call{ron, "ron_sunday", {
                std::string(c.sunday ? "It's Sunday, nevo! " : "Tomorrow's Sunday. ") + "Come to church with me - the Orthodox one in the east suburbs. I go for the singing.",
                "Dario takes his boys to the Baptist chapel on the boulevard. Same saints, different hymnbook. Don't tell him I said that."},
                c.orthodox_church, "the Orthodox church"};
    if (c.race_wins > 0 && !c.cop && !c.green_hand && fresh("dario_race"))
        return Call{dario, "dario_race", {
            "I hear you took Fulmo's pot on the ring road. Everybody in the bar heard it too.",
            "A man who drives like that, I can use. Come and see me - Ron doesn't need to know."}, c.hideout, "La Verda Mano's bar"};
    if (c.owns_car && c.races == 0 && c.hour >= 18 && fresh("ron_race"))
        return Call{ron, "ron_race", {
            "Your aunt would kill me for telling you. The young ones race on the ring road at night, out by the Marnafto station.",
            "Fulmo runs it - a thousand kronoj to get in, winner takes the pot. If you're going to drive like a fool anyway, get paid for it. And not a word to Dario."},
            c.race_meet, "the race meet"};
    if (c.green_hand && fresh("dario_family"))
        return Call{dario, "dario_family", {
            "Welcome to the family, properly this time. The boys like you.",
            "Sunday, chapel, half nine. They'll want to shake your hand on the steps."}, c.chapel, "the Baptist chapel"};
    if (c.played > 1800 && fresh("ron_soup"))
        return Call{ron, "ron_soup", {
            "Did you eat today? You sound thin. You can sound thin, I know these things.",
            "Come by the house. There's soup, and the radio's on."}, c.ron_home, "Uncle Ron's house"};
    return std::nullopt;
}

void Phone::update(float dt, const PhoneContext& c) {
    if (!(dt > 0)) return;
    if (call_) {
        if (ring_ > 0) { ring_ -= dt; return; }
        on_line_ += dt;
        const auto& lines = call_->lines;
        const float hold = line_seconds + static_cast<float>(lines[static_cast<std::size_t>(line_)].size()) / 40.0F;
        if (on_line_ >= hold) next();
        return;
    }
    if (c.busy) return;
    wait_ -= dt;
    if (wait_ > 0) return;
    if (auto chosen = choose(c)) {
        call_ = std::move(chosen);
        ring_ = ring_seconds;
        line_ = 0;
        on_line_ = 0;
        rang_ = true;
        seed_ = seed_ * 1664525U + 1013904223U;
        wait_ = gap + static_cast<float>(seed_ >> 24) * .7F;
    } else {
        wait_ = 30;   // nothing to say yet: look again in a while
    }
}

bool Phone::next() {
    if (!call_) return false;
    if (ring_ > 0) { ring_ = 0; on_line_ = 0; return true; }   // answered
    ++line_;
    on_line_ = 0;
    if (line_ >= static_cast<int>(call_->lines.size())) hang_up();
    return true;
}

void Phone::hang_up() {
    if (!call_) return;
    if (!said(call_->topic)) done_.push_back(call_->topic);
    ended_ = std::move(call_);
    call_.reset();
}

std::string Phone::line() const {
    if (!call_ || ring_ > 0 || line_ >= static_cast<int>(call_->lines.size())) return "";
    return call_->caller + " (phone): \"" + call_->lines[static_cast<std::size_t>(line_)] + "\"";
}

} // namespace outland::game::campaign
