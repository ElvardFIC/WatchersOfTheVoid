#pragma once
// Legacy garden/wall fixtures do not execute the new marker native boundary.
// The dedicated llama marker fixture includes the actual implementation instead.
#include "ue_wrap/actors/llama_marker.h"
namespace ue_wrap::llama_marker {
inline bool Is(void*){return false;}inline bool Ready(void*){return false;}
inline bool ValidKey(const std::wstring&){return false;}
inline bool Normalize(const save_record::SaveRecord&,save_record::SaveRecord&){return false;}
inline std::wstring Key(void*){return {};}
inline bool SetKey(void*,const std::wstring&){return false;}
inline bool Capture(void*,save_record::SaveRecord&){return false;}
inline bool Apply(void*,const save_record::SaveRecord&){return false;}
inline void* Spawn(const save_record::SaveRecord&){return nullptr;}
inline void OnDisconnect(){}
}
