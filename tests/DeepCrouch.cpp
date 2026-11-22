#include "../src/DeepCrouch.h"
#include "../src/Kneeling.h"
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
    Control gate;
    bool rise=false;
    auto poll=[&](Level l,Stick input,float y,unsigned t) { return gate.filter(l,input,y,t,rise); };
    assert(poll(Level::standing,Stick::released,0,0)==0);
    assert(poll(Level::standing,Stick::down,-1,10)==-1);
    assert(poll(Level::regular,Stick::held,-1,3010)==-1);
    assert(poll(Level::deep,Stick::released,0,3020)==0);
    assert(poll(Level::deep,Stick::down,-1,3030)==0 && rise);
    assert(poll(Level::regular,Stick::down,-1,3040)==0 && !rise);
    assert(poll(Level::regular,Stick::released,0,3050)==0);
    assert(poll(Level::regular,Stick::down,-1,3100)==0);
    assert(poll(Level::regular,Stick::released,0,3300)==-1);
    assert(poll(Level::standing,Stick::released,0,3400)==0);
    assert(poll(Level::regular,Stick::down,-1,3500)==0);
    assert(poll(Level::regular,Stick::held,-1,6600)==0);
    assert(poll(Level::deep,Stick::released,0,6700)==0);
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
    assert(!step(Stick::invalid,true,.55f) && s.active); // tracking gaps preserve height
    assert(!step(Stick::held,true,.55f) && s.active);
    assert(step(Stick::invalid,false,.55f) && next==1.30f && !s.active);
    assert(!step(Stick::held,true,1.30f));
    assert(poll(Level::deep,Stick::invalid,0,7000)==0);
    assert(poll(Level::deep,Stick::down,-1,7010)==0 && !rise);
    assert(poll(Level::deep,Stick::released,0,7030)==0);
    assert(poll(Level::deep,Stick::down,-1,7050)==0 && rise);
    assert(poll(Level::regular,Stick::released,0,7070)==0);
    assert(poll(Level::regular,Stick::down,-1,7100)==0);
    assert(poll(Level::regular,Stick::released,0,7150)==-1);
    assert(poll(Level::regular,Stick::released,0,7340)==0);
    assert(poll(Level::regular,Stick::released,0,7480)==-1);
    assert(poll(Level::standing,Stick::released,0,7490)==0);
    assert(poll(Level::regular,Stick::released,0,7500)==0);
    // The lower stance keeps the sole height and both limb lengths when the
    // stock solve extends the ankles in front of the player.
    Vec hip{2,0,7},knee{4,25,14},foot{0,48,4},forward{0,1,0},kn,an;
    assert(kneel(hip,knee,foot,forward,1.f,kn,an));
    assert(an.y>hip.y && an.y<knee.y && an.z==foot.z && an.x==foot.x);
    auto close=[](float a,float b) { return std::abs(a-b)<.001f; };
    assert(close((kn-hip).length(),(knee-hip).length()));
    assert(close((an-kn).length(),(foot-knee).length()));
    auto r=swing(knee-hip,kn-hip);
    assert((r.apply(knee-hip)-(kn-hip)).length()<.001f);
    assert(!kneel(hip,hip,foot,forward,1,kn,an));
    assert(!kneel(hip,knee,{NAN,0,0},forward,1,kn,an));
    assert(!kneel(hip,knee,foot,forward,0,kn,an));
}
