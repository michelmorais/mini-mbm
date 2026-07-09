# Plan: Remove the Last Worker-Thread `getFullPath`/`addPath` Call from Async Mesh Loading

Status: **implemented** (see `MBM_VERSION` history in `include/version/version.h` for the entry
that shipped this). Companion to `docs/async-loading-and-threading.md`, which documents the
mutex/deferred-queue fix already shipped for the bug this plan follows up on.

**Scope correction from the original write-up below**: this plan, as originally written, only
analyzed `MESH_MANAGER::Impl::workerLoop()` (the async/worker-thread caller of
`parse_v11_intermediate`) and asserted synchronous loading was untouched/out of scope. That was
wrong for `MESH_MBM::loadV11()` (no `_DEBUG`) — it shares `parse_v11_intermediate` with the async
path, reached via `MESH_MBM::load()` → `MESH_MANAGER::load(fileName, renderizable)`, and is called
with the caller's raw/relative `fileName`, never pre-resolved. (`MESH_MBM_DEBUG::loadV11()` is a
genuinely separate function with its own `util::openFile` call and really was untouched, as
originally claimed — the two got conflated.) The shipped fix resolves the path in **both**
`MESH_MANAGER::loadAsync()` and `MESH_MBM::loadV11()`, each keeping the caller's original,
unresolved `fileName` flowing into `finishLoadFromIntermediate`/`impl->fileName` unchanged (needed
because `onLoadMeshLua`, `mesh-lua.cpp`, compares `getFileName()` against the raw caller string to
skip redundant reloads) — only the path used to actually open the file is the resolved one.
`util::fopenApp` (previously file-local to `file-util.cpp`) was exposed in `util-interface.h`/
`file-util.h` as the no-side-effect "open this exact, already-resolved path" primitive
`parse_v11_intermediate` now uses instead of `util::openFile`.

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

- `finishLoadFromIntermediate`'s texture/extra-path handling — already correct, untouched.
- `MESH_MBM_DEBUG::loadV11()` — untouched; a genuinely separate function (own `util::openFile` call),
  never shared `parse_v11_intermediate`, never part of the async/worker-thread problem.
- The Android JNI investigation from the original bug (`docs/async-loading-and-threading.md`) —
  unrelated to this plan, already resolved (no fix needed, `copyFileFromAsset` doesn't touch JNI).
- Cache-hit behavior (`MESH_MANAGER::loadAsync`'s early return when `cached` is non-null) — still
  goes through `pumpAsyncLoads`'s `completedJobs` queue, never inline. See "Tried and reverted"
  below for why that's load-bearing, not just unfinished cleanup.

## What changed beyond the original scope

- `MESH_MANAGER::load()` / `MESH_MBM::loadV11()` (plain, non-`_DEBUG`) — **do** now resolve the path
  up front too, for the reason in the scope-correction note above.

## Tried and reverted: inline cache-hit dispatch

In the same pass, `MESH_MANAGER::loadAsync`'s cache-hit branch was changed to call `onComplete`
inline instead of queuing through `completedJobs`/`pumpAsyncLoads`, on the reasoning that nothing
async happens on a cache hit so the extra frame of latency was pure waste. Verified safe against
every `...::loadAsync` caller's *state-ordering* (`MESH`, `SPRITE`, `TILE`, `PARTICLE`, `BACKGROUND`
and their Lua wrappers all register refs / set up bookkeeping before calling `loadAsync`, so nothing
breaks if the callback fires before the call returns) — but that check missed *stack depth*.
`editor/scene_editor3d.lua`'s generated `_loadMeshAsyncQueued` (added to serialize concurrent
same-file `loadAsync` calls around the [[async-mesh-load-concurrency-crash]] SIGSEGV) chains
same-file requests by calling `processNext()` again from inside its own `loadAsync` callback — a
pattern that only works because the old contract guaranteed the callback runs on a *later* pump,
never nested inside the call that queued it. Once cache hits fired inline, every request after the
first for one file (verified with a real 400-placed-instance scene from Scene Editor 3D's own "Run")
resolved synchronously and recursed into `processNext()` with no yield back to the event loop,
exhausting the C/Lua call stack — reproduced as a real crash (`SIGABRT`, preceded by
`plugin-helper.cpp`'s `lua_getstack` failing, i.e. a corrupted debug stack from the overflow), not a
hypothetical. Reverted in full: `MESH_MANAGER::loadAsync`'s cache-hit branch queues through
`completedJobs` again, `Impl::AsyncResult::cacheHit`/`cachedMesh` and `pumpAsyncLoads`'s cache-hit
branch are back, and `include/render/mesh.h`'s "never inline" contract comment is restored.
**Lesson for next time this is reconsidered**: any inline-dispatch change to `loadAsync` needs to be
checked not just for "does calling code assume a later frame" but "does calling code recursively
re-enter `loadAsync` from within its own callback for a bounded-but-large N" — the second class of
bug doesn't show up with a handful of test calls, only at realistic scale (this reproduced with 400
placed meshes, not with 2 or 3).

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
