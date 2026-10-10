#include "outland/world/navigation/NavGrid.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <queue>

namespace outland::world::navigation {
namespace {
using physics::WorldCollision;

int floor_div(int a,int b) { return a>=0 ? a/b : -((-a+b-1)/b); }
std::uint64_t node_key(int x,int z,int level) {
    // 30 bits per axis is ±268 km of 0.5 m cells; 2 bits of level (max_levels = 4).
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)&0x3FFFFFFFU)<<34) |
           (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)&0x3FFFFFFFU)<<4) | static_cast<std::uint64_t>(level&3);
}
int sign_extend30(std::uint64_t v) {
    const auto bits=static_cast<std::uint32_t>(v&0x3FFFFFFFU);
    return static_cast<int>((bits&0x20000000U) ? (bits|0xC0000000U) : bits);
}
void unpack(std::uint64_t key,int& x,int& z,int& level) {
    x=sign_extend30(key>>34);z=sign_extend30(key>>4);level=static_cast<int>(key&3);
}
float flat(Vector3 a,Vector3 b) { return std::hypot(a.x-b.x,a.z-b.z); }
bool finite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
}

NavGrid::NavGrid(const VerdaRegion& region,NavSettings settings) : region_(region),settings_(settings) {
    settings_.cell=std::clamp(settings_.cell,.25F,2.0F);
    settings_.radius=std::clamp(settings_.radius,.1F,1.0F);
    settings_.step=std::clamp(settings_.step,.1F,1.0F);
    settings_.max_expansions=std::max(100,settings_.max_expansions);
}

void NavGrid::clear() { tiles_.clear();evaluated_=0;occupied_.clear();indexed_=false; }

namespace { constexpr float bucket=8; }
void NavGrid::build_index() {
    occupied_.clear();indexed_=true;
    const auto mark=[&](Vector3 p,float reach) {
        const int x0=static_cast<int>(std::floor((p.x-reach)/bucket)),x1=static_cast<int>(std::floor((p.x+reach)/bucket));
        const int z0=static_cast<int>(std::floor((p.z-reach)/bucket)),z1=static_cast<int>(std::floor((p.z+reach)/bucket));
        for(int z=z0;z<=z1;++z) for(int x=x0;x<=x1;++x)
            occupied_.insert((static_cast<std::uint64_t>(static_cast<std::uint32_t>(x))<<32)|static_cast<std::uint32_t>(z));
    };
    // Reaches match WorldCollision's own early-outs (plus the body radius and a metre of slack).
    for(const auto& settlement:region_.settlements()) {
        for(const auto& building:settlement.buildings) mark(building.position,building.size.x+building.size.z+settings_.radius+1);
        for(const auto& asset:settlement.assets) mark(asset.position,asset.size.x+asset.size.z+settings_.radius+3);
    }
}

bool NavGrid::open_ground(float x,float z) const {
    const int bx=static_cast<int>(std::floor(x/bucket)),bz=static_cast<int>(std::floor(z/bucket));
    return !occupied_.contains((static_cast<std::uint64_t>(static_cast<std::uint32_t>(bx))<<32)|static_cast<std::uint32_t>(bz));
}

Vector3 NavGrid::centre(int x,int z,float y) const {
    return {(static_cast<float>(x)+.5F)*settings_.cell,y,(static_cast<float>(z)+.5F)*settings_.cell};
}

const NavGrid::Cell& NavGrid::cell(int x,int z) {
    const int tx=floor_div(x,tile_cells),tz=floor_div(z,tile_cells);
    const auto key=(static_cast<std::uint64_t>(static_cast<std::uint32_t>(tx))<<32)|static_cast<std::uint32_t>(tz);
    auto& tile=tiles_[key];
    if(!tile) tile=std::make_unique<Tile>();
    auto& c=tile->cells[static_cast<std::size_t>((z-tz*tile_cells)*tile_cells+(x-tx*tile_cells))];
    if(!c.evaluated) {evaluate(x,z,c);c.evaluated=1;++evaluated_;}
    return c;
}

void NavGrid::evaluate(int x,int z,Cell& out) {
    out.count=0;
    const auto at=centre(x,z,0);
    const float terrain=terrain::TerrainHeight::sample(at.x,at.z);
    if(!std::isfinite(terrain)) return;
    if(region_.coastal_layout() && terrain<=layout::sea_level+.3F) return; // the sea
    if(!indexed_) build_index();
    if(open_ground(at.x,at.z)) {out.y[0]=terrain;out.count=1;return;}
    // Surfaces from the top down: roofs, upper floors, stairs, the ground floor, the terrain. Each
    // next query starts far enough below the last that a body would not fit between them.
    float y=WorldCollision::ground_height({at.x,terrain,at.z},terrain+settings_.max_height,region_,settings_.step);
    for(int i=0;i<8 && out.count<max_levels && std::isfinite(y);++i) {
        if(!WorldCollision::body_blocked({at.x,y,at.z},y,region_,settings_.radius)) out.y[out.count++]=y;
        if(y<=terrain+.05F) break;
        const float next=WorldCollision::ground_height({at.x,y,at.z},y-1.9F,region_,settings_.step);
        if(!(next<y-.05F)) break;
        y=next;
    }
}

int NavGrid::level_near(int x,int z,float y) {
    const auto& c=cell(x,z);
    int best=-1;float gap=settings_.step+1e-3F;
    for(int i=0;i<c.count;++i) {
        const float d=std::abs(c.y[static_cast<std::size_t>(i)]-y);
        if(d<=gap) {gap=d;best=i;}
    }
    return best;
}

bool NavGrid::walkable(Vector3 feet) {
    if(!finite(feet)) return false;
    const int x=static_cast<int>(std::floor(feet.x/settings_.cell)),z=static_cast<int>(std::floor(feet.z/settings_.cell));
    return level_near(x,z,feet.y)>=0;
}

bool NavGrid::nearest_walkable(Vector3 feet,float search,Vector3& out) {
    if(!finite(feet) || !std::isfinite(search)) return false;
    const int cx=static_cast<int>(std::floor(feet.x/settings_.cell)),cz=static_cast<int>(std::floor(feet.z/settings_.cell));
    const int reach=std::clamp(static_cast<int>(std::ceil(search/settings_.cell)),0,40);
    float best=1e30F;bool found=false;
    for(int r=0;r<=reach;++r) {
        for(int dz=-r;dz<=r;++dz) for(int dx=-r;dx<=r;++dx) {
            if(std::max(std::abs(dx),std::abs(dz))!=r) continue;
            const auto& c=cell(cx+dx,cz+dz);
            for(int i=0;i<c.count;++i) {
                const auto p=centre(cx+dx,cz+dz,c.y[static_cast<std::size_t>(i)]);
                // Vertical distance counts double: prefer this floor over the one above or below.
                const float d=flat(p,feet)+2*std::abs(p.y-feet.y);
                if(d<best && flat(p,feet)<=search+settings_.cell) {best=d;out=p;found=true;}
            }
        }
        if(found && static_cast<float>(r)*settings_.cell>best) break; // nothing closer further out
    }
    return found;
}

bool NavGrid::clear_line(Vector3 a,Vector3 b) {
    const float length=flat(a,b);
    const float spacing=settings_.cell*.5F;
    const int samples=std::max(1,static_cast<int>(std::ceil(length/spacing)));
    float y=a.y;
    for(int i=1;i<=samples;++i) {
        const float t=static_cast<float>(i)/static_cast<float>(samples);
        const Vector3 p{a.x+(b.x-a.x)*t,0,a.z+(b.z-a.z)*t};
        // The cell under the sample and the cells whose centres surround it must all be standing
        // room at about this height, so a smoothed line keeps the body's clearance.
        const int x=static_cast<int>(std::floor(p.x/settings_.cell)),z=static_cast<int>(std::floor(p.z/settings_.cell));
        const int level=level_near(x,z,y);
        if(level<0) return false;
        const float here=cell(x,z).y[static_cast<std::size_t>(level)];
        const int x0=static_cast<int>(std::floor(p.x/settings_.cell-.5F)),z0=static_cast<int>(std::floor(p.z/settings_.cell-.5F));
        for(int k=0;k<4;++k) if(level_near(x0+(k&1),z0+(k>>1),here)<0) return false;
        y=here;
    }
    return std::abs(y-b.y)<=settings_.step+.05F;
}

bool NavGrid::find_path(Vector3 from,Vector3 to,NavPath& path) {
    path={};
    if(!finite(from) || !finite(to)) return false;
    Vector3 start;
    if(!nearest_walkable(from,1.5F,start)) return false;
    const int sx=static_cast<int>(std::floor(start.x/settings_.cell)),sz=static_cast<int>(std::floor(start.z/settings_.cell));
    const int sl=level_near(sx,sz,start.y);
    if(sl<0) return false;
    // The goal cell: the goal itself, or the nearest standing room to it (a point inside a wall or
    // a tree). With nowhere to stand near the goal there is nothing to search for.
    Vector3 goal_point=to;
    const bool goal_ok=nearest_walkable(to,1.0F,goal_point) || nearest_walkable(to,goal_snap,goal_point);
    if(!goal_ok) return false;
    const int gx=static_cast<int>(std::floor(goal_point.x/settings_.cell)),gz=static_cast<int>(std::floor(goal_point.z/settings_.cell));
    const int gl=goal_ok ? level_near(gx,gz,goal_point.y) : -1;
    const auto goal_key=gl>=0 ? node_key(gx,gz,gl) : ~0ULL;
    const auto heuristic=[&](Vector3 p){return Vector3Distance(p,goal_point);};

    struct Node { float g; std::uint64_t parent; };
    std::unordered_map<std::uint64_t,Node> nodes;
    using Entry=std::pair<float,std::uint64_t>;
    std::priority_queue<Entry,std::vector<Entry>,std::greater<>> open;
    const auto start_key=node_key(sx,sz,sl);
    nodes[start_key]={0,start_key};
    open.push({heuristic(start),start_key});
    auto best_key=start_key;float best_h=heuristic(start);
    bool reached=false;
    int expansions=0;
    const auto cells_before=evaluated_;
    while(!open.empty() && expansions<settings_.max_expansions) {
        if(settings_.max_new_cells>0 && evaluated_-cells_before>static_cast<std::size_t>(settings_.max_new_cells)) {path.budget_hit=true;break;}
        const auto [f,key]=open.top();open.pop();
        int x,z,level;unpack(key,x,z,level);
        const float y=cell(x,z).y[static_cast<std::size_t>(level)];
        const auto here=centre(x,z,y);
        const float g=nodes[key].g;
        if(f>g+heuristic(here)+1e-3F) continue; // stale entry
        ++expansions;
        const float h=heuristic(here);
        if(h<best_h) {best_h=h;best_key=key;}
        if(key==goal_key) {reached=true;best_key=key;break;}
        for(int dz=-1;dz<=1;++dz) for(int dx=-1;dx<=1;++dx) {
            if(!dx && !dz) continue;
            const auto& next=cell(x+dx,z+dz);
            if(dx && dz && (level_near(x+dx,z,y)<0 || level_near(x,z+dz,y)<0)) continue; // no corner cutting
            for(int l=0;l<next.count;++l) {
                const float ny=next.y[static_cast<std::size_t>(l)];
                if(std::abs(ny-y)>settings_.step) continue;
                const auto there=centre(x+dx,z+dz,ny);
                // Climbing costs a little extra so level routes win ties.
                const float cost=Vector3Distance(here,there)+std::abs(ny-y)*.5F;
                const auto next_key=node_key(x+dx,z+dz,l);
                const auto found=nodes.find(next_key);
                if(found!=nodes.end() && found->second.g<=g+cost) continue;
                nodes[next_key]={g+cost,key};
                open.push({g+cost+heuristic(there),next_key});
            }
        }
    }
    path.expansions=expansions;
    path.complete=reached;
    // Walk back from the goal (or the nearest reachable cell) to the start.
    std::vector<Vector3> raw;
    for(auto key=best_key;;) {
        int x,z,level;unpack(key,x,z,level);
        raw.push_back(centre(x,z,cell(x,z).y[static_cast<std::size_t>(level)]));
        const auto parent=nodes[key].parent;
        if(parent==key) break;
        key=parent;
    }
    std::reverse(raw.begin(),raw.end());
    raw.front()=start;
    if(reached) raw.back()=goal_point;
    // String-pulling: keep a waypoint only where the straight walk past it would leave the grid.
    path.points.push_back(raw.front());
    std::size_t anchor=0;
    while(anchor+1<raw.size()) {
        std::size_t far=anchor+1;
        for(std::size_t j=raw.size()-1;j>anchor+1;--j) if(clear_line(raw[anchor],raw[j])) {far=j;break;}
        path.points.push_back(raw[far]);
        anchor=far;
    }
    return path.points.size()>1 || reached;
}

Vector3 PathFollower::steer(Vector3 feet,Vector3 goal,double now,const PathFinder& finder) {
    if(!finder || (flat(feet,goal)<direct_range && std::abs(feet.y-goal.y)<1.0F)) return goal;
    // A new goal starts straight unless it is on another floor.
    if(Vector3Distance(goal,direct_goal_)>goal_moved) {
        direct_goal_=goal;
        routing_=std::abs(goal.y-feet.y)>other_floor;
        if(!routing_) {planned_=false;path_={};next_=0;}
    }
    if(!routing_) return goal;
    const bool exhausted=planned_ && next_>=path_.points.size();
    // A search cut short by the cell budget continues soon; the cells it explored are cached.
    const bool need=!planned_ || Vector3Distance(goal,goal_)>goal_moved || exhausted || path_.budget_hit;
    if(need && now-planned_at_>=(path_.budget_hit ? continue_interval : replan_interval)) {
        planned_at_=now;goal_=goal;
        NavPath fresh;
        if(finder(feet,goal,fresh) && !fresh.points.empty()) {path_=std::move(fresh);next_=0;planned_=true;}
        else {path_={};next_=0;planned_=false;}
    }
    if(!planned_) return goal;
    const auto& points=path_.points;
    // Advance past waypoints that are reached, or whose segment the body has already run along.
    while(next_<points.size()) {
        const auto& p=points[next_];
        if(std::abs(feet.y-p.y)<1.2F && flat(feet,p)<arrive) {++next_;continue;}
        if(next_>0 && next_+1<points.size()) {
            const auto a=points[next_-1];
            const Vector2 seg{p.x-a.x,p.z-a.z},rel{feet.x-a.x,feet.z-a.z};
            const float len2=seg.x*seg.x+seg.y*seg.y;
            if(len2>1e-6F && (rel.x*seg.x+rel.y*seg.y)/len2>=1 && std::abs(feet.y-p.y)<1.2F) {++next_;continue;}
        }
        break;
    }
    if(next_>=points.size()) return path_.complete ? goal : (points.empty() ? goal : points.back());
    if(next_==0) return points[0];
    // Pure pursuit: aim a short way ahead on the current segment, not at the waypoint itself, so a
    // body that arrived off-line is pulled back onto it before narrow places (doors, stair lanes).
    const auto a=points[next_-1],b=points[next_];
    const Vector2 seg{b.x-a.x,b.z-a.z};
    const float len=std::sqrt(seg.x*seg.x+seg.y*seg.y);
    if(len<1e-3F) return b;
    const float along=std::clamp(((feet.x-a.x)*seg.x+(feet.z-a.z)*seg.y)/len,0.0F,len);
    const float ahead=std::min(len,along+lookahead);
    return {a.x+seg.x/len*ahead,a.y+(b.y-a.y)*(ahead/len),a.z+seg.y/len*ahead};
}
}
