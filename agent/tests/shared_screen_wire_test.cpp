#include "coop/interactables/shared_screen_wire.h"
#include "coop/net/ragdoll_pose_validation.h"
#include <cassert>
#include <limits>
#include <cstdio>
int main() {
    using namespace coop::shared_screen_wire;
    std::vector<Row> source(2), decoded;
    source[0].path = "pc/r/0"; source[0].flags = 1; source[0].tab = 4;
    source[1].path = "pc/r/1"; source[1].flags = 6; source[1].visibility = 3;
    source[1].scroll = 123.5f; source[1].text = L"Пошта\nТест \U0001F4BB";
    auto bytes = Encode(source);
    assert(!bytes.empty() && Decode(bytes, decoded));
    assert(decoded[0].tab == 4 && decoded[1].visibility == 3 && decoded[1].scroll == 123.5f);
    assert(decoded[1].text == source[1].text);
    for (size_t n = 0; n < bytes.size(); ++n) {
        std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + n);
        assert(!Decode(truncated, decoded));
    }
    auto extra = bytes; extra.push_back(0); assert(!Decode(extra, decoded));
    source[1].path = source[0].path; assert(!Decode(Encode(source), decoded));
    source[1].path = "../../other"; assert(!Decode(Encode(source), decoded));
    source[1].path = "pc/r/1";
    source[1].scroll = std::numeric_limits<float>::infinity(); assert(Encode(source).empty());
    source[1].scroll = 0; source[1].text.assign(4097, L'x'); assert(Encode(source).empty());
    source[1].text.clear(); source[0].tab = -1; assert(Decode(Encode(source), decoded));
    coop::net::RagdollPoseSnapshot pose{};
    pose.rigSignature = 1; pose.rigBones = 17; pose.boneCount = 2;
    pose.bones[0] = {1, 0, 0, 0, 0, 0, 0, 32767};
    pose.bones[1] = {4, 30, 0, 0, 0, 0, 0, 32767};
    assert(coop::net::ValidateRagdollBones(pose));
    pose.bones[1].index = 1; assert(!coop::net::ValidateRagdollBones(pose));
    pose.bones[1].index = 4; pose.bones[1].x = 1001; assert(!coop::net::ValidateRagdollBones(pose));
    pose.bones[1].x = 0; pose.bones[1].qw = 0; assert(!coop::net::ValidateRagdollBones(pose));
    pose.boneCount = 9; assert(!coop::net::ValidateRagdollBones(pose));
    static_assert(sizeof(coop::net::RagdollPosePacket) <= coop::net::kMaxPacketBytes);
    puts("PASS shared UI Unicode/state roundtrip, malformed/truncated bounds and ragdoll bone validation");
}
