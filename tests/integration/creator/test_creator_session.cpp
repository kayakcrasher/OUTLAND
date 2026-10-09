#include "outland/creator/CreatorSession.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace outland;
int main() {
    const auto dir=std::filesystem::temp_directory_path()/"outland_builder_session_test";std::filesystem::remove_all(dir);
    const auto path=dir/"maps/verda_creator.map";world::VerdaRegion region;
    auto& settlements=region.editable_settlements();assert(!settlements.empty());
    settlements.front().id="builder_test";settlements.front().name="Authored world";
    world::Road road;road.start={10,0,20};road.end={30,1,40};road.width=7;settlements.front().roads.push_back(road);
    assert(!settlements.front().buildings.empty());const auto original=settlements.front().buildings.front().position;
    creator::CreatorSession session(path.string());
    assert(session.edit(region,[&]{region.editable_settlements().front().buildings.front().position={100.25F,2.5F,-300.75F};return true;}));
    assert(session.dirty());assert(session.undo(region));assert(region.settlements().front().buildings.front().position.x==original.x);
    assert(session.redo(region));assert(region.settlements().front().buildings.front().position.x==100.25F);
    assert(session.edit(region,[&]{region.editable_settlements().front().assets.clear();return true;}));
    session.update(.5F,region);assert(!std::filesystem::exists(path));session.update(.6F,region);assert(!session.dirty() && std::filesystem::exists(path));
    world::VerdaRegion restored;assert(creator::CreatorMapIO::load(restored,path.string()));
    assert(restored.settlements().size()==region.settlements().size());assert(restored.settlements().front().name=="Authored world");
    assert(restored.settlements().front().buildings.front().position.x==100.25F);assert(restored.settlements().front().assets.empty());
    assert(restored.settlements().front().roads.back().width==7 && restored.settlements().front().roads.back().end.y==1);
    assert(session.undo(region));assert(session.dirty());assert(session.save(region));assert(session.load(restored));
    assert(!session.undo(restored)); // Reload begins a fresh history, current map is preserved.
    assert(creator::CreatorMapIO::writable_path("/app","/phone")=="/phone/.local/share/outland/maps/verda_creator.map");
    assert(creator::CreatorMapIO::writable_path("/app")=="/app/maps/verda_creator.map");
    assert(creator::CreatorMapIO::writable_path("/app","/phone","/custom")=="/custom/maps/verda_creator.map");
    const auto invalid=dir/"invalid.map";{std::ofstream out(invalid);out<<"OUTLAND_CREATOR_MAP 4\nSETTLEMENT \"test\" \"Test\" 0 0 0 1 0 0 0\nROAD broken\n";}
    const auto before=restored.settlements().front().buildings.size();assert(!creator::CreatorMapIO::load(restored,invalid.string()));assert(restored.settlements().front().buildings.size()==before);
    std::filesystem::create_directory(dir/"destination_directory");creator::CreatorSession failure((dir/"destination_directory").string());
    assert(failure.edit(restored,[]{return true;}));assert(!failure.save(restored) && failure.dirty());assert(std::filesystem::is_directory(dir/"destination_directory"));
    // Export uses the same V6 serializer; the published map loads without DEV tools.
    assert(session.edit(region,[&]{region.editable_settlements().front().name="Published Verda";return true;}));
    assert(session.export_world(region) && !session.dirty());
    assert(session.export_path()==(dir/"exports/verda_world.map").string());
    world::VerdaRegion published;assert(creator::CreatorMapIO::load(published,session.export_path()));
    assert(published.settlements().front().name=="Published Verda");
    const auto application=dir/"app",working=dir/"repo",user=dir/"user";
    assert(creator::CreatorMapIO::startup_map(application.string(),"",user.string(),working.string()).empty());
    std::filesystem::create_directories(application/"maps");
    std::filesystem::copy_file(session.export_path(),application/"maps/verda_world.map");
    assert(creator::CreatorMapIO::startup_map(application.string(),"",user.string(),working.string())==(application/"maps/verda_world.map").string());
    std::filesystem::create_directories(working/"maps");
    std::filesystem::copy_file(session.export_path(),working/"maps/verda_world.map");
    assert(creator::CreatorMapIO::startup_map(application.string(),"",user.string(),working.string())==(working/"maps/verda_world.map").string());
    std::filesystem::create_directories(user/"maps");
    std::filesystem::copy_file(path,user/"maps/verda_creator.map");
    assert(creator::CreatorMapIO::startup_map(application.string(),"",user.string(),working.string())==(user/"maps/verda_creator.map").string());
    // An export failure does not destroy the personal save or an existing destination.
    std::filesystem::remove(session.export_path());std::filesystem::create_directory(session.export_path());
    assert(!session.export_world(region) && !session.dirty() && std::filesystem::exists(path));
    assert(std::filesystem::is_directory(session.export_path()));
    std::filesystem::remove_all(dir);
    std::cout<<"[PASS] Full-world persistence, generated edits/deletions/roads, undo/redo, autosave, stable path and transactional failures\n";
}
