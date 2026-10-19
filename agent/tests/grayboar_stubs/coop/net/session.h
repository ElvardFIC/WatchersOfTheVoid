#pragma once
#include "coop/net/protocol.h"
#include <vector>
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
struct Session {
    Role mode=Role::Host;
    bool active=true;
    bool linked=true;
    uint32_t readyMask=2;bool sendOk=true;
    uint32_t failMask=0;
    struct ActionSend {int slot;GrayboarActionPayload packet;};
    std::vector<ActionSend> attempts,delivered;
    int sends=0;GrayboarActionPayload sent{};
    int turnSends=0;GrayboarTurnPayload turnSent{};
    struct KavotiaSend {int slot;KavotiaStatePayload packet;};
    std::vector<KavotiaSend> kavotiaAttempts,kavotiaDelivered;
    void (*sendHook)()=nullptr;
    Role role()const{return mode;}
    bool running()const{return active;}
    bool connected()const{return linked;}
    bool IsSlotWorldReady(int slot)const{return (readyMask&(1u<<slot))!=0;}
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,int size) {
        if(kind==ReliableKind::KavotiaState && size==sizeof(KavotiaStatePayload)) {
            KavotiaSend row{slot,*static_cast<const KavotiaStatePayload*>(data)};
            kavotiaAttempts.push_back(row);
            const bool accepted=sendOk && !(failMask&(1u<<slot));
            if(accepted)kavotiaDelivered.push_back(row);
            if(sendHook)sendHook();
            return accepted;
        }
        if(kind==ReliableKind::GrayboarTurn && size==sizeof(turnSent)) {
            ++turnSends;turnSent=*static_cast<const GrayboarTurnPayload*>(data);return sendOk;
        }
        if(kind!=ReliableKind::GrayboarAction || size!=sizeof(sent))return false;
        ++sends;sent=*static_cast<const GrayboarActionPayload*>(data);
        attempts.push_back({slot,sent});
        const bool accepted=sendOk && !(failMask&(1u<<slot));
        if(accepted)delivered.push_back({slot,sent});
        if(sendHook)sendHook();
        return accepted;
    }
};
}
