#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace noise {
struct Vec {
    float x{}, y{}, z{};
    Vec operator+(Vec b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec operator*(float s) const { return {x*s,y*s,z*s}; }
    float length() const { return std::sqrt(x*x+y*y+z*z); }
    bool valid() const { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); }
};
struct Box { Vec lo, hi; };

// Segment versus an expanded model-space box. Sampling only its endpoints misses fast taps.
inline bool crosses(Box b, Vec from, Vec to, float margin) {
    float enter=0, leave=1;
    auto slab=[&](float a,float z,float lo,float hi) {
        lo-=margin; hi+=margin;
        float step=z-a;
        if (std::abs(step)<0.00001f) return a>=lo && a<=hi;
        float first=(lo-a)/step, last=(hi-a)/step;
        if (first>last) std::swap(first,last);
        enter=std::max(enter,first); leave=std::min(leave,last);
        return enter<=leave;
    };
    return slab(from.x,to.x,b.lo.x,b.hi.x) && slab(from.y,to.y,b.lo.y,b.hi.y)
        && slab(from.z,to.z,b.lo.z,b.hi.z);
}

struct Tap {
    Vec previous{};
    bool sampled=false, touching=false;
    float step(Box box, Vec local, float radius, float scale, float dt) {
        if (!local.valid() || !(scale>0)) { *this={}; return 0; }
        bool inside=crosses(box,local,local,radius);
        float speed=0;
        if (sampled && !touching && dt>0 && dt<0.12f && crosses(box,previous,local,radius)) {
            // World game units -> metres. Reject model swaps / discontinuous poses.
            float relative=(local-previous).length()*scale/(70.f*dt);
            if (relative>=0.50f && relative<18.f) speed=relative;
        }
        touching=inside || (touching && crosses(box,local,local,radius+1.f/scale));
        previous=local; sampled=true;
        return speed;
    }
};

enum class Source { none, landing, bump, knock, strike };
struct Impact { Source source{}; float speed{}; };
struct Motion {
    Vec before{};
    bool seeded=false, heldBefore=false, playerBefore=false, contactBefore=false, spent=false;
    double lastBump=-10;
    bool thrown=false;
    double releasedAt{};
    void beginFlight(double now) { releasedAt=now; thrown=true; seeded=false; spent=false; }

    Impact step(Vec velocity,bool held,bool playerTouch,bool solid,double now,float dt) {
        Impact out;
        if (!velocity.valid()) { *this={}; return out; }
        const float speed=velocity.length(), previousSpeed=before.length();
        const float delta=(velocity-before).length();
        const bool continuous=seeded && dt>0 && dt<0.12f;
        if (held) {
            thrown=false; spent=false;
            if (continuous && heldBefore && solid && !contactBefore && previousSpeed>=0.65f && delta>=0.38f)
                out={Source::knock,previousSpeed};
        } else if (thrown) {
            // Speeds here are Havok units/s. Free flight slows on the ascent.
            // A solid hit changes horizontal velocity or interrupts gravity.
            float threshold=std::max(2.f,previousSpeed*.12f);
            float horizontal=std::hypot(before.x,before.y);
            float forwardLoss=horizontal>0 ? horizontal-(velocity.x*before.x+velocity.y*before.y)/horizontal : 0;
            bool rebound=before.z < -threshold && velocity.z-before.z>threshold;
            bool confirmed=solid && !playerTouch && (!contactBefore || forwardLoss>threshold || rebound);
            // The host finishes handing off the grab spring on release. Without
            // a contact manifold, infer a brief bounce only after that handoff.
            bool deflected=now-releasedAt>.10 && (rebound || forwardLoss>threshold);
            if (continuous && previousSpeed>=.75f && (confirmed || deflected)) {
                out={Source::landing,previousSpeed}; spent=true; thrown=false;
            }
        } else if (playerTouch && now-lastBump>0.35 &&
                   ((!playerBefore && speed>=0.65f) || (continuous && speed-previousSpeed>=0.65f))) {
            out={Source::bump,speed}; lastBump=now; spent=false;
        } else if (continuous && !spent && !playerTouch && previousSpeed>=0.75f) {
            // A real collision can begin and end between samples (particularly on terrain).
            // A change from the host's grab spring to throw velocity is not a collision.
            bool lostSpeed=previousSpeed-speed>=0.38f;
            bool abrupt=delta>std::max(0.6f,20.f*dt) && lostSpeed;
            if ((!heldBefore || solid) && ((solid && (lostSpeed || (!contactBefore && !heldBefore)))
                                         || (!heldBefore && abrupt))) {
                out={Source::landing,previousSpeed}; spent=true;
            }
        }
        before=velocity; heldBefore=held; playerBefore=playerTouch; contactBefore=solid; seeded=true;
        return out;
    }
};

// FalloutNV.esm SOUN forms and Havok material numbers. Native collisions supply
// their own audio. These forms voice a VR weapon strike that needs a foley sound.
struct Surface { unsigned light, hard; float loudness; };
inline Surface surface(unsigned mat) {
    switch (mat & 0xff) {
        case 25: return {0x1f0c4,0x1f0c3,1.10f}; // tin can
        case 24: case 3: return {0x1f0c2,0x1f0c1,1.04f};
        case 1: return {0x6339d,0x6339e,0.27f};
        case 4: case 6: case 7: return {0x44ee3,0x44ee3,0.45f};
        case 9: case 12: return {0xa795f,0xab7b1,0.80f};
        case 13: return {0x72dbc,0x72dbc,1.05f};
        case 14: return {0x2b0ba,0x2b0ba,0.67f};
        case 16: case 22: return {0x7c102,0x7befb,1.10f};
        case 17: return {0x7befd,0x7c103,1.10f};
        case 23: return {0x1f0bf,0x1f0c0,1.15f};
        case 26: return {0x5fccd,0x5fccc,0.9f};
        case 27: return {0x63399,0x63398,1.f};
        case 28: return {0x9950f,0x99510,1.1f};
        case 29: return {0x72dbd,0x72dbe,1.05f};
        case 30: return {0x47758,0x47758,0.75f};
        case 31: return {0x47759,0x47759,0.45f};
        case 5: case 11: case 20: case 21: return {0x62acc,0x62acd,1.f};
        case 0: case 10: case 15: case 19: return {0xa5388,0xa5389,0.80f};
        default: return {0,0,0.65f};
    }
}
inline int level(float speed,Surface s) {
    return std::clamp(static_cast<int>((15.f+speed*12.f)*s.loudness),1,85);
}
} // namespace noise
