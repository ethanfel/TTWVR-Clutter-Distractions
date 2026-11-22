#include "../src/TrapGesture.h"
#include <cassert>
using namespace trap;
int main() {
    for (int side : {-1,1}) {
        Gesture g;
        Vec p[]={{0.f,-4.f*side,11.f},{1.f,4.f*side,12.f}};
        assert(g.step(p,1,1,false,1)==Action::none);
        assert(g.step(p,3,2,false,1.1)==Action::none && g.paired);
        assert(g.step(p,3,0,false,1.3)==Action::none);
        p[0].x-=20; p[1].x+=20;
        assert(g.step(p,3,0,false,1.4)==Action::none);
        p[0].y=-15.f*side; p[1].y=15.f*side;
        assert(g.step(p,3,0,false,1.5)==Action::arm);
        g={};
        assert(g.step(p,3,1,true,2)==Action::none && g.paired);
        assert(g.step(p,3,0,true,2.5)==Action::none);
        assert(g.step(p,3,0,true,2.8)==Action::disarm);
    }
    Vec p[]={{0,4,12},{0,6,15}};
    Gesture g;
    assert(g.step(p,2,2,false,0)==Action::none && g.hand==1);
    assert(g.step(p,2,0,false,.2)==Action::carry);
    assert(g.step(p,0,0,false,.3)==Action::place);
    g={}; g.step(p,3,3,false,1); assert(!g.paired);
    p[0].y=-5; g={}; g.step(p,3,3,false,1); assert(g.paired);
    assert(g.step(p,1,0,false,1.1)==Action::cancel);
}
