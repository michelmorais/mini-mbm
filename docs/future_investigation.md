# Future Investigation

Open engineering questions that were surfaced during other work but deliberately not resolved —
either because reproducing them requires conditions this sandbox/dev environment doesn't have
(a real mouse, a specific desktop's display stack), or because fixing them wasn't in scope for the
task that found them. Each entry should have enough context that someone picking it up cold
doesn't have to re-derive the investigation from scratch.

---

## `mbm.getPickRay` / `obj:collide()` skip the `camera.scaleScreen2d` correction `mbm.to3d` applies

**Found:** 2026-07-15, while debugging a 3D mouse-picking bug in `editor/mesh_debug.lua`'s Bones
node (bone gizmo click-drag — since removed, see below).

**The inconsistency:**

- `DEVICE::rayCast(sx, sy, ...)` (`src/core_mbm/device-common.cpp:1360`) expects `sx,sy` in raw
  backbuffer pixel space (`impl->backBufferWidth`/`Height`).
- The engine's long-established `mbm.to3d` (`ontransform2dsto3dmbm` →
  `DEVICE::transformeScreen2dToWorld3d_scaled`, `device-common.cpp:1445`) first multiplies the
  input x,y by `camera.scaleScreen2d.x`/`.y` before calling `rayCast`.
- `camera.scaleScreen2d` = `backBufferWidth / camera.expectedScreen.x` (and the `.y` equivalent),
  computed in `CORE_MANAGER::adjustScaleScreen2d` (`core-manager-common.cpp:744`).
  `camera.expectedScreen` is normally pinned to the actual launch resolution on the first
  `onLoop()` iteration (`core-manager-common.cpp:231-232`), or explicitly overridden via the
  `-ew`/`-eh` CLI flags — so `scaleScreen2d` is `1.0` unless the window is resized after that
  point, or `-ew`/`-eh` diverges from `-w`/`-h`.
- The newer `mbm.getPickRay` Lua binding (`onGetPickRay`, `src/lua-wrap/framework-lua.cpp:1004`)
  and the existing `obj:collide(x, y)` 3D branch (`onCheckCollisionBoundingBoxRenderizable`,
  `src/lua-wrap/common-methods-lua.cpp:315`) both call `device->rayCast()` **directly**, with no
  `scaleScreen2d` correction at all.

**Why it matters:** `obj:collide()` is not a safer or more-correct alternative to
`mbm.getPickRay` for 3D screen-to-world picking — both hit the exact same raw path with the exact
same gap. Fixing one without the other, or assuming "switch to the other API" resolves a picking
bug, is a dead end.

**Status — NOT the confirmed root cause of the bug that surfaced it.** The bug report was:
in mesh_debug.lua's Bones node, clicking/dragging a bone gizmo failed completely at camera
distance >= 5 world units (only started working at <= 4), and the click landed increasingly far
left/up of the visible object as the window resolution was lowered. A live empirical test was
built to check this directly: a marker sphere placed at world origin, camera positioned on-axis
via the same `cam3dGetPos`/`applyCam3d` formula the editor uses, then a grid of screen pixels
scanned to find which pixel's `mbm.getPickRay` actually passes closest to the marker. By symmetry,
an on-axis marker under an on-axis camera must hit dead center in a bug-free pipeline — no
assumption needed about which internal formula might be wrong.

Result: **zero offset**, at every camera distance tried (100/200/400/800 world units), at 800x600,
and with an ImGui window actively rendering on top. So the raw `mbm.getPickRay` math is exact in
every configuration reproducible in this sandbox — no aspect-ratio bug, no per-distance drift, no
ImGui interference. The `scaleScreen2d` gap above is real, but for a normal (non-resized) launch it
evaluates to 1.0, so it can't be the cause of what the user saw.

The user's actual bug was never root-caused. It's suspected to be specific to their desktop's
display/input stack (HiDPI or fractional display scaling, window manager behavior, or a
multi-monitor setup) causing the real x,y that X11 delivers to `onTouchDown`/`onTouchMove` to not
be 1:1 with backbuffer pixel space — something this sandbox has no real mouse or matching display
stack to reproduce. Given that, the 3D mouse click-drag bone-editing feature in mesh_debug.lua's
Bones node was removed entirely rather than shipped half-working; bone position is now edited only
via numeric `DragFloat` X/Y/Z fields in the Bones table.

**How to apply, if mouse-based 3D picking is revisited anywhere in the editor tools:**

1. Fix `onGetPickRay` to apply `camera.scaleScreen2d` the same way `mbm.to3d` does, for
   consistency — worth doing even though it wasn't the active bug here, since it's a real
   divergence from the engine's own established convention and will bite the next `-ew`/`-eh` or
   post-launch-resize scenario.
2. Before re-attempting ray-based hit-testing in an editor tool, get a live on-screen readout of
   raw `onTouchMove` x,y next to `mbm.getRealSizeScreen()` running on the *actual* machine that
   will use the feature, and eyeball whether they line up at a couple of known reference points
   (e.g. the window's edges). Don't re-derive this purely from code reading or from an agent
   sandbox — the pure ray/projection math has already been proven correct in isolation; the gap is
   somewhere between the OS/window manager and the engine's touch-event pipeline, which only a
   real desktop session can expose.
