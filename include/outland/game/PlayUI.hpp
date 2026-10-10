#pragma once
#include "outland/game/ModeSession.hpp"
#include "outland/game/economy/Economy.hpp"
#include <unordered_set>
namespace outland::game {
enum class ResultAction { None, Restart, Home };
struct ShopAction { bool close{false}, buy{false}, sell{false}; std::size_t offer{0}; };
// Modal ownership and native touch IDs are shared by result and store screens.
class PlayUI {
public:
    void opened() { warm_=true; selected_=page_=0; }
    ResultAction result_input(int width,int height,bool focused);
    ShopAction shop_input(const economy::Economy&,const economy::Shop&,int width,int height,bool focused);
    static int draw_objective(const ModeSession&,int width);
    void draw_result(const ModeSession&,int width,int height) const;
    void draw_shop(const economy::Economy&,const economy::Shop&,const economy::EconomyState&,
                   const inventory::ItemRegistry&,const inventory::Inventory&,const std::string& status,int width,int height) const;
    static Rectangle result_button(int index,int width,int height);
    static Rectangle shop_row(int index,int width,int height);
    static Rectangle shop_button(int index,int width,int height);
private:
    std::vector<Vector2> clicks(bool focused);
    std::unordered_set<int> previous_; bool warm_{true}; std::size_t selected_{0},page_{0};
};
}
