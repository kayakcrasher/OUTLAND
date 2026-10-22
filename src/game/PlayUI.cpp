#include "outland/game/PlayUI.hpp"
#include "outland/input/PointerEvents.hpp"
#include <algorithm>
#include <sstream>
namespace outland::game {
namespace {
float scale(int w,int h) { return std::max(.1F,std::min(w/900.0F,h/720.0F)); }
Rectangle box(float x,float y,float w,float h,int width,int height) {
    const float s=scale(width,height);
    return {(width-900*s)/2+x*s,(height-720*s)/2+y*s,w*s,h*s};
}
void label(Rectangle r,const char* text,int size,Color color) {
    const float s=r.height/46;
    DrawRectangleRec(r,Color{40,65,44,255});DrawRectangleLinesEx(r,1,LIGHTGRAY);
    DrawText(text,static_cast<int>(r.x+8*s),static_cast<int>(r.y+12*s),std::max(10,static_cast<int>(size*s)),color);
}
}
Rectangle PlayUI::result_button(int i,int w,int h) { return box(i==0 ? 200 : 460,440,240,46,w,h); }
Rectangle PlayUI::shop_row(int i,int w,int h) { return box(30,145+i*52,840,46,w,h); }
Rectangle PlayUI::shop_button(int i,int w,int h) {
    const float x[]{740,30,260,490,30,740}, y[]{80,510,510,510,460,460};
    return box(x[i],y[i],i==0||i>=4 ? 130 : 210,46,w,h);
}
std::vector<Vector2> PlayUI::clicks(bool focused) {
    std::vector<Vector2> out; std::unordered_set<int> current;
    const auto buffered=input::PointerEvents::presses();
    if(focused && !warm_) for(const auto& p:buffered) out.push_back(p);
    const int count=std::max(0,GetTouchPointCount());
    for(int i=0;i<count;++i) {
        const int id=GetTouchPointId(i);
        if(id<0 || !current.insert(id).second) continue;
        if(focused && !warm_ && buffered.empty() && !previous_.contains(id)) out.push_back(GetTouchPosition(i));
    }
    if(focused && !warm_ && buffered.empty() && count==0 && previous_.empty() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) out.push_back(GetMousePosition());
    previous_=std::move(current);return out;
}
ResultAction PlayUI::result_input(int w,int h,bool focused) {
    ResultAction result=ResultAction::None;
    for(const auto& p:clicks(focused)) {
        if(CheckCollisionPointRec(p,result_button(0,w,h))) result=ResultAction::Restart;
        if(CheckCollisionPointRec(p,result_button(1,w,h))) result=ResultAction::Home;
    }
    if(focused && !warm_) {
        if(IsKeyPressed(KEY_ENTER)) result=ResultAction::Restart;
        if(IsKeyPressed(KEY_H)) result=ResultAction::Home;
    }
    warm_=false;return result;
}
ShopAction PlayUI::shop_input(const economy::Economy& economy,const economy::Shop& shop,int w,int h,bool focused) {
    std::vector<std::size_t> choices;
    for(std::size_t i=0;i<economy.offers().size();++i) if(economy.offers()[i].kind==shop.kind) choices.push_back(i);
    ShopAction action;
    if(choices.empty()) {action.close=true;return action;}
    selected_=std::min(selected_,choices.size()-1);page_=std::min(page_,(choices.size()-1)/6);
    for(const auto& p:clicks(focused)) {
        for(int row=0;row<6;++row) if(page_*6+row<choices.size() && CheckCollisionPointRec(p,shop_row(row,w,h))) selected_=page_*6+row;
        for(int i=0;i<6;++i) if(CheckCollisionPointRec(p,shop_button(i,w,h))) {
            if(i==0) action.close=true;
            if(i==1) action.buy=true;
            if(i==2) action.sell=true;
            if(i==3) action.close=true;
            if(i==4 && page_>0) {--page_;selected_=page_*6;}
            if(i==5 && page_<(choices.size()-1)/6) {++page_;selected_=page_*6;}
        }
    }
    if(focused && !warm_) {
        action.close|=IsKeyPressed(KEY_I);action.buy|=IsKeyPressed(KEY_ENTER);action.sell|=IsKeyPressed(KEY_DELETE);
        if(IsKeyPressed(KEY_DOWN)) selected_=std::min(selected_+1,choices.size()-1);
        if(IsKeyPressed(KEY_UP) && selected_>0) --selected_;
        page_=selected_/6;
    }
    action.offer=choices[selected_];warm_=false;return action;
}
int PlayUI::draw_objective(const ModeSession& session,int width) {
    constexpr int font=16,top=108,line_height=20;
    // Explore and Battle Royale need no standing banner (it only cluttered the screen - "be the
    // last survivor" goes without saying); the space it took closes up.
    if(session.mode()==GameMode::Explore || session.mode()==GameMode::BattleRoyale) return top-8;
    std::istringstream words(session.objective());std::string word,line;std::vector<std::string> lines;
    while(words>>word) {
        const auto candidate=line.empty() ? word : line+" "+word;
        if(!line.empty() && MeasureText(candidate.c_str(),font)>width-32) {lines.push_back(line);line=word;}
        else line=candidate;
    }
    if(!line.empty()) lines.push_back(line);
    const int bottom=top+static_cast<int>(lines.size())*line_height+8;
    DrawRectangle(8,top-4,std::max(1,width-16),bottom-top+4,Fade(BLACK,.65F));
    for(std::size_t i=0;i<lines.size();++i) DrawText(lines[i].c_str(),width/2-MeasureText(lines[i].c_str(),font)/2,
        top+static_cast<int>(i)*line_height,font,GOLD);
    return bottom;
}
void PlayUI::draw_result(const ModeSession& session,int w,int h) const {
    DrawRectangle(0,0,w,h,Fade(BLACK,.87F));
    const char* title=session.phase()==SessionPhase::Won ? (session.squad_won() ? "YOUR SIDE WINS" : "YOU WIN - LAST SURVIVOR") : "YOU DIED";
    const int font=std::max(14,static_cast<int>(30*scale(w,h)));
    DrawText(title,w/2-MeasureText(title,font)/2,h/2-80,font,session.phase()==SessionPhase::Won ? GOLD : RED);
    if(session.mode()==GameMode::BattleRoyale) {
        const auto& result=session.result();
        const char* text=TextFormat("PLACEMENT #%d / %d   KILLS %d   %s",result.placement,result.participants,result.kills,ai::bot_level_name(result.difficulty));
        DrawText(text,w/2-MeasureText(text,font/2)/2,h/2-30,font/2,RAYWHITE);
    } else DrawText("Your inventory and kronoj are kept. Respawn to continue exploring.",w/2-std::min(w/2-10,260),h/2-25,std::max(10,font/2),RAYWHITE);
    label(result_button(0,w,h),session.mode()==GameMode::BattleRoyale ? "RESTART / ENTER" : "RESPAWN / ENTER",18,WHITE);
    label(result_button(1,w,h),"HOME / H",18,WHITE);
}
void PlayUI::draw_shop(const economy::Economy& economy,const economy::Shop& shop,const economy::EconomyState& state,
                      const inventory::ItemRegistry& items,const inventory::Inventory& bag,const std::string& status,int w,int h) const {
    DrawRectangle(0,0,w,h,Fade(BLACK,.95F));const float s=scale(w,h);
    const auto at=box(30,86,650,46,w,h);
    DrawText(TextFormat("%s | %lld kronoj | BAG %d/%d",shop.name.c_str(),static_cast<long long>(state.money),static_cast<int>(bag.state().stacks.size()),bag.capacity(items)),static_cast<int>(at.x),static_cast<int>(at.y),std::max(10,static_cast<int>(22*s)),GOLD);
    std::vector<const economy::Offer*> choices;
    for(const auto& o:economy.offers()) if(o.kind==shop.kind) choices.push_back(&o);
    for(int row=0;row<6;++row) {
        const auto i=page_*6+row;if(i>=choices.size()) break;
        const auto& o=*choices[i];const auto r=shop_row(row,w,h);
        label(r,TextFormat("%s | BUY %d SELL %d | stock %d | owned %d",items.find(o.item)->name.c_str(),o.buy,o.sell,economy.stock(state,shop,o),bag.count(o.item)),17,i==selected_ ? GOLD : RAYWHITE);
    }
    const char* labels[]{"CLOSE","BUY ONE","SELL ONE","LEAVE","PREV","NEXT"};
    for(int i=0;i<6;++i) label(shop_button(i,w,h),labels[i],17,RAYWHITE);
    const auto footer=box(30,580,840,46,w,h);
    DrawText(status.c_str(),static_cast<int>(footer.x),static_cast<int>(footer.y),std::max(10,static_cast<int>(16*s)),RAYWHITE);
    DrawText("Scavenge and sell goods to earn money. Goods, cash and stock save together.",static_cast<int>(footer.x),static_cast<int>(footer.y+35*s),std::max(10,static_cast<int>(14*s)),GRAY);
}
}
