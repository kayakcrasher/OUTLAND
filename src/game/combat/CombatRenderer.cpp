#include "outland/game/combat/CombatRenderer.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

namespace outland::game::combat {

namespace {
// Parts are boxes and cylinders in the gun's own frame (metres; +z to the muzzle, -x is the
// shooter's right).
void box(float x,float y,float z,float w,float h,float d,Color c) { DrawCube({x,y,z},w,h,d,c); }
void tube(float z0,float z1,float r,float y,Color c,float x=0) { DrawCylinderEx({x,y,z0},{x,y,z1},r,r,10,c); }
// A part tilted about the gun's x axis (grips raked back, magazines raked forward).
void tilted(float degrees,float x,float y,float z,float w,float h,float d,Color c) {
    rlPushMatrix();rlTranslatef(x,y,z);rlRotatef(degrees,1,0,0);DrawCube({0,0,0},w,h,d,c);rlPopMatrix();
}
const Color black{30,32,33,255}, metal{48,51,54,255}, polymer{58,60,58,255}, dark{20,21,22,255},
            wood{122,80,46,255}, wood_dark{92,58,34,255}, olive{82,90,62,255}, tan{156,138,104,255},
            glove{46,44,40,255}, sleeve{72,78,64,255};

void rifle(float reload) {
    tube(.50F,.53F,.011F,0,dark);                         // flash hider
    tube(.39F,.50F,.0085F,0,metal);                       // barrel
    box(0,.024F,.385F,.018F,.045F,.02F,black);            // front sight block
    box(0,.054F,.385F,.004F,.018F,.004F,black);
    box(0,0,.27F,.05F,.054F,.24F,polymer);                // quad-rail handguard
    box(0,.031F,.27F,.022F,.008F,.24F,black);
    box(0,.012F,.07F,.032F,.034F,.22F,metal);             // upper receiver
    box(0,.033F,.07F,.022F,.008F,.20F,black);             // top rail
    box(0,.043F,.05F,.03F,.012F,.06F,black);              // holographic sight
    box(.0155F,.068F,.05F,.005F,.038F,.05F,black);
    box(-.0155F,.068F,.05F,.005F,.038F,.05F,black);
    box(0,.089F,.05F,.036F,.006F,.05F,black);
    box(0,-.024F,.05F,.03F,.04F,.16F,metal);              // lower receiver
    box(-.0165F,.012F,.06F,.002F,.012F,.04F,dark);        // ejection port (right side)
    box(0,.028F,-.045F,.03F,.006F,.012F,black);           // charging handle
    tilted(-12,0,-.115F-reload*.12F,.095F,.024F,.13F,.06F,polymer); // curved magazine, raked
    box(0,-.054F,.018F,.006F,.006F,.05F,black);           // trigger guard
    tilted(20,0,-.083F,-.03F,.028F,.095F,.036F,polymer);  // pistol grip
    tube(-.06F,-.20F,.015F,-.004F,black);                 // buffer tube
    box(0,-.012F,-.235F,.04F,.072F,.13F,polymer);         // stock
    box(0,-.02F,-.304F,.042F,.12F,.012F,dark);            // butt pad
}
void pistol(float reload) {
    box(0,.008F,.025F,.026F,.03F,.19F,black);             // slide
    box(-.0135F,.012F,.04F,.001F,.012F,.035F,dark);       // ejection port
    box(0,-.015F,.035F,.024F,.016F,.14F,polymer);         // frame
    box(0,-.03F,.012F,.005F,.005F,.04F,polymer);          // trigger guard
    tilted(18,0,-.068F-reload*.06F,-.045F,.028F,.105F,.044F,polymer); // grip
    box(0,.026F,-.06F,.02F,.006F,.006F,dark);             // rear sight
    box(0,.026F,.11F,.004F,.006F,.004F,dark);             // front sight
}
void smg(float reload) {
    tube(.24F,.30F,.011F,0,dark);                         // barrel and thread
    box(0,0,.07F,.04F,.065F,.34F,polymer);                // receiver
    box(0,.037F,.05F,.022F,.008F,.18F,black);             // rail
    box(0,.05F,.04F,.026F,.022F,.04F,black);              // red dot
    tube(.02F,.065F,.012F,.054F,black);
    box(.021F,.01F,.15F,.002F,.01F,.06F,black);           // cocking slot
    tilted(-5,0,-.105F-reload*.12F,.10F,.025F,.13F,.045F,black); // straight magazine
    box(0,-.045F,.018F,.006F,.006F,.045F,black);          // trigger guard
    tilted(18,0,-.075F,-.03F,.03F,.09F,.036F,polymer);    // grip
    box(0,0,-.18F,.02F,.02F,.16F,black);                  // folding stock frame
    box(0,-.045F,-.19F,.02F,.015F,.14F,black);
    box(0,-.022F,-.258F,.035F,.075F,.012F,dark);          // butt plate
}
void shotgun(float reload,float pump) {
    tube(.10F,.55F,.011F,.012F,metal);                    // barrel
    box(0,.025F,.54F,.004F,.005F,.004F,tan);              // bead
    tube(.10F,.46F,.010F,-.014F,metal);                   // magazine tube
    box(0,-.014F,.26F-pump*.07F,.042F,.042F,.16F,wood);   // pump fore-end
    box(0,0,.04F,.036F,.06F,.20F,black);                  // receiver
    box(-.0185F,.01F,.06F,.002F,.022F,.06F,dark);         // ejection port
    box(0,-.031F-reload*.02F,.07F,.02F,.006F,.06F,dark);  // loading gate
    box(0,-.04F,0,.006F,.006F,.045F,black);               // trigger guard
    tilted(-8,0,-.036F,-.11F,.032F,.05F,.12F,wood_dark);  // wrist
    box(0,-.054F,-.25F,.038F,.11F,.18F,wood);             // butt
    box(0,-.058F,-.334F,.04F,.122F,.012F,dark);           // recoil pad
}
void sniper(float reload,float bolt) {
    box(0,0,.70F,.026F,.02F,.045F,black);                 // muzzle brake
    tube(.40F,.68F,.0085F,0,metal);                       // barrel, tapering
    tube(.14F,.40F,.011F,0,metal);
    tube(-.08F,.14F,.016F,0,metal);                       // receiver
    // Bolt on the right: lifts and draws back between shots.
    const float back=bolt*.07F;
    DrawCylinderEx({-.016F,0,-.05F-back},{-.048F,-.022F+bolt*.03F,-.06F-back},.005F,.005F,6,metal);
    DrawSphereEx({-.05F,-.024F+bolt*.03F,-.06F-back},.009F,4,6,metal);
    tube(-.06F,.20F,.0125F,.06F,black);                   // scope tube
    tube(.16F,.25F,.021F,.06F,black);                     // objective bell
    tube(-.115F,-.05F,.018F,.06F,black);                  // eyepiece
    DrawCylinderEx({0,.072F,.065F},{0,.09F,.065F},.01F,.01F,8,black);   // elevation turret
    DrawCylinderEx({-.012F,.06F,.065F},{-.03F,.06F,.065F},.01F,.01F,8,black);
    box(0,.035F,0,.02F,.03F,.014F,black);                 // rings
    box(0,.035F,.12F,.02F,.03F,.014F,black);
    box(0,-.02F,.25F,.045F,.05F,.26F,olive);              // fore-end
    box(0,-.026F,.03F,.045F,.05F,.20F,olive);             // action bed
    box(0,-.062F-reload*.06F,.04F,.03F,.04F,.06F,black);  // magazine
    tilted(20,0,-.08F,-.05F,.032F,.09F,.04F,olive);       // grip
    box(0,-.036F,-.25F,.045F,.12F,.22F,olive);            // butt
    box(0,.026F,-.22F,.04F,.02F,.14F,olive);              // cheek riser
    box(0,-.04F,-.364F,.046F,.13F,.012F,dark);            // butt pad
    DrawCylinderEx({.012F,-.05F,.30F},{.012F,-.05F,.47F},.005F,.005F,6,black); // folded bipod
    DrawCylinderEx({-.012F,-.05F,.30F},{-.012F,-.05F,.47F},.005F,.005F,6,black);
}
// First-person hands and forearms: trigger hand on the grip, support hand under the fore-end.
void hands(WeaponId id,const GunLayout& layout) {
    if(id==WeaponId::Pistol) {
        tilted(18,0,-.068F,-.048F,.05F,.085F,.07F,glove);
        tilted(18,.026F,-.075F,-.04F,.028F,.08F,.065F,glove);
        DrawCylinderEx({0,-.09F,-.09F},{-.04F,-.17F,-.36F},.032F,.04F,8,sleeve);
        DrawCylinderEx({.03F,-.09F,-.08F},{.13F,-.17F,-.34F},.032F,.04F,8,sleeve);
        return;
    }
    tilted(18,0,-.075F,-.035F,.05F,.085F,.065F,glove);
    DrawCylinderEx({0,-.1F,-.08F},{-.05F,-.19F,-.34F},.033F,.042F,8,sleeve);
    box(.004F,-.042F,layout.support,.058F,.04F,.085F,glove);
    DrawCylinderEx({.01F,-.07F,layout.support-.04F},{.14F,-.2F,layout.support-.3F},.032F,.04F,8,sleeve);
}
}

GunLayout gun_layout(WeaponId id) {
    switch(id) {
        case WeaponId::Pistol: return {0,0,.026F,definition(id).length};
        case WeaponId::Smg: return {.26F,.17F,.054F,definition(id).length};
        case WeaponId::Shotgun: return {.34F,.24F,.028F,definition(id).length};
        case WeaponId::Sniper: return {.36F,.24F,.06F,definition(id).length};
        default: return {.30F,.22F,.068F,definition(id).length};
    }
}

Vector3 CombatRenderer::muzzle(Vector3 grip,Vector3 aim,WeaponId id) {
    return Vector3Add(grip,Vector3Scale(Vector3Normalize(aim),definition(id).length));
}

void CombatRenderer::draw_gun(Vector3 grip,Vector3 aim,Vector3 up,WeaponId id,float reload_fraction,float flash,bool draw_hands,float pump) {
    const auto z=Vector3Normalize(aim);
    auto y=Vector3Subtract(up,Vector3Scale(z,Vector3DotProduct(up,z)));
    if(Vector3LengthSqr(y)<1e-6F) y={0,1,0};
    y=Vector3Normalize(y);
    const auto x=Vector3CrossProduct(y,z);
    const Matrix basis{x.x,y.x,z.x,grip.x, x.y,y.y,z.y,grip.y, x.z,y.z,z.z,grip.z, 0,0,0,1};
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(basis));
    const float reload=std::sin(std::clamp(reload_fraction,0.0F,1.0F)*PI);
    rlRotatef(reload*18,0,0,1); // the gun rolls over while the magazine changes
    const float work=std::clamp(pump,0.0F,1.0F);
    switch(id) {
        case WeaponId::Pistol: pistol(reload);break;
        case WeaponId::Smg: smg(reload);break;
        case WeaponId::Shotgun: shotgun(reload,work);break;
        case WeaponId::Sniper: sniper(reload,work);break;
        default: rifle(reload);break;
    }
    if(draw_hands) hands(id,gun_layout(id));
    if(flash>0) {
        const float l=definition(id).length+.02F,size=id==WeaponId::Shotgun || id==WeaponId::Sniper ? 1.5F : 1.0F;
        DrawSphereEx({0,0,l+.03F},.035F*size,4,6,Color{255,228,135,255});
        DrawCylinderEx({0,0,l},{0,0,l+.16F*size},.03F*size,0,5,Color{255,169,65,255});
    }
    rlPopMatrix();
}

void CombatRenderer::draw_world(const WeaponSystem& weapons,const CombatWorld& world) {
    if(world.training_range()) for(const auto& target:world.targets()) {
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
