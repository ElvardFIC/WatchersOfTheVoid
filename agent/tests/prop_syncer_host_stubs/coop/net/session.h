#pragma once
#include "coop/net/protocol.h"
#include "coop/net/eid_pose_lane.h"
#include "coop/player/players_registry.h"
#include <array>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
namespace coop::net {
inline constexpr uint8_t kMaxPeers=coop::players::kMaxPeers;
struct Config;
enum class Role { Host, Client };
class Session {
public:
    struct ReliableMessage;
    Role mode=Role::Host;
    bool linked=true,sendOk=true;
    uint64_t stops=0;
    std::array<uint32_t,kMaxPeers> generations{};
    std::array<bool,kMaxPeers> ready{};
    std::vector<PropDriveRow> incoming[kMaxPeers];
    std::vector<PropDriveRow> clientIncoming;
    bool TakeRemotePropDriveBatch(std::vector<PropDriveRow>& out){out.swap(clientIncoming);clientIncoming.clear();return !out.empty();}
    std::vector<PropSyncAnswerPayload> answers;
    std::vector<PropSyncAnswerPayload> answerAttempts;
    std::function<void(const void*,size_t)> inspectAnswer;
    std::vector<uint8_t> recipients;
    std::vector<PropDriveEndPayload> ends;
    struct EndAttempt { int slot; PropDriveEndPayload payload; bool accepted; };
    std::vector<EndAttempt> endAttempts;
    std::array<bool,kMaxPeers> endSendOk=[] {std::array<bool,kMaxPeers> a{};a.fill(true);return a;}();
    std::function<void(const void*,size_t)> inspectEnd;
    PoseTurn turn=PoseTurn::Join;
    std::vector<PropDriveRow> poses;
    PoseTurn PropDrivePoseTurn(uint32_t)const{if(hook)hook("turn");return turn;}
    void PublishPropDrivePose(const PropDriveRow& p){poses.push_back(p);if(hook)hook("publish");}
    std::function<void(const std::string&)> hook;
    Session(){generations.fill(1);ready.fill(true);}
    bool connected()const{return linked;}
    Role role()const{return mode;}
    uint64_t StopCount()const{return stops;}
    uint32_t peerGenerationForSlot(int slot)const{return slot>=0&&slot<kMaxPeers?generations[slot]:0;}
    bool IsSlotConnected(int slot)const{return peerGenerationForSlot(slot)!=0;}
    bool IsSlotWorldReady(int slot)const{return slot>=0&&slot<kMaxPeers&&ready[slot];}
    int rttMsForSlot(int)const{return 50;}
    uint32_t TakeRemotePropDriveBatches(std::vector<PropDriveRow> (&out)[kMaxPeers]){
        uint32_t got=0;for(int slot=0;slot<kMaxPeers;++slot){out[slot].swap(incoming[slot]);if(!out[slot].empty())got|=uint32_t(1)<<slot;}
        if(hook)hook("take");return got;
    }
    bool SendReliableToSlot(uint8_t slot,ReliableKind kind,const void* data,size_t size){
        if(kind==ReliableKind::PropDriveEnd&&size==sizeof(PropDriveEndPayload)){
            PropDriveEndPayload p{};std::memcpy(&p,data,size);const bool accepted=sendOk&&endSendOk.at(slot);
            if(accepted){bool duplicate=false;for(const auto& old:ends)if(std::memcmp(&old,&p,sizeof(p))==0)duplicate=true;if(!duplicate)ends.push_back(p);}
            endAttempts.push_back({slot,p,accepted});auto inspect=std::move(inspectEnd);inspectEnd={};if(inspect)inspect(data,size);
            if(hook)hook("endSend");return accepted;
        }
        if(kind!=ReliableKind::PropSyncAnswer||size!=sizeof(PropSyncAnswerPayload))return false;
        PropSyncAnswerPayload answer{};std::memcpy(&answer,data,size);
        answerAttempts.push_back(answer);
        auto inspect=std::move(inspectAnswer);inspectAnswer={};if(inspect)inspect(data,size);
        if(sendOk){answers.push_back(answer);recipients.push_back(slot);}if(hook)hook("answer");return sendOk;
    }
    bool SendReliable(ReliableKind kind,const void* data,size_t size,int=-1){
        if(kind!=ReliableKind::PropDriveEnd||size!=sizeof(PropDriveEndPayload))return false;
        PropDriveEndPayload end{};std::memcpy(&end,data,size);
        if(sendOk)ends.push_back(end);if(hook)hook("endSend");return sendOk;
    }
};
}
