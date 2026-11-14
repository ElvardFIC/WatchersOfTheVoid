#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ue_wrap::emote_wheel {
struct Part { const wchar_t* animation; float duration; bool loop; const wchar_t* sound; };
inline constexpr Part kParts[] = {
#include "ue_wrap/actors/emote_catalogue.inc"
};
inline constexpr size_t kPartCount = sizeof(kParts)/sizeof(kParts[0]);
// Zero is stopped; wire IDs are stable manifest-order indices, never asset paths.
inline const Part* PartAt(uint16_t id) { return id && id<=kPartCount ? &kParts[id-1] : nullptr; }
struct State {
    uint32_t episode=0, musicEpisode=0;
    float position=0, musicPosition=0;
    uint16_t part=0, music=0;
};
struct Hand {
    uint32_t generation=0;
    float pose[7]{}; // bone-local XYZ and quaternion XYZW
    uint8_t bone=0; // 1=hand_L, 2=hand_R, 3=head
};
inline bool Valid(const Hand& hand){
    if(!hand.generation){if(hand.bone)return false;for(float v:hand.pose)if(v!=0)return false;return true;}
    if(hand.bone<1 || hand.bone>3)return false;
    float norm=0;
    for(int i=0;i<7;++i){if(!std::isfinite(hand.pose[i]) || std::abs(hand.pose[i])>(i<3?300.f:1.f))return false;if(i>=3)norm+=hand.pose[i]*hand.pose[i];}
    return norm>.99f && norm<1.01f;
}
inline bool Valid(const State& s) {
    const auto* part=PartAt(s.part);const auto* music=PartAt(s.music);
    return std::isfinite(s.position) && std::isfinite(s.musicPosition) &&
        s.position>=0 && s.musicPosition>=0 && s.musicPosition<=1.e6f &&
        (s.part ? part && s.episode && s.position<=part->duration+.1f : !s.episode && !s.position) &&
        (s.music ? music && music->sound && s.musicEpisode : !s.musicEpisode && !s.musicPosition);
}
}
