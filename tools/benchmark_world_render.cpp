// CPU-only benchmark: real foliage/world queries, no window, audio or GPU. Not an FPS claim.
#include "outland/world/foliage/FoliageSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <chrono>
#include <cstdio>
#include <algorithm>
extern "C" {
void DrawTriangle3D(Vector3,Vector3,Vector3,Color) {}
void DrawSphereEx(Vector3,float,int,int,Color) {}
void DrawLine3D(Vector3,Vector3,Color) {}
}
int main() {
    outland::world::VerdaRegion region(false);
    auto& towns=region.runtime_settlements();towns.clear();towns.emplace_back();
    // 6,000 collision props distributed across the country, plus a few nearby obstacles.
    for(int i=0;i<6000;++i) {
        outland::world::WorldAsset asset;
        asset.position={400.0F+(i%100)*8,0,-2000.0F+(i/100)*8};
        asset.collision=true;towns[0].assets.push_back(asset);
    }
    outland::world::foliage::FoliageSystem foliage;
    constexpr int frames=120;
    using Clock=std::chrono::steady_clock;
    double total=0,peak=0,steady=0;
    for(int i=0;i<frames;++i) {
        const auto start=Clock::now();foliage.draw({0,1,0},region);
        const double ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        total+=ms;peak=std::max(peak,ms);if(i>=100) steady+=ms;
    }
    std::printf("6000-prop foliage CPU: %.3f ms mean, %.3f ms peak, %.3f ms warm mean (%d frames)\n",total/frames,peak,steady/20,frames);
}
