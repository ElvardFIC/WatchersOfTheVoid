#pragma once
namespace coop::net { class Session; }
namespace coop::gravity_motion {
// The gravity gun also pushes/pulls without filling comp_gravitygun.obj.
// Keep that local physics author on the ordinary held/release stream.
void Tick(net::Session& session);
void* Target();
void YieldTarget(void* actor);
void NoteStreamed(void* actor);
void OnDisconnect();
}
