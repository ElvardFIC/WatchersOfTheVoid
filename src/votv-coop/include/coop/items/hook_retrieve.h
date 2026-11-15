#pragma once
#include <cstdint>
#include <string>
namespace coop::net { class Session; struct HookRetrievePayload; }
namespace coop::hook_retrieve {
void Install(coop::net::Session* session);
void Note(void* actor, bool knownAnchor = false);
void Tick();
void Receive(const coop::net::HookRetrievePayload& payload, uint8_t sender);
void SendAllToSlot(int slot);
void CaptureJoinHooks(int slot);
void SendJoinGone(int slot);
void OnPeerLeftSlot(int slot);
void OnDisconnect();
uint64_t GrantedCount();
uint64_t AskedCount();
uint64_t TakenCount();
uint64_t RefusedAnswerCount();
uint64_t GoneCopyCount();
void* FindHook(const std::wstring& key);
}
