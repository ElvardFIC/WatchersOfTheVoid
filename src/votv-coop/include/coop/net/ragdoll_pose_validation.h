#pragma once
#include "coop/net/protocol.h"
namespace coop::net {
inline bool ValidateRagdollBones(const RagdollPoseSnapshot& pose) {
    if (pose.boneCount > 7 || pose.rigBones > 64 || pose.boneCount > pose.rigBones ||
        (pose.boneCount && !pose.rigSignature)) return false;
    for (uint8_t i = 0; i < pose.boneCount; ++i) {
        const auto& b = pose.bones[i];
        const float norm = (float(b.qx)*b.qx + float(b.qy)*b.qy + float(b.qz)*b.qz + float(b.qw)*b.qw) / (32767.f*32767.f);
        if (b.index >= 1024 || !std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.z) ||
            std::fabs(b.x - pose.x) > 1000.f || std::fabs(b.y - pose.y) > 1000.f ||
            std::fabs(b.z - pose.z) > 1000.f || norm < .9f || norm > 1.1f) return false;
        for (uint8_t j = 0; j < i; ++j) if (pose.bones[j].index == b.index) return false;
    }
    return true;
}
}
