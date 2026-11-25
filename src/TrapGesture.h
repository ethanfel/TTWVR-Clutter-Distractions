// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Impacts.h"

namespace trap {
using noise::Vec;
enum class Action { none, carry, place, arm, disarm, cancel };

// All measurements here use the trap's own coordinates in game units.
// JawFront and JawRear open along local Y; crossing wrists is fine.
inline bool withinReach(Vec p, bool armed) {
    return p.valid() && std::abs(p.x)<28 && std::abs(p.y)<(armed?34:18) && p.z>-6 && p.z<29;
}
struct Gesture {
    int hand=-1, sign=0;
    bool paired=false, carried=false, pullSounded=false;
    double began=0;
    float separation=0;
    Action step(const Vec (&points)[2], unsigned grips, unsigned pressed, bool armed, double now) {
        if (paired) {
            if (grips!=3) return Action::cancel;
            float span=sign*(points[1].y-points[0].y);
            for (auto p:points)
                if (!p.valid() || std::abs(p.x)>36 || std::abs(p.y)>48 || p.z < -10 || p.z>36)
                    return Action::cancel;
            if (armed) {
                if (!withinReach(points[0],true) || !withinReach(points[1],true) || span<8) return Action::cancel;
                return now-began>=0.7 ? Action::disarm : Action::none;
            }
            return span>=separation+14 && span>=25 && sign*points[0].y<=-5 &&
                sign*points[1].y>=5 ? Action::arm : Action::none;
        }
        if (grips==3 && pressed && withinReach(points[0],armed) && withinReach(points[1],armed)) {
            float delta=points[1].y-points[0].y;
            if (std::abs(delta)>=3 && points[0].y*points[1].y<=0) {
                sign=delta>0 ? 1 : -1; separation=std::abs(delta); began=now;
                paired=true; return Action::none;
            }
        }
        if (hand<0 && !armed) {
            for (int h=0;h<2;++h) if ((pressed & (1u<<h)) && withinReach(points[h],false)) {
                hand=h; began=now; break;
            }
        }
        if (hand<0) return Action::none;
        if (!(grips & (1u<<hand))) return carried ? Action::place : Action::cancel;
        if (grips==3 || now-began<0.14) return Action::none;
        carried=true;
        return Action::carry;
    }
};
}
