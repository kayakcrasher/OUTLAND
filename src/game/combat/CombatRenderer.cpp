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
            glove{46,44,40,255}, sleeve{72,78,64,255}, tritium{150,255,120,255}, white_dot{235,235,225,255};

// Iron sights, built on the gun's sight line (sight_height: the bore up to where the eye looks),
// so the picture lines up exactly with the crosshair: the top of the front post sits on the line
// and the rear aperture (or notch) is centred on it.
// A front post in protective ears (an A-frame on a rifle), a tritium dot on its tip.
void front_post(float z,float line,float base,bool ears) {
    const float post_h=line-base;
    box(0,base+post_h*.5F,z,.0032F,post_h,.0032F,black);
    box(0,line-.0012F,z+.0005F,.0022F,.0022F,.001F,tritium);
    if(ears) for(const float side:{-1.0F,1.0F}) {
        box(side*.0105F,base+post_h*.55F,z,.0035F,post_h*1.1F,.006F,black);
        tilted(side*-14,side*.006F,base+.002F,z,.012F,.006F,.008F,black);
    }
}
// A rear peep: a square ring round a hole on the line (the eye looks through it, so it frames
// the front post), on a short base.
void rear_aperture(float z,float line,float base,float hole=.0056F,float ring=.0034F) {
    const float outer=hole+ring;
    box(0,line+hole+ring*.5F,z,outer*2,ring,.006F,black);           // top
    box(0,(base+line-hole)*.5F,z,outer*2,line-hole-base,.006F,black); // bottom down to the base
    for(const float side:{-1.0F,1.0F}) box(side*(hole+ring*.5F),line,z,ring,hole*2,.006F,black);
    for(const float side:{-1.0F,1.0F}) box(side*(outer+.002F),(base+line+outer)*.5F,z,.003F,line+outer-base,.01F,black); // guard wings
}
// A pistol's notch: two blades with a gap on the line, a white dot on each, and the front dot.
void rear_notch(float z,float line,float base,float gap=.0034F) {
    for(const float side:{-1.0F,1.0F}) {
        box(side*(gap+.0035F),(base+line+.002F)*.5F,z,.007F,line+.002F-base,.006F,dark);
        box(side*(gap+.0035F),line-.0012F,z-.0032F,.0024F,.0024F,.001F,white_dot);
    }
}

void rifle(float reload,bool irons,float line) {
    tube(.50F,.53F,.011F,0,dark);                         // flash hider
    tube(.39F,.50F,.0085F,0,metal);                       // barrel
    box(0,.024F,.385F,.018F,.045F,.02F,black);            // front sight block
    front_post(.385F,line,.046F,true);
    box(0,0,.27F,.05F,.054F,.24F,polymer);                // quad-rail handguard
    box(0,.031F,.27F,.022F,.008F,.24F,black);
    box(0,.012F,.07F,.032F,.034F,.22F,metal);             // upper receiver
    box(0,.033F,.07F,.022F,.008F,.20F,black);             // top rail
    if(irons) {                                           // flip-up rear aperture
        box(0,.041F,-.02F,.022F,.008F,.014F,black);
        rear_aperture(-.02F,line,.045F);
    }
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
void pistol(float reload,float line) {
    box(0,.008F,.025F,.026F,.03F,.19F,black);             // slide
    box(-.0135F,.012F,.04F,.001F,.012F,.035F,dark);       // ejection port
    box(0,-.015F,.035F,.024F,.016F,.14F,polymer);         // frame
    box(0,-.03F,.012F,.005F,.005F,.04F,polymer);          // trigger guard
    tilted(18,0,-.068F-reload*.06F,-.045F,.028F,.105F,.044F,polymer); // grip
    rear_notch(-.06F,line,.023F);                         // three-dot sights
    front_post(.11F,line,.023F,false);
    box(0,line-.0012F,.11F-.002F,.0024F,.0024F,.001F,white_dot);
}
void machine_pistol(float reload,float line) {
    // A boxy 9 mm machine pistol: a square receiver, a short threaded barrel, the 30-round stick
    // through the grip and well below it, a wire stock folded along the top.
    tube(.13F,.19F,.009F,.004F,dark);                      // barrel and thread
    box(0,.008F,.04F,.034F,.05F,.17F,black);               // receiver
    box(-.0175F,.018F,.06F,.001F,.012F,.04F,dark);         // ejection port
    box(0,.036F,.04F,.012F,.006F,.13F,dark);               // the folded wire stock
    box(0,.04F,-.04F,.024F,.008F,.012F,dark);
    box(0,-.026F,.04F,.005F,.005F,.04F,polymer);           // trigger guard
    tilted(12,0,-.07F-reload*.06F,-.012F,.03F,.11F,.036F,polymer); // grip
    tilted(12,0,-.16F-reload*.14F,-.03F,.022F,.10F,.028F,black);   // the long magazine below it
    rear_notch(-.03F,line,.034F);
    front_post(.12F,line,.034F,false);
}
void smg(float reload,bool irons,float line) {
    tube(.24F,.30F,.011F,0,dark);                         // barrel and thread
    box(0,0,.07F,.04F,.065F,.34F,polymer);                // receiver
    box(0,.037F,.05F,.022F,.008F,.18F,black);             // rail
    box(0,.038F,.21F,.016F,.012F,.012F,black);            // front sight base
    front_post(.21F,line,.044F,true);
    if(irons) {                                           // rear drum aperture
        box(0,.044F,-.06F,.022F,.006F,.012F,black);
        rear_aperture(-.06F,line,.047F,.0052F,.0032F);
    }
    box(.021F,.01F,.15F,.002F,.01F,.06F,black);           // cocking slot
    tilted(-5,0,-.105F-reload*.12F,.10F,.025F,.13F,.045F,black); // straight magazine
    box(0,-.045F,.018F,.006F,.006F,.045F,black);          // trigger guard
    tilted(18,0,-.075F,-.03F,.03F,.09F,.036F,polymer);    // grip
    box(0,0,-.18F,.02F,.02F,.16F,black);                  // folding stock frame
    box(0,-.045F,-.19F,.02F,.015F,.14F,black);
    box(0,-.022F,-.258F,.035F,.075F,.012F,dark);          // butt plate
}
void shotgun(float reload,float pump,float line) {
    tube(.10F,.55F,.011F,.012F,metal);                    // barrel
    box(0,.0275F,.32F,.010F,.007F,.44F,dark);             // raised vent rib along the barrel
    for(int k=0;k<6;++k) box(0,.026F,.14F+k*.07F,.010F,.004F,.01F,metal); // rib posts
    box(0,line-.0025F,.54F,.005F,.005F,.005F,tan);        // brass bead on the line
    box(0,line-.0015F,.33F,.0028F,.003F,.003F,white_dot); // mid bead
    for(const float side:{-1.0F,1.0F}) box(side*.0055F,line-.003F,.07F,.004F,.008F,.012F,dark); // rear groove on the receiver
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
// Optics, built on the rail top `r` at `z` along the gun.
const Color glass{150,200,225,46}, lens_blue{70,110,150,255}, fibre{120,200,60,255};
void red_dot(float r,float z,bool pistol) {
    const float w=pistol ? .024F : .036F, h=pistol ? .026F : .038F, c=r+(pistol ? .016F : .026F);
    box(0,r+.004F,z,w*.8F,.008F,pistol ? .03F : .04F,black);               // mount
    for(const float side:{-1.0F,1.0F}) box(side*(w*.5F-.002F),c,z,.004F,h,.03F,black); // open hood
    box(0,c+h*.5F,z,w,.005F,.03F,black);
    DrawCube({0,c,z+.008F},w-.006F,h-.008F,.002F,glass);                   // the window
}
void scope(float r,float z,float zoom) {
    const float c=r+(zoom>3 ? .036F : .032F);
    if(zoom>3) {                                          // 4x: a squat prism body
        box(0,r+.008F,z,.03F,.016F,.09F,black);
        box(0,c,z,.034F,.03F,.11F,black);
        tube(z+.05F,z+.088F,.021F,c,black);
        tube(z-.088F,z-.05F,.017F,c,black);
        DrawCylinderEx({0,c,z+.0885F},{0,c,z+.089F},.017F,.017F,10,lens_blue);
        box(0,c+.018F,z,.008F,.006F,.07F,fibre);          // illumination fibre on top
    } else {                                              // 2x: a short tube in rings
        for(const float at:{-.03F,.03F}) box(0,r+.012F,z+at,.016F,.024F,.01F,black);
        tube(z-.06F,z+.05F,.0145F,c,black);
        tube(z+.04F,z+.072F,.019F,c,black);
        tube(z-.075F,z-.05F,.017F,c,black);
        DrawCylinderEx({0,c,z+.0725F},{0,c,z+.073F},.015F,.015F,10,lens_blue);
    }
}
void optic(WeaponId id,Sight sight) {
    if(sight!=Sight::RedDot && sight!=Sight::Scope2x && sight!=Sight::Scope4x) return;
    float rail=.037F,z=.05F;
    if(id==WeaponId::Pistol) { rail=.023F;z=-.02F; }
    else if(id==WeaponId::Smg) { rail=.041F;z=.04F; }
    else if(id==WeaponId::Shotgun) { rail=.030F;z=.03F; box(0,.032F,.03F,.02F,.006F,.12F,black); } // saddle rail
    else if(id==WeaponId::DoubleBarrel) { rail=.030F;z=.0F; box(0,.032F,0,.02F,.006F,.08F,black); }
    else if(id==WeaponId::AutoShotgun) { rail=.046F;z=.10F; }
    else if(id==WeaponId::Rocket) { rail=.058F;z=.02F; box(0,.05F,.02F,.02F,.012F,.12F,black); }
    if(sight==Sight::RedDot) red_dot(rail,z,id==WeaponId::Pistol);
    else scope(rail,z,sight==Sight::Scope4x ? 4 : 2);
}
void double_barrel(float reload,float line) {
    // Side-by-side barrels hinged at the action: they drop open while it reloads.
    rlPushMatrix();
    rlTranslatef(0,-.004F,.05F);rlRotatef(reload*28,1,0,0);rlTranslatef(0,.004F,-.05F);
    for(const float side:{-1.0F,1.0F}) {
        tube(.05F,.50F,.0118F,.008F,metal,side*.0122F);
        DrawCylinderEx({side*.0122F,.008F,.499F},{side*.0122F,.008F,.501F},.0075F,.0075F,10,dark); // the bores
    }
    box(0,.0215F,.28F,.008F,.005F,.44F,dark);            // flat rib between the barrels
    box(0,line-.0025F,.49F,.005F,.005F,.005F,tan);        // brass bead on the line
    box(0,-.012F,.20F,.036F,.026F,.20F,wood);             // splinter fore-end under the barrels
    rlPopMatrix();
    for(const float side:{-1.0F,1.0F}) box(side*.006F,line-.004F,.035F,.003F,.007F,.01F,dark); // rear groove
    box(0,.0F,0,.040F,.05F,.12F,metal);                   // the action (box lock)
    box(0,.03F,-.02F,.008F,.012F,.04F,dark);              // top lever
    for(const float side:{-1.0F,1.0F}) tilted(-30,side*.006F,.026F,-.06F,.004F,.018F,.006F,dark); // twin hammers
    box(0,-.036F,-.01F,.006F,.006F,.05F,black);           // trigger guard
    tilted(-10,0,-.034F,-.11F,.032F,.05F,.12F,wood_dark); // wrist
    box(0,-.05F,-.24F,.038F,.10F,.17F,wood);              // butt
    box(0,-.054F,-.32F,.04F,.112F,.012F,dark);            // recoil pad
}
void auto_shotgun(float reload,bool irons,float line) {
    tube(.40F,.58F,.0125F,0,metal);                       // barrel
    for(int k=0;k<5;++k) tube(.42F+k*.03F,.43F+k*.03F,.0145F,0,dark); // muzzle brake fins
    box(0,0,.24F,.05F,.052F,.32F,polymer);                // ribbed shroud
    for(int k=0;k<6;++k) box(0,.027F,.13F+k*.045F,.04F,.004F,.012F,dark);
    box(0,.002F,-.02F,.054F,.07F,.26F,polymer);           // long receiver, stock in line
    box(0,.04F,.10F,.012F,.012F,.2F,black);               // carry handle rail
    for(const float at:{.0F,.19F}) box(0,.034F,at,.014F,.012F,.014F,black);
    box(0,.049F,.30F,.014F,.012F,.012F,black);            // front sight base
    front_post(.30F,line,.055F,true);
    if(irons) rear_aperture(.02F,line,.046F,.006F,.0035F);
    // The 20-round drum under the receiver (it drops away while it reloads).
    const float drop=reload*.1F;
    DrawCylinderEx({-.03F,-.085F-drop,.10F},{.03F,-.085F-drop,.10F},.062F,.062F,16,black);
    DrawCylinderEx({-.031F,-.085F-drop,.10F},{.031F,-.085F-drop,.10F},.02F,.02F,10,metal);
    box(0,-.035F-drop,.10F,.03F,.03F,.05F,black);         // drum neck
    box(0,-.046F,-.03F,.006F,.006F,.05F,black);           // trigger guard
    tilted(16,0,-.08F,-.07F,.03F,.09F,.036F,polymer);     // pistol grip
    box(0,-.03F,-.20F,.05F,.10F,.10F,polymer);            // butt
    box(0,-.03F,-.256F,.052F,.11F,.012F,dark);            // butt pad
}
void rocket_launcher(float reload,bool irons,float line) {
    tube(-.50F,.50F,.044F,0,olive);                       // the launch tube
    tube(.47F,.55F,.050F,0,dark);                         // muzzle ring
    DrawCylinderEx({0,0,-.58F},{0,0,-.50F},.062F,.046F,14,dark); // venturi bell at the back
    for(const float at:{-.30F,.10F,.36F}) tube(at,at+.03F,.047F,0,black); // bands
    if(reload<.05F) {                                     // the warhead, loaded
        tube(.55F,.62F,.036F,0,olive);
        DrawCylinderEx({0,0,.62F},{0,0,.74F},.04F,.0F,12,Color{92,98,70,255});
        DrawCylinderEx({0,0,.74F},{0,0,.77F},.008F,.003F,6,metal); // the fuse probe
    }
    box(0,.052F,.29F,.012F,.012F,.02F,black);             // front sight mount
    front_post(.29F,line,.06F,true);
    if(irons) {
        box(0,.05F,-.08F,.02F,.012F,.02F,black);
        rear_aperture(-.08F,line,.058F,.0065F,.0038F);
    }
    box(-.05F,.0F,.05F,.012F,.06F,.12F,dark);             // trigger housing on the side
    box(0,-.06F,.0F,.022F,.03F,.06F,black);               // trigger group
    box(0,-.07F,.01F,.006F,.006F,.045F,black);            // trigger guard
    tilted(14,0,-.10F,-.02F,.03F,.085F,.04F,black);       // pistol grip
    tilted(6,0,-.09F,.25F,.03F,.08F,.035F,black);         // fore grip
    box(0,-.05F,-.30F,.03F,.04F,.14F,dark);               // shoulder rest
}
// First-person hands and forearms: trigger hand on the grip, support hand under the fore-end.
void hands(WeaponId id,const GunLayout& layout) {
    if(id==WeaponId::Pistol || id==WeaponId::MachinePistol) {
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

GunLayout gun_layout(WeaponId id,Sight sight) {
    const float line=sight_height(id,sight);
    switch(id) {
        case WeaponId::Pistol: case WeaponId::MachinePistol: return {0,0,line,definition(id).length};
        case WeaponId::Smg: return {.26F,.17F,line,definition(id).length};
        case WeaponId::Shotgun: return {.34F,.24F,line,definition(id).length};
        case WeaponId::Sniper: return {.36F,.24F,line,definition(id).length};
        case WeaponId::DoubleBarrel: return {.33F,.20F,line,definition(id).length};
        case WeaponId::AutoShotgun: return {.26F,.25F,line,definition(id).length};
        case WeaponId::Rocket: return {.50F,.25F,line,definition(id).length};
        default: return {.30F,.22F,line,definition(id).length};
    }
}

Vector3 CombatRenderer::muzzle(Vector3 grip,Vector3 aim,WeaponId id) {
    return Vector3Add(grip,Vector3Scale(Vector3Normalize(aim),definition(id).length));
}

void CombatRenderer::draw_gun(Vector3 grip,Vector3 aim,Vector3 up,WeaponId id,float reload_fraction,float flash,bool draw_hands,float pump,Sight sight) {
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
    const float line=sight_height(id,Sight::Iron);
    switch(id) {
        case WeaponId::Pistol: pistol(reload,line);break;
        case WeaponId::Smg: smg(reload,sight==Sight::Iron,line);break;
        case WeaponId::Shotgun: shotgun(reload,work,line);break;
        case WeaponId::Sniper: sniper(reload,work);break;
        case WeaponId::DoubleBarrel: double_barrel(reload,line);break;
        case WeaponId::AutoShotgun: auto_shotgun(reload,sight==Sight::Iron,line);break;
        case WeaponId::Rocket: rocket_launcher(reload,sight==Sight::Iron,line);break;
        case WeaponId::MachinePistol: machine_pistol(reload,line);break;
        default: rifle(reload,sight==Sight::Iron,line);break;
    }
    if(draw_hands) hands(id,gun_layout(id));
    optic(id,sight);
    if(flash>0) {
        const float l=definition(id).length+.02F,size=id==WeaponId::Rocket ? 2.4F : is_shotgun(id) || id==WeaponId::Sniper ? 1.5F : 1.0F;
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
    // Bullets are not drawn in flight (no tracer "lasers"): the muzzle flash, the report and the
    // impact tell the story, as in PUBG.
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

void CombatRenderer::draw_sight_view(Sight sight,int width,int height,float aimed) {
    const Vector2 middle{width*.5F,height*.5F};
    const Color red{255,36,30,255};
    if(!sight_info(sight).scope) {
        if(sight!=Sight::RedDot || aimed<=.05F) return;
        const auto a=static_cast<unsigned char>(255*std::clamp(aimed,0.0F,1.0F));
        DrawCircleV(middle,5.5F,Color{255,40,30,static_cast<unsigned char>(a/4)});
        DrawCircleV(middle,2.6F,Color{255,60,40,a});
        return;
    }
    // A dark tube round a clear lens, its edge shaded as real glass is.
    const float lens=std::min(width,height)*(sight==Sight::Scope2x ? .40F : sight==Sight::Scope4x ? .44F : .46F);
    DrawRing(middle,lens,std::hypot(static_cast<float>(width),static_cast<float>(height)),0,360,96,BLACK);
    DrawRing(middle,lens-6,lens,0,360,96,Color{25,25,25,255});
    DrawRing(middle,lens-22,lens-6,0,360,96,Color{0,0,0,90});
    if(sight==Sight::Scope2x) {
        // 2x: a red dot inside a red ring, fine stadia lines in from the edge.
        DrawRing(middle,lens*.12F-1.2F,lens*.12F+1.2F,0,360,64,red);
        DrawCircleV(middle,2.8F,red);
        for(const float side:{-1.0F,1.0F}) {
            DrawLineEx({middle.x+side*lens*.55F,middle.y},{middle.x+side*lens,middle.y},1.4F,BLACK);
            DrawLineEx({middle.x,middle.y+side*lens*.55F},{middle.x,middle.y+side*lens},1.4F,BLACK);
        }
    } else if(sight==Sight::Scope4x) {
        // 4x: an illuminated chevron with bullet-drop marks below for longer ranges.
        DrawLineEx({middle.x-lens,middle.y},{middle.x-lens*.10F,middle.y},1.3F,BLACK);
        DrawLineEx({middle.x+lens*.10F,middle.y},{middle.x+lens,middle.y},1.3F,BLACK);
        DrawLineEx({middle.x-10,middle.y+13},{middle.x,middle.y},3,red);
        DrawLineEx({middle.x,middle.y},{middle.x+10,middle.y+13},3,red);
        DrawLineEx({middle.x,middle.y+18},{middle.x,middle.y+lens},1.3F,BLACK);
        int range=2;
        for(const float at:{.10F,.17F,.24F,.31F}) {
            const float half=lens*(.07F-at*.13F);
            DrawLineEx({middle.x-half,middle.y+lens*at},{middle.x+half,middle.y+lens*at},1.3F,BLACK);
            DrawText(TextFormat("%d",range++),static_cast<int>(middle.x+half+5),static_cast<int>(middle.y+lens*at-6),12,BLACK);
        }
        DrawLineEx({middle.x,middle.y-lens},{middle.x,middle.y-lens*.55F},4,BLACK);
    } else {
        // Sniper: fine cross-hairs with thick outer posts and mil ticks.
        DrawLineEx({middle.x-lens,middle.y},{middle.x+lens,middle.y},1.2F,BLACK);
        DrawLineEx({middle.x,middle.y-lens},{middle.x,middle.y+lens},1.2F,BLACK);
        for(const float side:{-1.0F,1.0F}) {
            DrawLineEx({middle.x+side*lens*.45F,middle.y},{middle.x+side*lens,middle.y},5,BLACK);
            DrawLineEx({middle.x,middle.y+side*lens*.45F},{middle.x,middle.y+side*lens},5,BLACK);
            for(int tick=1;tick<=4;++tick) {
                const float at=side*lens*.09F*static_cast<float>(tick);
                DrawLineEx({middle.x+at,middle.y-4},{middle.x+at,middle.y+4},1.2F,BLACK);
                DrawLineEx({middle.x-4,middle.y+at},{middle.x+4,middle.y+at},1.2F,BLACK);
            }
        }
    }
}
}
