// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include "Kneeling.h"
#include "World.h"

namespace crouch {
namespace {
using world::Ptr;

// TTWVR 1.1.7 reapplies these world transforms during the scene graph update.
struct BonePose {
    Ptr node;
    Vec position;
    Rotation rotation;
    float scale;
    Ptr callback;
    unsigned flags;
};
static_assert(sizeof(BonePose)==0x40 && offsetof(BonePose,rotation)==0x10);

using FindPose=const BonePose* (__cdecl*)(Ptr);
using ApplyPose=void (__cdecl*)(Ptr,const BonePose*);
FindPose findPose;
ApplyPose applyPose;
Ptr *bones,*boundRoot;
DWORD gameThread;
std::atomic<float> blend{};

bool adjust(Ptr node,BonePose& out,float weight) {
    int index=10;
    while (index<18 && bones[index]!=node) ++index;
    if (index==18) return false;
    Ptr pc=world::player();
    if (!pc || world::root(pc)!=*boundRoot) return false;
    int side=index%2;
    Ptr hip=bones[10+side],knee=bones[12+side],ankle=bones[14+side];
    if (!hip || !knee || !ankle || world::ptr(hip,0x18)!=bones[0] ||
        world::ptr(knee,0x18)!=hip || world::ptr(ankle,0x18)!=knee) return false;
    auto a=findPose(hip),b=findPose(knee),c=findPose(ankle);
    if (!a || !b || !c) return false;
    float yaw=world::field<float>(pc,0x2c);
    Vec forward{std::sin(yaw),std::cos(yaw),0},newKnee,newAnkle;
    if (!kneel(a->position,b->position,c->position,forward,weight,newKnee,newAnkle)) return false;
    if (node==hip) {
        out.rotation=swing(b->position-a->position,newKnee-a->position)*a->rotation;
    } else if (node==knee) {
        out.position=newKnee;
        out.rotation=swing(c->position-b->position,newAnkle-newKnee)*b->rotation;
    } else {
        // The ankle and toes keep the host's sole angle and contact height.
        out.position=out.position+(newAnkle-c->position);
    }
    return true;
}

void __cdecl applyKneelingPose(Ptr node,const BonePose* native) {
    float weight=blend.load(std::memory_order_acquire);
    if (weight>0 && native && node && GetCurrentThreadId()==gameThread) {
        BonePose placed=*native;
        if (adjust(node,placed,weight)) { applyPose(node,&placed); return; }
    }
    applyPose(node,native);
}
} // namespace

void bindLegPose(unsigned char* host) {
    auto call=host+0x45ddef;
    // Match TTWVR's lookup, the two pose arguments and the original call.
    if (host[0x45dddf]!=0xe8 || host+0x45dde4+world::field<int>(host,0x45dde0)!=host+0x45e860 ||
        std::memcmp(call-2,"\x57\x56\xe8",3) ||
        call+5+world::field<int>(call,1)!=host+0x45e290 ||
        std::memcmp(call+5,"\x8b\x47\x38\x83\xc4\x08",6)) return;
    DWORD old,unused;
    if (!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&old)) return;
    gameThread=GetCurrentThreadId();
    bones=reinterpret_cast<Ptr*>(host+0x10e5efc);
    boundRoot=reinterpret_cast<Ptr*>(host+0x10e5ef8);
    findPose=reinterpret_cast<FindPose>(host+0x45e860);
    applyPose=reinterpret_cast<ApplyPose>(host+0x45e290);
    auto displacement=reinterpret_cast<std::uintptr_t>(applyKneelingPose)-reinterpret_cast<std::uintptr_t>(call+5);
    std::memcpy(call+1,&displacement,4);
    FlushInstructionCache(GetCurrentProcess(),call,5);
    VirtualProtect(call,5,old,&unused);
}
void setKneelBlend(float value) { blend.store(value,std::memory_order_release); }
}
