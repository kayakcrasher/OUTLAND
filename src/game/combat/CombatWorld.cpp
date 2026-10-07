#include "outland/game/combat/CombatWorld.hpp"
#include "outland/world/assets/VerdaGeometry.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace outland::game::combat {
namespace {
Vector3 add(Vector3 a, Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 sub(Vector3 a, Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 mul(Vector3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
float dot(Vector3 a, Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vector3 cross(Vector3 a, Vector3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vector3 normalized(Vector3 v) {
    const float length=std::sqrt(dot(v,v));
    return length>0.000001F ? mul(v,1.0F/length) : Vector3{0,1,0};
}

bool box(Vector3 start, Vector3 end, Vector3 minimum, Vector3 maximum, float& fraction, Vector3& normal) {
    const auto delta=sub(end,start);
    const float origin[3]{start.x,start.y,start.z}, direction[3]{delta.x,delta.y,delta.z};
    const float lo[3]{minimum.x,minimum.y,minimum.z}, hi[3]{maximum.x,maximum.y,maximum.z};
    float enter=0,exit=1;normal={0,1,0};
    for(int axis=0;axis<3;++axis) {
        if(std::abs(direction[axis])<0.000001F) {
            if(origin[axis]<lo[axis] || origin[axis]>hi[axis]) return false;
            continue;
        }
        float a=(lo[axis]-origin[axis])/direction[axis], b=(hi[axis]-origin[axis])/direction[axis];
        const float sign=direction[axis]>0 ? -1.0F : 1.0F;
        if(a>b) std::swap(a,b);
        if(a>enter) {
            enter=a;normal={0,0,0};
            if(axis==0)normal.x=sign;
            if(axis==1)normal.y=sign;
            if(axis==2)normal.z=sign;
        }
        exit=std::min(exit,b);
        if(enter>exit) return false;
    }
    fraction=enter;return true;
}

bool triangle(Vector3 start, Vector3 end, Vector3 a, Vector3 b, Vector3 c,
              float& fraction, Vector3& normal) {
    const auto direction=sub(end,start), e1=sub(b,a), e2=sub(c,a);
    const auto p=cross(direction,e2);
    const float det=dot(e1,p);
    if(std::abs(det)<0.000001F) return false;
    const auto s=sub(start,a);
    const float u=dot(s,p)/det;
    if(u<0 || u>1) return false;
    const auto q=cross(s,e1);
    const float v=dot(direction,q)/det;
    if(v<0 || u+v>1) return false;
    fraction=dot(e2,q)/det;
    if(fraction<0 || fraction>1) return false;
    normal=normalized(cross(e1,e2));
    if(dot(normal,direction)>0) normal=mul(normal,-1);
    return true;
}

Vector3 local(Vector3 point, Vector3 origin, float yaw) {
    const auto p=sub(point,origin);
    return {p.x*std::cos(yaw)-p.z*std::sin(yaw),p.y,p.x*std::sin(yaw)+p.z*std::cos(yaw)};
}
Vector3 world_normal(Vector3 n, float yaw) {
    return {n.x*std::cos(yaw)+n.z*std::sin(yaw),n.y,-n.x*std::sin(yaw)+n.z*std::cos(yaw)};
}

bool trunk(Vector3 start, Vector3 end, Vector3 base, float radius, float height,
           float& fraction, Vector3& normal) {
    const auto s=sub(start,base),d=sub(end,start);
    const float a=d.x*d.x+d.z*d.z, b=2*(s.x*d.x+s.z*d.z), c=s.x*s.x+s.z*s.z-radius*radius;
    float enter=0,exit=1;
    if(a<0.000001F) { if(c>0) return false; }
    else {
        const float det=b*b-4*a*c;
        if(det<0) return false;
        enter=std::max(enter,(-b-std::sqrt(det))/(2*a));
        exit=std::min(exit,(-b+std::sqrt(det))/(2*a));
    }
    if(std::abs(d.y)<0.000001F) { if(s.y<0 || s.y>height) return false; }
    else {
        float low=-s.y/d.y,high=(height-s.y)/d.y;
        if(low>high)std::swap(low,high);
        enter=std::max(enter,low);exit=std::min(exit,high);
    }
    if(enter>exit) return false;
    fraction=enter;
    const auto point=add(s,mul(d,enter));
    normal=point.y<.001F ? Vector3{0,-1,0}
          : point.y>height-.001F ? Vector3{0,1,0} : normalized({point.x,0,point.z});
    return true;
}
}

CombatWorld::CombatWorld(const world::VerdaRegion& region) : region_(region) { reset_targets(); }

void CombatWorld::reset_targets() {
    for(std::size_t i=0;i<targets_.size();++i) {
        const float x=(static_cast<float>(i)-4)*4;
        targets_[i]={{x,world::terrain::TerrainHeight::sample(x,-18)+1,-18},100,0};
    }
}

void CombatWorld::update(float dt, bool respawn) {
    if(!respawn) return;
    for(auto& target:targets_) {
        if(target.health<=0) {
            target.reset_timer-=std::max(0.0F,dt);
            if(target.reset_timer<=0) { target.health=100;target.reset_timer=0; }
        }
    }
}

void CombatWorld::damage_target(int index, float damage) {
    if(index<0 || index>=static_cast<int>(targets_.size()) || damage<=0) return;
    auto& target=targets_[static_cast<std::size_t>(index)];
    if(target.health<=0) return;
    target.health=std::max(0.0F,target.health-damage);
    if(target.health==0) target.reset_timer=4.0F;
}

BulletHit CombatWorld::trace_segment(Vector3 start, Vector3 end) const {
    BulletHit best;
    const auto delta=sub(end,start);
    const auto record=[&](HitKind kind,float t,Vector3 normal,int target=-1) {
        if(t>=0 && t<=best.fraction && (!best.hit() || t<best.fraction)) {
            best={kind,t,add(start,mul(delta,t)),normal,target,false};
        }
    };
    float t=0;Vector3 n{};
    const float ground=world::terrain::TerrainHeight::sample(0,0);
    if(box(start,end,{-2,ground,-2},{2,ground+3,2},t,n)) record(HitKind::Structure,t,n);
    for(const auto& settlement:region_.settlements()) {
        for(const auto& building:settlement.buildings) {
            const float yaw=building.rotation_y*DEG2RAD;
            const Vector3 origin{building.position.x,
                world::terrain::TerrainHeight::sample(building.position.x,building.position.z),building.position.z};
            const auto a=local(start,origin,yaw),b=local(end,origin,yaw);
            const auto size=building.size;
            if(box(a,b,{-size.x*.5F-.125F,0,-size.z*.5F-.125F},
                       {size.x*.5F+.125F,.44F,size.z*.5F+.125F},t,n))
                record(HitKind::Building,t,world_normal(n,yaw));
            if(box(a,b,{-size.x*.5F,.44F,-size.z*.5F},
                       {size.x*.5F,size.y+.44F,size.z*.5F},t,n))
                record(HitKind::Building,t,world_normal(n,yaw));
            // Match pitched roof geometry rather than allowing shots through roof slopes.
            if(box(a,b,{-size.x*.5F-.35F,size.y+.44F,-size.z*.5F-.35F},
                       {size.x*.5F+.35F,size.y+.44F+size.x*.22F,size.z*.5F+.35F},t,n)) {
                const auto roof=world::assets::roof_vertices({0,.44F,0},size);
                for(const auto& face:world::assets::roof_faces)
                    if(triangle(a,b,roof[face[0]],roof[face[1]],roof[face[2]],t,n))
                        record(HitKind::Building,t,world_normal(n,yaw));
            }
        }
        for(const auto& asset:settlement.assets) {
            if(!asset.collision || asset.type!=world::AssetType::Tree) continue;
            const float scale=.85F+world::assets::variation(static_cast<int>(asset.position.x),
                static_cast<int>(asset.position.z),5)*.45F;
            Vector3 base=asset.position;base.y=world::terrain::TerrainHeight::sample(base.x,base.z);
            if(trunk(start,end,base,.35F*scale,3.5F*scale,t,n)) record(HitKind::Tree,t,n);
        }
    }
    for(std::size_t i=0;i<targets_.size();++i) {
        const auto& target=targets_[i];
        if(target.health<=0)continue;
        if(box(start,end,sub(target.center,{.5F,1,.25F}),add(target.center,{.5F,1,.25F}),t,n)) {
            record(HitKind::Target,t,n,static_cast<int>(i));
            if(best.target==static_cast<int>(i)) best.headshot=best.position.y>target.center.y+.5F;
        }
    }
    // Sample each swept segment, then bisect first ground contact.
    const float length=std::sqrt(dot(delta,delta));
    const int samples=std::max(1,static_cast<int>(std::ceil(length*best.fraction/.5F)));
    const auto clearance=[&](float f) {
        const auto p=add(start,mul(delta,f));
        return p.y-world::terrain::TerrainHeight::sample(p.x,p.z);
    };
    if(clearance(0)<=0)record(HitKind::Ground,0,{0,1,0});
    else for(int i=1;i<=samples;++i) {
        float high=best.fraction*static_cast<float>(i)/samples;
        if(clearance(high)>0)continue;
        float low=best.fraction*static_cast<float>(i-1)/samples;
        for(int iteration=0;iteration<9;++iteration) {
            const float mid=(low+high)*.5F;
            if(clearance(mid)>0)low=mid;else high=mid;
        }
        const auto p=add(start,mul(delta,high));
        const float hx=world::terrain::TerrainHeight::sample(p.x+.1F,p.z)-
                       world::terrain::TerrainHeight::sample(p.x-.1F,p.z);
        const float hz=world::terrain::TerrainHeight::sample(p.x,p.z+.1F)-
                       world::terrain::TerrainHeight::sample(p.x,p.z-.1F);
        record(HitKind::Ground,high,normalized({-hx,.2F,-hz}));
        break;
    }
    return best;
}

} // namespace outland::game::combat
