// SPDX-License-Identifier: GPL-3.0-only
#include "DeepCrouch.h"
#include "Kneeling.h"
#include <cstdio>
#include <atomic>
#include <algorithm>
#include <cstring>

namespace crouch {
namespace {
const unsigned char* manual;
float* target;
const float* smoothed;
Stance stance;
std::atomic<Level> level{Level::off};
std::atomic<bool> rise{};
bool rising=false, waitRelease=false;
double previous{};
bool operand(const unsigned char* op,const void* expected) {
    const void* address;
    std::memcpy(&address,op,sizeof address);
    return address==expected;
}
}
void bind(controller::Host host) {
    const auto& layout=host.layout->crouch;
    auto code=host.base+layout.update;
    auto flag=host.base+layout.manual;
    auto goal=host.base+layout.target;
    // Verify the relocated loads/stores, smoothing, and final rig field.
    unsigned rigOffset;
    std::memcpy(&rigOffset,code+0xae,sizeof rigOffset);
    if (code[0]!=0xa0 || code[0x0b]!=0xa2 ||
        !operand(code+1,flag) || !operand(code+0x0c,flag) ||
        std::memcmp(code+0x46,"\xf3\x0f\x11\x0d",4) || !operand(code+0x4a,goal) ||
        std::memcmp(code+0x5d,"\xf3\x0f\x10\x0d",4) || !operand(code+0x61,goal) ||
        !operand(code+0x69,host.base+layout.smoothed) ||
        std::memcmp(code+0x7f,"\xf3\x0f\x5c\xc2\xf3\x0f\x5f\xc1",8) ||
        std::memcmp(code+0xaa,"\xf3\x0f\x11\x86",4) || rigOffset!=layout.rigHeightOffset) return;
    manual=flag;
    target=reinterpret_cast<float*>(goal);
    smoothed=reinterpret_cast<const float*>(host.base+layout.smoothed);
    bindLegPose(host);
}
bool available() { return target!=nullptr; }
float filter(Stick stick,float y,std::uint32_t now,Control& control) {
    bool stepUp=false;
    float output=control.filter(level.load(std::memory_order_acquire),stick,y,now,stepUp);
    if (stepUp) rise.store(true,std::memory_order_release);
    return output;
}
void update(Stick stick,float height,double now,void (*log)(const char*)) {
    if (!target || height<=0) return;
    float dt=static_cast<float>(std::clamp(now-previous,0.,0.1));
    previous=now;
    bool request=rise.exchange(false,std::memory_order_acq_rel);
    if (request && stance.active && *manual) { rising=true; waitRelease=true; }
    if (stick==Stick::released) waitRelease=false;
    if (rising && stance.active && *manual && *target==stance.applied) {
        stance.applied=std::min(stance.original,stance.applied+dt*2.f);
        *target=stance.applied;
        if (*target==stance.original) {
            rising=false; stance.active=false; stance.armed=!waitRelease;
            log("deep_crouch=regular sneak_retained=1");
        }
    } else {
        rising=false;
        float next;
        if (stance.update(waitRelease ? Stick::down : stick,*manual!=0,*target,height,next)) {
            *target=next;
            char line[96];
            std::snprintf(line,sizeof line,"deep_crouch=%s target_cm=%.0f",stance.active?"entered":"host_stood",next*100);
            log(line);
        }
    }
    float fraction=stance.active ? std::clamp((stance.original-*smoothed)/.45f,0.f,1.f) : 0.f;
    setKneelBlend(fraction*fraction*(3.f-2.f*fraction));
    level.store(!*manual ? Level::standing : stance.active && !rising ? Level::deep : Level::regular,
                std::memory_order_release);
}
void suspend() {
    setKneelBlend(0);
    level.store(Level::off,std::memory_order_release);
    rise.store(false,std::memory_order_release);
    rising=false; waitRelease=false; previous=0;
    if (target && stance.active && *target==stance.applied) *target=stance.original;
    stance={};
}
}
