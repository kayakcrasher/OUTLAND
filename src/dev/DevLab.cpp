#include "outland/dev/DevLab.hpp"

#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/VerdaRegion.hpp"

#include <cstdio>
#include <algorithm>

namespace outland::dev {

namespace {

Vector3 grounded(
    const float x,
    const float z
) {
    return {
        x,
        world::terrain::TerrainHeight::sample(
            x,
            z
        ) + 1.0F,
        z
    };
}

}

namespace {
Rectangle travel_button(int index,int width) {return {width*.5F-200+(index%2)*205.0F,82.0F+(index/2)*54,195,44};}
Rectangle tools_button(int width) {return {width*.5F-75,8,150,36};}
}
bool DevLab::owns_point(Vector2 p,int width,int height)const {(void)height;return tools_open_||CheckCollisionPointRec(p,tools_button(width));}
void DevLab::draw_tools(int width,int height,bool building)const {
    DrawRectangleRec(tools_button(width),Fade(BLACK,.75F));DrawText("DEV TOOLS",width/2-56,16,18,YELLOW);
    if(!tools_open_)return;
    DrawRectangle(0,0,width,height,Fade(BLACK,.8F));
    const char* labels[]{building?"RETURN TO PLAY":"BUILD MODE","SPAWN HATCHBACK","CAPITAL","ESPERA","PORTO LUMA","SUDA HAVENO","ROKA","PREVIOUS LOCATION","CLOSE"};
    for(int i=0;i<9;++i){const auto rect=travel_button(i,width);DrawRectangleRec(rect,Color{47,64,65,255});DrawText(labels[i],static_cast<int>(rect.x+8),static_cast<int>(rect.y+12),16,RAYWHITE);}
}
void DevLab::update(int width,int height) {
    const bool focused=width<=0 || height<=0 || IsWindowFocused();
    if(focused && IsKeyPressed(KEY_F1))tools_open_=!tools_open_;
    if(focused && IsKeyPressed(KEY_F2))build_toggle_=true;
    if(focused && IsKeyPressed(KEY_F3))vehicle_spawn_=true;
    if(focused && IsKeyPressed(KEY_F4))return_requested_=true;
    if(width>0 && height>0){
        const auto press=[&](Vector2 point){
            if(!tools_open_){if(CheckCollisionPointRec(point,tools_button(width)))tools_open_=true;return;}
            for(int i=0;i<9;++i)if(CheckCollisionPointRec(point,travel_button(i,width))){
                if(i==0)build_toggle_=true;else if(i==1)vehicle_spawn_=true;else if(i==7)return_requested_=true;
                else if(i>=2&&i<=6){location_=i==2?DevLocation::TrainingGround:i==3?DevLocation::Espera:i==4?DevLocation::PortoLuma:i==5?DevLocation::SouthHaven:DevLocation::Roka;teleport_requested_=true;}
                tools_open_=false;break;
            }
        };
        std::vector<int> contacts;const int count=GetTouchPointCount();
        for(int i=0;i<count;++i){const int id=GetTouchPointId(i);contacts.push_back(id);if(std::find(touch_ids_.begin(),touch_ids_.end(),id)==touch_ids_.end() && IsWindowFocused())press(GetTouchPosition(i));}
        if(count==0&&!had_touch_&&IsWindowFocused()&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))press(GetMousePosition());
        touch_ids_=std::move(contacts);had_touch_=count>0;
    }

    if(focused && IsKeyPressed(KEY_EIGHT)) {location_=DevLocation::PortoLuma;teleport_requested_=true;}
    if(focused && IsKeyPressed(KEY_NINE)) {location_=DevLocation::SouthHaven;teleport_requested_=true;}
    if(focused && IsKeyPressed(KEY_ZERO)) {location_=DevLocation::Roka;teleport_requested_=true;}
    if (focused && IsKeyPressed(KEY_ONE)) {
        location_ =
            DevLocation::TrainingGround;

        teleport_requested_ = true;
    }

    if (focused && IsKeyPressed(KEY_TWO)) {
        location_ =
            DevLocation::Espera;

        teleport_requested_ = true;
    }

    if (focused && IsKeyPressed(KEY_THREE)) {
        location_ =
            DevLocation::Forest;

        teleport_requested_ = true;
    }

    if (focused && IsKeyPressed(KEY_FOUR)) {
        location_ =
            DevLocation::OpenField;

        teleport_requested_ = true;
    }

    if (focused && IsKeyPressed(KEY_FIVE)) {
        location_ =
            DevLocation::BuildingTest;

        teleport_requested_ = true;
    }

    if (focused && IsKeyPressed(KEY_SIX)) {
        location_ =
            DevLocation::VehicleTest;

        teleport_requested_ = true;
    }

    if (focused && IsKeyPressed(KEY_SEVEN)) {
        location_ =
            DevLocation::ZombieTest;

        teleport_requested_ = true;
    }
}

Vector3 DevLab::spawn_position(const world::VerdaRegion& region) const {
    const auto near_espera=[&](float x,float z){
        for(const auto& site:region.settlements()) if(site.id=="village_espera") return grounded(site.center.x+x,site.center.z+z);
        return grounded(x,z-70);
    };
    const auto at_site=[&](const char* id){
        for(const auto& site:region.settlements())if(site.id==id)return grounded(site.center.x,site.center.z+10);
        return grounded(0,8);
    };
    switch (location_) {
    case DevLocation::PortoLuma: return at_site("port_luma");
    case DevLocation::SouthHaven: return at_site("south_haven");
    case DevLocation::Roka: return at_site("west_roka");
    case DevLocation::TrainingGround:
        return grounded(
            0.0F,
            8.0F
        );

    case DevLocation::Espera:
        return near_espera(0,10);

    case DevLocation::Forest:
        return grounded(
            120.0F,
            -150.0F
        );

    case DevLocation::OpenField:
        return grounded(
            -140.0F,
            -90.0F
        );

    case DevLocation::BuildingTest:
        return near_espera(0,5);

    case DevLocation::VehicleTest:
        return grounded(
            70.0F,
            30.0F
        );

    case DevLocation::ZombieTest:
        return grounded(
            -80.0F,
            -160.0F
        );
    }

    return grounded(
        0.0F,
        8.0F
    );
}

bool DevLab::teleport_requested() const {
    return teleport_requested_;
}

void DevLab::clear_teleport() {
    teleport_requested_ = false;
}

void DevLab::draw_overlay(
    const Vector3 player_position,
    const float yaw,
    const float pitch,
    const bool grounded_state
) const {
    DrawRectangle(
        8,
        70,
        330,
        190,
        Fade(
            BLACK,
            0.58F
        )
    );

    DrawText(
        "OUTLAND DEV LAB",
        18,
        80,
        20,
        YELLOW
    );

    char buffer[160];

    std::snprintf(
        buffer,
        sizeof(buffer),
        "XYZ: %.2f  %.2f  %.2f",
        player_position.x,
        player_position.y,
        player_position.z
    );

    DrawText(
        buffer,
        18,
        108,
        18,
        RAYWHITE
    );

    std::snprintf(
        buffer,
        sizeof(buffer),
        "YAW %.2f   PITCH %.2f",
        yaw,
        pitch
    );

    DrawText(
        buffer,
        18,
        132,
        18,
        RAYWHITE
    );

    DrawText(
        grounded_state
            ? "GROUND: YES"
            : "GROUND: NO",
        18,
        156,
        18,
        grounded_state
            ? GREEN
            : RED
    );

    DrawText(
        "1 TRAIN  2 ESPERA  3 FOREST  4 FIELD",
        18,
        182,
        14,
        LIGHTGRAY
    );

    DrawText(
        "5 BUILD  6 VEHICLE  7 ZOMBIE",
        18,
        202,
        14,
        LIGHTGRAY
    );
    DrawText("8 LUMA  9 SOUTH HAVEN  0 ROKA",18,220,14,LIGHTGRAY);
    DrawText("T RESET TARGETS  /  UNLIMITED AMMO",18,240,14,YELLOW);
}

} // namespace outland::dev
