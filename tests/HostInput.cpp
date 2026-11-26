// SPDX-License-Identifier: GPL-3.0-only
// The TTWVR OpenVR backend runs against simulated tracked devices.
#include "ControllerInput.h"
#include "ClutterInput.h"
#include "TrapInput.h"
#include <cassert>
#include <cstdio>

namespace {
int squeezeHand=2;
unsigned pulses[2]{};
struct Matrix { float m[3][4]; };
struct DevicePose {
  Matrix pose;
  float velocity[6];
  int tracking;
  bool valid, connected;
};
static_assert(sizeof(DevicePose) == 80);
void setPose(Matrix &pose, float x) {
  pose = {};
  for (int i=0; i<3; ++i) pose.m[i][i] = 1.f;
  pose.m[0][3] = x;
}
int __fastcall poses(void*,void*,DevicePose *devices,unsigned count,void*,unsigned) {
  assert(count >= 3);
  for (unsigned n=0; n<3; ++n) {
    devices[n] = {};
    setPose(devices[n].pose, n==1 ? -.3f : n==2 ? .4f : 0.f);
    devices[n].valid = devices[n].connected = true;
    devices[n].tracking = 200;
  }
  return 0;
}
Matrix *__fastcall eye(void*,void*,Matrix *out,int side) {
  setPose(*out,side ? .032f : -.032f); return out;
}
void __fastcall projection(void*,void*,int,float *left,float *right,float *top,float *bottom) {
  *left = *top = -1; *right = *bottom = 1;
}
unsigned __fastcall role(void*,void*,unsigned hand) { return hand; }
bool __fastcall readController(void*,void*,unsigned index,char *out,unsigned size) {
    assert(size>=64 && (index==1 || index==2));
    std::memset(out,0,size);
    float axes[10]{};
    axes[2]=index==1 ? .75f : .125f;
    axes[4]=int(index)==squeezeHand ? .875f : .25f;
    std::memcpy(out+24,axes,sizeof axes);
    return true;
}
void __fastcall vibrate(void*,void*,unsigned index,unsigned axis,unsigned short duration) {
    assert(index>=1 && index<=2 && axis==0 && duration>0);
    ++pulses[index-1];
}
void logger(const char* message) { std::puts(message); }
}

int wmain(int argc,wchar_t** argv) {
    if (argc!=3 || (wcscmp(argv[1],L"audio") && wcscmp(argv[1],L"traps"))) return 2;
    const bool traps=wcscmp(argv[1],L"traps")==0;
    if (!LoadLibraryW(argv[2])) { std::printf("LoadLibrary failed %lu\n",GetLastError()); return 3; }
    auto host=controller::host();
    assert(host);
    void *systemTable[40]{},*compositorTable[3]{};
    systemTable[2]=(void*)projection; systemTable[5]=(void*)eye; systemTable[18]=(void*)role;
    systemTable[37]=(void*)readController; systemTable[39]=(void*)vibrate; compositorTable[2]=(void*)poses;
    void **system=systemTable,**compositor=compositorTable;
    auto **vr=reinterpret_cast<void**>(host.base+host.layout->vr.table);
    auto **xr=reinterpret_cast<void**>(host.base+host.layout->xr.table);
    auto originalVR=vr[6],originalXR=xr[6];
    void *backend[256]{};
    backend[0]=vr; backend[7]=&system; backend[8]=&compositor;
    if (traps) {
        const auto& stance=host.layout->crouch;
        auto call=host.base+stance.poseCall;
        int32_t original;
        std::memcpy(&original,call+1,sizeof original);
        assert(trap::installInput()==3 && crouch::available());
        assert(std::memcmp(call+1,&original,sizeof original)!=0);
        host.base[stance.manual]=1;
        auto& height=*reinterpret_cast<volatile float*>(host.base+stance.target);
        height=1.15f;
        crouch::update(crouch::Stick::released,.55f,1,logger);
        crouch::update(crouch::Stick::held,.55f,2,logger);
        assert(std::abs(height-.55f)<.0001f);
        crouch::suspend();
        assert(std::abs(height-1.15f)<.0001f);
    } else {
        clutter::initializeInput(logger);
    }
    assert(vr[6]!=originalVR && xr[6]!=originalXR);
    for (squeezeHand=1;squeezeHand<=2;++squeezeHand) {
        unsigned char frame[0x130]{};
        if (traps) {
            trap::captureJawGrip(1u<<(squeezeHand-1));
            trap::jawPulse(.5f,false);
        }
        assert(reinterpret_cast<controller::Poll>(vr[6])(backend,frame));
        int expected=12|(1<<(squeezeHand-1));
        assert((traps ? trap::grips() : clutter::grips())==expected);
        assert((controller::buttons(true,frame)&15u)==(traps ? 12u : unsigned(expected)));
        if (traps) assert(trap::triggers()==1 && trap::rightStick()==crouch::Stick::released);
    }
    assert(pulses[0]==(traps ? 2u : 0u) && pulses[1]==pulses[0]);
    std::printf("PASS TTWVR %s %s\n",host.layout->version,traps ? "bear traps" : "clutter audio");
    return 0;
}
