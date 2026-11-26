// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstdio>
#include "ClutterInput.h"
#include "ControllerInput.h"

namespace clutter {
namespace {
controller::Host host;
std::atomic<DWORD> sample{};
int (__cdecl* sharedGrips)();
bool (__cdecl* portableTrap)(unsigned char*);
void (*logInput)(const char*);
template<int BackendIndex> struct PassiveInput {
    static inline controller::Poll next;
    static bool __fastcall poll(void* self,void*,unsigned char* frame) {
        bool ok=next(self,frame);
        unsigned bits=controller::buttons(ok,frame)&15u;
        sample.store(((GetTickCount()&controller::clockMask)<<8)|bits,std::memory_order_relaxed);
        return ok;
    }
    static bool attach(unsigned table) {
        auto slot=reinterpret_cast<void**>(host.base+table)+6;
        return controller::attach(slot,next,reinterpret_cast<void*>(poll));
    }
};
void installPassive() {
    bool xr=host && PassiveInput<0>::attach(host.layout->xr.table);
    bool vr=host && PassiveInput<1>::attach(host.layout->vr.table);
    char line[80];
    std::snprintf(line,sizeof line,"clutter_input=passive openxr=%d openvr=%d",xr,vr);
    logInput(line);
}
}
void initializeInput(void (*logger)(const char*)) {
    logInput=logger;
    host=controller::host();
    // When running together use the original grip, sampled before the jaw filter.
    if (HMODULE traps=GetModuleHandleW(L"TTWVRBearTraps.dll")) {
        auto read=GetProcAddress(traps,"TTWVRBearTrapGripsV1");
        auto test=GetProcAddress(traps,"TTWVRBearTrapIsPortableV1");
        if (read && test) {
            std::memcpy(&sharedGrips,&read,sizeof read);
            std::memcpy(&portableTrap,&test,sizeof test);
            logger("clutter_input=shared_bear_traps");
            return;
        }
    }
    installPassive();
}
int grips() {
    if (sharedGrips) {
        int state=sharedGrips();
        if (state!=noProvider) return state;
        sharedGrips=nullptr;
        installPassive();
    }
    DWORD value=sample.load(std::memory_order_relaxed);
    bool fresh=((GetTickCount()-(value>>8))&controller::clockMask)<=250;
    return fresh && (value&12) ? int(value&15) : -1;
}
unsigned char* heldObject(int hand) { return controller::heldObject(host,hand); }
bool handPoint(int hand,noise::Vec& point) { return controller::handPoint(host,hand,point); }
bool isPortable(unsigned char* ref) { return portableTrap && portableTrap(ref); }
}
