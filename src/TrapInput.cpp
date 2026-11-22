// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include "TrapInput.h"

namespace trap {
namespace {
std::atomic<DWORD> sample{};
constexpr DWORD clockMask=0x0fffffffu;
using Poll=bool (__thiscall *)(void*, unsigned char*);
template<int Backend> struct Input {
    static inline Poll next;
    static bool __fastcall poll(void* self, void*, unsigned char* frame) {
        bool ok=next(self,frame);
        unsigned bits=0;
        if (ok && frame && frame[0x12c]) for (unsigned h=0; h<2; ++h) {
            const auto* pose=frame+0x40+h*0x40;
            if (pose[0x1c] && pose[0x1d]) {
                float squeeze; std::memcpy(&squeeze,frame+0x118+4*h,4);
                bits |= (4u | unsigned(squeeze>=0.55f))<<h;
            }
        }
        sample.store(((GetTickCount() & clockMask)<<4)|bits,std::memory_order_relaxed);
        return ok;
    }
    static bool attach(unsigned char* host, unsigned rva) {
        auto slot=reinterpret_cast<void**>(host+rva)+6;
        void* before=*slot;
        MEMORY_BASIC_INFORMATION page{};
        if (!VirtualQuery(before,&page,sizeof page) || page.State!=MEM_COMMIT ||
            !(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))
            return false;
        DWORD protection,ignored;
        if (!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection)) return false;
        next=reinterpret_cast<Poll>(before);
        bool installed=InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(poll),before)==before;
        VirtualProtect(slot,sizeof(void*),protection,&ignored);
        return installed;
    }
};
}
unsigned installInput() {
    auto host=reinterpret_cast<unsigned char*>(GetModuleHandleW(L"TTWVR.dll"));
    if (!host) return 0;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(host);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(host+dos->e_lfanew);
    const unsigned char xr[]{0x88,0x47,0x25,0x8d,0x4c,0x24,0x28},vr[]{0x88,0x41,0x20,0x8d,0x4c,0x24};
    if (nt->FileHeader.TimeDateStamp!=0x6afa9b4f || nt->OptionalHeader.SizeOfImage!=0x1d84000 ||
        std::memcmp(host+0x6e986d,xr,sizeof xr) || std::memcmp(host+0x6c8954,vr,sizeof vr)) return 0;
    unsigned backends=Input<0>::attach(host,0xaf3568)?1u:0u;
    if (Input<1>::attach(host,0xaf36d0)) backends|=2;
    return backends;
}
int grips() {
    DWORD value=sample.load(std::memory_order_relaxed);
    if (!(value&12) || ((GetTickCount()-(value>>4))&clockMask)>250) return -1;
    return int(value&15);
}
}
