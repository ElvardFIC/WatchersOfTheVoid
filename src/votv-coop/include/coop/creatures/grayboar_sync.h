#pragma once
namespace coop::net { class Session; }
namespace coop::grayboar_sync {
// The physical prop body owns movement; grayboarPawn is its pathfinding helper.
// Feed the existing prop stream from the native body tick, without a world scan.
void Install(coop::net::Session* session);
void OnDisconnect();
}
