// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DeepCrouch.h"
namespace trap {
// A passive copy of TTWVR's grip buttons. Returns -1 for stale/missing tracking.
unsigned installInput();
int grips();
crouch::Stick rightStick();
}
