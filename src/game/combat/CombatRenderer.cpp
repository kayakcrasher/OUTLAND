#include "outland/game/combat/CombatRenderer.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

namespace outland::game::combat {

void CombatRenderer::draw_gun(Vector3 muzzle,Vector3 direction,WeaponId id,float reload_fraction,float flash) {
    const Color steel{65,73,77,255}, polymer{43,48,44,255}, accent{129,133,126,255},skin{181,142,111,255};
    rlPushMatrix();
    rlTranslatef(muzzle.x,muzzle.y,muzzle.z);
    rlRotatef(std::atan2(direction.x,direction.z)*RAD2DEG,0,1,0);
    rlRotatef(-std::asin(std::clamp(direction.y,-1.0F,1.0F))*RAD2DEG,1,0,0);
    const float reload_motion=std::sin(std::clamp(reload_fraction,0.0F,1.0F)*PI);
    rlRotatef(reload_motion*24,0,0,1);
    if(id==WeaponId::Rifle) {
        DrawCube({0,0,-.47F},.13F,.15F,.34F,steel);
        DrawCube({0,0,-.24F},.12F,.12F,.22F,polymer);
        DrawCylinderEx({0,0,-.13F},{0,0,0},.022F,.022F,8,steel);
        DrawCube({0,-.14F,-.49F},.075F,.23F,.12F,polymer);
        DrawCube({0,-.12F-reload_motion*.15F,-.36F},.07F,.22F,.10F,polymer);
        DrawCube({0,-.02F,-.74F},.12F,.18F,.22F,polymer);
        DrawCube({0,.09F,-.46F},.08F,.035F,.31F,accent);
        DrawCube({0,.12F,-.59F},.018F,.04F,.035F,BLACK);
        DrawCube({0,.12F,-.18F},.018F,.04F,.025F,BLACK);
        DrawCube({.07F,0,-.45F},.015F,.055F,.09F,BLACK);
        DrawCube({.083F,.01F,-.42F},.04F,.022F,.022F,accent);
        DrawSphereEx({-.035F,-.09F,-.22F},.06F,4,6,skin);
    } else {
        DrawCube({0,.015F,-.15F},.09F,.10F,.28F,steel);
        DrawCube({0,-.11F-reload_motion*.08F,-.23F},.075F,.19F,.10F,polymer);
        DrawCube({0,.076F,-.25F},.018F,.025F,.03F,BLACK);
        DrawCube({0,.076F,-.04F},.015F,.025F,.02F,BLACK);
        DrawCube({.048F,.02F,-.13F},.012F,.035F,.05F,BLACK);
    }
    DrawSphereEx({.02F,-.10F,-.24F},.065F,4,6,skin);
    DrawCylinderEx({0,0,-.002F},{0,0,.002F},.014F,.014F,8,BLACK);
    if(flash>0) {
        DrawSphereEx({0,0,.06F},.07F,4,6,Color{255,228,135,255});
        DrawCylinderEx({0,0,.04F},{0,0,.20F},.04F,0,5,Color{255,169,65,255});
    }
    rlPopMatrix();
}

void CombatRenderer::draw_world(const WeaponSystem& weapons,const CombatWorld& world) {
    for(const auto& target:world.targets()) {
        if(target.health<=0) {
            DrawCube({target.center.x,target.center.y-.88F,target.center.z},1,.16F,1.7F,Color{102,88,72,255});
            continue;
        }
        const Color body=target.health<100 ? Color{203,123,60,255} : Color{190,75,58,255};
        DrawCube({target.center.x,target.center.y-.25F,target.center.z},1,1.5F,.5F,body);
        DrawCube({target.center.x,target.center.y+.75F,target.center.z},1,.5F,.5F,Color{220,189,139,255});
        DrawCube({target.center.x,target.center.y+.05F,target.center.z+.256F},.32F,.32F,.01F,Color{246,225,176,255});
        // Health bar faces the original firing line; no text allocations or textures.
        DrawCube({target.center.x,target.center.y+1.2F,target.center.z},1,.07F,.04F,BLACK);
        DrawCube({target.center.x-(1-target.health/100)*.5F,target.center.y+1.2F,target.center.z+.025F},
                 target.health/100,.07F,.02F,Color{132,194,101,255});
    }
    for(const auto& bullet:weapons.bullets()) {
        if(!bullet.active)continue;
        const auto trail=Vector3Scale(Vector3Normalize(bullet.velocity),bullet.tracer ? 3.0F : .65F);
        DrawLine3D(Vector3Subtract(bullet.position,trail),bullet.position,
                   bullet.tracer ? Color{255,221,128,255} : Color{184,179,140,255});
    }
    for(const auto& impact:weapons.impacts()) {
        if(impact.life<=0)continue;
        const float age=.4F-impact.life;
        const auto point=Vector3Add(impact.position,Vector3Scale(impact.normal,.018F));
        const Color color=impact.kind==HitKind::Ground ? Color{177,152,98,255}
                         : impact.kind==HitKind::Target ? Color{246,206,93,255} : Color{192,187,162,255};
        if(age<.12F)DrawSphereEx(point,.035F+age*.3F,3,5,color);
        DrawLine3D(point,Vector3Add(point,Vector3Scale(impact.normal,.06F+age*.25F)),color);
    }
}
}
