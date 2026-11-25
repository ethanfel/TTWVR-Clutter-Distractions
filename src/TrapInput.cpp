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
unsigned char* hostBase{};
constexpr DWORD clockMask=0x00ffffffu;
std::atomic<unsigned> pendingPulse{}, jawGripCapture{};
using Pulse=void (__thiscall *)(void*,bool,float,float);
using Poll=bool (__thiscall *)(void*, unsigned char*);
template<int Backend> struct Input {
    static inline Poll next;
    static inline Pulse pulse;
    static inline crouch::Hold hold;
    static inline crouch::Control control;
    static bool __fastcall poll(void* self, void*, unsigned char* frame) {
        bool ok=next(self,frame);
        unsigned bits=0;
        if (ok && frame && frame[0x12c]) for (unsigned h=0; h<2; ++h) {
            const auto* pose=frame+0x40+h*0x40;
            if (pose[0x1c] && pose[0x1d]) {
                float squeeze; std::memcpy(&squeeze,frame+0x118+4*h,4);
                float trigger; std::memcpy(&trigger,frame+0x110+4*h,4);
                if (trigger>=.55f) bits|=1u<<(6+h);
                bits |= (4u | unsigned(squeeze>=0.55f))<<h;
            }
        }
        float axes[2]{};
        const bool hasActions=ok && frame && frame[0x12c];
        if (hasActions) std::memcpy(axes,frame+0x108,sizeof axes);
        DWORD now=GetTickCount();
        unsigned request=pendingPulse.exchange(0,std::memory_order_relaxed);
        if (pulse && hasActions && (bits&12)==12 && (request&255) &&
            ((now-(request>>9))&0x7fffff)<120) {
            float amplitude=float(request&255)/255.f;
            float duration=(request&256) ? .095f : .030f;
            for (int h=0;h<2;++h) pulse(self,h!=0,amplitude,duration);
        }
        auto capture=jawGripCapture.load(std::memory_order_relaxed);
        if (hasActions && ((now-(capture>>2))&0x3fffffffu)<180) {
            float released=0;
            for (int h=0;h<2;++h) if (capture&(1u<<h))
                std::memcpy(frame+0x118+4*h,&released,sizeof released);
        }
        auto stick=hold.sample(hasActions,axes[0],axes[1],now);
        float filtered=crouch::filter(stick,axes[1],now,control);
        if (hasActions && filtered!=axes[1]) std::memcpy(frame+0x10c,&filtered,sizeof filtered);
        sample.store(((now & clockMask)<<8)|(unsigned(stick)<<4)|bits,std::memory_order_relaxed);
        return ok;
    }
    static bool attach(unsigned char* host, unsigned rva) {
        auto slot=reinterpret_cast<void**>(host+rva)+6;
        void* before=*slot;
        auto method=slot[28-6];
        const unsigned entry=Backend==0 ? 0x3f0d : 0x22868;
        const unsigned body=Backend==0 ? 0x6de400 : 0x6c9140;
        if (method==host+entry || method==host+body) pulse=reinterpret_cast<Pulse>(method);
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
    if (backends) { hostBase=host; crouch::bind(host); }
    return backends;
}
// TTWVR 1.1.7 world-grab slots and its world-space physical pickup pose.
unsigned char* heldObject(int hand) {
    if (!hostBase) return nullptr;
    if (hostBase[0x1538805] && *reinterpret_cast<int*>(hostBase+0x1035e60)==hand)
        return *reinterpret_cast<unsigned char**>(hostBase+0x15395e0);
    if (hostBase[0x15388d4] && *reinterpret_cast<int*>(hostBase+0x15388d8)==hand)
        return *reinterpret_cast<unsigned char**>(hostBase+0x15388dc);
    return nullptr;
}
bool handPoint(int hand,noise::Vec& point) {
    using Pose=bool (__thiscall*)(unsigned char*,int,int,noise::Vec*,float*);
    float matrix[9]{};
    return hostBase && reinterpret_cast<Pose>(hostBase+0x70acb0)(hostBase+0x1d1fab0,hand,0,&point,matrix) && point.valid();
}
static DWORD latest() {
    DWORD value=sample.load(std::memory_order_relaxed);
    return ((GetTickCount()-(value>>8))&clockMask)<=250 ? value : 0;
}
void captureJawGrip(unsigned mask) {
    jawGripCapture.store(mask ? (GetTickCount()<<2)|mask : 0,std::memory_order_relaxed);
}
void jawPulse(float strength,bool latch) {
    pendingPulse.store((GetTickCount()<<9)|(latch?256u:0u)|
        unsigned(std::clamp(strength,0.f,1.f)*255.f),std::memory_order_relaxed);
}
int grips() {
    DWORD value=latest();
    return value&12 ? int(value&15) : -1;
}
unsigned triggers() { return (latest()>>6)&3; }
crouch::Stick rightStick() {
    return static_cast<crouch::Stick>((latest()>>4)&3);
}
}
