#include "outland/game/HomeScreen.hpp"
#include "outland/input/PointerEvents.hpp"
#include <algorithm>
#include <array>
namespace outland::game {
namespace {
#ifdef OUTLAND_DEV_TOOLS
constexpr std::array<GameMode,4> modes{GameMode::BattleRoyale,GameMode::ZombieSurvival,GameMode::Explore,GameMode::DevLab};
constexpr const char* labels[]{"BATTLE ROYALE","ZOMBIE SURVIVAL","EXPLORE VERDA","CREATIVE BUILDER (DEV)"};
#else
constexpr std::array<GameMode,3> modes{GameMode::BattleRoyale,GameMode::ZombieSurvival,GameMode::Explore};
constexpr const char* labels[]{"BATTLE ROYALE","ZOMBIE SURVIVAL","EXPLORE VERDA"};
#endif
float scale(int w,int h) { return std::max(.1F,std::min(w/900.0F,h/720.0F)); }
Rectangle box(float x,float y,float w,float h,int width,int height) {
    const float s=scale(width,height);return {(width-900*s)/2+x*s,(height-720*s)/2+y*s,w*s,h*s};
}
void button(Rectangle r,const char* text,float s) {
    const bool hovered=CheckCollisionPointRec(GetMousePosition(),r);
    DrawRectangleRounded(r,.15F,8,hovered ? Color{80,100,72,240} : Color{38,45,38,230});
    DrawRectangleRoundedLines(r,.15F,8,WHITE);
    const int font=std::max(10,static_cast<int>(22*s));
    DrawText(text,static_cast<int>(r.x+r.width/2-MeasureText(text,font)/2),static_cast<int>(r.y+(r.height-font)/2),font,RAYWHITE);
}
}
Rectangle HomeScreen::mode_button(int index,int w,int h) { return box(240,290+index*72,420,58,w,h); }
Rectangle HomeScreen::difficulty_button(int w,int h) { return box(300,225,300,46,w,h); }
GameMode HomeScreen::update(int w,int h) {
    std::vector<Vector2> points;
    const auto buffered=input::PointerEvents::presses();
    points.assign(buffered.begin(),buffered.end());std::unordered_set<int> current;
    const int count=std::max(0,GetTouchPointCount());
    for(int i=0;i<count;++i) {
        const int id=GetTouchPointId(i);
        if(id>=0 && current.insert(id).second && buffered.empty() && !previous_.contains(id)) points.push_back(GetTouchPosition(i));
    }
    if(buffered.empty() && count==0 && previous_.empty() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) points.push_back(GetMousePosition());
    previous_=std::move(current);
    for(const auto p:points) {
        if(CheckCollisionPointRec(p,difficulty_button(w,h))) {
            bot_level_=bot_level_==ai::BotLevel::Easy ? ai::BotLevel::Medium : bot_level_==ai::BotLevel::Medium ? ai::BotLevel::Hard : ai::BotLevel::Easy;
            return GameMode::Home;
        }
        for(int i=0;i<static_cast<int>(modes.size());++i) if(CheckCollisionPointRec(p,mode_button(i,w,h))) {selected_=modes[i];return selected_;}
    }
    return GameMode::Home;
}
void HomeScreen::draw(int w,int h) const {
    ClearBackground(Color{24,31,25,255});DrawRectangleGradientV(0,0,w,h,Color{52,73,56,255},Color{17,20,17,255});
    const float s=scale(w,h);const auto title=box(0,70,900,80,w,h);
    const int size=std::max(16,static_cast<int>(72*s));
    DrawText("OUTLAND",w/2-MeasureText("OUTLAND",size)/2,static_cast<int>(title.y),size,RAYWHITE);
    const auto subtitle=box(0,165,900,40,w,h);const int small=std::max(10,static_cast<int>(22*s));
    DrawText("BONVENON AL VERDA",w/2-MeasureText("BONVENON AL VERDA",small)/2,static_cast<int>(subtitle.y),small,LIGHTGRAY);
    button(difficulty_button(w,h),TextFormat("BR BOTS: %s",ai::bot_level_name(bot_level_)),s);
    for(int i=0;i<static_cast<int>(modes.size());++i) button(mode_button(i,w,h),labels[i],s);
    const auto footer=box(30,650,840,35,w,h);
    DrawText("BR: last survivor | Explore: free roam, stores and police",static_cast<int>(footer.x),static_cast<int>(footer.y),std::max(10,static_cast<int>(16*s)),GRAY);
}
}
