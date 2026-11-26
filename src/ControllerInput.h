// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <cstring>
#include "Impacts.h"
#include "HostLayout.h"

namespace controller {
using Poll=bool (__thiscall *)(void*,unsigned char*);
constexpr unsigned clockMask=0x00ffffffu;

inline Host host() {
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(L"TTWVR.dll"));
    if (!base) return {};
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
    // Both backends store frame validity at offset 0x12c before returning.
    const unsigned char storeValid[]={0x88,0x86,0x2c,0x01,0,0};
    for (const auto& build:builds) {
        if (nt->FileHeader.TimeDateStamp==build.timestamp && nt->OptionalHeader.SizeOfImage==build.imageSize &&
            !std::memcmp(base+build.xr.validStore,storeValid,sizeof storeValid) &&
            !std::memcmp(base+build.vr.validStore,storeValid,sizeof storeValid)) return {base,&build};
    }
    return {};
}
inline unsigned buttons(bool ok,const unsigned char* frame) {
    unsigned bits=0;
    if (!ok || !frame || !frame[0x12c]) return 0;
    for (unsigned h=0; h<2; ++h) {
        const auto* pose=frame+0x40+h*0x40;
        if (!pose[0x1c] || !pose[0x1d]) continue;
        float squeeze, trigger;
        std::memcpy(&squeeze,frame+0x118+4*h,4);
        std::memcpy(&trigger,frame+0x110+4*h,4);
        if (trigger>=.55f) bits|=1u<<(6+h);
        bits|=(4u | unsigned(squeeze>=.55f))<<h;
    }
    return bits;
}
inline bool attach(void** slot,Poll& next,void* replacement) {
    void* before=*slot;
    MEMORY_BASIC_INFORMATION page{};
    if (!VirtualQuery(before,&page,sizeof page) || page.State!=MEM_COMMIT ||
        !(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))
        return false;
    DWORD protection,ignored;
    if (!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection)) return false;
    next=reinterpret_cast<Poll>(before);
    bool installed=InterlockedCompareExchangePointer(slot,replacement,before)==before;
    VirtualProtect(slot,sizeof(void*),protection,&ignored);
    return installed;
}
// Read the same two world-grab slots and physical pose that TTWVR uses.
inline unsigned char* heldObject(Host host,int hand) {
    if (!host) return nullptr;
    for (const auto* slot:{&host.layout->primary,&host.layout->secondary}) {
        if (host.base[slot->active] && *reinterpret_cast<int*>(host.base+slot->hand)==hand)
            return *reinterpret_cast<unsigned char**>(host.base+slot->object);
    }
    return nullptr;
}
inline bool handPoint(Host host,int hand,noise::Vec& point) {
    using Pose=bool (__thiscall*)(unsigned char*,int,int,noise::Vec*,float*);
    float matrix[9]{};
    if (!host) return false;
    auto pose=reinterpret_cast<Pose>(host.base+host.layout->physicalPose);
    return pose(host.base+host.layout->manager,hand,0,&point,matrix) && point.valid();
}
}
