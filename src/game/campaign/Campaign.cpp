#include "outland/game/campaign/Campaign.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace outland::game::campaign {
namespace {
const char* stage_name(Stage s) {
    switch (s) {
        case Stage::Arrival: return "arrival";
        case Stage::PortTalk: return "port";
        case Stage::ToTruck: return "truck";
        case Stage::Ride: return "ride";
        case Stage::HomeTalk: return "home";
        case Stage::Free: return "free";
    }
    return "arrival";
}
}

std::string serialise(const CampaignState& s) {
    std::ostringstream out;
    out.precision(9);
    out << "stage=" << stage_name(s.stage) << "\n";
    if (s.has_position) out << "position=" << s.position.x << "," << s.position.y << "," << s.position.z << "," << s.yaw << "\n";
    out << "played=" << static_cast<long long>(s.played) << "\n";
    out << "saved=" << static_cast<long long>(s.saved_at) << "\n";
    if (!s.place.empty()) out << "place=" << s.place << "\n";
    if (!s.ron_truck.empty()) out << "ron_truck=" << s.ron_truck << "\n";
    out << "heard=" << s.heard_trucker << s.heard_cop << s.heard_gang << "\n";
    out << "ron=" << s.picked_up << s.ultimatum << s.kicked_out << "," << static_cast<long long>(s.ultimatum_left) << "\n";
    return out.str();
}

bool parse(const std::string& text, CampaignState& s) {
    s = {};
    std::istringstream in(text);
    std::string line;
    bool staged = false;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "stage") {
            for (const auto st : {Stage::Arrival, Stage::PortTalk, Stage::ToTruck, Stage::Ride, Stage::HomeTalk, Stage::Free})
                if (value == stage_name(st)) { s.stage = st; staged = true; }
        } else if (key == "position") {
            float x = 0, y = 0, z = 0, yaw = 0;
            if (std::sscanf(value.c_str(), "%f,%f,%f,%f", &x, &y, &z, &yaw) == 4 && std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(yaw)) {
                s.has_position = true;
                s.position = {x, y, z};
                s.yaw = yaw;
            }
        } else if (key == "played") s.played = std::max(0.0, std::atof(value.c_str()));
        else if (key == "saved") s.saved_at = std::max<long long>(0, std::atoll(value.c_str()));
        else if (key == "place") s.place = value;
        else if (key == "ron_truck") s.ron_truck = value;
        else if (key == "heard" && value.size() >= 3) {
            s.heard_trucker = value[0] == '1'; s.heard_cop = value[1] == '1'; s.heard_gang = value[2] == '1';
        } else if (key == "ron" && value.size() >= 3) {
            s.picked_up = value[0] == '1'; s.ultimatum = value[1] == '1'; s.kicked_out = value[2] == '1';
            if (const auto comma = value.find(','); comma != std::string::npos)
                s.ultimatum_left = static_cast<float>(std::clamp(std::atof(value.c_str() + comma + 1), 0.0, 3600.0));
        }
    }
    return staged;
}

SlotInfo describe(int slot, const std::string& campaign_text, bool any_save) {
    SlotInfo info;
    info.title = "SLOT " + std::to_string(slot);
    CampaignState state;
    const bool known = !campaign_text.empty() && parse(campaign_text, state);
    info.used = known || any_save;
    if (!info.used) { info.detail = "New game - start the story"; return info; }
    if (!state.place.empty()) info.title += "  ·  " + state.place;
    std::string detail;
    if (known) {
        const int minutes = static_cast<int>(state.played / 60);
        const char* played = " min played";
        detail = state.stage != Stage::Free ? "Just home  ·  " : "";
        detail += minutes >= 60 ? std::to_string(minutes / 60) + " h " + (minutes % 60 < 10 ? "0" : "") + std::to_string(minutes % 60) + played
                                : std::to_string(minutes) + played;
        if (state.saved_at > 0) {
            const std::time_t t = static_cast<std::time_t>(state.saved_at);
            char stamp[32]{};
            if (const std::tm* local = std::localtime(&t)) std::strftime(stamp, sizeof stamp, "%d %b %H:%M", local);
            if (stamp[0]) detail += std::string("  ·  saved ") + stamp;
        }
    } else detail = "Saved game";
    info.detail = detail;
    return info;
}

// ---------------------------------------------------------------------------------- the story
const std::vector<Line>& Story::port_lines() {
    static const std::vector<Line> lines{
        {uncle, "There he is! Nevo! Welcome home to Verda! Come here - let me look at you."},
        {uncle, "All those years in Eastern Europe... Saluton, nevo! You still remember Esperanto? Ahaha! Don't worry, it comes back. "
                "Everything here is in Esperanto - the signs, the shops, the radio."},
        {uncle, "This is Porto Luma, the port. The ships come in here - containers, and the ranchers' cattle going abroad. "
                "Honest work, if you want it: Verda Transporto in Verda City always needs drivers. Tell Tomaso I sent you."},
        {uncle, "Verda's not big. Verda City in the middle - the capital, Parliament, the shops. Espera up north with the farms, "
                "Suda Haveno on the south coast, Roka out west with the factories, and the ring road round the whole island."},
        {uncle, "The ranchers - the Vakeroj - keep to themselves. Don't touch their cattle, and they won't touch you. "
                "The Policio are hiring too, if you want a uniform. They take good people."},
        {uncle, "Lost? Open the map (M). Money buys food, clothes, a car's worth of trouble... ahaha! "
                "USE talks to people and opens doors - you'll get the hang of it."},
        {uncle, "Come on, my truck's right here. Let's get you home to Verda City."},
    };
    return lines;
}

const std::vector<Line>& Story::home_lines() {
    static const std::vector<Line> lines{
        {uncle, "Here we are - Okcidenta Verda, the west side of the city. Not fancy, but it's ours, every last krono."},
        {uncle, "Listen, nevo. I'll tell you straight, before somebody else does. My brother Dario - your Uncle Dario - is the head "
                "of the Green Hand. La Verda Mano. He runs the whole thing now, from that bar downtown. Your cousins run with him."},
        {uncle, "I'm not proud of it. Our father worked the docks forty years and never took what wasn't his. The Green Hand "
                "say 'gangsters only, never the people'. Maybe. I've still been to too many funerals."},
        {uncle, "I won't tell you how to live. Dario will want you - family, he'll say. A flat by the bar, money, respect. Their bar is "
                "downtown, on the corner by Parliament. Just... think about it, eh? There are other roads."},
        {uncle, "This house is your home now, as long as you want it. Your room's made up. Keep your things here, sleep here - "
                "and if you ever get hurt, this is where you'll wake up."},
        {uncle, "Bonvenon hejmen, nevo. Welcome home. Now go and see your island."},
    };
    return lines;
}

const std::vector<Line>& Story::idle_lines() {
    static const std::vector<Line> lines{
        {uncle, "Need anything, nevo? There's soup on the stove."},
        {uncle, "Verda Transporto pays fair. Tell Tomaso I sent you."},
        {uncle, "Stay away from that corner bar downtown. Please. For your mother's sake."},
        {uncle, "Saluton! Did you see the ship come in this morning? Big one."},
        {uncle, "If you join the Policio, at least I'll sleep at night. Ahaha!"},
        {uncle, "Dario called. He asked about you. I told him you're tired."},
        {uncle, "You still remember Esperanto? 'Ĝis revido' - see you later! Ahaha!"},
    };
    return lines;
}

const std::vector<Line>& Story::idle_lines(Path path, bool kicked_out) {
    static const std::vector<Line> trucker{
        {uncle, "There's my trucker! Verda Transporto - honest work, honest money. Your grandfather would be proud."},
        {uncle, "How's the road, nevo? Mind the ring road in the rain - those big rigs slide."},
        {uncle, "Tomaso says you're a good driver. I told him: it runs in the family. Ahaha!"},
        {uncle, "Soup on the stove, and a cold one in the fridge for my working man."},
    };
    static const std::vector<Line> cop{
        {uncle, "Officer! Ahaha! I still can't believe it. My nephew, in the Policio!"},
        {uncle, "I told the whole street. Mrs Kovač cried. I nearly did too."},
        {uncle, "Be careful out there, eh? Come home safe. That's all I ask."},
        {uncle, "A cop in the family. Maybe Verda has a chance after all."},
    };
    static const std::vector<Line> gang{
        {uncle, "...You're home. Hm."},
        {uncle, "I hear things, nevo. People talk. I hope they're wrong about you."},
        {uncle, "Dario got to you, didn't he. Just like he got to everyone else."},
        {uncle, "There's soup. I'm not hungry."},
    };
    static const std::vector<Line> out{
        {uncle, "You made your choice, nevo. This isn't your home any more."},
        {uncle, "When you're done with them - really done - the door is open. Not before."},
    };
    if (kicked_out) return out;
    switch (path) {
        case Path::Trucker: return trucker;
        case Path::Cop: return cop;
        case Path::Gang: return gang;
        default: return idle_lines();
    }
}

const std::vector<Line>& Story::reaction_lines(Path path) {
    static const std::vector<Line> trucker{
        {uncle, "Verda Transporto? You took a job with Tomaso? Ahaha! Come here!"},
        {uncle, "Honest work, nevo. Real work. Loads off the ships, cattle to the port - that's how this island eats."},
        {uncle, "I'm happy for you. Truly. Now go and earn your keep!"},
    };
    static const std::vector<Line> cop{
        {uncle, "The Policio?! YOU? A badge? Ahaha! Let me look at you - the uniform!"},
        {uncle, "My nephew, an officer of the Policio de Verda. Wait till I tell the whole street. Wait till I tell your mother!"},
        {uncle, "Do you know what this means to me? After everything with Dario... one of us on the right side."},
        {uncle, "I'm so proud, nevo. So proud. Be careful. And go and clean up this island."},
    };
    static const std::vector<Line> gang{
        {uncle, "So it's true. You went to Dario. The Green Hand."},
        {uncle, "I asked you one thing, nevo. One thing. Think about it, I said. There are other roads."},
        {uncle, "I've buried friends because of men like them. And now my own blood wears their colours."},
        {uncle, "You can still sleep here. For now. But I won't pretend I'm not ashamed."},
    };
    static const std::vector<Line> none{};
    switch (path) {
        case Path::Trucker: return trucker;
        case Path::Cop: return cop;
        case Path::Gang: return gang;
        default: return none;
    }
}

const std::vector<Line>& Story::pickup_lines(bool gang) {
    static const std::vector<Line> with_gang{
        {uncle, "Get in the truck. ...No. First, listen to me."},
        {uncle, "I drove past those walls every week for twenty years thinking: never anyone of mine. And here I am."},
        {uncle, "This is the only time I come, nevo. The only time. Next time you walk home."},
        {uncle, "And this is how it is: you leave the Green Hand, or you leave my house. I won't have it under my roof."},
        {uncle, "You've got a little time to choose. Choose right. Now get in."},
    };
    static const std::vector<Line> without{
        {uncle, "Get in the truck. ...You look terrible."},
        {uncle, "I don't know what you did and I don't want to know. But this is the only time I come for you, nevo."},
        {uncle, "Next time, you walk home. Now get in. There's soup."},
    };
    return gang ? with_gang : without;
}

const std::vector<Line>& Story::kicked_out_lines() {
    static const std::vector<Line> lines{
        {uncle, "I warned you. You're still with them."},
        {uncle, "Get your things. This isn't your home any more. Go to Dario - he'll find you a bed."},
        {uncle, "If you ever leave them for good, come back. Until then... don't."},
    };
    return lines;
}

const std::vector<Line>& Story::welcome_back_lines() {
    static const std::vector<Line> lines{
        {uncle, "You're out? Really out? ...Come here, nevo."},
        {uncle, "Your room's as you left it. I couldn't bring myself to touch it."},
        {uncle, "Welcome home. Again. Ahaha! Let's not do that a third time, eh?"},
    };
    return lines;
}

bool Story::say(const std::vector<Line>& lines, bool then_drive_home) {
    if (lines.empty() || line_ >= 0 || stage_ != Stage::Free) return false;
    lines_ = lines;
    special_ = 0;
    then_home_ = then_drive_home;
    return true;
}

void Story::drive_home() {
    homeward_ = true;
    ride_ = 0;
}

void Story::start(Stage stage) {
    // A talk or the drive that was cut off by a quit starts again at its beginning.
    if (stage == Stage::PortTalk || stage == Stage::ToTruck) stage = Stage::Arrival;
    if (stage == Stage::Ride) stage = Stage::HomeTalk;
    stage_ = stage;
    line_ = stage == Stage::HomeTalk ? 0 : -1;
    ride_ = 0;
    special_ = -1;
    lines_.clear();
    then_home_ = homeward_ = false;
    teleport_ = home_given_ = changed_ = false;
}

int Story::lines() const {
    if (special_ >= 0) return static_cast<int>(lines_.size());
    switch (stage_) {
        case Stage::PortTalk: return static_cast<int>(port_lines().size());
        case Stage::HomeTalk: return static_cast<int>(home_lines().size());
        case Stage::Free: return 1;
        default: return 0;
    }
}

const Line* Story::line() const {
    if (special_ >= 0 && special_ < static_cast<int>(lines_.size())) return &lines_[static_cast<std::size_t>(special_)];
    if (line_ < 0) return nullptr;
    switch (stage_) {
        case Stage::PortTalk: return &port_lines()[static_cast<std::size_t>(line_)];
        case Stage::HomeTalk: return &home_lines()[static_cast<std::size_t>(line_)];
        case Stage::Free: {
            const auto& words = idle_lines(path_, kicked_out_);
            return &words[static_cast<std::size_t>(line_) % words.size()];
        }
        default: return nullptr;
    }
}

bool Story::talk() {
    if (special_ >= 0) {
        if (++special_ >= static_cast<int>(lines_.size())) {
            special_ = -1;
            lines_.clear();
            if (then_home_) { then_home_ = false; drive_home(); }
        }
        return true;
    }
    switch (stage_) {
        case Stage::Arrival:
            set(Stage::PortTalk);
            line_ = 0;
            return true;
        case Stage::PortTalk:
            if (++line_ >= static_cast<int>(port_lines().size())) { line_ = -1; set(Stage::ToTruck); }
            return true;
        case Stage::HomeTalk:
            if (++line_ >= static_cast<int>(home_lines().size())) { line_ = -1; set(Stage::Free); home_given_ = true; }
            return true;
        case Stage::Free:
            if (line_ >= 0) { line_ = -1; return true; }
            line_ = idle_++ % static_cast<int>(idle_lines(path_, kicked_out_).size());
            return true;
        default:
            return false;
    }
}

void Story::hush() {
    if (stage_ != Stage::Free) return;
    special_ = -1;
    lines_.clear();
    then_home_ = false;
    line_ = -1;
}

bool Story::board() {
    if (stage_ != Stage::ToTruck) return false;
    set(Stage::Ride);
    ride_ = 0;
    return true;
}

void Story::update(float dt) {
    if (homeward_ && dt > 0) {
        const float before = ride_;
        ride_ += std::min(dt, .25F);
        if (before < fade_in && ride_ >= fade_in) teleport_ = true;
        if (ride_ >= fade_in + hold + fade_out) homeward_ = false;
        return;
    }
    if (stage_ != Stage::Ride || !(dt > 0)) return;
    const float before = ride_;
    ride_ += std::min(dt, .25F);
    if (before < fade_in && ride_ >= fade_in) teleport_ = true;
    if (ride_ >= fade_in + hold + fade_out) {
        set(Stage::HomeTalk);
        line_ = 0;
    }
}

float Story::fade() const {
    if (stage_ != Stage::Ride && !homeward_) return 0;
    if (ride_ < fade_in) return ride_ / fade_in;
    if (ride_ < fade_in + hold) return 1;
    return std::max(0.0F, 1 - (ride_ - fade_in - hold) / fade_out);
}

const char* Story::caption() const {
    return homeward_ ? "Uncle Ron drives you home. Neither of you says much..." :
           stage_ == Stage::Ride ? "Uncle Ron drives you home to Verda City..." : "";
}

std::string Story::objective() const {
    switch (stage_) {
        case Stage::Arrival: return "Find Uncle Ron on the quay";
        case Stage::ToTruck: return "Get in Uncle Ron's truck";
        default: return "";
    }
}

} // namespace outland::game::campaign
