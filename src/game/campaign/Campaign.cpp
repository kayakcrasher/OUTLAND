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
    }
    return staged;
}

SlotInfo describe(int slot, const std::string& campaign_text, bool any_save, bool esperanto) {
    SlotInfo info;
    info.title = (esperanto ? "INGO " : "SLOT ") + std::to_string(slot);
    CampaignState state;
    const bool known = !campaign_text.empty() && parse(campaign_text, state);
    info.used = known || any_save;
    if (!info.used) { info.detail = esperanto ? "Nova ludo - komencu la rakonton" : "New game - start the story"; return info; }
    if (!state.place.empty()) info.title += "  ·  " + state.place;
    std::string detail;
    if (known) {
        const int minutes = static_cast<int>(state.played / 60);
        const char* played = esperanto ? " min ludita" : " min played";
        detail = state.stage != Stage::Free ? (esperanto ? "Ĵus hejme  ·  " : "Just home  ·  ") : "";
        detail += minutes >= 60 ? std::to_string(minutes / 60) + " h " + (minutes % 60 < 10 ? "0" : "") + std::to_string(minutes % 60) + played
                                : std::to_string(minutes) + played;
        if (state.saved_at > 0) {
            const std::time_t t = static_cast<std::time_t>(state.saved_at);
            char stamp[32]{};
            if (const std::tm* local = std::localtime(&t)) std::strftime(stamp, sizeof stamp, "%d %b %H:%M", local);
            if (stamp[0]) detail += std::string(esperanto ? "  ·  konservita " : "  ·  saved ") + stamp;
        }
    } else detail = esperanto ? "Konservita ludo" : "Saved game";
    info.detail = detail;
    return info;
}

// ---------------------------------------------------------------------------------- the story
namespace {
constexpr const char* uncle_eo = "Onklo Ron";
}

const std::vector<Line>& Story::port_lines(bool esperanto) {
    static const std::vector<Line> eo{
        {uncle_eo, "Jen li! Nevo! Bonvenon hejmen al Verda! Venu ĉi tien - lasu min rigardi vin."},
        {uncle_eo, "Ĉiuj tiuj jaroj en Orienta Eŭropo... Saluton, nevo! Ĉu vi ankoraŭ memoras Esperanton? Ahaha! Ne zorgu, ĝi revenas. "
                   "Ĉio ĉi tie estas en Esperanto - la ŝildoj, la butikoj, la radio."},
        {uncle_eo, "Jen Porto Luma, la haveno. La ŝipoj alvenas ĉi tie - konteneroj, kaj la brutaro de la rančoj, kiu iras eksterlanden. "
                   "Honesta laboro, se vi volas ĝin: Verda Transporto en Verda Urbo ĉiam bezonas ŝoforojn. Diru al Tomaso, ke mi sendis vin."},
        {uncle_eo, "Verda ne estas granda. Verda Urbo en la mezo - la ĉefurbo, la Parlamento, la butikoj. Espera norde kun la bienoj, "
                   "Suda Haveno ĉe la suda marbordo, Roka okcidente kun la fabrikoj, kaj la ringvojo ĉirkaŭ la tuta insulo."},
        {uncle_eo, "La rančistoj - la Vakeroj - restas inter si. Ne tuŝu ilian brutaron, kaj ili ne tuŝos vin. "
                   "Ankaŭ la Policio dungas, se vi volas uniformon. Ili prenas bonajn homojn."},
        {uncle_eo, "Ĉu perdita? Malfermu la mapon (M). Mono aĉetas manĝaĵon, vestojn, aŭton plenan de problemoj... ahaha! "
                   "UZI parolas kun homoj kaj malfermas pordojn - vi kutimiĝos."},
        {uncle_eo, "Venu, mia kamioneto estas ĝuste ĉi tie. Ni iru hejmen al Verda Urbo."},
    };
    if (esperanto) return eo;
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

const std::vector<Line>& Story::home_lines(bool esperanto) {
    static const std::vector<Line> eo{
        {uncle_eo, "Jen ni estas - Okcidenta Verda, la okcidenta flanko de la urbo. Ne luksa, sed ĝi estas nia, ĝis la lasta krono."},
        {uncle_eo, "Aŭskultu, nevo. Mi diros al vi rekte, antaŭ ol iu alia faros. Mia frato Dario - via Onklo Dario - estas la estro "
                   "de La Verda Mano. Li nun gvidas ĉion, el tiu drinkejo en la centro. Viaj kuzoj estas kun li."},
        {uncle_eo, "Mi ne fieras pri tio. Nia patro laboris kvardek jarojn ĉe la havenoj kaj neniam prenis tion, kio ne estis lia. "
                   "La Verda Mano diras 'nur gangsteroj, neniam la popolo'. Eble. Tamen mi ĉeestis tro multajn entombigojn."},
        {uncle_eo, "Mi ne diros al vi kiel vivi. Dario volos vin - familio, li diros. Apartamenton apud la drinkejo, monon, respekton. "
                   "Ilia drinkejo estas en la centro, ĉe la angulo apud la Parlamento. Nur... pripensu, ĉu ne? Ekzistas aliaj vojoj."},
        {uncle_eo, "Ĉi tiu domo nun estas via hejmo, tiom longe kiom vi volas. Via ĉambro estas preta. Tenu viajn aĵojn ĉi tie, "
                   "dormu ĉi tie - kaj se vi iam vundiĝos, ĉi tie vi vekiĝos."},
        {uncle_eo, "Bonvenon hejmen, nevo. Nun iru kaj vidu vian insulon."},
    };
    if (esperanto) return eo;
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

const std::vector<Line>& Story::idle_lines(bool esperanto) {
    static const std::vector<Line> eo{
        {uncle_eo, "Ĉu vi bezonas ion, nevo? Estas supo sur la forno."},
        {uncle_eo, "Verda Transporto pagas juste. Diru al Tomaso, ke mi sendis vin."},
        {uncle_eo, "Restu for de tiu angula drinkejo en la centro. Mi petas. Pro via patrino."},
        {uncle_eo, "Saluton! Ĉu vi vidis la ŝipon alveni ĉi-matene? Granda."},
        {uncle_eo, "Se vi aliĝos al la Policio, almenaŭ mi dormos nokte. Ahaha!"},
        {uncle_eo, "Dario telefonis. Li demandis pri vi. Mi diris al li, ke vi estas laca."},
        {uncle_eo, "Ĝis revido, nevo! Ahaha!"},
    };
    if (esperanto) return eo;
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

void Story::start(Stage stage) {
    // A talk or the drive that was cut off by a quit starts again at its beginning.
    if (stage == Stage::PortTalk || stage == Stage::ToTruck) stage = Stage::Arrival;
    if (stage == Stage::Ride) stage = Stage::HomeTalk;
    stage_ = stage;
    line_ = stage == Stage::HomeTalk ? 0 : -1;
    ride_ = 0;
    teleport_ = home_given_ = changed_ = false;
}

int Story::lines() const {
    switch (stage_) {
        case Stage::PortTalk: return static_cast<int>(port_lines().size());
        case Stage::HomeTalk: return static_cast<int>(home_lines().size());
        case Stage::Free: return 1;
        default: return 0;
    }
}

const Line* Story::line() const {
    if (line_ < 0) return nullptr;
    switch (stage_) {
        case Stage::PortTalk: return &port_lines(esperanto_)[static_cast<std::size_t>(line_)];
        case Stage::HomeTalk: return &home_lines(esperanto_)[static_cast<std::size_t>(line_)];
        case Stage::Free: return &idle_lines(esperanto_)[static_cast<std::size_t>(line_) % idle_lines(esperanto_).size()];
        default: return nullptr;
    }
}

bool Story::talk() {
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
            line_ = idle_++ % static_cast<int>(idle_lines().size());
            return true;
        default:
            return false;
    }
}

bool Story::board() {
    if (stage_ != Stage::ToTruck) return false;
    set(Stage::Ride);
    ride_ = 0;
    return true;
}

void Story::update(float dt) {
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
    if (stage_ != Stage::Ride) return 0;
    if (ride_ < fade_in) return ride_ / fade_in;
    if (ride_ < fade_in + hold) return 1;
    return std::max(0.0F, 1 - (ride_ - fade_in - hold) / fade_out);
}

const char* Story::caption() const {
    if (stage_ != Stage::Ride) return "";
    return esperanto_ ? "Onklo Ron veturigas vin hejmen al Verda Urbo..." : "Uncle Ron drives you home to Verda City...";
}

std::string Story::objective() const {
    switch (stage_) {
        case Stage::Arrival: return esperanto_ ? "Trovu Onklon Ron sur la kajo" : "Find Uncle Ron on the quay";
        case Stage::ToTruck: return esperanto_ ? "Eniru la kamioneton de Onklo Ron" : "Get in Uncle Ron's truck";
        default: return "";
    }
}

} // namespace outland::game::campaign
