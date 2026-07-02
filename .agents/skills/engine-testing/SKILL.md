---
name: engine-testing
description: "How to actually run and verify a mini-mbm change instead of guessing from reading code. Use when: testing a new or changed C++ backend/render feature, testing a Lua feature or gameplay script, verifying a bug fix, or any time you're about to run testLib or the mini-mbm executable. Covers the bundled Lua interpreter, the testLib C++ smoke-test harness (with its auto-exit timeout arg), and how to launch the real engine headlessly without getting stuck on its monitor/fullscreen/script picker dialog."
---

# Engine Testing — mini-mbm

## When to Use

- A change touches C++ backend/render code (`src/core_mbm`, `src/render`, `include/render`, `include/core_mbm`) and needs to be exercised, not just compiled.
- A change touches Lua bindings (`src/lua-wrap`) or a game's Lua script and needs to run.
- You're about to invoke `testLib` or the `mini-mbm` executable for any reason.
- You're tempted to search the OS for a `lua` binary — stop, it's bundled (see below).

Read this **before** starting a test run, not after it hangs.

---

## Decision: which harness?

| What changed | Use | Why |
|---|---|---|
| C++ render/backend feature (a new `RENDERIZABLE` type, mesh/texture/font/sprite/tile/particle code, shader plumbing) | **testLib** (`src/test-lib/`) | Directly instantiates the engine (`GAME`/`CORE_MANAGER`), no Lua, no dialog, fastest iteration. Existing menu in `my-scene-test.cpp` already exercises texture/gif/sprite/mesh/shape/line/particle/render-to-texture/steered-particle/font/tile — extend it rather than writing a new harness. |
| Lua API / gameplay feature | **mini-mbm + a `.lua` scene** | Runs the real Lua binding layer (`src/lua-wrap`), which testLib never touches. |
| Something that must prove the two integrate (e.g. a new Lua binding for a new C++ type) | Both: testLib first to prove the C++ side renders correctly, then a `.lua` script to prove the binding | testLib alone can't catch a binding-layer regression. |

---

## Path A — C++ feature: testLib

Build and run:

```sh
cd build   # existing configured build dir, see README.md for a fresh cmake invocation
cmake --build . --target testLib -j$(nproc)
./bin/debug/linux_x86/testLib <seconds> [mesh_file] [world]
```

**Always pass `<seconds>`.** `testLib` is a real-time render loop (`while (device->isRunning())`
in `CORE_MANAGER::onLoop`, `src/core_mbm/core-manager-common.cpp:201`) — it runs forever
waiting for a window-close/key event that an agent will never send. `src/test-lib/main.cpp`
takes an optional first argv as a timeout in seconds; `MY_SCENE::onLoop()`
(`src/test-lib/my-scene-test.cpp`) accumulates `device->delta` against it and calls
`device->setRun(false)` once elapsed, so the process exits with code 0 on its own instead of
needing `timeout -s KILL`:

```sh
./bin/debug/linux_x86/testLib 5    # runs ~5s, exits 0 on its own
```

If you omit `<seconds>` it behaves exactly like before this flag existed (runs until killed) —
useful for the one time a human is actually watching the window, never for an agent-driven check.

By default `testLib` only ever loads its bundled font — none of its menu objects (mesh, sprite,
texture, gif, particle, etc.) load until a human clicks the on-screen menu, which an agent can't
do. `main.cpp` also takes two more optional positional args to preload a specific mesh without
touching the menu at all: `[mesh_file]` (looked up via the engine's normal asset search paths,
same as `util::addPath` — a bare filename works if it's next to `src/test-lib/*.cpp`, same as the
built-in `Crate.msh` fixture) and `[world]` (`2ds`, `2dw`, or `3d`, defaults to `3d`):

```sh
./bin/debug/linux_x86/testLib 5 Crate.msh 3d     # preload the bundled fixture mesh
./bin/debug/linux_x86/testLib 5 MyNewThing.msh 2dw
./bin/debug/linux_x86/testLib 0 MyNewThing.msh 3d  # 0 = no timeout, still preloads the mesh
```

This only covers `MenuObjectType::MESH` today (`MY_SCENE::cliMeshFile`/`cliMeshMode`,
`src/test-lib/my-scene-test.{h,cpp}`) — it's the type the mesh-format work in this repo touches
most. Follow the same pattern (a `cli<Type>File` field + a check in `onInitScene()` after
`buildMenu()`) if another object type needs the same non-interactive preload.

### Extending testLib for a new feature

Add your object next to the existing ones in `MY_SCENE` (`src/test-lib/my-scene-test.h`/`.cpp`):
follow the `MenuRow`/`loadObjectAt` pattern already there (menu toggle, load in `2dS`/`2dW`/`3d`
as applicable) rather than writing a standalone `main()`. Assets (`.msh`, `.spt`, `.ptl`, `.tile`,
`.fnt`, textures) live flat in `src/test-lib/` — drop new fixtures there.

### The quit() footgun

If you ever need to stop the loop from inside scene code, use `device->setRun(false)`
(`include/core_mbm/device.h`), **not** `mbm::DEVICE::quit()`. `DEVICE::quit()` immediately
`delete`s the DEVICE singleton — it's a one-shot teardown call meant to run exactly once, from
`GAME::~GAME()`, after `onLoop()` has already returned. Calling it mid-frame from inside
`onLoop()` frees the device out from under `CORE_MANAGER::onLoop()`'s own loop, which then
segfaults on its next `device->` dereference in the same frame. This is exactly how the
`<seconds>` timeout is implemented correctly — copy that pattern, don't call `quit()` early.

### The missing-texture dialog — a hang that no timeout can catch

This applies to **both** testLib and mini-mbm, C++ and Lua. When a mesh/sprite/etc. references a
texture the engine can't find on its known search paths, `TEXTURE_MANAGER::getFilePathTexture`
(`src/core_mbm/texture-manager.cpp:989`, on Windows/Linux/macOS — not Android) falls back to a
**blocking native "where is this file?" file-open dialog**
(`dialog_util::openFileDialog`, line 1037) and waits for a human to click something. This is not
part of the render loop, so none of the timeout mechanisms above help — `testTimeoutSeconds`,
`mbm.getTimeRun()`, even `timeout -s KILL` from the shell, none of it fires while the process is
blocked inside that native dialog call. The only real fix is to never let it trigger:

- **C++**: call `util::addPath("dir/containing/textures")` (`include/core_mbm/util-interface.h`)
  for every asset directory the mesh/sprite/etc. under test needs, before loading it.
- **Lua**: call `mbm.addPath("dir")` in `onInitScene()` before loading anything that references
  a texture — `game-template/main.lua:22-28` shows the standard set of subfolders
  (`assets`, `scenes`, `scenes/textures`, etc.) to add.

If a test run seems to hang with no log output past a mesh/sprite/texture load line, this dialog
is almost certainly why — check the asset's texture references are all reachable from an
already-added path before assuming it's a real engine bug.

---

## Path B — Lua feature: mini-mbm + a script

### The bundled Lua interpreter — don't search for one

The engine bundles its own Lua 5.4.1 build; do not spend time checking whether the OS has
`lua` installed or searching `$PATH`. It's built from `third-party/lua-5.4.1` and always named
`lua-5.4.1.exe` **even on Linux and macOS** (that's the literal filename, not a mistake):

```
bin/debug/linux_x86/lua-5.4.1.exe        # Linux
bin/debug/<platform_dir>/lua-5.4.1.exe   # Windows / macOS build dirs, same naming
```

`bin/debug/linux_x86/shortcuts.sh` sets up an `alias lua=...lua-5.4.1.exe`, `LUA_PATH`,
`LUA_CPATH`, and `LD_LIBRARY_PATH`, plus a `minimbm` alias for the engine binary itself — source
it if you want short commands in an interactive shell, but for scripted/agent runs just call the
full path directly, it's simpler to reason about.

### Skipping the monitor/fullscreen/script dialog

Launched normally, `mini-mbm` (`platform-linux/main-lua.cpp`, and the equivalent
`platform-macos/main-lua.cpp`) opens a blocking native dialog (hand-rolled X11/Xrandr on Linux,
`src/mini-mbm-lib/mini-mbm-lib-Linux.cpp:36`) asking a human to pick a monitor, fullscreen, and
which script to run. **Pass `--disable_select_monitor` together with `--scene <file>.lua`** to
skip straight past it and run that scene:

```sh
./bin/debug/linux_x86/mini-mbm --scene path/to/scene.lua --disable_select_monitor --nosplash -w 320 -h 240
```

Without `--disable_select_monitor` the process blocks on the picker and a plain `timeout N`
often won't reliably stop it either — you'll end up needing `timeout -s KILL`. `--scene` alone
is not enough: it only pre-selects which script *will* run once the dialog is dismissed, it
does not skip the dialog itself.

Full flag list: `./mini-mbm --help` (implemented in `src/core_mbm/usage-help.cpp`). Notable ones:
`-s/--scene`, `-w/-h` (width/height), `-ew/-eh` (expected width/height), `-x/-y` (window
position), `--nosplash`, `--noborder`, `-a/--addpath`, and `--disable_select_monitor`. Any
bare `name=value` argument becomes a Lua global readable via `mbm.getGlobal("name")`.

### Making the Lua process exit on its own

`--disable_select_monitor` gets you into the render loop without a dialog, but the loop itself
still runs forever until something calls `mbm.quit()` (which is `device->setRun(false)` under
the hood — see the C++ footgun note above; the Lua binding already does this correctly, e.g.
`src/lua-wrap/framework-linux-lua.cpp:99`). Give your test script its own timeout using
`mbm.getTimeRun()`, following `.agents/skills/engine-testing/assets/smoke-test.lua`:

```lua
local start_time = nil
function onInitScene()
    start_time = mbm.getTimeRun()
    -- ... set up the feature under test ...
end

function onLoop(delta)
    -- ... exercise/assert the feature under test ...
    if start_time and (mbm.getTimeRun() - start_time) >= 5 then
        mbm.quit()
    end
end
```

Combine both belts: `--disable_select_monitor` to skip the dialog, and an in-script
`mbm.getTimeRun()` deadline to end the loop, then still wrap the invocation in
`timeout -s KILL <n+buffer>` as a hard backstop in case the script under test never reaches
`onLoop` (e.g. `onInitScene` itself hangs or errors before the deadline logic ever runs).

For the full Lua lifecycle callbacks (`onInitScene`, `onLoop(delta)`, input callbacks) and the
complete `mbm.*` API, see the **`lua-game`** skill and `docs/lua-api.md` — this skill only covers
how to *launch and stop* a test run, not the scripting API itself.

---

## Quick reference: full command patterns

```sh
# C++ feature, 5s auto-exit
cd build && cmake --build . --target testLib -j$(nproc) && ./bin/debug/linux_x86/testLib 5

# C++ feature, preload a specific mesh in 3D, 5s auto-exit
./bin/debug/linux_x86/testLib 5 MyNewThing.msh 3d

# Lua feature, headless, 5s in-script deadline, 15s hard backstop
timeout -s KILL 15 ./bin/debug/linux_x86/mini-mbm \
    --scene .agents/skills/engine-testing/assets/smoke-test.lua \
    --disable_select_monitor --nosplash -w 320 -h 240
```

## Common mistakes to avoid

| Mistake | Correct approach |
|---|---|
| Searching `$PATH` or the OS package manager for `lua` | Use the bundled `lua-5.4.1.exe` under `bin/debug/<platform>/` |
| Running `testLib` or `mini-mbm` with no timeout at all | Pass `testLib <seconds>`, or give the Lua script its own `mbm.getTimeRun()` deadline |
| Running `mini-mbm --scene x.lua` without `--disable_select_monitor` | Always add `--disable_select_monitor` for agent-driven runs — otherwise it blocks on the picker dialog |
| Calling `mbm::DEVICE::quit()` from C++ scene code to stop a loop | Call `device->setRun(false)` instead; `DEVICE::quit()` is a one-shot destructor-time teardown |
| Writing a brand-new C++ test harness for a render feature | Extend `MY_SCENE` in `src/test-lib/my-scene-test.cpp` — it already has the menu/load/release scaffolding |
| Assuming `timeout N` (SIGTERM) reliably stops the engine | These are GUI/render-loop processes; prefer the in-process timeout mechanisms above and treat `timeout -s KILL` as a backstop, not the primary mechanism |
| Loading a mesh/sprite/texture without `util::addPath`/`mbm.addPath` for its directory first | Missing textures fall back to a blocking native file-picker dialog that no timeout can interrupt — always add the asset path first |
| Trying to click through testLib's menu to load a specific mesh for an agent-driven check | Pass it as `testLib <seconds> <mesh_file> <world>` instead — no mouse interaction needed |
