// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Impacts.h"

namespace clutter {
using noise::Vec;
constexpr float gameUnitsPerMetre=70.f;
constexpr float havokUnitsPerMetre=gameUnitsPerMetre/6.99912548f;

// Estimate release speed over a short part of the swing. A few frames between
// the end of a flick and opening the physical grip should still give a toss.
class ThrowMotion {
    struct Sample { Vec point{}; double time{}; };
    std::array<Sample,40> samples{};
    unsigned next{},count{};
public:
    void sample(Vec relative,double now) {
        if (!relative.valid() || relative.length()>200.f) { count=0; return; }
        if (count) {
            auto& last=samples[(next+samples.size()-1)%samples.size()];
            if (now==last.time) { last.point=relative; return; }
            if (now<last.time || now-last.time>.12 || (relative-last.point).length()>55.f) count=0;
        }
        samples[next]={relative,now};
        next=(next+1)%samples.size();
        count=std::min<unsigned>(count+1,samples.size());
    }
    Vec release(double now) const {
        Vec best{};
        for (unsigned i=0;i<count;++i) {
            const auto& end=samples[(next+samples.size()-1-i)%samples.size()];
            if (now-end.time>.045) break;
            for (unsigned j=i+1;j<count;++j) {
                const auto& start=samples[(next+samples.size()-1-j)%samples.size()];
                double duration=end.time-start.time;
                if (duration>.11) break;
                if (duration<.055) continue;
                Vec speed=(end.point-start.point)*float(1/(duration*gameUnitsPerMetre));
                // Prefer the latest direction on reversals.
                if ((best.x*speed.x+best.y*speed.y+best.z*speed.z)>=0 && speed.length()>best.length())
                    best=speed;
                break;
            }
        }
        float speed=best.length();
        if (speed<.75f) return {};
        return best*std::min(1.65f,12.f/speed);
    }
};
}
