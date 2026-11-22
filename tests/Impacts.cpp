#include "../src/Impacts.h"
#include <cassert>
#include <cstdio>
using namespace noise;
int main() {
    Motion thrown;
    auto toss=[&](Vec v,bool held,bool floor,double t){return thrown.step(v,held,false,floor,t,0.02f).source;};
    assert(toss({4,0,.5f},true,false,1)==Source::none);
    assert(toss({4,0,.3f},false,false,1.02)==Source::none);
    assert(toss({4,0,.1f},false,false,1.04)==Source::none);
    assert(toss({.4f,0,.5f},false,true,1.06)==Source::landing);
    assert(toss({.3f,0,.1f},false,false,1.08)==Source::none);
    assert(toss({},false,true,1.10)==Source::none);
    assert(toss({},true,false,3)==Source::none);
    assert(toss({0,0,-2},false,false,3.02)==Source::none);
    assert(toss({0,0,.3f},false,false,3.04)==Source::landing); // short terrain contact

    Motion bump;
    assert(bump.step({},false,true,true,0,.02f).source==Source::none);
    assert(bump.step({1,0,0},false,true,true,.02,.02f).source==Source::bump);
    assert(bump.step({1,0,0},false,true,true,.04,.02f).source==Source::none);
    Motion held;
    held.step({1.8f,0,0},true,false,false,0,.02f);
    assert(held.step({},true,false,true,.02,.02f).source==Source::knock);
    assert(held.step({},true,false,true,.04,.02f).source==Source::none);
    Motion placed;
    placed.step({0,0,-.18f},true,false,false,0,.02f);
    assert(placed.step({},false,false,true,.02,.02f).source==Source::none);

    // Positions are the stationary can transformed into the moving weapon's frame.
    Box blade{{-15,-.6f,-.4f},{15,.6f,.4f}};
    Tap tap;
    assert(tap.step(blade,{8,6,0},1,1,.02f)==0);
    assert(tap.step(blade,{8,0,0},1,1,.02f)>1);
    assert(tap.step(blade,{8,.2f,0},1,1,.02f)==0);
    assert(tap.step(blade,{8,7,0},1,1,.02f)==0);
    assert(tap.step(blade,{8,-7,0},1,1,.02f)>1); // crossed between frames
    assert(tap.step(blade,{26,-7,0},1,1,.02f)==0);
    assert(tap.step(blade,{26,7,0},1,1,.02f)==0); // above the tip
    Tap initial;
    assert(initial.step(blade,{0,0,0},1,1,.02f)==0); // equipping in contact
    assert(surface(25).light==0x1f0c4 && surface(25).hard==0x1f0c3);
    assert(level(1,surface(25))>level(1,surface(1)));
    assert(level(3,surface(25))>level(1,surface(25)));
    puts("Passed throws, short contacts, bounces, re-grabs, quiet placement, bumps and stationary-can weapon taps.");
}
