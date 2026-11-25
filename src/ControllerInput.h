// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <cstring>
#include "Impacts.h"

namespace controller {
using Poll=bool (__thiscall *)(void*,unsigned char*);
constexpr unsigned clockMask=0x00ffffffu;

inline unsigned char* host() {
    auto host=reinterpret_cast<unsigned char*>(GetModuleHandleW(L"TTWVR.dll"));
    if (!host) return nullptr;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(host);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(host+dos->e_lfanew);
    const unsigned char xr[]{0x88,0x47,0x25,0x8d,0x4c,0x24,0x28},vr[]{0x88,0x41,0x20,0x8d,0x4c,0x24};
    if (nt->FileHeader.TimeDateStamp!=0x6afa9b4f || nt->OptionalHeader.SizeOfImage!=0x1d84000 ||
        std::memcmp(host+0x6e986d,xr,sizeof xr) || std::memcmp(host+0x6c8954,vr,sizeof vr)) return nullptr;
    return host;
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
// TTWVR 1.1.7 world-grab slots and its world-space physical pickup pose.
inline unsigned char* heldObject(unsigned char* hostBase,int hand) {
    if (!hostBase) return nullptr;
    if (hostBase[0x1538805] && *reinterpret_cast<int*>(hostBase+0x1035e60)==hand)
        return *reinterpret_cast<unsigned char**>(hostBase+0x15395e0);
    if (hostBase[0x15388d4] && *reinterpret_cast<int*>(hostBase+0x15388d8)==hand)
        return *reinterpret_cast<unsigned char**>(hostBase+0x15388dc);
    return nullptr;
}
inline bool handPoint(unsigned char* hostBase,int hand,noise::Vec& point) {
    using Pose=bool (__thiscall*)(unsigned char*,int,int,noise::Vec*,float*);
    float matrix[9]{};
    return hostBase && reinterpret_cast<Pose>(hostBase+0x70acb0)(hostBase+0x1d1fab0,hand,0,&point,matrix) && point.valid();
}
}
