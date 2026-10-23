#pragma once
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <array>
#include <cstring>
#include <algorithm>
#include <functional>
#include <vector>

namespace coop::props::container_contents_sync::detail {
struct AppliedBinding {
    ue_wrap::CachedObjRef actor,inventory,save;
    uint8_t* slot=nullptr;
    std::array<uint8_t,16> header{};
    bool Matches(void* a,void* inv,void* s,uint8_t* address)const {
        return actor.Is(a) && inventory.Is(inv) && save.Is(s) && slot && slot==address &&
            !std::memcmp(header.data(),address,header.size());
    }
};
// Stack-owned, game-thread scopes: no ledger grows for untrusted element ids.
// Accepted input/local mutation invalidates older dispatch for that eid.
class ApplySource {
    inline static thread_local std::vector<ApplySource*> active_;
    net::Session* session_;
    uint32_t eid_,serial_,world_,peer_=0;
    uint8_t author_;
    net::Role role_;
    bool valid_=true;
    std::function<bool()> installed_;
public:
    ApplySource(net::Session* session,uint32_t eid,uint8_t author,std::function<bool()> installed)
        :session_(session),eid_(eid),serial_(net::session_serial::Current()),world_(ue_wrap::world_identity::Generation()),
         author_(author),role_(session?session->role():net::Role::Host),installed_(std::move(installed)) {
        valid_=session && author<net::kMaxPeers;
        if(valid_)peer_=session->peerGenerationForSlot(author);
        active_.push_back(this);
    }
    ~ApplySource(){active_.erase(std::find(active_.begin(),active_.end(),this));}
    ApplySource(const ApplySource&)=delete;
    ApplySource& operator=(const ApplySource&)=delete;
    static void Changed(uint32_t eid){for(auto* scope:active_)if(scope->eid_==eid)scope->valid_=false;}
    void Claim(){for(auto* scope:active_)if(scope!=this && scope->eid_==eid_)scope->valid_=false;}
    bool Current()const {
        return valid_ && installed_ && installed_() && serial_==net::session_serial::Current() &&
            world_==ue_wrap::world_identity::Generation() && session_->running() && session_->connected() &&
            session_->role()==role_ && session_->peerGenerationForSlot(author_)==peer_ &&
            (role_!=net::Role::Host || !author_ || session_->HasPeerConn(author_));
    }
};
}
