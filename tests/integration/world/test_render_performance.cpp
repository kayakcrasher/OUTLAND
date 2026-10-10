#include "outland/engine/render/ViewFrustum.hpp"
#include "outland/engine/render/FrameTimings.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
int main() {
    using namespace outland::engine;
    Camera3D camera{{0,2,0},{0,2,-1},{0,1,0},60,CAMERA_PERSPECTIVE};
    ViewFrustum landscape(camera,16.0F/9),portrait(camera,9.0F/16);
    assert(landscape.sphere({0,2,-100},1));
    assert(!landscape.sphere({0,2,100},1));
    assert(!landscape.sphere({100,2,-10},1));
    assert(!landscape.sphere({0,100,-10},1));
    assert(landscape.sphere({8,2,-10},1) && !portrait.sphere({8,2,-10},1));
    assert(landscape.sphere({0,2,0},2)); // camera inside an object
    assert(landscape.sphere({100,2,-10},120)); // large geometry spanning the view
    assert(landscape.sphere({11,2,-10},1)); // sphere intersects the side plane
    camera.target={0,2,1};ViewFrustum turned(camera,1);
    assert(turned.sphere({0,2,100},1) && !turned.sphere({0,2,-100},1));
    camera.projection=CAMERA_ORTHOGRAPHIC;ViewFrustum ortho(camera,1);
    assert(ortho.sphere({200,2,100},1)); // unsupported projection fails open
    setenv("OUTLAND_PERF","1",1);FrameTimings timings;
    assert(timings.enabled());timings.begin(1);timings.mark(FrameStage::Simulation,1.002);
    assert(timings.peak(FrameStage::Simulation)>1.99 && timings.peak(FrameStage::Simulation)<2.01);
    timings.mark(FrameStage::Foliage,1.003);
    assert(timings.peak(FrameStage::Foliage)>.99 && timings.peak(FrameStage::Foliage)<1.01);
    std::cout<<"[PASS] Conservative camera culling, portrait aspect, large bounds and measured frame stages\n";
}
