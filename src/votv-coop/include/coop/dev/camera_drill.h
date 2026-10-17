// coop/dev/camera_drill.h -- [dev] a picked-up camera: does it stick to its wall again, and by whose hand.
//   HOST   -- once its world is up, with no wait for a client: finds a flat wall near the player's start that a walker
//             reaches by a NavMesh route, hangs a prop_camera_bad_C on it, sticks it through the prop-stick lane's
//             converge, and reads whether the camera is stuck AND attached to the wall (a raw frozen-write fallback
//             is frozen but attached to nothing, and is reported as a failure, never as stuck). It says
//             "camera_drill: host READY stuck key=... at=..." only when it reads so.
//   CLIENT -- once world-ready: scans (once a second) for the nearest stuck prop_camera_bad_C in its world, walks to it
//             by the director, grabs it by the use key's grab chain, holds 3 s of the world's own clock standing still,
//             lets go, and watches 2.5 s more. Every tick while held it samples the camera's stuck flags; the first
//             stuck sample is "camera_drill: re-stuck while held at +...ms".
// The deciding read is the prop-stick lane's own lines on both peers (broadcast UNSTICK / STICK, STICK applied);
// this drill adds none of its own wire. "[CAMERA-DRILL] FAIL" is the lane failing a step (--fail-marker),
// "[CAMERA-DRILL] ABANDONED" the drill unable to do its part (--dead-marker). Run with camera_drill=grab and
// --done-marker "[CAMERA-DRILL] client DONE".

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::camera_drill {

// Game thread, once per pump tick; a latched bool read when off.
void Tick(coop::net::Session* session);

// The per-session half: a rejoin's client runs its legs again; the host's camera stays, or is hung again if it is gone.
void OnDisconnect();

}  // namespace coop::dev::camera_drill
