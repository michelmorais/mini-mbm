# Plan: Remove the Last Worker-Thread `getFullPath`/`addPath` Call from Async Mesh Loading

Status: **not started** — written up for later, kept in-repo per request. Companion to
`docs/async-loading-and-threading.md`, which documents the mutex/deferred-queue fix already
shipped for the bug this plan follows up on.

## Why

The shipped fix (`file-util.cpp`: `g_lsPathMutex`, `invokeOrDeferAddPathScript`,
`pumpDeferredAddPathScripts()`) makes it *safe* for `util::openFile`/`getFullPath`/`addPath` to be
called from a worker thread, but every call still pays a mutex lock, even on the main thread where
there's never any contention. This plan removes the *need* for the worker thread to call those
functions at all for mesh loading specifically, so the hot async-loading path stops touching the
mutex entirely. The general-purpose fix stays in place as a safety net for anything else that might
call these functions off the main thread in the future — this plan is about performance/cleanliness
for the one known, common case, not a replacement for it.

## Corrected scope (verified against the current code, not assumed)

An earlier pass through this bug assumed texture references discovered mid-parse were a second,
harder-to-pin-down source of worker-thread path calls. That assumption was wrong — checked and
ruled out:

- `fillTextureReferenceForHeader()` (`mesh-manager.cpp:268`, contains the only other
  `util::getFullPath` call in this file besides the one below) is called **only** from
  `MESH_MBM_DEBUG::saveV11()` (call sites at `mesh-manager.cpp:2386` and `:2405`) — the *save* path,
  always invoked synchronously from the main thread. Not reachable from `loadAsync`.
- The actual *load* parser, `parse_v11_frame_intermediate()` (`mesh-manager.cpp:599`), copies raw
  texture path strings straight into `IntermediateSubsetV11`/`IntermediateExtraSlotV11`
  (`subset.primaryTexturePath = subsetDesc.primaryTexture.path;` etc., `mesh-manager.cpp:674,687`)
  with **zero** path resolution during parsing.
- `SECTION_EXTRA_PATHS` (embedded extra search paths) is parsed the same way — raw strings only,
  collected into `MESH_LOAD_INTERMEDIATE_V11::extraPaths` (`mesh-manager.cpp:119`), no `addPath`
  call during parsing (verified: `parse_v11_intermediate`'s own body, `mesh-manager.cpp:702-903`,
  has no `addPath` call at all).
- `MESH_MBM::finishLoadFromIntermediate()` (`mesh-manager.cpp:4002`, called only from
  `MESH_MANAGER::pumpAsyncLoads()`, i.e. always main-thread) is where all of that raw data actually
  gets resolved: `util::addPath()` for every collected `extraPaths` entry
  (`mesh-manager.cpp:4025-4026`) and `TEXTURE_MANAGER::getInstance()`-driven texture loading
  (`mesh-manager.cpp:4033` onward, which itself resolves paths internally) — already exactly the
  "collect raw data on the worker thread, resolve it on the main thread" pattern this plan proposes,
  just not yet applied to the one remaining call site.

**The only unsafe call left is `parse_v11_intermediate`'s own top-level file open:**
`FILE *fp = util::openFile(fileNamePath, "rb");` at `mesh-manager.cpp:704`. Unlike the texture/extra-path
case, this one path *is* known in full before the job is ever queued — it's the literal `fileName`
argument `mesh:loadAsync(fileName, callback)` was called with — so it doesn't need the
"defer to finish" treatment those other two needed. It can simply be resolved once, up front, on the
main thread.

## The fix

1. **`MESH_MANAGER::loadAsync()`** (`mesh-manager.cpp:4231`, called from `MESH::loadAsync`
   (`mesh.cpp:88`), always on the main thread — confirmed no code path reaches it off-thread today):
   before constructing and queuing the `Impl::AsyncJob`, resolve the full path once:
   ```cpp
   void MESH_MANAGER::loadAsync(const char *fileName, MeshAsyncLoadCallback onComplete)
   {
       const std::string fileNameBase = util::getBaseName(fileName);
       auto cached = this->impl->lsMeshes[fileNameBase];
       if (cached) { /* unchanged cache-hit path */ }

       // NEW: resolve on the main thread, same as every synchronous load already does.
       const char *resolvedPath = util::getFullPath(fileName, nullptr);

       this->impl->ensureWorkersStarted();
       Impl::AsyncJob job;
       job.fileName   = resolvedPath;   // was: fileName
       job.onComplete = std::move(onComplete);
       ...
   }
   ```

2. **`parse_v11_intermediate()`** (`mesh-manager.cpp:702`): stop calling `util::openFile` (which
   carries the `addPath` side effect that caused the original bug) and open the now-already-resolved
   absolute path directly. Two ways to do this, pick whichever reads better in context:
   - (a) A new minimal helper, e.g. `util::fopenAppRaw(fullPath, mode)` — a thin wrapper around
     the same low-level `fopen`/platform file-open primitive `util::openFile` uses internally, but
     with **no** `getFullPath`/`addPath` call. Exposed from `util-interface.h` alongside
     `openFile`/`getFullPath`, documented as "caller must pass an already-resolved path; does not
     search `lsPath` or register anything."
   - (b) Skip the wrapper and call the platform primitive directly in `mesh-manager.cpp`
     (whatever `util::openFile`'s "r" branch calls internally after resolving the path — check
     `file-util.cpp:539-561` for the exact primitive at fix time).
     Slightly less reusable than (a) but zero new public API surface.

   Either way, `parse_v11_intermediate` no longer calls `util::openFile`/`getFullPath`/`addPath` at
   all, so it no longer touches `g_lsPathMutex` or the deferred-script queue — the worker thread
   goes back to being genuinely "pure CPU, zero shared state," matching what its own comment
   (`mesh-manager.cpp:868`) already claims.

3. **Leave the general fix in place.** `g_lsPathMutex`, `invokeOrDeferAddPathScript`,
   `pumpDeferredAddPathScripts()` stay exactly as shipped. They're now a safety net for any *other*
   worker-thread code path that calls these functions (present or future), not the primary
   protection for mesh loading — and since mesh loading no longer contends on them at all after this
   change, there's no downside to keeping them.

## What this does NOT change

- Cache-hit behavior (`MESH_MANAGER::loadAsync`'s early return when `cached` is non-null) —
  unaffected, still dispatches through `pumpAsyncLoads`'s `completedJobs` queue as today.
- `finishLoadFromIntermediate`'s texture/extra-path handling — already correct, untouched.
- Synchronous `MESH_MANAGER::load()` / `MESH_MBM_DEBUG::loadV11()` — untouched; they were never
  part of the async/worker-thread problem (always main-thread callers).
- The Android JNI investigation from the original bug (`docs/async-loading-and-threading.md`) —
  unrelated to this plan, already resolved (no fix needed, `copyFileFromAsset` doesn't touch JNI).

## Risk / size estimate

Small and low-risk relative to the originally-floated "restructure `MESH_LOAD_INTERMEDIATE_V11` to
carry unresolved texture references forward" idea — that idea is now known to be unnecessary, since
texture/extra-path resolution was already correctly deferred to the main thread all along. This plan
is really just: one new call in `loadAsync` (already-proven `getFullPath`, just called one call
earlier) + one call-site swap in `parse_v11_intermediate` (an already-resolved-path file open
instead of a resolve-and-register one). No changes to the intermediate representation's shape, no
changes to `finishLoadFromIntermediate`.

## Verification plan

1. Reproduce the original scenario (add a mesh via `mesh:loadAsync` from a busy ImGui editor, e.g.
   `scene_editor3d.lua`'s "Add Mesh" flow) under the `-DUSE_ASAN=1` build — confirm still clean
   (regression check against the original bug).
2. Add a temporary counter/log around `g_lsPathMutex`'s lock in `file-util.cpp` (or a quick
   `printf` in `invokeOrDeferAddPathScript`) and confirm it is **not** entered at all during a
   `mesh:loadAsync` call once this fix lands — i.e. confirm the mutex is genuinely off the hot path
   now, not just still-safe-but-still-paid-for.
3. Confirm a mesh whose file needs search-path resolution (not just an already-fully-qualified path)
   still loads correctly via `loadAsync` — the new up-front `getFullPath` call in step 1 must behave
   identically to what `openFile` used to do internally for the same input.
4. Re-run the existing mesh v11 test coverage (if any) / manually load a representative set of
   `.msh` files (with and without textures, with and without `SECTION_EXTRA_PATHS`) via both
   `load()` and `loadAsync()` and confirm identical results.
