#include "outland/creator/CreatorTouchUI.hpp"
#include "outland/input/PointerEvents.hpp"
#include "outland/input/TouchLayout.hpp"

#ifdef OUTLAND_DEV_TOOLS

#include <array>
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <string>
#include <rlgl.h>

namespace outland::creator {

namespace {

// Fit the complete toolbar/hotbar in narrow or letterboxed Termux-X11 windows.
float ui_scale(int width,int height){return input::editor_scale(width,height);}
constexpr std::size_t category_count = 7;
constexpr std::size_t assets_per_page = 8;

constexpr std::array<
    CreatorAssetCategory,
    category_count
> categories{{
    CreatorAssetCategory::Building,
    CreatorAssetCategory::BuildingPart,
    CreatorAssetCategory::Nature,
    CreatorAssetCategory::Road,
    CreatorAssetCategory::Prop,
    CreatorAssetCategory::Vehicle,
    CreatorAssetCategory::Gameplay
}};

const char* tool_name(
    const CreatorTouchTool tool
) {
    switch (tool) {
        case CreatorTouchTool::Select:    return "SELECT";
        case CreatorTouchTool::Place:     return "PLACE";
        case CreatorTouchTool::Move:      return "MOVE";
        case CreatorTouchTool::Rotate:    return "ROTATE";
        case CreatorTouchTool::Duplicate: return "DUP";
        case CreatorTouchTool::Delete:    return "DELETE";
    }

    return "?";
}

} // namespace

const CreatorTouchActions&
CreatorTouchUI::actions() const {
    return actions_;
}

void CreatorTouchUI::clear_actions() {
    actions_.clear();
}

bool CreatorTouchUI::inventory_open() const {
    return inventory_open_;
}

void CreatorTouchUI::set_inventory_open(
    const bool open
) {
    inventory_open_ = open;
}

void CreatorTouchUI::toggle_inventory() {
    inventory_open_ = !inventory_open_;
}

CreatorTouchTool
CreatorTouchUI::active_tool() const {
    return active_tool_;
}

void CreatorTouchUI::set_active_tool(
    const CreatorTouchTool tool
) {
    active_tool_ = tool;
}

Rectangle CreatorTouchUI::control_button(BuilderControl control,int width,int height) const {
    const float scale=ui_scale(width,height);
    const auto result=logical_control_button(control,static_cast<int>(width/scale),static_cast<int>(height/scale));
    return {result.x*scale,result.y*scale,result.width*scale,result.height*scale};
}
Rectangle CreatorTouchUI::logical_control_button(BuilderControl control,int width,int height) const {
    Rectangle result{};
    if(control==BuilderControl::All)result={24,static_cast<float>(height-260),120,36};
    else if(control==BuilderControl::Catalog)result={width*.5F-90,40,180,36};
    else if(control==BuilderControl::Export)result={static_cast<float>(width-108),192,98,44};
    else if(control==BuilderControl::Purpose)result={static_cast<float>(width-138),244,128,44};
    else if(control==BuilderControl::Search)result={22,130,static_cast<float>(width-44),40};
    else if(control==BuilderControl::Up || control==BuilderControl::Down)
        result={10,control==BuilderControl::Up ? 92.0F:142.0F,98,44};
    else {
        const auto index=static_cast<int>(control);
        const float cell=std::min(98.0F,(width-250.0F)/9);
        result={120+index*cell,92,cell-4,44};
    }
    return result;
}

Rectangle CreatorTouchUI::inventory_button(
    const int screen_width,
    const int screen_height
) const {
    return {
        static_cast<float>(screen_width - 126),
        static_cast<float>(screen_height - 132),
        116.0F,
        48.0F
    };
}

Rectangle CreatorTouchUI::save_button(
    const int screen_width,
    const int
) const {
    return {
        static_cast<float>(screen_width - 108),
        92.0F,
        98.0F,
        44.0F
    };
}

Rectangle CreatorTouchUI::fly_button(
    const int screen_width,
    const int
) const {
    return {
        static_cast<float>(screen_width - 108),
        142.0F,
        98.0F,
        44.0F
    };
}

Rectangle CreatorTouchUI::tool_button(
    const CreatorTouchTool tool,
    const int screen_width,
    const int screen_height
) const {
    constexpr float width = 100.0F;
    constexpr float height = 44.0F;

    switch (tool) {
        case CreatorTouchTool::Select:
            return {
                10.0F,
                static_cast<float>(screen_height - 278),
                width,
                height
            };

        case CreatorTouchTool::Move:
            return {
                10.0F,
                static_cast<float>(screen_height - 228),
                width,
                height
            };

        case CreatorTouchTool::Delete:
            return {
                10.0F,
                static_cast<float>(screen_height - 178),
                width,
                height
            };

        case CreatorTouchTool::Place:
            return {
                static_cast<float>(screen_width - 110),
                static_cast<float>(screen_height - 278),
                width,
                height
            };

        case CreatorTouchTool::Rotate:
            return {
                static_cast<float>(screen_width - 110),
                static_cast<float>(screen_height - 228),
                width,
                height
            };

        case CreatorTouchTool::Duplicate:
            return {
                static_cast<float>(screen_width - 110),
                static_cast<float>(screen_height - 178),
                width,
                height
            };
    }

    return {};
}

Rectangle CreatorTouchUI::hotbar_slot(
    const std::size_t slot,
    const int screen_width,
    const int screen_height
) const {
    constexpr float slot_size = 52.0F;
    constexpr float gap = 5.0F;

    const float total_width =
        static_cast<float>(CreatorController::hotbar_size) * slot_size +
        static_cast<float>(CreatorController::hotbar_size - 1) * gap;

    const float start_x =
        (
            static_cast<float>(screen_width) -
            total_width
        ) * 0.5F;

    return {
        start_x +
            static_cast<float>(slot) *
            (slot_size + gap),
        static_cast<float>(screen_height) - 66.0F,
        slot_size,
        slot_size
    };
}

bool CreatorTouchUI::pressed(
    const Rectangle rectangle
) const {
    for (const auto point : presses_) {
        if (CheckCollisionPointRec(point, rectangle)) return true;
    }
    return false;
}

bool CreatorTouchUI::owns_point(Vector2 point, int width, int height) const {
    const float scale=ui_scale(width,height);point={point.x/scale,point.y/scale};
    width=static_cast<int>(width/scale);height=static_cast<int>(height/scale);
    if (inventory_open_) return true; // Modal drawer owns the entire pointer surface.
    if (CheckCollisionPointRec(point, inventory_button(width, height)) ||
        CheckCollisionPointRec(point, save_button(width, height)) ||
        CheckCollisionPointRec(point, fly_button(width, height))) return true;
    for (auto tool : {CreatorTouchTool::Select, CreatorTouchTool::Place,
         CreatorTouchTool::Move, CreatorTouchTool::Rotate,
         CreatorTouchTool::Duplicate, CreatorTouchTool::Delete}) {
        if (CheckCollisionPointRec(point, tool_button(tool, width, height))) return true;
    }
    for (std::size_t slot = 0; slot < CreatorController::hotbar_size; ++slot)
        if (CheckCollisionPointRec(point, hotbar_slot(slot, width, height))) return true;
    for(auto control:{BuilderControl::Undo,BuilderControl::Redo,BuilderControl::Load,BuilderControl::Ground,
        BuilderControl::Grid,BuilderControl::Near,BuilderControl::Far,BuilderControl::Lower,BuilderControl::Raise,
        BuilderControl::Up,BuilderControl::Down,BuilderControl::Export,BuilderControl::Catalog})
        if(CheckCollisionPointRec(point,logical_control_button(control,width,height)))return true;
    if(!purpose_label_.empty() && CheckCollisionPointRec(point,logical_control_button(BuilderControl::Purpose,width,height))) return true;
    return false;
}


// ============================================================
// FRAME UPDATE
// ============================================================

bool CreatorTouchUI::key_pressed(int key) const {
    return IsKeyPressed(key) || std::find(queued_keys_.begin(),queued_keys_.end(),key)!=queued_keys_.end();
}

void CreatorTouchUI::update(
    CreatorController& controller,
    const int physical_width,
    const int physical_height,
    const bool blocked,
    const std::function<bool(Vector2)>& world_reserved
) {
    const bool resized=window_width_!=0 && (window_width_!=physical_width || window_height_!=physical_height);
    window_width_=physical_width;window_height_=physical_height;input_scale_=ui_scale(physical_width,physical_height);
    const int screen_width=static_cast<int>(physical_width/input_scale_);
    const int screen_height=static_cast<int>(physical_height/input_scale_);
    if(resized){vertical_owner_=-1;vertical_direction_=0;}
    actions_.clear();
    queued_keys_.clear();
    // Raylib queues key-down events even if a software keyboard releases within one poll.
    for(int i=0;i<16;++i){const int key=GetKeyPressed();if(key==0)break;queued_keys_.push_back(key);}
    presses_.clear();
    if(vertical_owner_!=-1) {
        bool held=false;
        if(vertical_owner_==-2)held=GetTouchPointCount()==0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        for(int i=0;i<GetTouchPointCount();++i) if(GetTouchPointId(i)==vertical_owner_)held=true;
        if(!held){vertical_owner_=-1;vertical_direction_=0;}
    }
    const bool had_native_touch=!previous_touches_.empty();
    std::vector<int> current;
    for (int i = 0; i < GetTouchPointCount(); ++i) {
        const int id = GetTouchPointId(i);
        current.push_back(id);
        if (std::find(previous_touches_.begin(), previous_touches_.end(), id) == previous_touches_.end())
            presses_.push_back(logical_point(GetTouchPosition(i)));
    }
    previous_touches_ = std::move(current);
    if(resized)presses_.clear();
    if (!resized && GetTouchPointCount() == 0 && !had_native_touch) {
        const auto buffered=input::PointerEvents::presses();
        if(!buffered.empty())presses_.push_back(logical_point(buffered.front()));
        else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))presses_.push_back(logical_point(GetMousePosition()));
    }

    if (blocked || !controller.enabled()) {vertical_owner_=-1;vertical_direction_=0;return;}
    if(key_pressed(KEY_B) || key_pressed(KEY_TAB) || (!search_open_ && key_pressed(KEY_I))) {
        toggle_inventory();return;
    }
    if(!inventory_open_) {
        for(const auto control:{BuilderControl::Up,BuilderControl::Down}) {
            const auto rect=logical_control_button(control,screen_width,screen_height);
            for(int i=0;i<GetTouchPointCount();++i)
                if(vertical_owner_==-1 && pressed({logical_point(GetTouchPosition(i)).x-.1F,logical_point(GetTouchPosition(i)).y-.1F,.2F,.2F}) && CheckCollisionPointRec(logical_point(GetTouchPosition(i)),rect)) {
                    vertical_owner_=GetTouchPointId(i);vertical_direction_=control==BuilderControl::Up ? 1.0F:-1.0F;
                }
            if(GetTouchPointCount()==0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(logical_point(GetMousePosition()),rect)) {
                vertical_owner_=-2;vertical_direction_=control==BuilderControl::Up ? 1.0F:-1.0F;
            }
        }
        actions_.fly_vertical=vertical_direction_;
        if(IsKeyDown(KEY_SPACE)) actions_.fly_vertical=1;
        if(IsKeyDown(KEY_LEFT_CONTROL)) actions_.fly_vertical=-1;
        if(key_pressed(KEY_F5))actions_.save=true;
        if(key_pressed(KEY_F6))actions_.export_world=true;
        if(key_pressed(KEY_DELETE) || key_pressed(KEY_BACKSPACE))actions_.erase=true;
        if(IsKeyDown(KEY_LEFT_CONTROL) && key_pressed(KEY_Z))actions_.undo=true;
        if(IsKeyDown(KEY_LEFT_CONTROL) && key_pressed(KEY_Y))actions_.redo=true;
    }
    const bool was_open = inventory_open_;
    const auto world_presses=presses_;
    bool ui_press=false;
    for(const auto p:presses_)if(owns_point({p.x*input_scale_,p.y*input_scale_},physical_width,physical_height))ui_press=true;
    if (!inventory_open_) update_toolbar(
        controller,
        screen_width,
        screen_height
    );

    update_hotbar(
        controller,
        screen_width,
        screen_height
    );

    if (inventory_open_) {
        vertical_owner_=-1;vertical_direction_=0;actions_.fly_vertical=0;
        if (was_open && (pressed(inventory_button(screen_width, screen_height)) ||
            pressed(logical_control_button(BuilderControl::Catalog,screen_width,screen_height)))) {
            inventory_open_ = false;
            return;
        }
        update_inventory(
            controller,
            screen_width,
            screen_height
        );
    }
    if(!was_open && !inventory_open_ && !ui_press) {
        for(const auto point:world_presses) {
            const Vector2 physical{point.x*input_scale_,point.y*input_scale_};
            if(point.y<80 || (world_reserved && world_reserved(physical)))continue;
            actions_.world_pointer=true;actions_.world_point=physical;break;
        }
    }
}

void CreatorTouchUI::resolve_world_press(CreatorController& controller,const world::VerdaRegion& region,Ray ray) {
    if(!actions_.world_pointer || !controller.enabled() || inventory_open_)return;
    actions_.world_pointer=false;
    if(active_tool_==CreatorTouchTool::Move) {
        if(controller.selection().valid() && controller.point_preview(region,ray) && !controller.preview().blocked)actions_.move=true;
        else hint_="SELECT an object, then MOVE and tap its destination";
        return;
    }
    const bool selected=controller.select_target(region,ray.position,ray.direction);
    if(selected) {
        hint_="Object selected - MOVE, ROTATE, DUP or DELETE; USE sets a building's purpose; SAVE keeps changes";
        if(active_tool_==CreatorTouchTool::Rotate)actions_.rotate=true;
        if(active_tool_==CreatorTouchTool::Duplicate)actions_.duplicate=true;
        if(active_tool_==CreatorTouchTool::Delete)actions_.erase=true;
        return;
    }
    if(active_tool_!=CreatorTouchTool::Place){hint_="No object here - tap a visible building or model";return;}
    if(!controller.point_preview(region,ray)){hint_="Aim at ground within 120m, or turn GROUND off for free placement";return;}
    if(controller.preview().blocked){hint_="Placement blocked - choose clear ground";return;}
    actions_.place=true;hint_="Placed asset - keep building; SAVE / EXPORT preserve your world";
}

// ============================================================
// TOOLBAR INPUT
// ============================================================

void CreatorTouchUI::update_toolbar(
    CreatorController& controller,
    const int screen_width,
    const int screen_height
) {
    if (
        pressed(
            inventory_button(
                screen_width,
                screen_height
            )
        ) || pressed(logical_control_button(BuilderControl::Catalog,screen_width,screen_height))
    ) {
        toggle_inventory();
        return;
    }

    if(pressed(logical_control_button(BuilderControl::Export,screen_width,screen_height))) {
        actions_.export_world=true;return;
    }
    if(!purpose_label_.empty() && pressed(logical_control_button(BuilderControl::Purpose,screen_width,screen_height))) {
        actions_.purpose=true;return;
    }
    if (
        pressed(
            save_button(
                screen_width,
                screen_height
            )
        )
    ) {
        actions_.save = true;
        return;
    }

    if (
        pressed(
            fly_button(
                screen_width,
                screen_height
            )
        )
    ) {
        actions_.toggle_fly = true;

        controller.state().flying =
            !controller.state().flying;

        controller.state().noclip =
            controller.state().flying;

        return;
    }

    for(auto control:{BuilderControl::Undo,BuilderControl::Redo,BuilderControl::Load,BuilderControl::Ground,
        BuilderControl::Grid,BuilderControl::Near,BuilderControl::Far,BuilderControl::Lower,BuilderControl::Raise}) {
        if(!pressed(logical_control_button(control,screen_width,screen_height)))continue;
        switch(control) {
            case BuilderControl::Undo:actions_.undo=true;break;
            case BuilderControl::Redo:actions_.redo=true;break;
            case BuilderControl::Load:actions_.load=true;break;
            case BuilderControl::Ground:controller.state().snap_to_ground=!controller.state().snap_to_ground;break;
            case BuilderControl::Grid:controller.state().grid_step=controller.state().grid_step<=0 ? 1.0F : controller.state().grid_step<1.5F ? 1.5F : 0.0F;break;
            case BuilderControl::Near:controller.decrease_placement_distance();break;
            case BuilderControl::Far:controller.increase_placement_distance();break;
            case BuilderControl::Lower:controller.state().placement_height-=controller.state().height_step();break;
            case BuilderControl::Raise:controller.state().placement_height+=controller.state().height_step();break;
            default:break;
        }
        return;
    }
    constexpr std::array<
        CreatorTouchTool,
        6
    > tools{{
        CreatorTouchTool::Select,
        CreatorTouchTool::Move,
        CreatorTouchTool::Delete,
        CreatorTouchTool::Place,
        CreatorTouchTool::Rotate,
        CreatorTouchTool::Duplicate
    }};

    for (const auto tool : tools) {
        if (
            !pressed(
                tool_button(
                    tool,
                    screen_width,
                    screen_height
                )
            )
        ) {
            continue;
        }

        active_tool_ = tool;

        switch (tool) {
            case CreatorTouchTool::Select:
                actions_.select = true;
                break;

            case CreatorTouchTool::Place:
                actions_.place = true;
                break;

            case CreatorTouchTool::Move:
                hint_="MOVE: tap clear ground to move the selected object";
                break;

            case CreatorTouchTool::Rotate:
                actions_.rotate = true;

                break;

            case CreatorTouchTool::Duplicate:
                actions_.duplicate = true;
                break;

            case CreatorTouchTool::Delete:
                actions_.erase = true;
                break;
        }

        return;
    }
}

// ============================================================
// HOTBAR INPUT
// ============================================================

void CreatorTouchUI::update_hotbar(
    CreatorController& controller,
    const int screen_width,
    const int screen_height
) {
    if (inventory_open_) {
        return;
    }
    for(int slot=0;slot<static_cast<int>(CreatorController::hotbar_size);++slot)if(key_pressed(KEY_ONE+slot)) {
        controller.select_hotbar_slot(static_cast<std::size_t>(slot));controller.clear_selection();active_tool_=CreatorTouchTool::Place;return;
    }

    for (
        std::size_t slot = 0;
        slot < CreatorController::hotbar_size;
        ++slot
    ) {
        if (
            !pressed(
                hotbar_slot(
                    slot,
                    screen_width,
                    screen_height
                )
            )
        ) {
            continue;
        }

        controller.select_hotbar_slot(
            slot
        );
        controller.clear_selection();

        active_tool_ =
            CreatorTouchTool::Place;

        return;
    }
}

// ============================================================
// INVENTORY LAYOUT
// ============================================================

Rectangle CreatorTouchUI::drawer_panel(
    const int screen_width,
    const int screen_height
) const {
    return {
        16.0F,
        72.0F,
        static_cast<float>(
            screen_width
        ) - 32.0F,
        static_cast<float>(
            screen_height
        ) - 154.0F
    };
}

Rectangle CreatorTouchUI::category_button(
    const std::size_t category_index,
    const int screen_width,
    const int
) const {
    constexpr float margin = 22.0F;
    constexpr float gap = 4.0F;

    const float available =
        static_cast<float>(
            screen_width
        ) - margin * 2.0F;

    const float width =
        (
            available -
            gap *
            static_cast<float>(
                category_count - 1
            )
        ) /
        static_cast<float>(
            category_count
        );

    return {
        margin +
            static_cast<float>(
                category_index
            ) * (width + gap),
        86.0F,
        width,
        38.0F
    };
}

Rectangle CreatorTouchUI::asset_button(
    const std::size_t visible_index,
    const int screen_width,
    const int screen_height
) const {
    constexpr std::size_t columns = 4;
    constexpr float gap = 8.0F;
    const float card_height = std::clamp((screen_height-460.0F)/2,36.0F,92.0F);

    const std::size_t column =
        visible_index % columns;

    const std::size_t row =
        visible_index / columns;

    const float panel_width =
        static_cast<float>(
            screen_width
        ) - 44.0F;

    const float card_width =
        (
            panel_width -
            gap * 3.0F
        ) / 4.0F;

    return {
        22.0F +
            static_cast<float>(
                column
            ) * (card_width + gap),

        180.0F +
            static_cast<float>(
                row
            ) * (card_height + gap),

        card_width,
        card_height
    };
}

Rectangle CreatorTouchUI::pack_button(int width, int height) const {
    return {width * .5F - 80, static_cast<float>(height - 260), 160, 36};
}

std::vector<const CreatorAssetDefinition*> CreatorTouchUI::drawer_assets(const CreatorController& controller) const {
    std::vector<const CreatorAssetDefinition*> assets;
    if(all_categories_)for(const auto& asset:controller.registry().assets())assets.push_back(&asset);
    else assets=controller.registry().category(drawer_category_);
    auto lower=[](std::string text){for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;};
    const auto query=lower(search_);
    if(!query.empty())std::erase_if(assets,[&](const auto* asset){std::string text=asset->id+" "+asset->name+" "+asset->model_path;for(const auto& tag:asset->tags)text+=" "+tag;return lower(text).find(query)==std::string::npos;});
    if (pack_filter_ != 0) std::erase_if(assets, [this](const auto* asset) {
        const auto prefix = pack_filter_ == 1 ? "assets/verda/urban/" : pack_filter_ == 2 ? "assets/verda/characters/" : pack_filter_ == 3 ? "assets/verda/survival/" : pack_filter_ == 4 ? "assets/verda/industrial/" : "assets/verda/vehicles/";
        return !asset->model_path.starts_with(prefix);
    });
    return assets;
}

Rectangle CreatorTouchUI::page_button(bool next, int width, int height) const {
    return {next ? static_cast<float>(width - 140) : 24.0F,
        static_cast<float>(height - 214), 116.0F, 40.0F};
}

// ============================================================
// INVENTORY INPUT
// ============================================================

void CreatorTouchUI::update_inventory(
    CreatorController& controller,
    const int screen_width,
    const int screen_height
) {
    if(pressed(logical_control_button(BuilderControl::All,screen_width,screen_height))) {show_all_assets();return;}
    if(pressed(logical_control_button(BuilderControl::Search,screen_width,screen_height)))search_open_=!search_open_;
    if(search_open_) {
        int code=0;while((code=GetCharPressed())!=0)if(code>=32 && code<127 && search_.size()<60)search_+=static_cast<char>(code);
        if(key_pressed(KEY_BACKSPACE) && !search_.empty())search_.pop_back();
        if(key_pressed(KEY_ENTER))search_open_=false;
        constexpr std::string_view keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ";
        for(std::size_t i=0;i<keys.size()+3;++i) {
            const float cell=(screen_width-44.0F)/10;
            const Rectangle key{22+static_cast<float>(i%10)*cell,180+static_cast<float>(i/10)*48,cell-4,44};
            if(!pressed(key))continue;
            if(i<keys.size() && search_.size()<60)search_+=keys[i];
            else if(i==keys.size() && !search_.empty())search_.pop_back();
            else if(i==keys.size()+1)search_.clear();
            else if(i==keys.size()+2)search_open_=false;
        }
        drawer_page_=0;return;
    }
    for (
        std::size_t index = 0;
        index < categories.size();
        ++index
    ) {
        if (
            !pressed(
                category_button(
                    index,
                    screen_width,
                    screen_height
                )
            )
        ) {
            continue;
        }

        all_categories_=false;
        drawer_category_ =
            categories[index];

        drawer_page_ = 0;

        controller.set_category(
            drawer_category_
        );

        return;
    }

    if (pressed(pack_button(screen_width, screen_height))) {
        pack_filter_ = (pack_filter_ + 1) % 6;
        if (pack_filter_ >= 3)all_categories_=true;
        if (pack_filter_ == 2) {drawer_category_ = CreatorAssetCategory::Prop;all_categories_=false;}
        drawer_page_ = 0;
        return;
    }

    const auto category_assets = drawer_assets(controller);

    const std::size_t page_count = std::max<std::size_t>(1,
        (category_assets.size() + assets_per_page - 1) / assets_per_page);
    drawer_page_ = std::min(drawer_page_, page_count - 1);
    if (pressed(page_button(false, screen_width, screen_height)) && drawer_page_ > 0) {
        --drawer_page_;
        return;
    }
    if (pressed(page_button(true, screen_width, screen_height)) && drawer_page_ + 1 < page_count) {
        ++drawer_page_;
        return;
    }

    const std::size_t first =
        drawer_page_ *
        assets_per_page;

    for (
        std::size_t visible = 0;
        visible < assets_per_page;
        ++visible
    ) {
        const std::size_t category_index =
            first + visible;

        if (
            category_index >=
            category_assets.size()
        ) {
            break;
        }

        if (
            !pressed(
                asset_button(
                    visible,
                    screen_width,
                    screen_height
                )
            )
        ) {
            continue;
        }

        const auto* chosen =
            category_assets[
                category_index
            ];

        const auto& all =
            controller.registry().assets();

        for (
            std::size_t registry_index = 0;
            registry_index < all.size();
            ++registry_index
        ) {
            if (
                &all[registry_index] !=
                chosen
            ) {
                continue;
            }

            controller.assign_hotbar_slot(
                controller.selected_hotbar_slot(),
                registry_index
            );

            inventory_open_ = false;
            controller.clear_selection();

            active_tool_ =
                CreatorTouchTool::Place;

            return;
        }
    }
}


// ============================================================
// GENERIC BUTTON DRAW
// ============================================================

void CreatorTouchUI::draw_button(
    const Rectangle rectangle,
    const char* label,
    const bool active
) const {
    const Color fill =
        active
            ? Fade(YELLOW, 0.78F)
            : Fade(BLACK, 0.72F);

    const Color border =
        active
            ? YELLOW
            : Fade(RAYWHITE, 0.65F);

    const Color text =
        active
            ? BLACK
            : RAYWHITE;

    DrawRectangleRec(
        rectangle,
        fill
    );

    DrawRectangleLinesEx(
        rectangle,
        active ? 3.0F : 1.5F,
        border
    );

    constexpr int font_size = 14;

    const int width =
        MeasureText(
            label,
            font_size
        );

    DrawText(
        label,
        static_cast<int>(
            rectangle.x +
            (
                rectangle.width -
                static_cast<float>(width)
            ) * 0.5F
        ),
        static_cast<int>(
            rectangle.y +
            (
                rectangle.height -
                static_cast<float>(font_size)
            ) * 0.5F
        ),
        font_size,
        text
    );
}

// ============================================================
// MAIN DRAW
// ============================================================

void CreatorTouchUI::draw(
    const CreatorController& controller,
    const int physical_width,
    const int physical_height
) const {
    if (!controller.enabled()) {
        return;
    }

    const float scale=ui_scale(physical_width,physical_height);
    const int screen_width=static_cast<int>(physical_width/scale),screen_height=static_cast<int>(physical_height/scale);
    rlPushMatrix();rlScalef(scale,scale,1);
    draw_toolbar(
        controller,
        screen_width,
        screen_height
    );

    draw_hotbar(
        controller,
        screen_width,
        screen_height
    );

    if (inventory_open_) {
        draw_inventory(
            controller,
            screen_width,
            screen_height
        );
    }
    draw_button(logical_control_button(BuilderControl::Catalog,screen_width,screen_height),inventory_open_ ? "CLOSE ASSETS [B]":"ASSETS [B]",inventory_open_);
    rlPopMatrix();
}

// ============================================================
// TOOLBAR
// ============================================================

void CreatorTouchUI::draw_toolbar(
    const CreatorController& controller,
    const int screen_width,
    const int screen_height
) const {
    const float grid=controller.state().grid_step;
    const std::array<const char*,9> labels{"UNDO","REDO","LOAD","GROUND",grid>=1.5F ? "GRID 1.5" : grid>0 ? "GRID 1" : "GRID","NEAR","FAR","LOWER","RAISE"};
    for(std::size_t i=0;i<labels.size();++i)draw_button(logical_control_button(static_cast<BuilderControl>(i),screen_width,screen_height),labels[i],
        i==3 ? controller.state().snap_to_ground:i==4 && controller.state().grid_step>0);
    draw_button(logical_control_button(BuilderControl::Up,screen_width,screen_height),"UP",actions_.fly_vertical>0);
    draw_button(logical_control_button(BuilderControl::Down,screen_width,screen_height),"DOWN",actions_.fly_vertical<0);
    if(!purpose_label_.empty()) draw_button(logical_control_button(BuilderControl::Purpose,screen_width,screen_height),purpose_label_.c_str(),false);
    DrawText(status_.c_str(),120,143,16,YELLOW);
    DrawText(hint_.c_str(),120,216,14,RAYWHITE);
    const auto* selected=controller.selected_asset();
    DrawText(selected ? selected->name.c_str():"Choose an asset",120,188,16,RAYWHITE);
    DrawText(TextFormat("Distance %.0fm  Height %+.2fm - SELECT object, look at destination, MOVE",controller.state().placement_distance,controller.state().placement_height),120,165,14,RAYWHITE);
    constexpr std::array<
        CreatorTouchTool,
        6
    > tools{{
        CreatorTouchTool::Select,
        CreatorTouchTool::Move,
        CreatorTouchTool::Delete,
        CreatorTouchTool::Place,
        CreatorTouchTool::Rotate,
        CreatorTouchTool::Duplicate
    }};

    for (const auto tool : tools) {
        draw_button(
            tool_button(
                tool,
                screen_width,
                screen_height
            ),
            tool_name(tool),
            active_tool_ == tool
        );
    }

    draw_button(
        inventory_button(
            screen_width,
            screen_height
        ),
        inventory_open_
            ? "CLOSE"
            : "ASSETS",
        inventory_open_
    );

    draw_button(
        save_button(
            screen_width,
            screen_height
        ),
        "SAVE",
        false
    );
    draw_button(logical_control_button(BuilderControl::Export,screen_width,screen_height),"EXPORT",false);

    draw_button(
        fly_button(
            screen_width,
            screen_height
        ),
        controller.state().flying
            ? "FLY ON"
            : "FLY OFF",
        controller.state().flying
    );
}

// ============================================================
// HOTBAR
// ============================================================

void CreatorTouchUI::draw_hotbar(
    const CreatorController& controller,
    const int screen_width,
    const int screen_height
) const {
    const auto& hotbar =
        controller.hotbar();

    const auto& assets =
        controller.registry().assets();

    for (
        std::size_t slot = 0;
        slot < CreatorController::hotbar_size;
        ++slot
    ) {
        const Rectangle rectangle =
            hotbar_slot(
                slot,
                screen_width,
                screen_height
            );

        const bool active =
            slot ==
            controller.selected_hotbar_slot();

        DrawRectangleRec(
            rectangle,
            active
                ? Fade(YELLOW, 0.82F)
                : Fade(BLACK, 0.78F)
        );

        DrawRectangleLinesEx(
            rectangle,
            active ? 3.0F : 1.0F,
            active
                ? YELLOW
                : LIGHTGRAY
        );

        char slot_text[8];

        std::snprintf(
            slot_text,
            sizeof(slot_text),
            "%zu",
            slot + 1
        );

        DrawText(
            slot_text,
            static_cast<int>(
                rectangle.x + 4.0F
            ),
            static_cast<int>(
                rectangle.y + 3.0F
            ),
            11,
            active ? BLACK : GRAY
        );

        const std::size_t asset_index =
            hotbar[slot];

        if (asset_index >= assets.size()) {
            continue;
        }

        const auto& asset =
            assets[asset_index];

        char abbreviation[3]{
            '?',
            '\0',
            '\0'
        };

        if (!asset.name.empty()) {
            abbreviation[0] =
                asset.name[0];

            if (asset.name.size() > 1) {
                abbreviation[1] =
                    asset.name[1];
            }
        }

        const int text_width =
            MeasureText(
                abbreviation,
                18
            );

        DrawText(
            abbreviation,
            static_cast<int>(
                rectangle.x +
                (
                    rectangle.width -
                    static_cast<float>(
                        text_width
                    )
                ) * 0.5F
            ),
            static_cast<int>(
                rectangle.y + 24.0F
            ),
            18,
            active ? BLACK : RAYWHITE
        );
    }
}

// ============================================================
// ASSET CARD
// ============================================================

void CreatorTouchUI::draw_asset_card(
    const Rectangle rectangle,
    const CreatorAssetDefinition& asset,
    const bool selected
) const {
    DrawRectangleRec(
        rectangle,
        selected
            ? Fade(YELLOW, 0.82F)
            : Fade(DARKGRAY, 0.95F)
    );

    DrawRectangleLinesEx(
        rectangle,
        selected ? 3.0F : 1.0F,
        selected
            ? YELLOW
            : Fade(RAYWHITE, 0.55F)
    );

    // Live 3D preview on the right of the card; the name wraps in the space that remains.
    float text_width=rectangle.width-12;
    if(const Texture2D* preview=thumbnails_ ? thumbnails_(asset) : nullptr) {
        const float side=rectangle.height-8;
        const Rectangle target{rectangle.x+rectangle.width-side-4,rectangle.y+4,side*1.33F>rectangle.width*.55F ? side : side*1.33F,side};
        const Rectangle placed{rectangle.x+rectangle.width-target.width-4,target.y,target.width,target.height};
        // Render textures are stored upside down.
        DrawTexturePro(*preview,{0,0,static_cast<float>(preview->width),-static_cast<float>(preview->height)},placed,{0,0},0,WHITE);
        text_width=placed.x-rectangle.x-10;
    }
    const int font=rectangle.height<60 ? 12:14;
    const int lines=std::max(1,static_cast<int>((rectangle.height-12)/(font+3)));
    std::string remaining=asset.name;
    for(int line=0;line<lines && !remaining.empty();++line) {
        std::size_t length=remaining.size();
        while(length>1 && MeasureText(remaining.substr(0,length).c_str(),font)>text_width)--length;
        if(length<remaining.size() && line+1<lines) {
            const auto space=remaining.rfind(' ',length);if(space!=std::string::npos && space>0)length=space;
        }
        std::string text=remaining.substr(0,length);remaining.erase(0,length);
        while(!remaining.empty() && remaining.front()==' ')remaining.erase(remaining.begin());
        if(line+1==lines && !remaining.empty()) {if(text.size()>3)text.resize(text.size()-3);text+="...";}
        DrawText(text.c_str(),static_cast<int>(rectangle.x+6),static_cast<int>(rectangle.y+6+line*(font+3)),font,selected ? BLACK:RAYWHITE);
    }
}

// ============================================================
// INVENTORY DRAWER
// ============================================================

void CreatorTouchUI::draw_inventory(
    const CreatorController& controller,
    const int screen_width,
    const int screen_height
) const {
    const Rectangle panel =
        drawer_panel(
            screen_width,
            screen_height
        );

    DrawRectangleRec(
        panel,
        Fade(BLACK, 0.94F)
    );

    DrawRectangleLinesEx(
        panel,
        2.0F,
        YELLOW
    );

    DrawText(
        "CREATIVE BUILDER - UNLIMITED ASSETS",
        24,
        52,
        18,
        YELLOW
    );

    for (
        std::size_t index = 0;
        index < categories.size();
        ++index
    ) {
        const auto category =
            categories[index];

        draw_button(
            category_button(
                index,
                screen_width,
                screen_height
            ),
            category_name(
                category
            ).data(),
            !all_categories_ && drawer_category_ ==
                category
        );
    }

    draw_button(logical_control_button(BuilderControl::All,screen_width,screen_height),"ALL TYPES",all_categories_);
    draw_button(logical_control_button(BuilderControl::Search,screen_width,screen_height),search_.empty() ? "SEARCH ASSETS - TAP TO TYPE" : search_.c_str(),search_open_);
    if(search_open_) {
        constexpr std::string_view keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ";
        for(std::size_t i=0;i<keys.size()+3;++i) {
            const float cell=(screen_width-44.0F)/10;
            const Rectangle key{22+static_cast<float>(i%10)*cell,180+static_cast<float>(i/10)*48,cell-4,44};
            std::string label=i<keys.size() ? std::string(1,keys[i]):i==keys.size() ? "DEL":i==keys.size()+1 ? "CLEAR":"DONE";
            draw_button(key,label.c_str(),false);
        }
        return;
    }
    draw_button(pack_button(screen_width, screen_height),
        pack_filter_ == 5 ? "VEHICLES" : pack_filter_ == 4 ? "INDUSTRIAL" : pack_filter_ == 3 ? "SURVIVAL" : pack_filter_ == 2 ? "CHARACTERS" : (pack_filter_ == 1 ? "URBAN" : "ALL"), pack_filter_ != 0);

    const auto category_assets = drawer_assets(controller);

    const std::size_t page_count = std::max<std::size_t>(1,
        (category_assets.size() + assets_per_page - 1) / assets_per_page);
    draw_button(page_button(false, screen_width, screen_height), "PREVIOUS", drawer_page_ > 0);
    draw_button(page_button(true, screen_width, screen_height), "NEXT", drawer_page_ + 1 < page_count);
    DrawText(TextFormat("PAGE %d / %d  (%d ASSETS)", static_cast<int>(drawer_page_ + 1),
        static_cast<int>(page_count), static_cast<int>(category_assets.size())),
        screen_width / 2 - 105, screen_height - 203, 16, RAYWHITE);

    const std::size_t first =
        drawer_page_ *
        assets_per_page;

    const auto* selected =
        controller.selected_asset();

    for (
        std::size_t visible = 0;
        visible < assets_per_page;
        ++visible
    ) {
        const std::size_t index =
            first + visible;

        if (
            index >=
            category_assets.size()
        ) {
            break;
        }

        const auto* asset =
            category_assets[index];

        draw_asset_card(
            asset_button(
                visible,
                screen_width,
                screen_height
            ),
            *asset,
            selected != nullptr &&
                selected->id == asset->id
        );
    }

    if (category_assets.empty()) {
        DrawText(
            "NO ASSETS REGISTERED",
            32,
            160,
            18,
            GRAY
        );
    }
}

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
