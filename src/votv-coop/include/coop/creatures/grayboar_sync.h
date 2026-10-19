#pragma once
namespace coop::net { class Session; }
namespace coop::grayboar_sync {
// The physical prop body owns movement; grayboarPawn is its pathfinding helper.
// Feed the existing prop stream from the native body tick, without a world scan.
// Host bites on puppets relay damage to their owners; host-driven client copies
// cannot inflict that damage a second time through the local save singleton.
void Install(coop::net::Session* session);
void OnDisconnect();
}
