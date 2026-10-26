#pragma once
#include "ue_wrap/actors/save_record.h"
#include <string>
namespace ue_wrap::llama_marker {
bool Is(void* actor);
bool Ready(void* actor);
bool ValidKey(const std::wstring& key);
bool Normalize(const save_record::SaveRecord& input,save_record::SaveRecord& output);
std::wstring Key(void* actor);
bool SetKey(void* actor,const std::wstring& key);
bool Capture(void* actor,save_record::SaveRecord& output);
bool Apply(void* actor,const save_record::SaveRecord& input);
void* Spawn(const save_record::SaveRecord& input);
// Only failed bodies created by this wrapper are eligible for cleanup.
void OnDisconnect();
}
