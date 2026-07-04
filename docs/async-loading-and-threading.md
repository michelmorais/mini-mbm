# Async Loading & Threading Model

This doc covers two things every future async-loading feature needs to know: the engine's
per-frame execution order, and a specific pitfall (already hit once, see below) where a seemingly
innocuous file utility turns out to be thread-affine.

## Per-frame order

For each iteration of `CORE_MANAGER::onLoop()`'s main loop (`core-manager-common.cpp`):

```
for each plugin: plugin->onPrepare()      // ImGui plugin calls ImGui::NewFrame() here
CORE_MANAGER::update()
    device->updateFps() / adjustScaleScreen2d()
    this->logic()                         // -> SCENE_SCRIPT::onLoop() -> the Lua onLoop() callback
                                           //    (all ImGui widget calls happen here)
    MESH_MANAGER::pumpAsyncLoads()        // dispatches mesh:loadAsync() completions
    util::pumpDeferredAddPathScripts()    // dispatches deferred addPath() Lua-touching work
    this->updatePhysis() / updateAudio()
    for each plugin: plugin->onLoop(delta)  // ImGui plugin's onLoop is a no-op (just stores delta)
this->render()
    for each plugin: plugin->onRender()   // ImGui plugin calls ImGui::EndFrame() / ImGui::Render() here
this->swapBuffers()
```

Key point: `pumpAsyncLoads()` and `pumpDeferredAddPathScripts()` run **on the main thread**,
**inside** the same `NewFrame()`/`EndFrame()` bracket as the Lua `onLoop()` callback, sequentially
after it. A completion callback dispatched from either pump function is not "outside the frame" or
"on a background thread" — it runs at a well-defined point within the current frame, on the same
thread as everything else. If you're chasing a bug you suspect is about "callback timing relative
to the frame," check whether it's actually about *which thread* first computed a value that later
gets used unsynchronized — that's a different bug with different symptoms (see below).

## The async worker-thread contract

`MESH_MANAGER::loadAsync()` (`mesh-manager.cpp`) does file I/O and mesh-format parsing on a small
worker-thread pool (lazily started on first use). The contract, stated in the surrounding code
comments: worker threads only do **pure CPU work** — no GPU context, no global engine state, no
Lua. `pumpAsyncLoads()` (main thread only) does the GPU-affine finishing work (buffer/texture
creation) and fires the Lua completion callback.

**This contract was violated by a function nobody expected to be thread-affine**: `util::openFile()`
(`file-util.cpp`) — a "just open a file" utility — has a side effect of calling `util::addPath()`
on every successfully opened file, to auto-register its directory as a known search path.
`addPath()` in turn calls a registered `onAddPathScript` hook (set by the Lua layer,
`LUA_MANAGER::onAddPathScript`) that runs `luaL_dostring()` on the shared `lua_State*` to extend
`package.path` — completely fine from the main thread, a guaranteed crash from a worker thread.

**Correction from an earlier version of this doc**: it was initially assumed `parse_v11_intermediate`
also resolved texture-reference paths mid-parse, ruling out a narrower fix. Verified false —
`fillTextureReferenceForHeader` (the other `getFullPath` call in `mesh-manager.cpp`) is only reached
from `MESH_MBM_DEBUG::saveV11()`, the save path, always main-thread. The load path
(`parse_v11_frame_intermediate`) carries texture path strings and `SECTION_EXTRA_PATHS` entries
forward as raw, unresolved data; `finishLoadFromIntermediate` (main-thread-only, called from
`pumpAsyncLoads`) already resolves and registers all of it safely. The *only* unsafe call turned out
to be `parse_v11_intermediate`'s own top-level `util::openFile` call — see
`docs/async-mesh-path-resolution-refactor.md` for a scoped, low-risk follow-up plan that removes
even that one call from the worker thread, so mesh loading stops touching the mutex/deferred-queue
machinery below entirely. That plan is *not yet implemented* — the general fix here is what's
currently shipped, and it remains the right safety net regardless (it protects any other
worker-thread code path that calls these functions, present or future):

- `lsPath` (the global search-path list) is now guarded by a mutex — it was previously read/written
  unsynchronized from both the main thread and worker threads.
- `addPath()` only invokes `onAddPathScript` inline when called from the main thread. Off the main
  thread, it defers the path onto a queue; `util::pumpDeferredAddPathScripts()` (called from
  `CORE_MANAGER::update()`, right next to `MESH_MANAGER::pumpAsyncLoads()`) flushes it safely, at
  most one frame later.
- `pathRet` and a couple of function-local statics in `getFullPath()` that were returned as raw
  `.c_str()` pointers are now `thread_local` — they were shared mutable buffers being handed out as
  pointers, unsafe the moment more than one thread can call `getFullPath()`.

None of this changes observable behavior for main-thread callers (100% of usage before
`mesh:loadAsync` existed) — it only makes the off-main-thread path safe instead of undefined.

### Platform note (Android) — checked, not an issue

`getFullPath()`'s Android-only branch calls `mbm::androidCopyFileFromAsset()` to pull files out of
the APK's asset bundle. This was initially suspected of having the same class of bug via a
thread-affine `JNIEnv*` — investigation of the actual implementation
(`SPECIFIC_AUX_CONTEXT_DEVICE::copyFileFromAsset`, `specific-android.cpp`) showed it uses the
NDK-native `AAssetManager_open`/`AAsset_*` API exclusively, with zero `JNIEnv*` involved. The
codebase had already migrated away from a JNI-based implementation for this specific path (see the
comment on `addPathDroid`). No fix was needed here — but it was close enough to the same shape of
bug that it's worth remembering to check this file specifically (not a sibling function) if async
loading on Android ever needs debugging.

## If you add a new async-loaded asset type

- Worker-thread code must not call anything that reaches Lua, the GPU/GL context, or a
  platform-UI-thread-affine API (JNIEnv, etc.) — even indirectly, even through a utility function
  that looks like plain I/O. `util::openFile` was exactly that trap.
- If in doubt, reproduce under AddressSanitizer (`-DUSE_ASAN=1`, see `CMakeLists.txt`) rather than
  guessing from a single crash's stack trace — the same underlying corruption can manifest as
  different-looking crashes (a segfault in the Lua lexer, a segfault formatting an error message, a
  plain "attempt to call nil" — all observed from the exact same trigger across different runs)
  depending on memory layout and timing.
