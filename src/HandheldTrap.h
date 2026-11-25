// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Impacts.h"

namespace trap {
using noise::Vec;

// Grip locations on the curved rims in Held.nif's TrapCarryMesh coordinates.
// The front and rear rims pivot about X in opposite directions.
inline Vec jawGrip(int side, int end, float opened) {
    float y = 1.47f + 16.75f * opened;
    float z = 2.55f + 16.75f * std::sqrt(std::max(0.f, 1.f - opened * opened));
    return {end * 9.5f, side * y, z};
}

// A neutral controller holds the trap level, with the opening travel across the
// player's reach. TTWVR then rotates the entire prop with the drawing wrist.
inline Vec intoGripFrame(Vec v) { return {-v.z,-v.x,v.y}; }
inline Vec intoJawFrame(Vec v) { return {-v.y,v.z,-v.x}; }

struct HandOpening {
    int side = 1, end = 1;
    bool pulling = false, latched = false;
    float spread = 0, startY = 0;
    Vec opposite() const { return jawGrip(-side,-end,spread) - jawGrip(side,end,spread); }
    Vec contact{};
    bool inReach(Vec other) const {
        Vec p=other+jawGrip(side,end,spread);
        float x=std::clamp(p.x,-17.f,17.f);
        float radius=18.5f-std::max(0.f,std::abs(x)-5.f)*.43f;
        Vec rim{x,-side*(1.47f+radius*spread),2.55f+radius*std::sqrt(1.f-spread*spread)};
        return other.length()>5 && (p-rim).length()<=9.f;
    }

    void grab(Vec other) {
        pulling = true;
        contact = other;
        startY = other.y + side * 33.5f * spread;
    }
    // Both points are expressed in the moving carrier's frame. Translating
    // or rotating both hands with the tool contributes no opening travel.
    void update(Vec other, bool squeeze, float dt) {
        if (latched) return;
        if (pulling && (!squeeze || !other.valid() || std::abs(other.x-contact.x)>20.f ||
                        std::abs(other.z-contact.z)>20.f)) pulling=false;
        if (!pulling) {
            spread=std::max(0.f,spread-4.f*dt);
            return;
        }
        spread=std::clamp((startY-other.y)*side/33.5f,0.f,1.f);
        if (spread>=0.97f) { spread=1; latched=true; pulling=false; }
    }
};
}
