#pragma once
#include <cstdint>
namespace coop::net { class Session; struct ServicePanelPayload; }
namespace coop::service_panel_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::ServicePanelPayload& p,uint8_t slot);
void SendAllToSlot(int slot);
void OnDisconnect();
}
