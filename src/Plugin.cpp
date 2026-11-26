// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include "Addon.h"
#include "World.h"

namespace addon {
Scripts* scripts;
Arrays* arrays;
double seconds() {
    static const double hz=[](){ LARGE_INTEGER f; QueryPerformanceFrequency(&f); return double(f.QuadPart); }();
    // Fallout can select single-precision x87 arithmetic. Subtract counters as
    // integers first so long system uptime cannot round several frames to one time.
    static const auto start=[](){ LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }();
    LARGE_INTEGER t; QueryPerformanceCounter(&t); return (t.QuadPart-start)/hz;
}
void log(const char* message) {
    if (FILE* f=std::fopen(logPath,"a")) {
        std::fprintf(f,"%lu %s\n",GetTickCount(),message); std::fclose(f);
    }
}
}

namespace {
struct Info { unsigned version; const char* name; unsigned build; };
struct Message { const char* sender; unsigned type,length; void* data; };
struct Messages { unsigned version; bool (__cdecl* listen)(unsigned,const char*,void (__cdecl*)(Message*)); };
bool initialized=false, ready=false, inWorld=false, playing=false;
world::Ptr cell;
double beforeFrame;
void* getMode;

void clearSession() {
    playing=false;
    cell=nullptr;
    beforeFrame=0;
}
void tick() {
    auto pc=world::player(), nextCell=world::ptr(pc,0x40);
    if (!nextCell) {
        if (playing) addon::reset();
        clearSession();
        return;
    }
    Value mode;
    if (!addon::scripts->call(getMode,nullptr,nullptr,&mode,0) || mode.type!=1 || mode.number<1) {
        if (playing) addon::suspend();
        clearSession();
        return;
    }
    double now=addon::seconds();
    if (nextCell!=cell || now-beforeFrame>0.30) {
        if (playing) {
            if (nextCell!=cell) addon::suspend();
            else addon::interruptHands();
        }
        clearSession();
        cell=nextCell;
    }
    float dt=beforeFrame ? float(now-beforeFrame) : 0.f;
    beforeFrame=now;
    playing=true;
    addon::frame(pc,now,dt,mode.number==2);
}
void __cdecl message(Message* msg) {
    switch (msg->type) {
        case 1: case 2: case 6: case 7: case 8: case 14:
            inWorld=msg->type==14 || (msg->type==8 && msg->data);
            if (ready) addon::reset();
            clearSession();
            return;
        case 20: break;
        default: return;
    }
    if (!inWorld) return;
    if (!initialized) {
        initialized=true;
        if (GetModuleHandleW(L"TTWVRClutterDistractions.dll")) {
            addon::log("inactive: legacy combined DLL loaded; disable the old mod, equip patch and bundled audio test");
            return;
        }
        getMode=addon::scripts->compile("int menu\nbegin Function {}\n let menu := GetActiveMenuMode\n if eval (menu == 0 || menu == 1004) && player.GetDead == 0\n  SetFunctionValue (1 + player.IsWeaponOut)\n endif\nend");
        ready=getMode && addon::initialize();
        if (ready) addon::reset();
        addon::log(ready ? "ready version=" TTWVR_VERSION " build=17" : "inactive: initialization failed");
    }
    if (ready) tick();
}
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSE* n,Info* info) {
    if (!n || !info) return false;
    *info={1,addon::name,17};
    return !n->isEditor && n->runtime==0x040020d0 && n->version>=6;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSE* n) {
    addon::scripts=static_cast<Scripts*>(n->query(6));
    addon::arrays=static_cast<Arrays*>(n->query(5));
    auto messages=static_cast<Messages*>(n->query(2));
    return addon::scripts && addon::scripts->compile && addon::arrays && messages &&
           addon::connect(n) && messages->listen(n->handle(),"NVSE",message);
}
