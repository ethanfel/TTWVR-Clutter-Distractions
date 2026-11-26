// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "HostLayout.h"
#include "Impacts.h"

namespace crouch {
using noise::Vec;
inline float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec unit(Vec v) { return v*(1.f/v.length()); }

struct Rotation {
    float m[3][3]{};
    Vec apply(Vec v) const {
        return {dot({m[0][0],m[0][1],m[0][2]},v),dot({m[1][0],m[1][1],m[1][2]},v),
                dot({m[2][0],m[2][1],m[2][2]},v)};
    }
    Rotation operator*(const Rotation& b) const {
        Rotation r;
        for (int i=0;i<3;++i) for (int j=0;j<3;++j)
            for (int k=0;k<3;++k) r.m[i][j]+=m[i][k]*b.m[k][j];
        return r;
    }
};
// Shortest arc between two limb directions, with a deterministic axis for
// a half turn. The resulting rotation preserves lengths and twist about the limb.
inline Rotation swing(Vec from,Vec to) {
    from=unit(from); to=unit(to);
    float cosine=std::clamp(dot(from,to),-1.f,1.f);
    Vec axis=cross(from,to);
    if (cosine<-.9999f) {
        axis=unit(cross(from,std::abs(from.z)<.8f ? Vec{0,0,1} : Vec{1,0,0}));
        cosine=-1;
    } else if (axis.length()>.00001f) axis=unit(axis);
    const float a[]{axis.x,axis.y,axis.z};
    const float sine=std::sqrt(std::max(0.f,1-cosine*cosine));
    Rotation r;
    for (int i=0;i<3;++i) for (int j=0;j<3;++j)
        r.m[i][j]=(i==j ? cosine : 0.f)+(1-cosine)*a[i]*a[j];
    r.m[0][1]-=sine*axis.z; r.m[1][0]+=sine*axis.z;
    r.m[0][2]+=sine*axis.y; r.m[2][0]-=sine*axis.y;
    r.m[1][2]-=sine*axis.x; r.m[2][1]+=sine*axis.x;
    return r;
}
inline bool kneel(Vec hip,Vec knee,Vec ankle,Vec forward,float blend,Vec& newKnee,Vec& newAnkle) {
    float thigh=(knee-hip).length(),shin=(ankle-knee).length();
    if (!hip.valid() || !knee.valid() || !ankle.valid() || !forward.valid() ||
        thigh<4 || shin<4 || thigh>80 || shin>80 || !(blend>0)) return false;
    // In a low squat the hips sit behind the heels. Limit excess forward reach
    // without pulling the feet all the way back beneath the seated pelvis.
    float advance=dot(ankle-hip,forward);
    float reach=.55f*shin;
    if (advance<=reach) return false;
    newAnkle=ankle-forward*(blend*(advance-reach));
    Vec delta=newAnkle-hip;
    float distance=delta.length();
    if (distance<=std::abs(thigh-shin)+.05f || distance>=thigh+shin-.05f) return false;
    Vec line=unit(delta);
    Vec bend=forward-line*dot(forward,line);
    if (bend.length()<.05f) bend=Vec{0,0,1}-line*line.z;
    if (bend.length()<.001f) return false;
    Vec oldBend=knee-hip-line*dot(knee-hip,line);
    bend=unit(bend);
    if (oldBend.length()>.001f) {
        Vec mix=unit(oldBend)*(1-blend)+bend*blend;
        if (mix.length()>.001f) bend=unit(mix);
    }
    float along=(thigh*thigh-shin*shin+distance*distance)/(2*distance);
    newKnee=hip+line*along+bend*std::sqrt(std::max(0.f,thigh*thigh-along*along));
    return true;
}
void bindLegPose(controller::Host host);
void setKneelBlend(float value);
}
