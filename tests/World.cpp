// Exercise the runtime's 32-bit game layouts and actual mesh traversal with a
// can held motionless. The weapon moves. No headset or game files are required.
#define __thiscall __attribute__((thiscall))
#include "../src/World.h"
#include <cassert>
#include <cstdio>
using namespace world;
static_assert(sizeof(Ptr)==4);
template<unsigned N> struct Blob {
    alignas(16) unsigned char bytes[N]{};
    operator Ptr() { return bytes; }
    template<class T> void set(unsigned offset,T value) {
        assert(offset+sizeof(T)<=N); std::memcpy(bytes+offset,&value,sizeof value);
    }
};
static Ptr __thiscall identity(Ptr p) { return p; }
static Ptr __thiscall empty(Ptr) { return nullptr; }
static Pose pose(float x,float y) { return {{{1,0,0},{0,1,0},{0,0,1}},{x,y,10},1}; }

struct Event { unsigned level; Vec point; Ptr owner,location; unsigned kind; };
static Event event;
static void __thiscall createEvent(Ptr,Ptr owner,float x,float y,float z,unsigned level,unsigned kind,Ptr location) {
    event={level,{x,y,z},owner,location,kind};
}
static Ptr __thiscall currentEvent(Ptr,Ptr pc) {
    return pc==event.owner ? reinterpret_cast<Ptr>(&event) : nullptr;
}
int main() {
    using Cast=Ptr(__thiscall*)(Ptr);
    Cast nodeTable[16]{},meshTable[16]{},collisionTable[16]{};
    nodeTable[3]=identity; nodeTable[7]=empty;
    meshTable[3]=empty; meshTable[7]=identity;
    collisionTable[14]=identity;
    Blob<0x700> pc;
    Blob<0x70> held;
    Blob<0x20> pc3d,item3d;
    Blob<0xb0> mount,item;
    Blob<0xc8> blade;
    Blob<0x40> meshData;
    Blob<0x20> coll,wrapper,shape,bethShape;
    Blob<0x220> body;
    Ptr child=blade;
    Vec vertices[]={{-15,-1,-1},{15,1,1}};
    Bound can{{30,20,10},3.5f};
    pc.set(0x64,Ptr(pc3d)); pc3d.set(0x14,Ptr(mount));
    held.set(0x64,Ptr(item3d)); item3d.set(0x14,Ptr(item));
    mount.set(0,nodeTable); mount.set(8,"Weapon"); mount.set(0xa0,&child);
    mount.set(0xa4,(unsigned short)1); mount.set(0xa6,(unsigned short)1);
    mount.set(0x68,pose(25,20)); blade.set(0,meshTable); blade.set(0xb8,Ptr(meshData));
    meshData.set(8,(unsigned short)2); meshData.set(0x20,&vertices[0]);
    item.set(0,nodeTable); item.set(0x20,&can); item.set(0x1c,Ptr(coll));
    coll.set(0,collisionTable); coll.set(0x10,Ptr(wrapper)); wrapper.set(8,Ptr(body));
    body.set(0x10,Ptr(shape)); shape.set(8,Ptr(bethShape)); bethShape.set(0x10,25u);
    body.set(0x28,(unsigned char)1); body.set(0xe8,(unsigned char)3);
    assert(mobileBody(root(held))==Ptr(body) && material(body)==25);
    body.set(0xe8,(unsigned char)4);
    assert(!mobileBody(root(held)) && mobileBody(root(held),true)==Ptr(body));

    WeaponContact contact;
    auto swing=[&](float y) {
        blade.set(0x68,pose(22,y));
        return contact.sample(pc,held,.02f);
    };
    assert(!swing(8)); assert(swing(20)>0);
    assert(!swing(20)); assert(!swing(8)); assert(swing(29)>0);
    // The other hand holding the weapon produces the same impact.
    contact.reset();
    assert(!swing(32)); assert(swing(20)>0);
    // Just equipping with the can intersecting the weapon produces no sound.
    contact.reset(); assert(!swing(20)); assert(!swing(20));
    // Same item and event owner, two distinct locations. A gentle tap should
    // draw attention to the held can, then a throw should move the search.
    Blob<0x70> process;
    Blob<0x110> functions;
    functions.set(0xfc,&createEvent); functions.set(0xf8,&currentEvent);
    process.set(0,Ptr(functions)); pc.set(0x68,Ptr(process));
    assert(reportAt(pc,held,can.center,29));
    assert(event.owner==pc && event.location==held && event.kind==3 && event.point.x==30);
    noise::Vec landing{322,501,-46};
    assert(reportAt(pc,held,landing,63));
    assert(event.location==held && event.level==63 && (event.point-landing).length()==0);
    puts("Passed player-owned sound event routing at the handheld can and at its later landing.");
    puts("Passed x86 game-layout fixture for grabbed can, weapon geometry, both swing directions and rearming.");
}
