#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/sky/DayNight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland::world;
namespace {
void check(bool condition,const std::string& message) {
    if(!condition) {std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
int diff(Color a,Color b) {return std::max({std::abs(a.r-b.r),std::abs(a.g-b.g),std::abs(a.b-b.b)});}
}
int main() {
    const auto noon=sky::sky_at(12),midnight=sky::sky_at(0);
    check(noon.ambient.r==255 && noon.ambient.g==255 && noon.ambient.b==255,"noon is unlit by the multiply");
    check(noon.daylight==1 && noon.lamps==0 && noon.stars==0,"noon: full day, lamps off, no stars");
    check(noon.sun.y>.9F && noon.moon.y<0,"sun overhead at noon");
    check(midnight.daylight==0 && midnight.lamps==1 && midnight.stars==1,"midnight: dark, lamps on, stars out");
    check(midnight.sun.y<0 && midnight.moon.y>.5F,"moon up at midnight");
    check(midnight.ambient.r<80 && midnight.ambient.b>midnight.ambient.r,"nights are dark and blue");
    check(sky::sky_at(7.5F).lamps==0 && sky::sky_at(20.5F).lamps>.9F,"lamps off in the morning, on at night");
    check(sky::sky_at(5.0F).sun.y<0 && sky::sky_at(7.0F).sun.y>0 && sky::sky_at(19.0F).sun.y<0,"sunrise around 6, sunset around 18-19");
    // Warm light at the ends of the day.
    const auto dusk=sky::sky_at(18.8F);
    check(dusk.horizon.r>dusk.horizon.b+60,"sunset horizon is warm");
    // Smooth: minute to minute nothing jumps.
    for(int minute=0;minute<24*60;++minute) {
        const auto a=sky::sky_at(minute/60.0F),b=sky::sky_at((minute+1)/60.0F);
        check(diff(a.ambient,b.ambient)<=6 && diff(a.zenith,b.zenith)<=6 && diff(a.horizon,b.horizon)<=8,"smooth colours at minute "+std::to_string(minute));
        check(std::abs(a.daylight-b.daylight)<=.02F && std::abs(a.lamps-b.lamps)<=.05F,"smooth daylight at minute "+std::to_string(minute));
    }
    for(float h=4.5F;h<8;h+=.25F) check(sky::sky_at(h+.25F).daylight>=sky::sky_at(h).daylight,"dawn brightens");
    for(float h=17;h<21;h+=.25F) check(sky::sky_at(h+.25F).daylight<=sky::sky_at(h).daylight,"dusk darkens");
    // Hours wrap; nonsense is noon.
    check(diff(sky::sky_at(25).ambient,sky::sky_at(1).ambient)==0 && diff(sky::sky_at(-1).zenith,sky::sky_at(23).zenith)==0,"hours wrap");
    check(sky::sky_at(NAN).daylight==1,"NaN hour is noon");
    // The sky drawn before the multiply shows as wanted after it.
    for(const float h:{0.0F,5.5F,12.0F,19.5F,22.0F}) {
        const auto s=sky::sky_at(h);
        const auto pre=sky::before_multiply(s.zenith,s.ambient);
        const Color shown{static_cast<unsigned char>(pre.r*s.ambient.r/255),static_cast<unsigned char>(pre.g*s.ambient.g/255),static_cast<unsigned char>(pre.b*s.ambient.b/255),255};
        check(diff(shown,s.zenith)<=3,"pre-multiplied sky at "+std::to_string(h));
    }
    // Lights in the published world: downtown's street lights and the trailer park's fire pit.
    VerdaRegion region(true);
    check(outland::creator::CreatorMapIO::load(region,std::string(OUTLAND_SOURCE_DIR)+"/maps/verda_world.map"),"load world map");
    const auto lights=sky::collect_lights(region);
    int lamps=0,fires=0;
    for(const auto& light:lights) {
        if(light.kind==sky::LightKind::StreetLamp) {
            ++lamps;
            check(light.bulb.y>light.ground.y+5.5F && light.radius>5,"lamp heads are up on the pole");
            check(std::hypot(light.bulb.x-light.ground.x,light.bulb.z-light.ground.z)<2,"pool under the lamp head");
        } else ++fires;
    }
    std::cout<<lamps<<" street lamps, "<<fires<<" fires\n";
    check(lamps>=80 && fires>=1,"street lamps and the park fire are lights");
    std::cout<<"day/night tests passed\n";
}
