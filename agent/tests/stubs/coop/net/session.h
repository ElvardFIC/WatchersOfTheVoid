#pragma once
// Offline transport double; production protocol and receiver implementation are unchanged.
#include "coop/net/protocol.h"
#include "coop/net/eid_pose_lane.h"
#include <cstring>
#include <vector>
namespace coop::net {
struct Config;
inline constexpr uint8_t kMaxPeers=4;
enum class Role : uint8_t { Host, Client };
class Session {
public:
    using BulkSinkFn = void (*)(int, const uint8_t*, int);
    BulkSinkFn bulkSink = nullptr;
    BulkSinkFn saveBeginSink = nullptr;
    void SetBulkSink(BulkSinkFn fn) { bulkSink = fn; }
    void SetSaveBeginSink(BulkSinkFn fn) { saveBeginSink = fn; }
    bool TrySendReliableToSlot(int slot, ReliableKind kind, const void* data, int size) {
        return SendReliableToSlot(slot, kind, data, size);
    }
    struct ReliableMessage;
    std::vector<PropPoseSnapshot> incoming;
    std::vector<PropPoseSnapshot> published;
    std::vector<PropDriveEndPayload> ends;
    bool sendOk = true;
    bool linked=true;
    Role mode=Role::Host;
    bool connected() const { return linked; }
    bool running() const { return linked; }
    Role role() const { return mode; }
    std::vector<ServicePanelPayload> panels;
    std::vector<PowerGridPayload> grids;
    std::vector<RadioStatePayload> radios;
    std::vector<KitchenStatePayload> kitchens;
    std::vector<KitchenIntentPayload> kitchenIntents;
    PoseTurn PropDrivePoseTurn(uint32_t) { return PoseTurn::Join; }
    void PublishPropDrivePose(const PropPoseSnapshot& p) { published.push_back(p); }
    bool SendReliable(ReliableKind kind, const void* data, size_t size) {
        if (!sendOk) return false;
        if (kind==ReliableKind::PowerGridState && size==sizeof(PowerGridPayload)) {
            PowerGridPayload p{};std::memcpy(&p,data,size);grids.push_back(p);return true;
        }
        if (kind==ReliableKind::RadioState && size==sizeof(RadioStatePayload)) {
            RadioStatePayload p{};std::memcpy(&p,data,size);radios.push_back(p);return true;
        }
        if (kind==ReliableKind::ServicePanel && size==sizeof(ServicePanelPayload)) {
            ServicePanelPayload p{}; std::memcpy(&p,data,size); panels.push_back(p); return true;
        }
        if (kind==ReliableKind::KitchenState && size==sizeof(KitchenStatePayload)) {
            KitchenStatePayload p{}; std::memcpy(&p,data,size); kitchens.push_back(p); return true;
        }
        if (kind==ReliableKind::KitchenIntent && size==sizeof(KitchenIntentPayload)) {
            KitchenIntentPayload p{}; std::memcpy(&p,data,size); kitchenIntents.push_back(p); return true;
        }
        PropDriveEndPayload p{};
        if (size != sizeof(p)) return false;
        std::memcpy(&p, data, size); ends.push_back(p); return true;
    }
    bool SendReliableToSlot(int, ReliableKind kind,const void* data,size_t size) { return SendReliable(kind,data,size); }
    bool TakeRemotePropDriveBatch(std::vector<PropPoseSnapshot>& out) {
        out.swap(incoming); incoming.clear(); return !out.empty();
    }
};
}
