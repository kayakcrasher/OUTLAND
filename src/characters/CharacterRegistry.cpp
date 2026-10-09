#include "outland/characters/CharacterRegistry.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace outland::characters {
namespace {
std::vector<std::string> columns(std::string line) {
    if (!line.empty() && line.back()=='\r') line.pop_back();
    std::vector<std::string> result;
    std::size_t start=0;
    for (;;) {
        const auto end=line.find('\t',start);
        result.push_back(line.substr(start,end==std::string::npos ? end : end-start));
        if (end==std::string::npos) break;
        start=end+1;
    }
    return result;
}
bool pack_path(const std::string& value) {
    if (!value.starts_with("assets/verda/characters/") || value.find('\\')!=std::string::npos) return false;
    for (const auto& part:std::filesystem::path(value)) if (part==".." || part==".") return false;
    return true;
}
std::string runtime_path(const std::string& path) {
    if (std::filesystem::path(path).extension()!=".fbx") return path;
    auto output=std::filesystem::path(path);
    output.replace_extension(".glb");
    if (path.starts_with("assets/verda/characters/first_person_arms/")) return output.generic_string();
    return "assets/verda/characters/runtime/"+output.generic_string().substr(std::string("assets/verda/characters/").size());
}
}
bool CharacterRegistry::load(const std::string& path, std::string& error) {
    error.clear();
    std::ifstream stream(path);
    std::string line;
    if (!stream || !std::getline(stream,line)) { error="Cannot read character manifest: "+path; return false; }
    if (line.starts_with("\xEF\xBB\xBF")) line.erase(0,3);
    if (columns(line)!=std::vector<std::string>{"id","name","category","role","model","texture","source"}) {
        error="Invalid character manifest header"; return false;
    }
    std::vector<CharacterDefinition> parsed;
    std::size_t number=1;
    while (std::getline(stream,line)) {
        ++number;
        if (line.empty() || line=="\r") continue;
        const auto fields=columns(line);
        auto fail=[&](const std::string& reason) { error="Character manifest line "+std::to_string(number)+": "+reason; return false; };
        if (fields.size()!=7) return fail("expected seven TSV fields");
        if (fields[0].empty() || fields[1].empty() || fields[6].empty()) return fail("empty identity/source");
        if (!pack_path(fields[4]) || (!fields[5].empty() && !pack_path(fields[5]))) return fail("path outside character pack");
        const auto extension=std::filesystem::path(fields[4]).extension();
        if (extension!=".fbx" && extension!=".glb" && extension!=".gltf") return fail("unsupported source model format");
        CharacterPool category;
        if (fields[2]=="civilian") category=CharacterPool::Civilian;
        else if (fields[2]=="emergency" || fields[2].starts_with("emergency_")) category=CharacterPool::Emergency;
        else if (fields[2]=="hostile") category=CharacterPool::Hostile;
        else if (fields[2]=="creature") category=CharacterPool::Creature;
        else if (fields[2]=="player_arms") category=CharacterPool::Arms;
        else return fail("unknown category: "+fields[2]);
        if ((category!=CharacterPool::Arms && fields[3]!="npc") ||
            (category==CharacterPool::Arms && fields[3]!="first_person" && fields[3]!="legacy_first_person")) return fail("incompatible role/category");
        CharacterDefinition asset{fields[0],fields[1],fields[2],fields[3],fields[4],runtime_path(fields[4]),fields[5],fields[6],category};
        asset.z_up=asset.source=="elbolilloduro_characters_psx";
        asset.facing_degrees=asset.z_up ? -90.0F : 0.0F;
        auto duplicate=std::find_if(parsed.begin(),parsed.end(),[&](const auto& existing){return existing.id==asset.id;});
        if (duplicate!=parsed.end()) {
            if (duplicate->model_path!=asset.model_path || duplicate->category!=asset.category || duplicate->role!=asset.role ||
                duplicate->texture_path!=asset.texture_path || duplicate->source!=asset.source || duplicate->name!=asset.name ||
                duplicate->source_model==asset.source_model) return fail("conflicting/duplicate ID: "+asset.id);
            if (extension!=".fbx") *duplicate=std::move(asset);
        } else parsed.push_back(std::move(asset));
    }
    if (stream.bad() || parsed.empty()) { error="Empty or unreadable character manifest"; return false; }
    std::sort(parsed.begin(),parsed.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    if (std::none_of(parsed.begin(),parsed.end(),[](const auto& asset){return asset.pool==CharacterPool::Civilian;})) {
        error="Manifest requires a civilian player body"; return false;
    }
    assets_=std::move(parsed);
    return true;
}
const CharacterDefinition* CharacterRegistry::find(const std::string& id) const {
    const auto result=std::find_if(assets_.begin(),assets_.end(),[&](const auto& asset){return asset.id==id;});
    return result==assets_.end() ? nullptr : &*result;
}
const CharacterDefinition* CharacterRegistry::find_model(const std::string& path) const {
    const auto result=std::find_if(assets_.begin(),assets_.end(),[&](const auto& asset){return asset.model_path==path;});
    return result==assets_.end() ? nullptr : &*result;
}
std::vector<const CharacterDefinition*> CharacterRegistry::pool(CharacterPool category) const {
    std::vector<const CharacterDefinition*> result;
    for (const auto& asset:assets_) if (asset.pool==category && asset.role=="npc") result.push_back(&asset);
    return result;
}
const CharacterDefinition* CharacterRegistry::choose(CharacterPool category,std::uint64_t seed) const {
    const auto matches=[&](const auto& asset){return asset.pool==category && asset.role=="npc";};
    const auto count=std::count_if(assets_.begin(),assets_.end(),matches);
    if (count==0) return nullptr;
    auto remaining=seed%static_cast<std::uint64_t>(count);
    for (const auto& asset:assets_) if (matches(asset)) {
        if (remaining--==0) return &asset;
    }
    return nullptr;
}
const CharacterDefinition* CharacterRegistry::player() const { return choose(CharacterPool::Civilian,0); }
std::uint64_t character_seed(const std::string& key) {
    std::uint64_t result=14695981039346656037ULL;
    for (const unsigned char byte:key) { result^=byte; result*=1099511628211ULL; }
    return result;
}
CharacterPool npc_pool_for_marker(const std::string& id) {
    // Creator-generated IDs and user-authored IDs use the same backward-compatible prefix.
    for (const auto& prefix:{std::string("creator_marker_npc_spawn_"),std::string("npc_spawn_")}) {
        if (!id.starts_with(prefix)) continue;
        const auto suffix=id.substr(prefix.size());
        const auto matches=[&](const char* name){return suffix==name || suffix.starts_with(std::string(name)+"_");};
        if (matches("emergency")) return CharacterPool::Emergency;
        if (matches("hostile")) return CharacterPool::Hostile;
        if (matches("creature")) return CharacterPool::Creature;
    }
    return CharacterPool::Civilian;
}
} // namespace outland::characters
