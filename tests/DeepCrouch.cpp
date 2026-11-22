#include "../src/DeepCrouch.h"
#include <cassert>
using namespace crouch;
int main() {
    Hold hold;
    constexpr std::uint32_t start=0xfffffa00u; // crosses GetTickCount rollover
    for (unsigned dt=0;dt<3000;dt+=100)
        assert(hold.sample(true,0,-1,start+dt)==Stick::down);
    assert(hold.sample(true,0,-0.6f,start+3000)==Stick::held);
    assert(hold.sample(true,-1,-0.7f,start+3001)==Stick::released);
    assert(hold.sample(true,0,-0.6f,start+3002)==Stick::released);
    assert(hold.sample(true,0,-1,start+3003)==Stick::down);
    assert(hold.sample(true,0,-1,start+7000)==Stick::down);
    assert(hold.sample(false,0,-1,start+7001)==Stick::invalid);
    assert(hold.sample(true,0,NAN,start+7002)==Stick::invalid);
    Stance s;
    float next=0;
    auto step=[&](Stick stick,bool manual,float current) { return s.update(stick,manual,current,.55f,next); };
    assert(!step(Stick::held,true,1.20f)); // held while entering gameplay
    assert(!step(Stick::released,false,1.20f));
    assert(!step(Stick::held,false,1.20f)); // standing / physical sneak
    assert(!step(Stick::down,true,1.20f));
    assert(step(Stick::held,true,1.20f) && next==.55f);
    assert(!step(Stick::released,true,.55f) && s.active);
    assert(step(Stick::released,false,.55f) && next==1.20f && !s.active);
    assert(!step(Stick::released,true,.45f));
    assert(!step(Stick::held,true,.45f)); // avoid raising an already lower stance
    assert(!step(Stick::released,true,1.15f));
    assert(step(Stick::held,true,1.15f));
    assert(!step(Stick::released,false,1.30f) && !s.active); // host took ownership
    assert(step(Stick::held,true,1.30f));
    assert(step(Stick::invalid,true,.55f) && next==1.30f);
    assert(!step(Stick::held,true,1.30f)); // require a fresh press after tracking loss
}
