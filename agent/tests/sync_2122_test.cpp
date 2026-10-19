#include "coop/interactables/cord_wire.h"
#include "coop/net/session_lanes.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    using namespace coop;
    net::CordEndpoint child{};
    child.kind = 3; child.key.len = 4;
    std::memcpy(child.key.data, "oven", 4);
    std::strcpy(child.name, "cord");
    assert(cord_wire::Valid(child));
    auto peer = child;
    peer.elementId = 123; // same key, different availability of the eid during bind
    assert(cord_wire::Same(child, peer));
    std::strcpy(peer.name, "socket");
    assert(!cord_wire::Same(child, peer)); // two child components of one parent differ
    peer = child; std::memcpy(peer.key.data, "lamp", 4);
    assert(!cord_wire::Same(child, peer)); // identical component names on distinct props differ
    peer = child; peer.key.len = 32;
    assert(!cord_wire::Valid(peer));
    peer = child; std::memset(peer.name, 'x', sizeof(peer.name));
    assert(!cord_wire::Valid(peer)); // unterminated input must never reach strcmp
    peer = child; peer.name[0] = '\n';
    assert(!cord_wire::Valid(peer));
    peer = child; peer.key = {}; peer.elementId = 0;
    assert(!cord_wire::Valid(peer)); // a child with no parent identity cannot be addressed
    peer = child; peer.key = {}; peer.elementId = 7;
    assert(cord_wire::Valid(peer));
    auto another = peer; another.elementId = 8;
    assert(!cord_wire::Same(peer, another));
    net::CordEndpoint socket{};
    socket.kind = 2; std::strcpy(socket.name, "cordSocket_4");
    assert(cord_wire::Valid(socket));
    another = socket; another.elementId = 9;
    assert(!cord_wire::Valid(another)); // a level name cannot also name a prop eid
    net::CordStatePayload state{};
    state.cord = child;
    assert(cord_wire::Valid(state)); // unplug
    state.powered = 1;
    assert(!cord_wire::Valid(state)); // unplugged with power is not canonical
    state.socket = socket;
    assert(cord_wire::Valid(state));
    state.powered = 2;
    assert(!cord_wire::Valid(state));
    state.powered = 0; state.socket = {}; state.socket.elementId = 3;
    assert(!cord_wire::Valid(state)); // disguised nonempty null endpoint
    state.socket = socket; state.cord.kind = 4;
    assert(!cord_wire::Valid(state));
    for (auto kind : {net::ReliableKind::CordState, net::ReliableKind::CordIntent,
                     net::ReliableKind::PropSupport, net::ReliableKind::KitchenState,
                     net::ReliableKind::KitchenIntent}) {
        assert(net::LaneForKind(kind) == net::Lane::Normal);
        assert(!net::IsClientRelayableReliableKind(kind));
        assert(!net::IsPreWorldSendableKind(kind));
    }
    assert(net::IsClientRelayableReliableKind(net::ReliableKind::LightState)); // negative control
    assert(net::IsPreWorldSendableKind(net::ReliableKind::Join));
    std::puts("PASS: endpoint identity, malformed wire, authority routing and pre-world gating");
}
