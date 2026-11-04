#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <vector>
#include <functional>
#include <cstring>
namespace coop::net {
inline constexpr int kMaxPeers=32;
enum class Role {Host,Client};
class Session {
public:
    Role mode=Role::Host;bool active=true,linked=true,sendOk=true;
    uint32_t ready=0x80000006u,fail=0;
    std::array<uint32_t,32> generations{};
    struct Send {int slot;ReliableKind kind;std::vector<uint8_t> bytes;};
    std::vector<Send> sends;
    std::function<void()> sendHook;
    Session(){generations.fill(1);}
    Role role()const{return mode;}bool running()const{return active;}bool connected()const{return linked;}
    bool IsSlotWorldReady(int slot)const{return slot>=0&&slot<32&&(ready&(1u<<slot));}
    uint32_t peerGenerationForSlot(int slot)const{return slot>=0&&slot<32?generations[slot]:0;}
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t size) {
        Send row{slot,kind,{}};row.bytes.resize(size);std::memcpy(row.bytes.data(),data,size);sends.push_back(row);
        const bool result=sendOk&&(slot<0||!(fail&(1u<<slot)));
        if(sendHook)sendHook();return result;
    }
    bool SendReliable(ReliableKind kind,const void* data,size_t size){return SendReliableToSlot(-1,kind,data,size);}
};
}
