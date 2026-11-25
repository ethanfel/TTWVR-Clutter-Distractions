// SPDX-License-Identifier: GPL-3.0-only
#include "Addon.h"
#include "BearTraps.h"
#include "TrapInput.h"
#include "ClutterInput.h"

namespace addon {
const char* const name="TTWVR Bear Traps";
const char* const logPath="Data/NVSE/Plugins/TTWVRBearTraps.log";
static bool inputUnavailable=false;
bool connect(const NVSE*) { return true; }
bool initialize() {
    bool ok=trap::initialize(scripts,arrays,log);
    inputUnavailable=!ok || !trap::inputAvailable();
    return ok;
}
void reset() { trap::reset(); }
void suspend() { trap::suspend(); }
void interruptHands() { trap::interruptHands(); }
void frame(unsigned char* pc,double now,float,bool drawn) { trap::frame(pc,now,drawn); }
}

// Optional read-only interface for Clutter Audio. Both DLLs run independently.
// Until our first game tick, report missing tracking so either load order works.
extern "C" __declspec(dllexport) int __cdecl TTWVRBearTrapGripsV1() {
    return addon::inputUnavailable ? clutter::noProvider : trap::grips();
}
extern "C" __declspec(dllexport) bool __cdecl TTWVRBearTrapIsPortableV1(unsigned char* ref) {
    return trap::isPortable(ref);
}
