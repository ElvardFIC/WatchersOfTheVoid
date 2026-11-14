#pragma once
#include <cstdint>
#include <string>
#include <functional>

namespace ue_wrap::radio_player {
struct State { uint64_t track=0; int64_t ticks=0; uint8_t playing=0,mode=0; };
// Current custom-radio interface, whose imported catalog is local on every peer.
bool IsRadio(void* actor);
// Class-pure form for the shared scan hub's per-class predicate cache.
bool IsRadioClass(void* cls);
bool HasLocalTrack(void* actor);
void* UiRadio(void* widget);
bool OwnsPlayer(void* player);
// Synchronous caller-owned access. Authority is checked around every reflected
// call; callbacks are never retained. False may follow an attempted native call.
bool Read(void* actor,State& out,const std::function<bool()>& current={});
bool Apply(void* actor,const State& state,const std::function<bool()>& current={});
// Stable across different installation roots; all UTF-16 code units participate.
inline uint64_t TrackId(std::wstring path) {
    for (auto& c:path) { if (c==L'\\') c=L'/'; if (c>=L'A' && c<=L'Z') c+=L'a'-L'A'; }
    const auto root=path.rfind(L"/radio/");
    if (root!=std::wstring::npos) path.erase(0,root+7);
    else if (path.rfind(L"radio/",0)==0) path.erase(0,6);
    else { const auto slash=path.find_last_of(L'/'); if (slash!=std::wstring::npos) path.erase(0,slash+1); }
    if (path.empty()) return 0;
    uint64_t h=14695981039346656037ull;
    for (wchar_t c:path) for (unsigned shift:{0u,8u}) { h^=(uint16_t(c)>>shift)&255; h*=1099511628211ull; }
    return h ? h : 1;
}
}
