#include "coop/net/ragdoll_pose_buffer.h"
#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
using namespace coop::net;

int main(int argc, char** argv) {
    RagdollPoseSnapshot meta{};
    meta.rigTag = 1; meta.rigSignature = 42; meta.rigBones = 17;
    std::vector<RagdollBonePose> bones;
    for (uint16_t i = 0; i < 17; ++i) bones.push_back({i, float(i), 0, 0, 0, 0, 0, 32767});
    const bool legacy = argc > 1;
    if (legacy && std::string(argv[1]) == "legacy-time") {
        std::set<uint16_t> received;
        // A 75ms publisher/effective-send cadence always samples one 25ms phase.
        for (int ms = 0; ms < 7500; ms += 75) {
            const size_t start = (size_t(ms / 25) % 3) * 7;
            for (size_t j = start; j < (std::min)(start + 7, bones.size()); ++j) received.insert(bones[j].index);
        }
        if (received.size() != 17) { puts("REPRODUCED: wall-clock batching starves limbs at a 75ms send cadence"); return 1; }
    }
    if (legacy) {
        StreamSlot<RagdollPoseSnapshot> inbox;
        RagdollPoseOutbox sender; sender.Publish(meta, bones.data(), bones.size());
        for (int i = 0; i < 3; ++i) inbox.Offer(i, sender.Next());
        RagdollPoseSnapshot p{}; bool fresh = false; inbox.Read(p, &fresh);
        if (p.boneCount != 17) { puts("REPRODUCED: latest-packet inbox discards other limbs during a game-thread hitch"); return 1; }
    }
    RagdollPoseOutbox sender;
    RagdollPoseInbox receiver;
    uint32_t sequence = 0xfffffff0u;
    for (int frame = 0; frame < 100; ++frame) {
        // Hundreds of publishes between sends may refresh values, never the turn.
        sender.Publish(meta, bones.data(), bones.size());
        if (frame % 30 == 0) assert(receiver.Offer(++sequence, sender.Next()));
    }
    std::set<uint16_t> received;
    RagdollPoseSnapshot out{}; bool fresh = false;
    while (receiver.Read(out, &fresh) && fresh)
        for (int i = 0; i < out.boneCount; ++i) received.insert(out.bones[i].index);
    assert(received.size() == 17);
    assert(!receiver.Offer(sequence - 2, out)); // No rewind after a hitch/drain.
    meta.rigTag = 2;
    meta.rigBones = 1; bones.resize(1); bones[0].index = 99;
    sender.Publish(meta, bones.data(), bones.size());
    assert(receiver.Offer(++sequence, sender.Next()));
    assert(receiver.Read(out, &fresh) && fresh && out.boneCount == 1 && out.bones[0].index == 99);
    assert(receiver.Read(out, &fresh) && !fresh); // No limb from the preceding entry.
    receiver = {}; // A reconnect/new occupancy starts a new sequence space.
    assert(receiver.Offer(0, sender.Next()));
    assert(receiver.Read(out, &fresh) && fresh);
    auto invalid=sender.Next(); invalid.bones[0].qw=0;
    assert(!receiver.Offer(1000, invalid));
    assert(receiver.Offer(1, sender.Next())); // Invalid input must not advance the sequence.
    // Loss, long receive bursts, duplication, reordering and sequence wrap.
    for (int count : {1, 5, 17, 35, 64}) {
        sender = {}; receiver = {}; meta.rigBones = static_cast<uint16_t>(count);
        bones.clear(); for (int i = 0; i < count; ++i) bones.push_back({uint16_t(i),float(i),0,0,0,0,0,32767});
        received.clear(); sequence = 0xffffffb0u;
        for (int tick = 0; tick < 300; ++tick) {
            sender.Publish(meta, bones.data(), bones.size());
            auto packet = sender.Next(); ++sequence;
            if (tick % 5 != 0) {
                assert(receiver.Offer(sequence, packet));
                assert(!receiver.Offer(sequence, packet));
                assert(!receiver.Offer(sequence - 1, packet));
            }
            if (tick % 23 == 0) while (receiver.Read(out, &fresh) && fresh)
                for (int i = 0; i < out.boneCount; ++i) received.insert(out.bones[i].index);
        }
        if (received.size() != size_t(count)) {
            std::fprintf(stderr, "rig=%d received=%zu\n", count, received.size());
            for (int i = 0; i < count; ++i) if (!received.count(uint16_t(i))) std::fprintf(stderr, "missing=%d\n", i);
            return 2;
        }
    }
    puts("PASS per-send ragdoll rotation and per-bone receive merge: cadence, hitches, loss, wrap, rigs and reset");
}
