#include "outland/game/HomeScreen.hpp"
#include "outland/input/PointerEvents.hpp"
#include "outland/net/LanSession.hpp"
#include <algorithm>
#include <array>
namespace outland::game {
namespace {
#ifdef OUTLAND_DEV_TOOLS
constexpr std::array<GameMode,4> modes{GameMode::BattleRoyale,GameMode::ZombieSurvival,GameMode::Explore,GameMode::DevLab};
constexpr const char* labels[]{"BATTLE ROYALE","ZOMBIE SURVIVAL","CAMPAIGN","CREATIVE BUILDER (DEV)"};
#else
constexpr std::array<GameMode,3> modes{GameMode::BattleRoyale,GameMode::ZombieSurvival,GameMode::Explore};
constexpr const char* labels[]{"BATTLE ROYALE","ZOMBIE SURVIVAL","CAMPAIGN"};
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
// LAN play (friends on the same Wi-Fi), under the modes; not in the browser (no LAN sockets there).
Rectangle HomeScreen::lan_button(int w,int h) { return mode_button(static_cast<int>(modes.size()),w,h); }
Rectangle HomeScreen::slot_button(int index,int w,int h) { return box(150,250+index*96,520,80,w,h); }
Rectangle HomeScreen::erase_button(int index,int w,int h) { return box(684,250+index*96+18,96,44,w,h); }
Rectangle HomeScreen::back_button(int w,int h) { return box(330,560,240,52,w,h); }
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
    if(slots_open_) {
        // The campaign's save slots.
        for(const auto p:points) {
            if(CheckCollisionPointRec(p,back_button(w,h))) {slots_open_=false;erase_armed_=0;return GameMode::Home;}
            for(int i=0;i<slot_count;++i) {
                if(slots_[static_cast<std::size_t>(i)].used && CheckCollisionPointRec(p,erase_button(i,w,h))) {
                    if(erase_armed_==i+1 && GetTime()<erase_until_) {erase_=i+1;erase_armed_=0;}
                    else {erase_armed_=i+1;erase_until_=GetTime()+4;}
                    return GameMode::Home;
                }
                if(CheckCollisionPointRec(p,slot_button(i,w,h))) {slot_=i+1;slots_open_=false;erase_armed_=0;return GameMode::Explore;}
            }
        }
        if(IsKeyPressed(KEY_ESCAPE)) slots_open_=false;
        return GameMode::Home;
    }
    for(const auto p:points) {
        if(CheckCollisionPointRec(p,difficulty_button(w,h))) {
            bot_level_=bot_level_==ai::BotLevel::Easy ? ai::BotLevel::Medium : bot_level_==ai::BotLevel::Medium ? ai::BotLevel::Hard : ai::BotLevel::Easy;
            return GameMode::Home;
        }
        for(int i=0;i<static_cast<int>(modes.size());++i) if(CheckCollisionPointRec(p,mode_button(i,w,h))) {
            if(modes[i]==GameMode::Explore) {
                if(auto_slot_>0) {slot_=auto_slot_;return GameMode::Explore;}
                slots_open_=true;erase_armed_=0;return GameMode::Home;
            }
            selected_=modes[i];return selected_;
        }
        if(net::LanSession::supported() && CheckCollisionPointRec(p,lan_button(w,h))) {lan_=true;return GameMode::Home;}
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
    if(slots_open_) {
        // CAMPAIGN: the three save slots.
        const auto heading=box(0,200,900,30,w,h);const int head=std::max(12,static_cast<int>(26*s));
        DrawText("CAMPAIGN - CHOOSE A SAVE SLOT",w/2-MeasureText("CAMPAIGN - CHOOSE A SAVE SLOT",head)/2,static_cast<int>(heading.y),head,Color{185,202,139,255});
        for(int i=0;i<slot_count;++i) {
            const auto& slot=slots_[static_cast<std::size_t>(i)];
            const auto r=slot_button(i,w,h);
            const bool hovered=CheckCollisionPointRec(GetMousePosition(),r);
            DrawRectangleRounded(r,.12F,8,hovered ? Color{80,100,72,240} : Color{38,45,38,230});
            DrawRectangleRoundedLines(r,.12F,8,slot.used ? WHITE : Color{150,160,140,255});
            const int big=std::max(10,static_cast<int>(22*s)),small=std::max(9,static_cast<int>(15*s));
            const char* title=slot.title.empty() ? TextFormat("SLOT %d",i+1) : slot.title.c_str();
            DrawText(title,static_cast<int>(r.x+18*s),static_cast<int>(r.y+14*s),big,RAYWHITE);
            DrawText(slot.detail.c_str(),static_cast<int>(r.x+18*s),static_cast<int>(r.y+46*s),small,slot.used ? LIGHTGRAY : Color{185,202,139,255});
            const char* action=slot.used ? "CONTINUE" : "NEW GAME";
            DrawText(action,static_cast<int>(r.x+r.width-MeasureText(action,small)-16*s),static_cast<int>(r.y+16*s),small,Color{230,170,60,255});
            if(slot.used) {
                const auto e=erase_button(i,w,h);
                const bool armed=erase_armed_==i+1 && GetTime()<erase_until_;
                DrawRectangleRounded(e,.2F,6,armed ? Color{170,40,36,255} : Color{60,36,34,230});
                const char* label=armed ? "SURE?" : "ERASE";
                DrawText(label,static_cast<int>(e.x+e.width/2-MeasureText(label,small)/2),static_cast<int>(e.y+(e.height-small)/2),small,RAYWHITE);
            }
        }
        button(back_button(w,h),"BACK",s);
        const auto footer=box(30,670,840,35,w,h);
        DrawText("The game saves itself as you play; SAVE GAME in the menu saves now.",static_cast<int>(footer.x),static_cast<int>(footer.y),std::max(10,static_cast<int>(16*s)),GRAY);
        return;
    }
    button(difficulty_button(w,h),TextFormat("BR BOTS: %s",ai::bot_level_name(bot_level_)),s);
    for(int i=0;i<static_cast<int>(modes.size());++i) button(mode_button(i,w,h),labels[i],s);
    if(net::LanSession::supported()) button(lan_button(w,h),"LAN PLAY - SAME WI-FI",s);
    const auto footer=box(30,670,840,35,w,h);
    DrawText("BR: last survivor | Campaign: come home to Verda - three save slots",static_cast<int>(footer.x),static_cast<int>(footer.y),std::max(10,static_cast<int>(16*s)),GRAY);
}
}
