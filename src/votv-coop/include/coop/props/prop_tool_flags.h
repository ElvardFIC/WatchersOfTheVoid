#pragma once

namespace coop::net {
class Session;
struct PropToolFlagsPayload;
}

namespace coop::prop_tool_flags {

void Install(coop::net::Session* session);
void Tick();
void OnToolFlags(int senderSlot, const coop::net::PropToolFlagsPayload& payload);
void OnDisconnect();

}
