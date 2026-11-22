// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "GameScript.h"
namespace trap {
bool initialize(Scripts*,Arrays*,void (*logger)(const char*));
void frame(unsigned char* pc,double now);
void suspend();
void reset();
}
