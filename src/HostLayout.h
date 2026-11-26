// SPDX-License-Identifier: GPL-3.0-only
#pragma once

namespace controller {
struct Backend {
    unsigned table, validStore, pulseEntry, pulseBody;
};
struct GrabSlot {
    unsigned active, hand, object;
};
struct CrouchLayout {
    unsigned update, manual, target, smoothed, rigHeightOffset;
    unsigned poseCall, findPose, applyPose, boundRoot, bones;
};
struct Layout {
    const char* version;
    unsigned timestamp, imageSize;
    Backend xr, vr;
    GrabSlot primary, secondary;
    unsigned physicalPose, manager;
    CrouchLayout crouch;
};
inline constexpr Layout builds[]={
    {"1.1.8",0x6b07a53e,0x1ffa000,
     {0xcf62e8,0x8302bb,0x45c0,0x81d8c0},{0xcf6450,0x80b0f4,0x275d9,0x808680},
     {0x1744d05,0x1240ed8,0x1745ae0},{0x1744dd4,0x1744dd8,0x1744ddc},
     0x84d1b0,0x1f8cbe0,
     {0x88437,0x12f3514,0x12f3528,0x12f352c,0xc08,
      0x47c73f,0x47d1b0,0x47cbe0,0x12f2378,0x12f237c}},
    {"1.1.7",0x6afa9b4f,0x1d84000,
     {0xaf3568,0x6f0e9b,0x3f0d,0x6de400},{0xaf36d0,0x6cbbb4,0x22868,0x6c9140},
     {0x1538805,0x1035e60,0x15395e0},{0x15388d4,0x15388d8,0x15388dc},
     0x70acb0,0x1d1fab0,
     {0x7e407,0x10e7094,0x10e70a8,0x10e70ac,0xb48,
      0x45ddef,0x45e860,0x45e290,0x10e5ef8,0x10e5efc}},
};
struct Host {
    unsigned char* base{};
    const Layout* layout{};
    explicit operator bool() const { return layout!=nullptr; }
};
}
