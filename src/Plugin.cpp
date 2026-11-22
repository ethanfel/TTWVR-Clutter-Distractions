#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "World.h"
#include "BearTraps.h"
#include <cstdio>
#include <map>

namespace {
using world::Ptr;
using world::ptr;
using world::field;
using noise::Source;

struct NVSE { unsigned version,runtime,editor,isEditor; void *add,*opcode;
    void* (__cdecl* query)(unsigned); unsigned (__cdecl* handle)(); };
struct Info { unsigned version; const char* name; unsigned build; };
struct Message { const char* sender; unsigned type,length; void* data; };
struct Messages { unsigned version; bool (__cdecl* listen)(unsigned,const char*,void (__cdecl*)(Message*)); };
struct Events { void* before[3]; bool (__cdecl* listen)(const char*,void (__cdecl*)(void*,void*)); };
Scripts* scripts;
Arrays* arrays;
Events* events;
void *getMode,*getContacts,*playFoley;
bool initialized=false,ready=false,inWorld=false,playing=false;
DWORD mainThread;
Ptr cell;
double beforeFrame;
world::WeaponContact weapon;

struct Track {
    noise::Motion motion;
    double expires{},lastSound=-100;
    noise::Vec lastPoint{};
    bool playerTouch{},melee{};
};
std::map<unsigned,Track> tracks;
struct Candidate { Ptr ref{}; noise::Vec point{}; Source source{}; unsigned sound{}; int level{}; };

void clear() { tracks.clear(); weapon.reset(); cell=nullptr; playing=false; beforeFrame=0; }

double seconds() {
    static const double hz=[](){ LARGE_INTEGER f; QueryPerformanceFrequency(&f); return double(f.QuadPart); }();
    LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart/hz;
}
void log(const char* message) {
    if (FILE* f=std::fopen("Data/NVSE/Plugins/TTWVRClutterDistractions.log","a")) {
        std::fprintf(f,"%lu %s\n",GetTickCount(),message); std::fclose(f);
    }
}
Track* track(Ptr ref,double now,bool held=false) {
    if (!world::loose(ref)) return nullptr;
    unsigned key=world::id(ref);
    auto it=tracks.find(key);
    if (it!=tracks.end()) return &it->second;
    if (!key || tracks.size()>=64 || !world::mobileBody(world::root(ref),held)) return nullptr;
    auto& fresh=tracks[key]; fresh.expires=now+4;
    return &fresh;
}
// xNVSE dispatches classic OnHit with [struck reference, attacker]. For a gunshot
// against clutter the second argument is a projectile, so it does not match.
void __cdecl hit(void*,void* raw) {
    if (GetCurrentThreadId()!=mainThread || !playing || !raw) return;
    auto args=static_cast<Ptr*>(raw);
    if (args[1]!=world::player()) return;
    if (auto state=track(args[0],seconds())) state->melee=true;
}
void playerContacts(double now) {
    Value answer;
    if (!scripts->call(getContacts,nullptr,nullptr,&answer,0) || answer.type!=4 || !answer.object) return;
    unsigned count=std::min(64u,arrays->size(answer.object));
    for (unsigned i=0;i<count;++i) {
        Value key,contact; key.type=1; key.number=i;
        if (arrays->get(answer.object,key,contact) && contact.type==2)
            if (auto state=track(contact.object,now)) state->playerTouch=true;
    }
}
bool initialize() {
    getMode=scripts->compile("int menu\nbegin Function {}\n let menu := GetActiveMenuMode\n if eval (menu == 0 || menu == 1004) && player.GetDead == 0\n  SetFunctionValue (1 + player.IsWeaponOut)\n endif\nend");
    getContacts=scripts->compile("begin Function {}\n SetFunctionValue (player.GetContactRefs)\nend");
    playFoley=scripts->compile("ref target\nref noiseForm\nbegin Function {target noiseForm}\n if eval target && noiseForm\n  if eval !(IsSoundPlaying noiseForm target)\n   target.PlaySound3D noiseForm\n  endif\n endif\nend");
    return getMode && getContacts && playFoley && events->listen("OnHit",hit);
}
const char* label(Source source) {
    switch(source) {
        case Source::landing: return "landing";
        case Source::strike: return "weapon";
        case Source::knock: return "knock";
        case Source::bump: return "bump";
        default: return "none";
    }
}
void emit(const Candidate& e,Ptr pc) {
    bool recorded=world::reportAt(pc,e.ref,e.point,e.level);
    if (e.sound) {
        Ptr sound=world::lookup(e.sound);
        if (field<unsigned char>(sound,4)==0x0d) {
            Value result;
            scripts->call(playFoley,nullptr,nullptr,&result,2,e.ref,sound);
        }
    }
    char line[240];
    std::snprintf(line,sizeof line,"impact=%s ref=%08x level=%d pos=%.1f,%.1f,%.1f recorded=%d",
        label(e.source),world::id(e.ref),e.level,e.point.x,e.point.y,e.point.z,recorded);
    log(line);
}

void frame() {
    Ptr pc=world::player(), nextCell=ptr(pc,0x40);
    if (!nextCell) { if (playing) trap::reset(); clear(); return; }
    Value mode;
    if (!scripts->call(getMode,nullptr,nullptr,&mode,0) || mode.type!=1 || mode.number<1) {
        if (playing) { trap::suspend(); clear(); }
        return;
    }
    double now=seconds();
    if (nextCell!=cell || now-beforeFrame>0.30) {
        if (playing) trap::suspend();
        clear(); cell=nextCell;
    }
    float dt=beforeFrame ? float(now-beforeFrame) : 0.f;
    beforeFrame=now; playing=true;
    trap::frame(pc,now,mode.number==2);
    Ptr held=ptr(pc,0x638);
    unsigned heldID=world::id(held);
    if (held) track(held,now,true);
    for (auto& pair:tracks) pair.second.playerTouch=false;
    playerContacts(now);
    float tap=(held && mode.number==2 && tracks.count(heldID)) ? weapon.sample(pc,held,dt) : 0.f;
    if (!held || mode.number!=2) weapon.reset();
    Candidate loudest;
    for (auto it=tracks.begin();it!=tracks.end();) {
        Ptr ref=world::lookup(it->first);
        auto& state=it->second;
        bool inHand=ref && ref==held;
        if (inHand || state.playerTouch || state.melee) state.expires=now+5;
        Ptr node=world::loose(ref) ? world::root(ref) : nullptr;
        Ptr body=world::mobileBody(node,inHand);
        if (!body || now>state.expires) { it=tracks.erase(it); continue; }
        noise::Vec velocity=field<noise::Vec>(body,0x1b0);
        bool solid=field<unsigned short>(body,0xb0) || field<unsigned>(body,0xb8);
        auto impact=state.motion.step(velocity,inHand,state.playerTouch,solid,now,dt);
        if (state.melee || (inHand && tap>0.f))
            impact={Source::strike,std::max(inHand ? tap : 0.f,state.melee ? 1.25f : 0.f)};
        state.melee=false;
        if (impact.source!=Source::none) {
            auto bound=world::bound(node);
            auto point=bound.radius>0 && bound.center.valid() ? bound.center : field<world::Pose>(node,0x68).pos;
            // Collapse multiple reports for the same tap. An immediately thrown
            // can is still allowed to land at a different location.
            if (point.valid() && (now-state.lastSound>=0.23 || (point-state.lastPoint).length()>30.f)) {
                auto surface=noise::surface(world::material(body));
                int level=noise::level(impact.speed,surface);
                unsigned sound=impact.source==Source::strike ? (impact.speed>=1.5f ? surface.hard : surface.light) : 0;
                if (level>loudest.level) loudest={ref,point,impact.source,sound,level};
                state.lastSound=now; state.lastPoint=point;
            }
        }
        ++it;
    }
    // Fallout stores one current event per owner. Simultaneous impacts use the loudest.
    if (loudest.ref) emit(loudest,pc);
}
void __cdecl message(Message* msg) {
    switch (msg->type) {
        case 1: case 2: case 6: case 7: inWorld=false; trap::reset(); clear(); return;
        case 8: case 14: inWorld=msg->type==14 || msg->data!=nullptr; trap::reset(); clear(); return;
        case 20: break;
        default: return;
    }
    if (!inWorld) return;
    if (!initialized) {
        initialized=true; mainThread=GetCurrentThreadId();
        ready=initialize();
        if (ready && !trap::initialize(scripts,arrays,log)) log("bear_traps inactive: check ESP or script compilation");
        log(ready ? "ready version=0.2.2" : "inactive: check xNVSE and JIP LN script compilation");
    }
    if (ready) frame();
}
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSE* n,Info* info) {
    if (!n || !info) return false;
    *info={1,"TTWVR Clutter Distractions",4};
    return !n->isEditor && n->runtime==0x040020d0 && n->version>=6;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSE* n) {
    scripts=static_cast<Scripts*>(n->query(6)); arrays=static_cast<Arrays*>(n->query(5));
    events=static_cast<Events*>(n->query(8));
    auto messages=static_cast<Messages*>(n->query(2));
    return scripts && scripts->compile && arrays && events && messages && messages->listen(n->handle(),"NVSE",message);
}
