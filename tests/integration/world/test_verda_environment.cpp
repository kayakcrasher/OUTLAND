// Headless behavior checks against the real raylib declarations.
// Draw/audio calls are recorded; no graphics or audio device is needed.
#include "outland/world/assets/VerdaGeometry.hpp"
#include "outland/world/assets/GroundSurface.hpp"
#include "outland/world/assets/VerdanArchitecture.hpp"
#include "outland/world/foliage/FoliageSystem.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/engine/audio/EnvironmentAudio.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {
struct Triangle { Vector3 a,b,c; Color color; };
std::vector<Triangle> triangles;
int cubes=0, sphere_triangles=0, played=0, unloaded=0, closed=0;
bool device=false, allow_device=true;
unsigned int sound_id=0;
Vector3 normal(Vector3 a, Vector3 b, Vector3 c) {
    const Vector3 u{b.x-a.x,b.y-a.y,b.z-a.z}, v{c.x-a.x,c.y-a.y,c.z-a.z};
    return {u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
}
}

extern "C" {
void DrawTriangle3D(Vector3 a,Vector3 b,Vector3 c,Color color) { triangles.push_back({a,b,c,color}); }
void DrawCube(Vector3,float,float,float,Color) { ++cubes; }
void DrawSphereEx(Vector3,float,int rings,int slices,Color) { sphere_triangles += 2*(rings-1)*slices; }
void DrawCylinderEx(Vector3,Vector3,float,float,int,Color) {}
void DrawLine3D(Vector3,Vector3,Color) {}
void rlPushMatrix() {}
void rlPopMatrix() {}
void rlTranslatef(float,float,float) {}
void rlRotatef(float,float,float,float) {}
bool IsAudioDeviceReady() { return device; }
void InitAudioDevice() { device=allow_device; }
void CloseAudioDevice() { device=false; ++closed; }
bool FileExists(const char*) { return true; }
Music LoadMusicStream(const char*) { Music m{}; m.ctxData=&device; return m; }
bool IsMusicValid(Music m) { return m.ctxData!=nullptr; }
void PlayMusicStream(Music) {}
void PauseMusicStream(Music) {}
void ResumeMusicStream(Music) {}
void UpdateMusicStream(Music) {}
void SetMusicVolume(Music,float v) { assert(v>=0 && v<=1); }
void UnloadMusicStream(Music) { ++unloaded; }
Sound LoadSound(const char*) { Sound s{}; s.frameCount=++sound_id; return s; }
bool IsSoundValid(Sound s) { return s.frameCount!=0; }
void SetSoundVolume(Sound,float v) { assert(v>=0 && v<=1); }
void SetSoundPitch(Sound,float v) { assert(v>=.97F && v<=1.04F); }
void PlaySound(Sound) { ++played; }
void UnloadSound(Sound) { ++unloaded; }
}

int main() {
    using namespace outland::world;
    using namespace outland::world::assets;
    const Vector3 base{10,2,-4}, size{8,4,7};
    const auto roof=roof_vertices(base,size);
    assert(roof[4].y > base.y+size.y);
    const Vector3 inside{base.x,base.y+size.y+.3F,base.z};
    for(const auto& face : roof_faces) {
        const auto a=roof[face[0]],b=roof[face[1]],c=roof[face[2]];
        const auto n=normal(a,b,c);
        // Closed roof must face away from its interior, including gable ends.
        assert(n.x*(inside.x-a.x)+n.y*(inside.y-a.y)+n.z*(inside.z-a.z)<0);
    }
    for(int x : {-100,-1,0,1,100,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
        const float v=variation(x,x,45);
        assert(v>=0 && v<1 && v==variation(x,x,45));
    }
    Road diagonal{{0,0,0},{10,0,10},2,RoadType::Gravel};
    assert(on_road({5,0,5},diagonal));
    assert(!on_road({5,0,8},diagonal));
    Road zero{{1,0,1},{1,0,1},2,RoadType::Dirt};
    assert(!on_road({1,0,1},zero));

    VerdaRegion region(false); // Legacy training-world geometry remains unchanged.
    assert(ground_surface({0,0,-40},region)==GroundSurface::Dirt);
    assert(ground_surface({20,0,-72},region)==GroundSurface::Gravel);
    assert(ground_surface({70,0,-70},region)==GroundSurface::Grass);
    region.draw({0,1,-70});
    int road_triangles=0;
    for(const auto& t : triangles) {
        if(t.color.r!=161 && t.color.r!=157) continue;
        if(t.color.g!=128 && t.color.g!=151) continue;
        ++road_triangles;
        assert(normal(t.a,t.b,t.c).y>0);
        for(const auto p : {t.a,t.b,t.c})
            assert(std::abs(p.y-terrain::TerrainHeight::sample(p.x,p.z)-.06F)<.00001F);
    }
    assert(road_triangles==190);
    const int near_cost=cubes+sphere_triangles;
    cubes=0;sphere_triangles=0;
    region.draw({0,1,110});
    assert(cubes+sphere_triangles<near_cost);

    // Mutate the generated, non-const region solely to construct a collision fixture.
    auto& settlements=const_cast<std::vector<Settlement>&>(region.settlements());
    settlements.clear();settlements.emplace_back();
    Building b;b.position={0,0,0};b.size={8,4,2};b.rotation_y=90;
    settlements[0].buildings.push_back(b);
    // The rotated hollow shell blocks the side wall, not its playable interior.
    assert(physics::WorldCollision::blocked({0,0,4},region,.1F));
    assert(!physics::WorldCollision::blocked({0,0,3},region,.1F));
    assert(!physics::WorldCollision::blocked({3,0,0},region,.1F));
    // Vaulting crosses a real front window only when facing toward the wall.
    settlements[0].buildings[0].rotation_y = 0;
    Vector3 landing{};
    assert(physics::WorldCollision::window_vault_target({2.32F,1,-2}, {0,0,1}, region, landing));
    assert(landing.z > -1);
    assert(!physics::WorldCollision::window_vault_target({2.32F,1,-2}, {0,0,-1}, region, landing));
    region.generate_training_region();

    triangles.clear();
    foliage::FoliageSystem foliage;
    foliage.draw({0,1,-70},region);
    assert(!triangles.empty());
    assert(triangles.size()%2==0);
    for(std::size_t i=0;i<triangles.size();i+=2) {
        const auto& a=triangles[i];const auto& b2=triangles[i+1];
        const auto na=normal(a.a,a.b,a.c),nb=normal(b2.a,b2.b,b2.c);
        assert(na.x*nb.x+na.y*nb.y+na.z*nb.z<0);
        for(const auto& settlement : region.settlements())
            for(const auto& road : settlement.roads)
                assert(!on_road(a.a,road));
    }
    {
        outland::engine::audio::EnvironmentAudio audio("test-assets");
        assert(audio.ready());
        audio.update({70,1,0},true,true,region);
        audio.update({71,1,0},true,true,region);
        audio.update({72,1,0},true,true,region);assert(played==1);
        audio.update({72,1,0},true,true,region);assert(played==1);
        audio.update({73,1,0},false,true,region);
        audio.update({74,1,0},true,true,region);assert(played==1);
        audio.update({100,1,0},true,true,region);assert(played==1);
        audio.toggle_mute();audio.set_volume(2);assert(audio.volume()==1);
        audio.update({101,1,0},true,true,region);
        audio.update({102,1,0},true,true,region);assert(played==1);
        audio.toggle_mute();
        audio.update({103,1,0},true,false,region);
        audio.update({104,1,0},true,true,region);assert(played==1);
        audio.update({105,1,0},true,true,region);
        audio.update({106,1,0},true,true,region);assert(played==2);
        outland::game::combat::WeaponEvents shot;shot.shots=1;
        audio.play_combat(outland::game::combat::WeaponId::Rifle,shot);assert(played==3);
        audio.toggle_mute();
        shot.target_hits=1;shot.reload_started=true;shot.dry_fire=true;
        audio.play_combat(outland::game::combat::WeaponId::Pistol,shot);assert(played==3);
    }
    assert(unloaded==19 && closed==1);
    allow_device=false;
    {
        outland::engine::audio::EnvironmentAudio unavailable("test-assets");
        assert(!unavailable.ready());
        unavailable.toggle_mute();
        unavailable.update({0,0,0},true,true,region);
    }
    assert(closed==1);
    std::cout<<"[PASS] Roof winding, road terrain/winding, LOD reduction, rotated collision, two-sided foliage/exclusion, audio cadence/mute/cleanup/failure\n";
}
