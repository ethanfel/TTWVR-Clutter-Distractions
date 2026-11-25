// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <map>
#include <cstdio>
#include "Addon.h"
#include "ClutterInput.h"
#include "World.h"
#include "ThrowMotion.h"

namespace addon {
const char* const name="TTWVR Clutter Audio";
const char* const logPath="Data/NVSE/Plugins/TTWVRClutterAudio.log";
using world::Ptr;
using world::ptr;
using world::field;
using noise::Source;
static Events* events;
static void *getContacts,*playFoley;
static DWORD mainThread;
static bool playing;
static world::WeaponContact weapon;

bool connect(const NVSE* n) {
    events=static_cast<Events*>(n->query(8));
    return events && events->listen;
}
struct Track {
    noise::Motion motion;
    double expires{},lastSound=-100;
    noise::Vec lastPoint{};
    bool playerTouch{},melee{};
};
std::map<unsigned,Track> tracks;
struct Candidate { Ptr ref{}; noise::Vec point{}; Source source{}; unsigned sound{}; int level{}; };

struct ThrowHand {
    unsigned reference{};
    clutter::ThrowMotion motion{};
    noise::Vec release{};
    double missingSince{},releasedAt{};
};
std::array<ThrowHand,2> hands{};
void reset() { hands={}; tracks.clear(); weapon.reset(); playing=false; }
void suspend() { reset(); }
void interruptHands() { reset(); }
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
    mainThread=GetCurrentThreadId();
    getContacts=scripts->compile("begin Function {}\n SetFunctionValue (player.GetContactRefs)\nend");
    playFoley=scripts->compile("ref target\nref noiseForm\nbegin Function {target noiseForm}\n if eval target && noiseForm\n  if eval !(IsSoundPlaying noiseForm target)\n   target.PlaySound3D noiseForm\n  endif\n endif\nend");
    if (!getContacts || !playFoley || !events->listen("OnHit",hit)) return false;
    clutter::initializeInput(log);
    return true;
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

bool throwable(Ptr ref) {
    if (!world::loose(ref)) return false;
    switch (field<unsigned char>(ptr(ref,0x20),4)) {
        case 0x18: case 0x19: case 0x1a: // armour, books, clothing
        case 0x1d: case 0x22:             // ingredients, movable statics
        case 0x29: case 0x2e: case 0x2f: case 0x31: // ammo, keys, aid, notes
            return true;
        case 0x1f: return !clutter::isPortable(ref); // miscellaneous items
        default: return false;
    }
}
void updateThrow(int h,Ptr pc,const std::array<Ptr,2>& held,Ptr nativeHeld,int grips,double now) {
    auto& hand=hands[h];
    bool tracked=grips>=0 && (grips&(4<<h));
    bool pressed=tracked && (grips&(1<<h));
    Ptr item=held[h];
    if (!tracked) { hand={}; return; }
    if (pressed && item && throwable(item)) {
        if (hand.reference!=world::id(item) || hand.releasedAt) hand={};
        hand.reference=world::id(item);
        hand.missingSince=0;
    }
    if (!hand.reference) return;
    Ptr ref=world::lookup(hand.reference);
    if (!throwable(ref) || ptr(ref,0x40)!=ptr(pc,0x40) || (item && item!=ref) || held[1-h]==ref) {
        hand={}; return;
    }
    if (!hand.releasedAt) {
        if (!item && !hand.missingSince) hand.missingSince=now;
        if (hand.missingSince && now-hand.missingSince>.075) { hand={}; return; }
        noise::Vec point{};
        if (!clutter::handPoint(h,point)) { hand={}; return; }
        hand.motion.sample(point-field<noise::Vec>(pc,0x30),now);
        if (pressed) return;
        hand.release=hand.motion.release(now);
        if (!hand.release.length()) { hand={}; return; }
        hand.releasedAt=now;
    }
    if (pressed || now-hand.releasedAt>.12) { hand={}; return; }
    if (item || nativeHeld==ref) return;
    Ptr body=world::mobileBody(world::root(ref));
    if (!body) return;
    auto current=field<noise::Vec>(body,0x1b0);
    auto desired=hand.release*clutter::havokUnitsPerMetre;
    if (current.valid() && current.length()<desired.length()) {
        float velocity[4]{desired.x,desired.y,desired.z,0.f};
        std::memcpy(body+0x1b0,velocity,sizeof velocity);
        reinterpret_cast<void (__thiscall*)(Ptr)>(0xc9c1d0)(body); // hkpRigidBody::UpdateMotion
        char line[144];
        std::snprintf(line,sizeof line,"clutter_throw ref=%08x hand=%d native_mps=%.2f release_mps=%.2f",
            hand.reference,h,current.length()/clutter::havokUnitsPerMetre,hand.release.length());
        log(line);
    }
    if (auto state=track(ref,now)) state->motion.beginFlight(now);
    hand={};
}

void frame(Ptr pc,double now,float dt,bool drawn) {
    playing=true;
    Ptr held=ptr(pc,0x638);
    std::array<Ptr,2> nativeHands{clutter::heldObject(0),clutter::heldObject(1)};
    int grips=clutter::grips();
    for (int h=0;h<2;++h) {
        if (nativeHands[h]) track(nativeHands[h],now,true);
        updateThrow(h,pc,nativeHands,held,grips,now);
    }
    unsigned heldID=world::id(held);
    if (held) track(held,now,true);
    for (auto& pair:tracks) pair.second.playerTouch=false;
    playerContacts(now);
    float tap=(held && drawn && tracks.count(heldID)) ? weapon.sample(pc,held,dt) : 0.f;
    if (!held || !drawn) weapon.reset();
    Candidate loudest;
    for (auto it=tracks.begin();it!=tracks.end();) {
        Ptr ref=world::lookup(it->first);
        auto& state=it->second;
        bool inHand=ref && (ref==held || ref==nativeHands[0] || ref==nativeHands[1]);
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
}
