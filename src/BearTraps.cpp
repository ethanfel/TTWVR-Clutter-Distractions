#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// SPDX-License-Identifier: GPL-3.0-only
#include "BearTraps.h"
#include "TrapGesture.h"
#include "TrapInput.h"
#include "TrapCarrier.h"
#include "World.h"
#include <cstdio>
#include <map>

namespace trap {
namespace {
using namespace world;
Scripts* api;
Arrays* arrays;
void (*writeLog)(const char*);
void *scanScript,*swapScript,*positionScript,*openTrapScript,*spentScript,*skillScript,*noticeScript;
unsigned prefix, previousTriggers=3;
void *grantScript,*deployScript,*stockScript,*emptyScript,*bodyScript,*ownerScript,*floorScript;
void *caughtActorScript,*injuryScript,*creatureReactionScript,*directReactionScript,*idleReactionScript;
void *heldVictimScript,*holdPositionScript,*releaseVictimScript;
Ptr carrier,neutralFaction,stockList,catchSpell,actorCatchSpell;
int repairRequired=55;
float deepHeight;
bool ready, blockOnWorldEntry;
void* allowActivate;
struct Seen {
    Ptr root{};
    bool observedArmed=false, ownerReady=false, settling=false, latchPending=false;
    double due=0, recoverAt=0;
    unsigned heldActor=0;
    Vec catchPosition{};
    float lastHeight=1e9f;
};
std::map<unsigned,Seen> nearby;
std::vector<Value> scanBuffer;
double nextScan, nextMove;
unsigned selected,previousGrips=3,previousValid=0;
Gesture gesture;
Vec handOffset;
bool holdingBody=false;
int carryHand=-1;

// Only ACTI forms supplied by this addon use Fallout's live trap
// script. Closed inventory objects can be carried, traded, stacked and repaired.
unsigned tierForm(int tier,bool armed) {
    if (tier%5==0) return (armed ? 0x810 : 0x800) + tier/5;
    return (armed ? 0x920 : 0x900) + tier;
}
int stockTier(unsigned key) {
    if (key>=0x800 && key<=0x804) return 5*int(key-0x800);
    if (key>0x900 && key<0x914 && (key-0x900)%5) return int(key-0x900);
    return -1;
}
int tierOf(Ptr ref,bool& armed) {
    if (!loose(ref)) return -1;
    unsigned base=id(ptr(ref,0x20));
    if ((base&0xff000000)!=prefix) return -1;
    unsigned key=base&0xffffff;
    armed=key>=0x811 && key<=0x814;
    if (armed) return 5*int(key-0x810);
    armed=key>0x920 && key<0x934 && (key-0x920)%5;
    return armed ? int(key-0x920) : stockTier(key);
}
constexpr char swapSource[]=R"(ref oldTrap
ref newItem
ref replacement
ref parent
ref ownerForm
int makeArmed
int minimumRepair
int stack
float px
float py
float pz
float yaw
begin Function {oldTrap newItem makeArmed minimumRepair}
 if eval !oldTrap || !newItem || oldTrap.GetDisabled
  return
 endif
 if eval makeArmed && (player.GetAV Repair) < minimumRepair
  return
 endif
 let parent := oldTrap.GetParentCell
 let px := oldTrap.GetPos X
 let py := oldTrap.GetPos Y
 let pz := oldTrap.GetPos Z
 let yaw := oldTrap.GetAngle Z
 let stack := oldTrap.GetRefCount
 let replacement := PlaceAtCell newItem 1 parent px py pz
 if eval !replacement
  return
 endif
 replacement.SetAngle Z yaw
 if makeArmed
  let ownerForm := player.GetBaseObject
  replacement.SetOwnership ownerForm
 endif
 if eval makeArmed && stack > 1
  let stack -= 1
  oldTrap.SetRefCount stack
  if eval oldTrap.GetRefCount != stack
   replacement.Disable
   replacement.MarkForDelete
   return
  endif
 else
  oldTrap.Disable
  oldTrap.MarkForDelete
 endif
 SetFunctionValue replacement
end)";
// The ray command returns distance from its END point to the surface. Turn
// off only this item's collision during the synchronous query to skip its mesh.
constexpr char positionSource[]=R"(ref item
int fx
int fy
int fz
int settle
float px
float py
float pz
float highZ
float lowZ
float hitDistance
begin Function {item fx fy fz settle}
 let px := fx / 16.0
 let py := fy / 16.0
 let pz := fz / 16.0
 if settle
  let highZ := pz + 20
  let lowZ := pz - 200
  item.ToggleObjectCollision 0
  let hitDistance := GetP2PRayCastRange px py highZ px py lowZ 6
  item.ToggleObjectCollision 1
  if hitDistance <= 0
   return
  endif
  let pz := lowZ + hitDistance + 2.25
  item.SetAngle X 0
  item.SetAngle Y 0
 endif
 item.SetPosEx px py pz
 SetFunctionValue 1
end)";
template<typename... Args> Value run(void* script,Args... args) {
    Value v;
    if (!api->call(script,nullptr,nullptr,&v,sizeof...(args),args...)) return {};
    return v;
}
void note(const char* action,unsigned ref,int tier) {
    char line[140]; std::snprintf(line,sizeof line,"bear_trap=%s ref=%08x condition=%d",action,ref,tier*5);
    writeLog(line);
}
bool move(Ptr ref,Vec p,bool settle) {
    if (!p.valid() || std::abs(p.x)>1e7 || std::abs(p.y)>1e7 || std::abs(p.z)>1e7) return false;
    auto v=run(positionScript,ref,int(std::lround(p.x*16)),int(std::lround(p.y*16)),
               int(std::lround(p.z*16)),int(settle));
    return v.type==1 && v.number==1;
}
void stop() {
    if (holdingBody) {
        auto ref=lookup(selected);
        if (loose(ref) && root(ref)) {
            run(bodyScript,ref,0);
            auto& state=nearby[selected];
            state.settling=true; state.lastHeight=1e9f;
        }
    }
    holdingBody=false; selected=0; gesture={}; carryHand=-1; nextMove=0;
}
Ptr swap(Ptr ref,int nextTier,bool armed) {
    auto form=lookup(prefix+tierForm(nextTier,armed));
    if (!form) return nullptr;
    auto result=run(swapScript,ref,form,int(armed),repairRequired);
    if (result.type!=2 || !result.object) return nullptr;
    auto& created=nearby[id(result.object)];
    created={};
    created.observedArmed=armed;
    created.latchPending=armed;
    if (!armed) run(bodyScript,result.object,2);
    return result.object;
}
// Each shipped creature rig below has consecutive left/right leg-hit IDLE forms.
// Resolve the left form through its master so TTW load order and leveled spawns work.
constexpr char creatureReactionSource[]=R"(ref victim
ref actorBase
ref leftIdle
string_var modelPath
begin Function {victim}
 let actorBase := victim.GetLeveledActorBase
 if eval !actorBase
  return
 endif
 let modelPath := GetSkeletonModel actorBase
 if eval (sv_Find "Creatures\SMSpineBreakerDC\" modelPath) >= 0
  let leftIdle := GetFormFromMod "Fallout3.esm" "020EAA"
 elseif eval (sv_Find "Creatures\SMSpineBreaker\" modelPath) >= 0
  let leftIdle := GetFormFromMod "FalloutNV.esm" "020EAA"
 elseif eval (sv_Find "Creatures\SMBehemoth\" modelPath) >= 0
  let leftIdle := GetFormFromMod "FalloutNV.esm" "021376"
 elseif eval (sv_Find "Creatures\SMBoneCrusher\" modelPath) >= 0
  let leftIdle := GetFormFromMod "BrokenSteel.esm" "008C7A"
 elseif eval (sv_Find "Creatures\Centaur\" modelPath) >= 0
  let leftIdle := GetFormFromMod "FalloutNV.esm" "03397C"
 endif
 sv_Destruct modelPath
 SetFunctionValue leftIdle
end)";
Ptr legReaction(Ptr victim,bool right) {
    if (victim==player()) return nullptr;
    if (field<unsigned char>(victim,4)==0x3b) return lookup(right ? 0x14cc40 : 0x14cc3d);
    auto left=run(creatureReactionScript,victim);
    if (left.type!=2 || field<unsigned char>(left.object,4)!=0x48) return nullptr;
    Ptr idle=right ? lookup(id(left.object)+1) : left.object;
    return field<unsigned char>(idle,4)==0x48 ? idle : nullptr;
}
void catchActor(Ptr ref,bool right,Seen& state) {
    const auto victim=run(caughtActorScript,ref);
    if (victim.type!=2 || !victim.object) return;
    const auto type=field<unsigned char>(victim.object,4);
    if (type!=0x3b && type!=0x3c) return;
    // The seated and standing player camera stays under TTWVR control.
    Ptr idle=legReaction(victim.object,right);
    auto spell=victim.object==player() ? catchSpell : actorCatchSpell;
    auto result=run(injuryScript,victim.object,int(right),spell,ref);
    const char* route="none";
    if (result.type==1 && result.number==0 && victim.object!=player() && root(victim.object)) {
        state.heldActor=id(victim.object);
        state.catchPosition=field<Vec>(victim.object,0x30);
        // FalloutNV 1.4.0.525 Actor::QueueReplacementLocomotions(force).
        // Recompute the native gait using this actor's skeleton and cripple flags.
        using RefreshGait=void (__thiscall *)(Ptr,bool);
        reinterpret_cast<RefreshGait>(0x8b78c0)(victim.object,true);
        if (idle) {
            // An attacking creature can discard the queued idle before it plays.
            if (type==0x3c && directReactionScript && run(directReactionScript,victim.object,idle).number==1)
                route="clip";
            else {
                run(idleReactionScript,victim.object,idle);
                route="queued";
            }
        }
    }
    char line[160];
    std::snprintf(line,sizeof line,"bear_trap=caught ref=%08x actor=%08x leg=%s limb_after=%.1f reaction=%08x route=%s",
                  id(ref),id(victim.object),right?"right":"left",result.type==1?result.number:-1.,id(idle),route);
    writeLog(line);
}
// SpeedMult stops locomotion but an attack or recoil animation can still move
// the character controller. Limit only horizontal travel at the point of impact.
void keepAtJaws(Ptr trapRef,Seen& state) {
    Ptr victim=lookup(state.heldActor);
    if (!root(victim) || ptr(victim,0x40)!=ptr(trapRef,0x40)) {
        state.heldActor=0;
        return;
    }
    Vec p=field<Vec>(victim,0x30);
    Vec delta=p-state.catchPosition;
    float distance=std::hypot(delta.x,delta.y);
    // Leave external teleports, falls and cell changes to the game.
    if (!p.valid() || distance>100 || std::abs(delta.z)>55) {
        state.heldActor=0;
        return;
    }
    if (distance<=2) return;
    Vec limit=state.catchPosition+delta*(1.f/distance);
    run(holdPositionScript,victim,actorCatchSpell,int(std::lround(limit.x*16)),int(std::lround(limit.y*16)));
}
void scan(Ptr pc,double now) {
    if (run(grantScript,carrier,stockList).number==1)
        writeLog("bear_trap=holster_available");
    std::vector<unsigned> found;
    for (int type:{21,31}) {
        auto v=run(scanScript,type);
        if (v.type!=4 || !v.object) continue;
        unsigned count=arrays->size(v.object);
        if (count>65535 || !count) continue;
        scanBuffer.resize(count);
        if (!arrays->elements(v.object,scanBuffer.data(),nullptr)) continue;
        for (const auto& item:scanBuffer) {
            bool armed=false;
            if (item.type==2 && tierOf(item.object,armed)>=0 && root(item.object))
                found.push_back(id(item.object));
        }
    }
    for (auto key:found) nearby.try_emplace(key);
    for (auto it=nearby.begin();it!=nearby.end();) {
        auto ref=lookup(it->first);
        bool armed=false;
        // A newly armed replacement may be waiting for its first loaded 3D frame.
        bool waitingFor3D=it->second.latchPending && ptr(ref,0x40)==ptr(pc,0x40);
        if (tierOf(ref,armed)<0 || (!root(ref) && !waitingFor3D) ||
            !ptr(ref,0x40) || (field<Vec>(ref,0x30)-field<Vec>(pc,0x30)).length()>2200) {
            it=nearby.erase(it);
        } else ++it;
    }
    nextScan=now+0.65;
}
bool hands(Ptr pc,Vec (&out)[2]) {
    Ptr rig=root(pc);
    for (int h=0;h<2;++h) {
        const char* name=h ? "Bip01 R Finger1" : "Bip01 L Finger1";
        Ptr bone=named(rig,name);
        if (!bone) bone=named(ptr(pc,0x694),name);
        if (!bone) return false;
        out[h]=field<Pose>(bone,0x68).pos;
        if (!out[h].valid() || (out[h]-field<Vec>(pc,0x30)).length()>260) return false;
    }
    return true;
}
// TTWVR moves the Weapon node to the drawing hand. Read the actual mesh
// center for placement and use only the trigger beside its grip.
void useCarrier(Ptr pc,unsigned valid,bool drawn) {
    unsigned buttons=triggers();
    unsigned pressed=buttons & ~previousTriggers & valid;
    previousTriggers=buttons | (3u & ~valid);
    if (!drawn || !pressed) return;
    Ptr process=ptr(pc,0x68);
    if (!process || field<unsigned char>(process,0x28)>1 || ptr(ptr(process,0x114),8)!=carrier) return;
    Ptr rig=root(pc);
    Ptr mount=named(rig,"Weapon");
    Ptr base=named(mount,"Base:0");
    Vec palms[2];
    if (!mount || !base || !hands(pc,palms)) return;
    auto grip=field<Pose>(mount,0x68).pos;
    float left=(palms[0]-grip).length(), right=(palms[1]-grip).length();
    unsigned hand=right<left ? 1 : 0;
    if (!(pressed & (1u<<hand)) || std::min(left,right)>24) return;
    Value v=run(stockScript,stockList);
    if (v.type!=2 || !v.object) { run(emptyScript); return; }
    auto pos=field<Pose>(base,0x68).pos;
    if (!pos.valid() || (pos-field<Vec>(pc,0x30)).length()>260) return;
    auto result=run(deployScript,carrier,v.object,int(std::lround(pos.x*16)),
                    int(std::lround(pos.y*16)),int(std::lround(pos.z*16)));
    if (result.type==2 && result.object) {
        run(bodyScript,result.object,2);
        nearby.try_emplace(id(result.object));
        note("placed_from_holster",id(result.object),stockTier(id(v.object)&0xffffff));
    }
}
void interact(Ptr ref,int tier,bool armed,const Vec (&palms)[2],unsigned gripsNow,unsigned pressed,double now) {
    Pose pose=field<Pose>(root(ref),0x68);
    if (!(pose.scale>0.001f)) { stop(); return; }
    Vec local[]{pose.local(palms[0]),pose.local(palms[1])};
    unsigned key=id(ref);
    Action action=gesture.step(local,gripsNow,pressed,armed,now);
    if (!armed && !holdingBody && (gesture.paired || gesture.hand>=0)) {
        run(bodyScript,ref,1);
        holdingBody=true;
    }
    if (gesture.hand>=0 && carryHand<0) {
        carryHand=gesture.hand;
        handOffset=pose.pos-palms[carryHand];
    }
    switch(action) {
        case Action::none:
            if (gesture.hand<0 && !gesture.paired) stop();
            return;
        case Action::carry:
            if (now>=nextMove) { move(ref,palms[carryHand]+handOffset,false); nextMove=now+0.022; }
            return;
        case Action::cancel: case Action::place: break;
        case Action::arm: {
            auto skill=run(skillScript);
            if (!tier || skill.type!=1 || skill.number<repairRequired) run(noticeScript,int(tier==0),repairRequired);
            else if (move(ref,pose.pos,true) && swap(ref,tier,true)) note("armed",key,tier);
            break;
        }
        case Action::disarm: {
            // If the native trap fired during the gesture, the spent path owns its wear.
            auto spent=run(spentScript,ref);
            if (spent.type==1 && spent.number<=0 && swap(ref,tier,false)) note("recovered",key,tier);
            break;
        }
    }
    stop();
}
} // namespace

bool initialize(Scripts* scripts,Arrays* arrayApi,void (*logger)(const char*)) {
    api=scripts; arrays=arrayApi; writeLog=logger;
    auto resolve=api->compile("begin Function {}\n SetFunctionValue (GetFormFromMod \"TTWVRClutterDistractions.esp\" \"804\")\nend");
    if (!resolve) return false;
    Value checked=run(resolve);
    if (checked.type!=2 || !checked.object || field<unsigned char>(checked.object,4)!=31) return false;
    prefix=id(checked.object)&0xff000000;
    neutralFaction=lookup(prefix+0x832);
    if (field<unsigned char>(neutralFaction,4)!=8) return false;
    catchSpell=lookup(prefix+0x835);
    actorCatchSpell=lookup(prefix+0x836);
    if (field<unsigned char>(catchSpell,4)!=0x14 || field<unsigned char>(actorCatchSpell,4)!=0x14) return false;
    carrier=lookup(prefix+0x830);
    if (field<unsigned char>(carrier,4)!=40) return false;
    stockList=lookup(prefix+0x833);
    if (field<unsigned char>(stockList,4)!=85) return false;
    // Restore box motion when released or reloaded, including old hovering saves.
    bodyScript=api->compile(R"(ref item
int hold
begin Function {item hold}
 if hold == 2
  item.AuxVarSetFlt "_TTWVRClutterPlaced" 1
  return
 elseif hold == 1
  item.AuxVarSetFlt "_TTWVRClutterPlaced" 1
  item.SetCollProp "TTWVRPortableTrap" 8 5
 elseif eval (item.GetCollProp "TTWVRPortableTrap" 8) == 5
  item.SetCollProp "TTWVRPortableTrap" 8 3
  item.SetLinearVelocity "TTWVRPortableTrap" 0 0 0
 endif
 SetFunctionValue (item.AuxVarGetFlt "_TTWVRClutterPlaced")
end)");
    // The moving box settles under gravity first. Only park it when its visible
    // base reaches support. Releasing it from the hand starts gravity again.
    floorScript=api->compile(R"(ref item
int depth16
float x
float y
float z
float lo
float hi
float hit
array_var origin
begin Function {item depth16}
 let origin := item.GetNifBlockTranslation "TTWVRPortableTrap" 1
 if eval (ar_Size origin) != 3
  return
 endif
 let x := origin[0]
 let y := origin[1]
 let z := origin[2]
 let hi := z + 9
 let lo := z - depth16 / 16.0 - 8
 item.ToggleObjectCollision 0
 let hit := GetP2PRayCastRange x y hi x y lo 6
 item.ToggleObjectCollision 1
 if eval hit <= 0 || abs (z - lo - hit - depth16 / 16.0) > 3
  return
 endif
 item.SetCollProp "TTWVRPortableTrap" 8 5
 SetFunctionValue 1
end)");
    ownerScript=api->compile(R"(ref item
ref faction
begin Function {item faction}
 if eval item.GetOwner != faction
  item.SetOwnership faction
 endif
 SetFunctionValue (item.GetOwner == faction)
end)");
    grantScript=api->compile(grantCarrier);
    deployScript=api->compile(placeCarrier);
    stockScript=api->compile(bestStock);
    emptyScript=api->compile("begin Function {}\n MessageEx \"No portable bear traps in inventory.\"\nend");
    if (!floorScript || !bodyScript || !ownerScript || !grantScript || !deployScript || !stockScript || !emptyScript) return false;
    scanScript=api->compile("int kind\nbegin Function {kind}\n SetFunctionValue (player.GetRefs kind 1 1 2048)\nend");
    swapScript=api->compile(swapSource);
    positionScript=api->compile(positionSource);
    openTrapScript=api->compile(R"(ref item
int newlyArmed
begin Function {item newlyArmed}
 item.PlayGroup Backward 1
 if newlyArmed
  item.PlaySound3D TRPPressurePlate
 endif
end)");
    spentScript=api->compile(R"(ref item
begin Function {item}
 if eval item.GetVariable "disarmed"
  if eval (item.GetVariable "button") == 1
   SetFunctionValue 1
  elseif eval (item.GetVariable "legToDisable") > 0
   if eval (item.GetVariable "legToDisable") == 2
    SetFunctionValue 4
   else
    SetFunctionValue 3
   endif
  else
   SetFunctionValue 2
  endif
 elseif eval item.GetVariable "justActivated"
  SetFunctionValue -1
 else
  SetFunctionValue 0
 endif
end)");
    caughtActorScript=api->compile(R"(ref item
begin Function {item}
 SetFunctionValue (item.GetRefVariable "trapVictim")
end)");
    // Query the native spell so menus, cell reloads and saved catches use game time.
    heldVictimScript=api->compile(R"(ref item
ref spell
ref victim
begin Function {item spell}
 let victim := item.GetRefVariable "trapVictim"
 if eval victim && !(victim.GetDead)
  if eval victim.IsSpellTargetAlt spell
   SetFunctionValue victim
  endif
 endif
end)");
    holdPositionScript=api->compile(R"(ref victim
ref spell
int fx
int fy
float px
float py
float pz
begin Function {victim spell fx fy}
 if eval !(victim.GetDead) && !(victim.GetKnockedState) && (victim.IsSpellTargetAlt spell)
  let px := fx / 16.0
  let py := fy / 16.0
  let pz := victim.GetPos Z
  victim.SetPosEx px py pz
 endif
end)");
    releaseVictimScript=api->compile(R"(ref victim
begin Function {victim}
 if eval !(victim.GetDead) && !(victim.GetKnockedState)
  victim.EvaluatePackage
 endif
end)");
    injuryScript=api->compile(R"(ref victim
int rightLeg
ref spell
ref trapRef
float condition
begin Function {victim rightLeg spell trapRef}
 trapRef.PlaySound3D FXMeleeTrapImpact
 if victim.GetDead
  SetFunctionValue -1
  return
 endif
 if rightLeg
  if eval (victim.GetAV RightMobilityCondition) > 0
   victim.CrippleLimb 5
  endif
  let condition := victim.GetAV RightMobilityCondition
 else
  if eval (victim.GetAV LeftMobilityCondition) > 0
   victim.CrippleLimb 4
  endif
  let condition := victim.GetAV LeftMobilityCondition
 endif
 victim.CastImmediate spell
 SetFunctionValue condition
end)");
    creatureReactionScript=api->compile(creatureReactionSource);
    idleReactionScript=api->compile("ref victim\nref idleForm\nbegin Function {victim idleForm}\n victim.PlayIdleEx idleForm\nend");
    if (!caughtActorScript || !heldVictimScript || !holdPositionScript || !releaseVictimScript || !injuryScript || !creatureReactionScript || !idleReactionScript) return false;
    if (GetModuleHandleA("kNVSE.dll"))
        directReactionScript=api->compile(R"(ref victim
ref idleForm
string_var clip
int played
begin Function {victim idleForm}
 let clip := "meshes\" + (GetIdleAnimPath idleForm)
 let played := victim.PlayAnimationPath (clip) 0
 sv_Destruct clip
 SetFunctionValue played
end)");
    skillScript=api->compile("begin Function {}\n SetFunctionValue (player.GetAV Repair)\nend");
    noticeScript=api->compile("int broken\nint minimumRepair\nbegin Function {broken minimumRepair}\n if broken\n  MessageEx \"Repair this bear trap at a workbench.\"\n else\n  MessageEx \"Repair %.0f required to arm a bear trap.\" minimumRepair\n endif\nend");
    allowActivate=api->compile("ref item\nbegin Function {item}\n SetInteractionDisabled 0 item\n SetFunctionValue (GetInteractionDisabled item)\nend");
    if (!scanScript || !swapScript || !positionScript || !openTrapScript || !spentScript || !skillScript || !noticeScript || !allowActivate) return false;
    for (unsigned tier=1;tier<=4;++tier) {
        Ptr form=lookup(prefix+0x810+tier);
        if (field<unsigned char>(form,4)!=21 || run(allowActivate,form).number!=0) return false;
    }
    constexpr char ini[]=".\\Data\\NVSE\\Plugins\\TTWVRClutterDistractions.ini";
    repairRequired=std::clamp(int(GetPrivateProfileIntA("BearTrap","MinimumRepair",55,ini)),0,100);
    int cm=int(GetPrivateProfileIntA("DeepCrouch","EyeHeightCm",55,ini));
    deepHeight=cm>0 ? std::clamp(cm,30,120)*0.01f : 0.f;
    ready=true;
    unsigned inputs=installInput();
    char line[100]; std::snprintf(line,sizeof line,"bear_traps ready forms=%02x minimum_repair=%d controller_openxr=%u controller_openvr=%u",
                                  prefix>>24,repairRequired,inputs&1,inputs>>1); logger(line);
    std::snprintf(line,sizeof line,"deep_crouch available=%d eye_height_cm=%.0f",crouch::available(),deepHeight*100);
    logger(line);
    return true;
}
void interruptHands() {
    stop(); previousGrips=3; previousValid=0; previousTriggers=3;
}
void suspend() {
    crouch::suspend();
    interruptHands();
}
void reset() {
    crouch::suspend();
    blockOnWorldEntry=true;
    stop(); nearby.clear(); nextScan=0; previousGrips=3; previousValid=0; previousTriggers=3;
}
void frame(Ptr pc,double now,bool drawn) {
    if (!ready) return;
    crouch::update(rightStick(),deepHeight,now,writeLog);
    if (blockOnWorldEntry) {
        for (unsigned tier=1; tier<=4; ++tier)
            if (auto form=lookup(prefix+0x810+tier)) run(allowActivate,form);
        blockOnWorldEntry=false;
    }
    if (now>=nextScan) scan(pc,now);
    int input=grips();
    unsigned valid=input<0 ? 0u : unsigned(input>>2);
    unsigned down=input<0 ? 0u : unsigned(input&3);
    unsigned pressed=down & ~previousGrips & previousValid & valid;
    previousGrips=down; previousValid=valid;
    useCarrier(pc,valid,drawn);
    bool canReach=false;
    for (auto& [key,state]:nearby) {
        Ptr ref=lookup(key); bool armed=false;
        int tier=tierOf(ref,armed);
        Ptr node=tier<0 ? nullptr : root(ref);
        if (!node) continue;
        if (state.root!=node) {
            state.root=node;
            if (!armed) {
                state.settling=run(bodyScript,ref,0).number==1;
                state.lastHeight=1e9f;
            }
            else {
                if (run(spentScript,ref).number==0) run(openTrapScript,ref,int(state.latchPending));
                state.latchPending=false;
            }
        }
        if (!armed && state.settling && selected!=key && now>=state.due) {
            state.due=now+0.12;
            Pose pose=field<Pose>(node,0x68);
            float z=pose.pos.z;
            // Project the rigid box half-extents from Portable.nif onto world Z.
            // A folded trap can land flat, on its side, or against a slope.
            float depth=pose.scale*(48.3f*std::abs(pose.rot[2][0])+7.7f*std::abs(pose.rot[2][1])+
                                    2.8f*std::abs(pose.rot[2][2]));
            if (std::abs(z-state.lastHeight)<0.5f && depth>0 && depth<100 &&
                run(floorScript,ref,int(std::lround(depth*16))).number==1) {
                state.settling=false;
                note("landed",key,tier);
            }
            state.lastHeight=z;
        }
        if (armed && state.heldActor) keepAtJaws(ref,state);
        if (armed && now>=state.due) {
            state.due=now+0.16;
            // TTW exempts the owner on trigger entry. Protect the setter while
            // their feet overlap the jaws, then arm for everyone, including them.
            Vec feet=field<Pose>(node,0x68).local(field<Vec>(pc,0x30));
            if (!state.ownerReady && (std::abs(feet.x)>48 || std::abs(feet.y)>48 ||
                                      feet.z>24 || feet.z< -120)) {
                state.ownerReady=run(ownerScript,ref,neutralFaction).number==1;
                if (state.ownerReady) note("step_ready",key,tier);
            }
            auto spent=run(spentScript,ref);
            if (spent.type==1 && spent.number>0) {
                auto pos=field<Vec>(ref,0x30);
                bool manual=spent.number==1;
                if (!manual && state.observedArmed && !state.recoverAt) {
                    // Keep the native activator through the jaw animation, sound,
                    // and give the timed catch its first engine update.
                    state.recoverAt=now+1.;
                    if (spent.number>=3) catchActor(ref,spent.number==4,state);
                    world::reportAt(pc,ref,pos,70);
                }
                if (now<state.recoverAt) continue;
                if (spent.number>=3) {
                    auto held=run(heldVictimScript,ref,actorCatchSpell);
                    if (held.type==2 && held.object) {
                        if (!state.observedArmed && root(held.object)) {
                            state.heldActor=id(held.object);
                            state.catchPosition=field<Vec>(held.object,0x30);
                            state.observedArmed=true;
                            // The native script has already applied this catch.
                            state.recoverAt=now;
                        }
                        continue;
                    }
                }
                if (state.heldActor) {
                    Ptr victim=lookup(state.heldActor);
                    if (root(victim)) run(releaseVictimScript,victim);
                    state.heldActor=0;
                }
                int nextTier=manual ? tier : std::max(0,tier-2);
                if (swap(ref,nextTier,false)) {
                    note(manual ? "recovered" : "sprung",key,nextTier);
                    if (selected==key) stop();
                }
                continue;
            }
            if (spent.type==1) { state.observedArmed=true; state.recoverAt=0; }
        }
        auto delta=field<Vec>(pc,0x30)-field<Vec>(ref,0x30);
        if (delta.length()<210) canReach=true;
    }
    if (!canReach && !selected) return;
    Vec palms[2]{};
    if (!valid || (!down && !selected) || !hands(pc,palms)) {
        if (selected) stop();
        return;
    }
    if (selected) {
        Ptr ref=lookup(selected); bool armed=false; int tier=tierOf(ref,armed);
        if (tier<0 || !root(ref)) { stop(); return; }
        if ((gesture.paired && valid!=3) || (carryHand>=0 && !(valid&(1u<<carryHand)))) {
            stop(); return;
        }
        interact(ref,tier,armed,palms,down,pressed,now);
        return;
    }
    if (!pressed) return;
    float best=1e9f;
    for (auto& [key,state]:nearby) {
        (void)state;
        Ptr ref=lookup(key); bool armed=false;
        if (tierOf(ref,armed)<0 || !root(ref) || (armed && down!=3)) continue;
        Pose pose=field<Pose>(root(ref),0x68);
        if (!(pose.scale>0.001f)) continue;
        for (int h=0;h<2;++h) if (pressed & (1u<<h)) {
            auto local=pose.local(palms[h]);
            float dist=(local-Vec{0,0,10}).length();
            if (withinReach(local,armed) && dist<best) { best=dist; selected=key; }
        }
    }
    if (selected) {
        Ptr ref=lookup(selected); bool armed=false; int tier=tierOf(ref,armed);
        interact(ref,tier,armed,palms,down,pressed,now);
    }
}
} // namespace trap
