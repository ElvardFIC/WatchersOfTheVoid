#pragma once
#include "coop/net/protocol.h"
#include "coop/net/eid_pose_lane.h"
#include <cstring>
#include <functional>
#include <string>
#include <vector>
namespace coop::net {
struct Config;
enum class Role { Host, Client };
class Session {
public:
    Role mode=Role::Client;
    bool linked=true,sendOk=true;
    uint64_t stops=0;
    PoseTurn turn=PoseTurn::Join;
    std::vector<PropSyncClaimPayload> claims;
    std::vector<PropDriveEndPayload> ends;
    std::vector<PropDriveRow> poses;
    std::function<void(const std::string&)> hook;
    bool connected()const{return linked;}
    Role role()const{return mode;}
    uint64_t StopCount()const{return stops;}
    PoseTurn PropDrivePoseTurn(uint32_t)const{if(hook)hook("turn");return turn;}
    void PublishPropDrivePose(const PropDriveRow& p){poses.push_back(p);if(hook)hook("publish");}
    bool SendReliable(ReliableKind kind,const void* data,size_t size){
        if(kind==ReliableKind::PropSyncClaim && size==sizeof(PropSyncClaimPayload)){
            PropSyncClaimPayload p{};std::memcpy(&p,data,size);if(sendOk)claims.push_back(p);
            if(hook)hook("sendClaim");return sendOk;
        }
        if(kind==ReliableKind::PropDriveEnd && size==sizeof(PropDriveEndPayload)){
            PropDriveEndPayload p{};std::memcpy(&p,data,size);if(sendOk)ends.push_back(p);
            if(hook)hook("sendEnd");return sendOk;
        }
        return false;
    }
};
}
