// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include "TrapInput.h"
#include "ControllerInput.h"

namespace trap {
namespace {
std::atomic<DWORD> sample{};
controller::Host boundHost;
using controller::clockMask;
std::atomic<unsigned> pendingPulse{}, jawGripCapture{};
using Pulse=void (__thiscall *)(void*,bool,float,float);
using controller::Poll;
template<int Backend> struct Input {
    static inline Poll next;
    static inline Pulse pulse;
    static inline crouch::Hold hold;
    static inline crouch::Control control;
    static bool __fastcall poll(void* self, void*, unsigned char* frame) {
        bool ok=next(self,frame);
        unsigned bits=controller::buttons(ok,frame);
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
    static bool attach(unsigned char* host, const controller::Backend& backend) {
        auto slot=reinterpret_cast<void**>(host+backend.table)+6;
        auto method=slot[28-6];
        if (method==host+backend.pulseEntry || method==host+backend.pulseBody) pulse=reinterpret_cast<Pulse>(method);
        return controller::attach(slot,next,reinterpret_cast<void*>(poll));
    }
};
}
unsigned installInput() {
    auto host=controller::host();
    if (!host) return 0;
    unsigned backends=Input<0>::attach(host.base,host.layout->xr)?1u:0u;
    if (Input<1>::attach(host.base,host.layout->vr)) backends|=2;
    if (backends) { boundHost=host; crouch::bind(host); }
    return backends;
}
bool inputAvailable() { return bool(boundHost); }
unsigned char* heldObject(int hand) { return controller::heldObject(boundHost,hand); }
bool handPoint(int hand,noise::Vec& point) { return controller::handPoint(boundHost,hand,point); }
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
