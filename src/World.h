// Read-only FalloutNV 1.4.0.525 layouts. Offsets follow JIP LN's GameObjects,
// netimmerse and havok headers. All access is confined to the game thread.
#pragma once
#include "Impacts.h"
#include <cstring>
#include <cstdint>
#include <vector>

namespace world {
using noise::Vec;
using noise::Box;
using Ptr = unsigned char*;
template<typename T> T field(Ptr p,unsigned offset) {
    T result{};
    if (p) std::memcpy(&result,p+offset,sizeof result);
    return result;
}
inline Ptr ptr(Ptr p,unsigned offset) { return field<Ptr>(p,offset); }
inline Ptr player() { return *reinterpret_cast<Ptr*>(0x11dea3c); }
inline unsigned id(Ptr ref) { return field<unsigned>(ref,0xc); }
inline Ptr lookup(unsigned formID) {
    auto map=*reinterpret_cast<Ptr*>(0x11c54c0);
    unsigned count=field<unsigned>(map,4);
    Ptr buckets=ptr(map,8);
    if (!formID || !buckets || !count) return nullptr;
    for (Ptr entry=ptr(buckets,4*(formID%count));entry;entry=ptr(entry,0))
        if (field<unsigned>(entry,4)==formID) return ptr(entry,8);
    return nullptr;
}
inline bool loose(Ptr ref) {
    return field<unsigned char>(ref,4)==0x3a &&
           (field<unsigned>(ref,8)&(0x20|0x800))==0; // deleted / initially disabled
}
inline Ptr root(Ptr ref) { return ptr(ptr(ref,0x64),0x14); }
inline Ptr as(Ptr object,unsigned slot) {
    if (!object) return nullptr;
    using Cast=Ptr(__thiscall *)(Ptr);
    return field<Cast>(ptr(object,0),slot*4)(object);
}
class Children {
    Ptr data{}; unsigned count{};
public:
    explicit Children(Ptr object) {
        if (Ptr node=as(object,3)) {
            data=ptr(node,0xa0);
            auto used=field<unsigned short>(node,0xa6);
            if (data && used<=field<unsigned short>(node,0xa4)) count=used;
        }
    }
    unsigned size() const { return count; }
    Ptr operator[](unsigned idx) const { return ptr(data,idx*4); }
};
inline Ptr named(Ptr n,const char* name,unsigned depth=0) {
    if (!n || depth>35) return nullptr;
    const char* s=field<const char*>(n,8);
    if (s && std::strcmp(s,name)==0) return n;
    Children children(n);
    for (unsigned i=0;i<children.size();++i)
        if (auto result=named(children[i],name,depth+1)) return result;
    return nullptr;
}
inline Ptr mobileBody(Ptr n,bool held=false,unsigned depth=0) {
    if (!n || depth>14) return nullptr;
    Ptr collision=as(ptr(n,0x1c),14);
    Ptr wrapper=ptr(collision,0x10);
    Ptr body=ptr(wrapper,8);
    unsigned motion=field<unsigned char>(body,0xe8);
    if (field<unsigned char>(body,0x28)==1 && ((motion&2) || (held && motion==4)))
        return body;
    Children children(n);
    for (unsigned i=0;i<children.size();++i)
        if (auto result=mobileBody(children[i],held,depth+1)) return result;
    return nullptr;
}
inline unsigned material(Ptr body) {
    auto wrapper=ptr(ptr(body,0x10),8);
    return wrapper ? field<unsigned>(wrapper,0x10) : 255;
}
// These are the same actor-process methods used for the stock generic sound
// event. Use the current world coordinate even when a grabbed reference's
// stored TESObjectREFR translation has not caught up to its rendered mesh.
inline bool reportAt(Ptr pc,Ptr ref,Vec point,unsigned level) {
    Ptr process=ptr(pc,0x68);
    if (!process || !point.valid()) return false;
    using Create=void (__thiscall*)(Ptr,Ptr,float,float,float,unsigned,unsigned,Ptr);
    using Read=Ptr (__thiscall*)(Ptr,Ptr);
    auto table=ptr(process,0);
    field<Create>(table,0xfc)(process,pc,point.x,point.y,point.z,level,3,ref);
    Ptr event=field<Read>(table,0xf8)(process,pc);
    return event && field<unsigned>(event,0)==level &&
           (field<Vec>(event,4)-point).length()<2.f;
}
struct Bound { Vec center; float radius; };
inline Bound bound(Ptr n) { return field<Bound>(ptr(n,0x20),0); }
struct Pose {
    float rot[3][3]; Vec pos; float scale;
    Vec local(Vec v) const {
        v=(v-pos)*(1.f/scale);
        return {rot[0][0]*v.x+rot[1][0]*v.y+rot[2][0]*v.z,
                rot[0][1]*v.x+rot[1][1]*v.y+rot[2][1]*v.z,
                rot[0][2]*v.x+rot[1][2]*v.y+rot[2][2]*v.z};
    }
};
struct Piece { Ptr object{},data{}; Box box{}; noise::Tap tap{}; unsigned frame{}; };

class WeaponContact {
    std::vector<Piece> pieces;
    unsigned frame=0;
    Ptr previousMount{},previousHeld{};
    bool inContact=false;
    float visit(Ptr n,Bound item,float dt,unsigned depth) {
        if (!n || depth>16 || (field<unsigned>(n,0x30)&1)) return 0;
        if (Ptr geometry=as(n,7)) { // NiTriBasedGeom, excludes particle systems
            Ptr data=ptr(geometry,0xb8);
            auto vertices=field<const Vec*>(data,0x20);
            unsigned count=field<unsigned short>(data,8);
            if (!count || !vertices) return 0;
            auto it=std::find_if(pieces.begin(),pieces.end(),[=](auto& p){return p.object==n && p.data==data;});
            if (it==pieces.end()) {
                Box box{vertices[0],vertices[0]};
                for (unsigned i=1;i<count;++i) {
                    auto v=vertices[i];
                    box.lo={std::min(box.lo.x,v.x),std::min(box.lo.y,v.y),std::min(box.lo.z,v.z)};
                    box.hi={std::max(box.hi.x,v.x),std::max(box.hi.y,v.y),std::max(box.hi.z,v.z)};
                }
                pieces.push_back({n,data,box,{},frame});
                it=pieces.end()-1;
            }
            it->frame=frame;
            auto pose=field<Pose>(n,0x68);
            float scale=std::abs(pose.scale);
            if (!(scale>0.001f && scale<100)) return 0;
            return it->tap.step(it->box,pose.local(item.center),item.radius/scale,scale,dt);
        }
        float speed=0;
        Children children(n);
        for (unsigned i=0;i<children.size();++i)
            speed=std::max(speed,visit(children[i],item,dt,depth+1));
        return speed;
    }
public:
    void reset() { pieces.clear(); previousMount=nullptr; previousHeld=nullptr; inContact=false; }
    float sample(Ptr pc,Ptr held,float dt) {
        Bound item=bound(root(held));
        if (!item.center.valid() || !(item.radius>0 && item.radius<250)) { reset(); return 0; }
        // The inscribed part of a small clutter bound keeps a near miss quiet.
        item.radius=std::clamp(item.radius*0.65f,0.7f,14.f);
        Ptr third=named(root(pc),"Weapon");
        Ptr first=named(ptr(pc,0x694),"Weapon");
        auto distance=[&](Ptr n) {
            return n && !(field<unsigned>(n,0x30)&1) && Children(n).size() ? (field<Pose>(n,0x68).pos-item.center).length() : 1e9f;
        };
        float thirdDistance=distance(third), firstDistance=distance(first);
        Ptr mount=thirdDistance<=firstDistance ? third : first;
        if ((previousMount==third || previousMount==first) &&
            distance(previousMount)<std::min(thirdDistance,firstDistance)+7.f) mount=previousMount;
        if (!mount || distance(mount)>180.f) { reset(); return 0; }
        if (mount!=previousMount || held!=previousHeld) reset();
        previousMount=mount; previousHeld=held; ++frame;
        float speed=visit(mount,item,dt,0);
        pieces.erase(std::remove_if(pieces.begin(),pieces.end(),[&](auto& p){return p.frame!=frame;}),pieces.end());
        bool wasTouching=inContact;
        inContact=std::any_of(pieces.begin(),pieces.end(),[](auto& p){return p.tap.touching;});
        return wasTouching ? 0 : speed;
    }
};
} // namespace world
