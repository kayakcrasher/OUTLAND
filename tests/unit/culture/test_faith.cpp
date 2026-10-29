// Verda's churches: the feasts both keep, and what a church's door says.
#include "outland/game/culture/Faith.hpp"
#include <iostream>
#include <string>

using namespace outland::game::culture;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
} // namespace

int main() {
    // A feast every few days, in turn, the same for both churches; round the year again.
    check(feast_on(0) == 0 && feast_on(1) == -1 && feast_on(feast_days) == 1, "a feast every few days, in turn");
    check(feast_on(feast_days * static_cast<int>(feasts.size())) == 0, "round the year again");
    int in = 0;
    check(next_feast(1, in) == 1 && in == feast_days - 1, "the next feast and how long to wait");
    check(next_feast(feast_days, in) == 2 && in == feast_days, "the day of a feast: the one after it");
    bool zamenhof = false;
    for (const auto& f : feasts) zamenhof |= std::string(f.name) == "Zamenhof-Tago";
    check(zamenhof, "both churches keep Zamenhof Day");

    Church chapel;
    chapel.denomination = Denomination::Baptist;
    chapel.saint = "PETRO";
    chapel.green_hand = true;
    const auto said = church_notice(chapel, 1);
    check(said.find("BAPTISTA PREĜEJO DE SANKTA PETRO") == 0, "the chapel's name and its saint");
    check(said.find("Esperanto King James") != std::string::npos && said.find("saints") != std::string::npos, "Verdan Baptists: the saints, and the Esperanto King James");
    check(said.find("La Verda Mano") != std::string::npos, "La Verda Mano's chapel says so");
    check(said.find("Next feast: Teofanio in 3 days") != std::string::npos, "the next feast: " + said);
    Church church;
    church.denomination = Denomination::Orthodox;
    church.saint = "NIKOLAO";
    const auto feast = church_notice(church, 2 * feast_days);
    check(feast.find("ORTODOKSA PREĜEJO DE SANKTA NIKOLAO") == 0 && feast.find("Today: Pasko") != std::string::npos && feast.find("La Verda Mano") == std::string::npos,
          "an Orthodox church on Easter: " + feast);

    if (failures == 0) std::cout << "faith ok\n";
    return failures == 0 ? 0 : 1;
}
