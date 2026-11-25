// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DeepCrouch.h"
#include "Impacts.h"
namespace trap {
// A passive copy of TTWVR's grip buttons. Returns -1 for stale/missing tracking.
unsigned installInput();
int grips();
unsigned char* heldObject(int hand);
bool handPoint(int hand,noise::Vec& point);
unsigned triggers();
void captureJawGrip(unsigned mask);
void jawPulse(float strength, bool latch);
crouch::Stick rightStick();
}
