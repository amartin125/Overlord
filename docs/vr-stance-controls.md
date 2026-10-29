# Right-stick posture control

The existing turn vector's Y axis supplies deliberate stance changes. No new
SteamVR action or binding file is needed; right-stick click remains the existing
jump action. Left-stick movement and horizontal turning retain their controls.

- Push down: stand to crouch, or crouch to prone. Push up: prone to crouch, or
  crouch to stand. Up while standing does not jump.
- Activation is immediate at 0.75 vertical magnitude, with no dwell or horizontal
  angle limit. Quest/Touch testers routinely missed the former 0.90 threshold. Only the vertical axis must return to within 0.30 to rearm;
  horizontal turning neither blocks acquisition nor consumes this arming.
- A second neutral-and-push advances another level. Keeping the same direction
  held advances once more after 450 ms, only after native posture confirms the
  first step. Continuation requires at least 0.65 vertical magnitude, without a
  horizontal limit. Each stroke produces at most two requests.
  A request unconfirmed for 1.5 seconds cannot resume later without neutral.
- Right-stick click while crouched/prone requests stand and suppresses only the
  VR jump bit for that click's entire hold. This prevents jumping as soon as the
  rise completes. A fresh click while standing retains the existing jump input
  and its native `+gostand` script notification. Keyboard command bits are not
  cleared by the VR adapter.
- Pause, focus/tracking loss, reference change, invalid input, ladder, mantling,
  mounted weapons and body-owning scripted actions suppress/reset the gesture.
  Recovery requires neutral; old held input cannot change posture on resumption.

## Physical crouch

Bending down previously lowered only the camera; the native capsule stayed
standing, so players could not fit under cover. `physical_crouch` now requests
the native crouch once when the tracked head drops more than
`vr_physicalCrouchDepth` metres (default 0.35, 0 disables) below the recentered
standing height, and requests stand when the head rises back within 0.10 m of
that depth, only if it caused the crouch and the native posture is still crouch.
It shares the stick's admission, native dispatcher and `stance_binding`; a stick
or click request in the same command takes priority. It never enters or leaves
prone. A recenter re-evaluates against the new standing height, so recenter
while standing.

While the head is physically low, the tracked offset already lowers the camera.
The camera boundary therefore lifts a crouched native eye back to standing view
height (60 units) in ordinary gameplay so the view follows the real head instead
of descending twice. `vr_input_status` reports depth, low state and requests.

## Native boundary

`controller_stance.hpp` plans one discrete request from actual predicted posture,
not a VR-owned posture variable. Observed H2 `pm_flags` at offset 0x54 uses bit 0
for prone and bit 1 for crouch. Native input-request state at 0x141E8A61C instead
uses 0/1/2 for stand/crouch/prone; the two encodings are deliberately distinct.

The adapter invokes the existing binding dispatcher at 0x1403CF1E0:
`gocrouch` (101) and `goprone` (100) set absolute native requests. Rising clears
the known current native request with `+togglecrouch` (89) or `toggleprone` (99).
An already-standing request produces no toggle. No held keyboard key, direct
playerState write or forced movement pose is introduced. The dispatcher owns its
ordinary reliable binding notification. Jump presses retain the existing
explicit `+gostand` notification used by campaign scripts.

Initialization verifies the native stance stores, input-state base instruction
and exact whitelist strings. Failure disables only the added stance feature.
`vr_input_status` reports admission, request count and actual/requested posture.
Native collision, animation and script constraints remain authoritative; a low
ceiling can prevent standing, and requests may take a command frame to apply.

## Validation

Read-only native sampling observed prone request 2 / flag 1, a native rise request
0, intermediate crouch flag 2 and eventual stand flag 0 with camera height rising
from 11 to 60 native units. Native dispatcher branches and key-state behavior
were inspected in the captured native code and original whitelist.

Pure regressions cover the two tap/hold directions, immediate 0.90 activation,
diagonal acceptance, subthreshold rejection, repeat timing, vertical neutral rearming, blocked native transitions,
invalid samples, focus/reference changes, unavailable turn input with a valid
click, and suppression of jump throughout a held rise click. Headset acceptance
is pending: verify stand/crouch/prone in open space and constrained headroom,
then verify the more permissive diagonal and immediate activation feels correct.
