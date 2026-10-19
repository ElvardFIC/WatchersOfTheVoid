#pragma once
#include "coop/net/protocol.h"
#include "ue_wrap/devices/kitchen_machine.h"
#include <cmath>
namespace coop::kitchen_wire {
inline void EncodePose(const ue_wrap::kitchen_machine::State& s,net::KitchenStatePayload& p) {
    p.hasDoorPose=s.hasDoorPose ? 1 : 0;
    if (!s.hasDoorPose) return;
    const auto& t=s.doorPose;
    const float values[]={t.RotX,t.RotY,t.RotZ,t.RotW,t.TX,t.TY,t.TZ,t.SX,t.SY,t.SZ};
    for (int i=0;i<10;++i) p.doorPose[i]=values[i];
}
inline bool ValidPose(const net::KitchenStatePayload& p) {
    if (p.hasDoorPose>1) return false;
    if (!p.hasDoorPose) return true;
    if (p.kind!=1 && p.kind!=4 && p.kind!=6) return false;
    for (float n:p.doorPose) if (!std::isfinite(n)) return false;
    float length=0;
    for (int i=0;i<4;++i) length+=p.doorPose[i]*p.doorPose[i];
    if (length<0.98f || length>1.02f) return false;
    for (int i=4;i<7;++i) if (std::fabs(p.doorPose[i])>500.f) return false;
    for (int i=7;i<10;++i) if (p.doorPose[i]<=0 || p.doorPose[i]>10.f) return false;
    return true;
}
inline ue_wrap::FTransform DecodePose(const net::KitchenStatePayload& p) {
    ue_wrap::FTransform t{};
    t.RotX=p.doorPose[0]; t.RotY=p.doorPose[1]; t.RotZ=p.doorPose[2]; t.RotW=p.doorPose[3];
    t.TX=p.doorPose[4]; t.TY=p.doorPose[5]; t.TZ=p.doorPose[6];
    t.SX=p.doorPose[7]; t.SY=p.doorPose[8]; t.SZ=p.doorPose[9]; return t;
}
inline bool SamePose(const net::KitchenStatePayload& a,const net::KitchenStatePayload& b) {
    if (a.hasDoorPose!=b.hasDoorPose) return false;
    if (!a.hasDoorPose) return true;
    for (int i=0;i<10;++i) if (std::fabs(a.doorPose[i]-b.doorPose[i])>(i<4 ? 0.001f : 0.02f)) return false;
    return true;
}
}
