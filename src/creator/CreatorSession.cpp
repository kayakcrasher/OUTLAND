#include "outland/creator/CreatorSession.hpp"
#ifdef OUTLAND_DEV_TOOLS
#include "outland/creator/CreatorMapIO.hpp"
#include <cmath>
#include <filesystem>
namespace outland::creator {
void CreatorSession::changed(){dirty_=true;quiet_=0;status_="Unsaved changes - autosaving";}
bool CreatorSession::edit(world::VerdaRegion& region,const std::function<bool()>& operation) {
    auto before=region.settlements();
    if(!operation()){status_="No change - select an object or asset first";return false;}
    if(undo_.size()==16)undo_.erase(undo_.begin());
    undo_.push_back(std::move(before));redo_.clear();changed();return true;
}
bool CreatorSession::undo(world::VerdaRegion& region) {
    if(undo_.empty()){status_="Nothing to undo";return false;}
    redo_.push_back(region.settlements());region.editable_settlements()=std::move(undo_.back());undo_.pop_back();changed();return true;
}
bool CreatorSession::redo(world::VerdaRegion& region) {
    if(redo_.empty()){status_="Nothing to redo";return false;}
    undo_.push_back(region.settlements());region.editable_settlements()=std::move(redo_.back());redo_.pop_back();changed();return true;
}
bool CreatorSession::save(const world::VerdaRegion& region) {
    const bool result=CreatorMapIO::save(region,path_);
    if(result){dirty_=false;status_="SAVED - map restored next launch";}
    else status_="SAVE FAILED - edits kept in memory; try SAVE again";
    quiet_=0;return result;
}
std::string CreatorSession::export_path() const {
    return (std::filesystem::path(path_).parent_path().parent_path()/"exports/verda_world.map").string();
}
bool CreatorSession::export_world(const world::VerdaRegion& region) {
    if(!save(region))return false;
    if(!CreatorMapIO::save(region,export_path())) {
        status_="EXPORT FAILED - personal map is saved";return false;
    }
    status_="EXPORTED - copy exports/verda_world.map into project maps/";
    return true;
}
bool CreatorSession::load(world::VerdaRegion& region) {
    if(dirty_ && !save(region))return false;
    if(!CreatorMapIO::load(region,path_)){status_="LOAD FAILED - current map kept";return false;}
    undo_.clear();redo_.clear();dirty_=false;status_="Saved map loaded";return true;
}
void CreatorSession::update(float dt,const world::VerdaRegion& region) {
    if(!dirty_ || !std::isfinite(dt) || dt<=0)return;
    quiet_+=dt;
    const float delay=status_.starts_with("SAVE FAILED") ? 5.0F:1.0F;
    if(quiet_>=delay)save(region);
}
}
#endif
