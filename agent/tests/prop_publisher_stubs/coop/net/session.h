#pragma once
#include "coop/net/protocol.h"
#include "coop/net/eid_pose_lane.h"
#include <array>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
namespace coop::net {
struct Config;
inline constexpr uint8_t kMaxPeers=kPeerCapacity;
enum class Role { Host, Client };
class Session {
public:
    struct ReliableMessage;
    Role mode=Role::Host;
    bool linked=true,sendOk=true,active=true;
    uint64_t stops=0;
    PoseTurn turn=PoseTurn::Join;
    std::array<uint32_t,kMaxPeers> generations{};
    std::array<bool,kMaxPeers> ready{};
    std::vector<PropDriveRow> poses;
    std::vector<PropDriveEndPayload> ends;
    struct EndAttempt { int slot; PropDriveEndPayload payload; bool accepted; };
    std::vector<EndAttempt> endAttempts;
    std::array<bool,kMaxPeers> endSendOk=[] {std::array<bool,kMaxPeers> a{};a.fill(true);return a;}();
    std::function<void(const void*,size_t)> inspectEnd;
    std::function<void(const std::string&)> hook;
    Session(){generations.fill(1);ready.fill(true);}
    bool connected()const{return linked;}
    bool running()const{return active;}
    void SetLocalNpcPoseBatch(const std::vector<EntityPoseSnapshot>&){}
    int kerfusSends=0;
#ifdef OMEGA_COMMAND_FIXTURE
std::vector<PropReleasePayload> releases;
std::vector<HandTakenPayload> taken;
struct TakenAttempt{int slot;HandTakenPayload packet;bool accepted;};
std::vector<TakenAttempt> takenAttempts;
void SendPropRelease(const PropReleasePayload& p){releases.push_back(p);if(hook)hook("takeRelease");}
#endif
    struct CommandAttempt{KerfusIntentPayload packet;bool accepted;};
    std::vector<CommandAttempt> commands;
    struct KerfusAttempt{int slot;KerfusStatePayload packet;bool accepted;};
    std::vector<KerfusAttempt> kerfusAttempts;
    std::array<uint64_t,kMaxPeers> worldRevisions{};
    uint64_t SlotWorldReadyRevision(int slot)const{return worldRevisions.at(slot);}

    Role role()const{return mode;}
    uint64_t StopCount()const{return stops;}
    uint32_t peerGenerationForSlot(int slot)const{return slot>=0&&slot<kMaxPeers?generations[slot]:0;}
    bool IsSlotConnected(int slot)const{return peerGenerationForSlot(slot)!=0;}
    bool IsSlotWorldReady(int slot)const{return slot>=0&&slot<kMaxPeers&&ready[slot];}
    PoseTurn PropDrivePoseTurn(uint32_t)const{if(hook)hook("turn");return turn;}
    void PublishPropDrivePose(const PropDriveRow& p){poses.push_back(p);if(hook)hook("publish");}
    bool SendReliable(ReliableKind kind,const void* data,size_t size){
        if(kind==ReliableKind::KerfusIntent&&size==sizeof(KerfusIntentPayload)){KerfusIntentPayload p{};std::memcpy(&p,data,size);commands.push_back({p,sendOk});if(hook)hook("commandSend");return sendOk;}
        if(kind==ReliableKind::KerfusState){++kerfusSends;if(hook)hook("kerfusSend");return sendOk;}
        if(kind!=ReliableKind::PropDriveEnd||size!=sizeof(PropDriveEndPayload))return false;
        PropDriveEndPayload p{};std::memcpy(&p,data,size);if(sendOk)ends.push_back(p);
        if(hook)hook("sendEnd");return sendOk;
    }
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t size){
        #ifdef OMEGA_COMMAND_FIXTURE
        if(kind==ReliableKind::HandTaken&&size==sizeof(HandTakenPayload)){HandTakenPayload p{};std::memcpy(&p,data,size);taken.push_back(p);const bool accepted=sendOk&&endSendOk.at(slot);takenAttempts.push_back({slot,p,accepted});if(hook)hook("handTaken");return accepted;}
#endif
        if(kind==ReliableKind::KerfusState&&size==sizeof(KerfusStatePayload)){
            KerfusStatePayload p{};std::memcpy(&p,data,size);const bool accepted=sendOk&&endSendOk.at(slot);
            kerfusAttempts.push_back({slot,p,accepted});if(accepted)++kerfusSends;if(hook)hook("kerfusSend");return accepted;
        }
        if(kind!=ReliableKind::PropDriveEnd||size!=sizeof(PropDriveEndPayload))return false;
        PropDriveEndPayload p{};std::memcpy(&p,data,size);const bool accepted=sendOk&&endSendOk.at(slot);
        if(accepted){bool duplicate=false;for(const auto& old:ends)if(std::memcmp(&old,&p,sizeof(p))==0)duplicate=true;if(!duplicate)ends.push_back(p);}
        endAttempts.push_back({slot,p,accepted});auto inspect=std::move(inspectEnd);inspectEnd={};if(inspect)inspect(data,size);
        if(hook)hook("sendEnd");return accepted;
    }
};
}
