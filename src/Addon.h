// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "GameScript.h"

struct NVSE { unsigned version,runtime,editor,isEditor; void *add,*opcode;
    void* (__cdecl* query)(unsigned); unsigned (__cdecl* handle)(); };
struct Events { void* before[3]; bool (__cdecl* listen)(const char*,void (__cdecl*)(void*,void*)); };

namespace addon {
extern const char *const name, *const logPath;
extern Scripts* scripts;
extern Arrays* arrays;
void log(const char* message);
double seconds();
bool connect(const NVSE* n);
bool initialize();
void reset();
void suspend();
void interruptHands();
void frame(unsigned char* player,double now,float dt,bool drawn);
}
