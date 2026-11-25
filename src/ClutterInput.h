// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Impacts.h"

namespace clutter {
constexpr int noProvider=-2;
void initializeInput(void (*logger)(const char*));
int grips();
unsigned char* heldObject(int hand);
bool handPoint(int hand,noise::Vec& point);
bool isPortable(unsigned char* ref);
}
