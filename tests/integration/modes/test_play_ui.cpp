#include "outland/game/PlayUI.hpp"
#include "outland/game/HomeScreen.hpp"
#include <cassert>
#include <iostream>
#include <vector>
namespace {
struct Touch { int id; Vector2 point; };std::vector<Touch> touches;
Vector2 mouse{};bool pressed=false;int key=0;
Vector2 center(Rectangle r) {return {r.x+r.width/2,r.y+r.height/2};}
}
extern "C" {
int __wrap_GetTouchPointCount(){return static_cast<int>(touches.size());}
int __wrap_GetTouchPointId(int i){return touches.at(i).id;}
Vector2 __wrap_GetTouchPosition(int i){return touches.at(i).point;}
Vector2 __wrap_GetMousePosition(){return mouse;}
bool __wrap_IsMouseButtonPressed(int b){return b==MOUSE_BUTTON_LEFT && pressed;}
bool __wrap_IsKeyPressed(int k){return k!=0 && k==key;}
}
int main() {
    using namespace outland::game;
    for(const auto size:{Vector2{300,600},Vector2{590,1280},Vector2{1280,400},Vector2{1920,1080}}) {
        const int w=static_cast<int>(size.x),h=static_cast<int>(size.y);
        auto inside=[&](Rectangle r){assert(r.x>=0 && r.y>=0 && r.x+r.width<=w && r.y+r.height<=h);};
        for(int i=0;i<4;++i) inside(HomeScreen::mode_button(i,w,h));
        inside(HomeScreen::difficulty_button(w,h));
        for(int i=0;i<HomeScreen::slot_count;++i) {inside(HomeScreen::slot_button(i,w,h));inside(HomeScreen::erase_button(i,w,h));}
        inside(HomeScreen::back_button(w,h));
        for(int i=0;i<2;++i) inside(PlayUI::result_button(i,w,h));
        for(int i=0;i<6;++i) {inside(PlayUI::shop_button(i,w,h));inside(PlayUI::shop_row(i,w,h));}
    }
    HomeScreen home;touches={{10,center(HomeScreen::difficulty_button(590,720))}};
    assert(home.update(590,720)==GameMode::Home && home.bot_level()==ai::BotLevel::Hard);
    assert(home.update(590,720)==GameMode::Home && home.bot_level()==ai::BotLevel::Hard);
    touches.clear();home.update(590,720);
    touches={{11,center(HomeScreen::mode_button(0,590,720))}};assert(home.update(590,720)==GameMode::BattleRoyale);
    touches.clear();home.update(590,720);
    touches={{13,center(HomeScreen::mode_button(3,590,720))}};
#ifdef OUTLAND_DEV_TOOLS
    assert(home.update(590,720)==GameMode::DevLab);
#else
    assert(home.update(590,720)==GameMode::Home); // production menu excludes DEV
#endif
    PlayUI ui;ui.opened();touches={{91,center(PlayUI::result_button(0,590,720))}};
    assert(ui.result_input(590,720,true)==ResultAction::None);
    assert(ui.result_input(590,720,true)==ResultAction::None);
    touches.clear();ui.result_input(590,720,true);
    touches={{91,center(PlayUI::result_button(0,590,720))}};
    assert(ui.result_input(590,720,true)==ResultAction::Restart);
    assert(ui.result_input(590,720,true)==ResultAction::None);
    touches.push_back({5,center(PlayUI::result_button(1,590,720))});
    assert(ui.result_input(590,720,true)==ResultAction::Home);
    std::swap(touches[0],touches[1]);assert(ui.result_input(590,720,true)==ResultAction::None);
    touches.clear();ui.result_input(590,720,true);
    pressed=true;mouse=center(PlayUI::result_button(0,590,720));assert(ui.result_input(590,720,true)==ResultAction::Restart);
    pressed=false;key=KEY_H;assert(ui.result_input(590,720,true)==ResultAction::Home);key=0;
    // Stores cannot retrigger trades by holding, dragging, touch-index changes or focus regain.
    inventory::ItemRegistry items;economy::Economy economy;std::string error;const std::string root=OUTLAND_SOURCE_DIR;
    assert(items.load(root+"/assets/verda/survival/gameplay/items.tsv",root+"/assets/verda/survival/gameplay/loot_tables.tsv",error));
    assert(economy.load(items,root+"/assets/verda/economy/offers.tsv",error));
    economy.build({{"test_shop","test_shop",life::PlaceKind::Shop}});const auto shop=economy.shops().front();
    ui.opened();touches={{12,center(PlayUI::shop_button(1,590,720))}};
    assert(!ui.shop_input(economy,shop,590,720,true).buy);assert(!ui.shop_input(economy,shop,590,720,true).buy);
    touches.clear();ui.shop_input(economy,shop,590,720,true);
    touches={{13,center(PlayUI::shop_button(1,590,720))}};assert(ui.shop_input(economy,shop,590,720,true).buy);
    touches[0].point=center(PlayUI::shop_button(2,590,720));assert(!ui.shop_input(economy,shop,590,720,true).sell);
    touches.clear();ui.shop_input(economy,shop,590,720,true);
    touches={{14,center(PlayUI::shop_button(2,590,720))}};
    assert(!ui.shop_input(economy,shop,590,720,false).sell && !ui.shop_input(economy,shop,590,720,true).sell);
    touches.clear();ui.shop_input(economy,shop,590,720,true);
    key=KEY_ENTER;assert(ui.shop_input(economy,shop,590,720,true).buy);key=KEY_DELETE;assert(ui.shop_input(economy,shop,590,720,true).sell);
    key=KEY_I;assert(ui.shop_input(economy,shop,590,720,true).close);
    std::cout<<"Mode/store UI: responsive layouts, native touch IDs, no held/reordered/focus replay, difficulty, mouse and restart/home passed\n";
}
