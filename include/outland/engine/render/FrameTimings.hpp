#pragma once
#include <array>
#include <cstdlib>
#include <algorithm>
namespace outland::engine {
enum class FrameStage { Simulation, Terrain, Foliage, World, Characters, Count };
// Wall-clock CPU/submission costs, not GPU timings or EndDrawing's frame limiter.
class FrameTimings {
public:
    FrameTimings() { const auto* s=std::getenv("OUTLAND_PERF");enabled_=s && *s=='1'; }
    bool enabled() const { return enabled_; }
    void begin(double now) { last_=now; }
    void mark(FrameStage stage,double now) {
        if(!enabled_) return;
        const auto i=static_cast<std::size_t>(stage);
        const double ms=std::max(0.0,now-last_)*1000;
        last_=now;
        average_[i]=average_[i]*.9+ms*.1;
        peak_[i]=std::max(peak_[i],ms);
    }
    double average(FrameStage stage) const { return average_[static_cast<std::size_t>(stage)]; }
    double peak(FrameStage stage) const { return peak_[static_cast<std::size_t>(stage)]; }
private:
    bool enabled_{false};
    double last_{0};
    std::array<double,static_cast<std::size_t>(FrameStage::Count)> average_{},peak_{};
};
}
