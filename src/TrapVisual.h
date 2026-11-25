// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "World.h"

namespace trap {
// These rotations are the closed/open keys from BearTrap01.nif. Only write
// transforms on our unanimated mesh, never on a hand bone or the live activator.
inline void setCarryBasis(world::Ptr mesh,bool handheld) {
    const float carry[3][3]={{0,0,-1},{-1,0,0},{0,1,0}};
    const float original[3][3]={{1,0,0},{0,1,0},{0,0,1}};
    std::memcpy(mesh+0x34,handheld ? carry : original,sizeof carry);
}
inline void poseTrap(world::Ptr mesh,float spread,const noise::Vec* origin=nullptr) {
    if (!mesh) return;
    if (origin) std::memcpy(mesh+0x58,origin,sizeof(*origin));
    float angle=std::asin(std::clamp(spread,0.f,1.f));
    auto rotate=[&](const char* name,int axis,float rad) {
        auto part=world::named(mesh,name);
        if (!part) return;
        float matrix[3][3]={{1,0,0},{0,1,0},{0,0,1}};
        int j=(axis+1)%3,k=(axis+2)%3;
        matrix[j][j]=matrix[k][k]=std::cos(rad);
        matrix[j][k]=-std::sin(rad); matrix[k][j]=std::sin(rad);
        std::memcpy(part+0x34,matrix,sizeof matrix);
    };
    rotate("JawFront01",0,angle); rotate("JawFront02",0,angle);
    rotate("JawRear01",0,-angle); rotate("JawRear02",0,-angle);
    float spring=-0.08726646f+0.43633231f*(angle/1.57079633f);
    rotate("UpperLeftTension",1,spring); rotate("UpperRightTension",1,spring);
    const unsigned updateData[3]{};
    using Update=void (__thiscall*)(world::Ptr,const void*,unsigned);
    world::field<Update>(world::ptr(mesh,0),0xa4)(mesh,updateData,0);
}
}
