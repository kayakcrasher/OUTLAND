#pragma once
#ifdef OUTLAND_DEV_TOOLS
#include "outland/world/VerdaRegion.hpp"
#include <functional>
#include <string>
#include <vector>
namespace outland::creator {
// World-edit history and durable saves are separate from UI/input/rendering.
class CreatorSession {
public:
    explicit CreatorSession(std::string path):path_(std::move(path)){}
    bool edit(world::VerdaRegion& region,const std::function<bool()>& operation);
    bool undo(world::VerdaRegion& region);
    bool redo(world::VerdaRegion& region);
    bool save(const world::VerdaRegion& region);
    bool load(world::VerdaRegion& region);
    bool export_world(const world::VerdaRegion& region);
    std::string export_path() const;
    void update(float dt,const world::VerdaRegion& region);
    void runtime_changed(){changed();}
    bool dirty() const {return dirty_;}
    const std::string& status() const {return status_;}
    const std::string& path() const {return path_;}
private:
    using Snapshot=std::vector<world::Settlement>;
    std::vector<Snapshot> undo_,redo_;
    std::string path_,status_{"Ready - changes autosave"};
    bool dirty_{false};float quiet_{0};
    void changed();
};
}
#endif
