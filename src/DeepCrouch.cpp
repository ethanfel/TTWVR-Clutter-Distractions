// SPDX-License-Identifier: GPL-3.0-only
#include "DeepCrouch.h"
#include <cstdio>
#include <cstring>

namespace crouch {
namespace {
const unsigned char* manual;
float* target;
Stance stance;
bool operand(const unsigned char* op,const void* expected) {
    const void* address;
    std::memcpy(&address,op,sizeof address);
    return address==expected;
}
}
void bind(unsigned char* host) {
    // TTWVR 1.1.7 computes this target on manual sneak entry. Its rig update
    // eases toward it each frame and handles standing up. Check the relocated
    // operands as well as the load, store and smoothing instructions.
    auto flag=host+0x10e7094;
    auto goal=host+0x10e70a8;
    if (host[0x7e407]!=0xa0 || host[0x7e412]!=0xa2 ||
        !operand(host+0x7e408,flag) || !operand(host+0x7e413,flag) ||
        std::memcmp(host+0x7e44d,"\xf3\x0f\x11\x0d",4) || !operand(host+0x7e451,goal) ||
        std::memcmp(host+0x7e464,"\xf3\x0f\x10\x0d",4) || !operand(host+0x7e468,goal) ||
        std::memcmp(host+0x7e486,"\xf3\x0f\x5c\xc2\xf3\x0f\x5f\xc1",8) ||
        std::memcmp(host+0x7e4b1,"\xf3\x0f\x11\x86\x48\x0b\x00\x00",8)) return;
    manual=flag;
    target=reinterpret_cast<float*>(goal);
}
bool available() { return target!=nullptr; }
void update(Stick stick,float height,void (*log)(const char*)) {
    if (!target || height<=0) return;
    float next;
    if (stance.update(stick,*manual!=0,*target,height,next)) {
        *target=next;
        char line[96];
        std::snprintf(line,sizeof line,"deep_crouch=%s target_cm=%.0f",stance.active?"entered":"left",next*100);
        log(line);
    }
}
void suspend() {
    if (target && stance.active && *target==stance.applied) *target=stance.original;
    stance={};
}
}
