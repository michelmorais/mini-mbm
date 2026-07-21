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
| A Lua ImGui editor widget (`editor/*.lua`, `editor_utils.lua`) — new function, math, drawing logic | **Path C below**: an isolated throwaway smoke-test scene, not the full editor tool | Exercising the real editor tool (e.g. `mesh_debug.lua`) requires navigating its UI with mouse clicks to reach the code path under test, which this sandbox cannot automate (see Path C). An isolated scene calls the new function directly with mock data and needs zero navigation. |

---

## Path A — C++ feature: testLib

Build and run:

```sh
cd build   # existing configured build dir, see README.md for a fresh cmake invocation
cmake --build . --target testLib -j$(nproc)
./bin/debug/linux_x86/testLib <seconds> [mesh_file] [world]
```

If configuring a **fresh** build dir for agent-driven runs, also pass
`-DUSE_TEXTURE_MISSING_DIALOG=0` at cmake-configure time — it's enabled by default (see "The
missing-texture dialog" below) and, left on, a missing texture opens a blocking dialog no timeout
can interrupt.

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
(`src/core_mbm/texture-manager.cpp:970`, on Windows/Linux/macOS — not Android) falls back to a
**blocking native "where is this file?" file-open dialog** and waits for a human to click
something. This is not part of the render loop, so none of the timeout mechanisms above help —
`testTimeoutSeconds`, `mbm.getTimeRun()`, even `timeout -s KILL` from the shell, none of it fires
while the process is blocked inside that native dialog call.

This is gated behind a preprocessor define, `USE_TEXTURE_MISSING_DIALOG`, **enabled by default**
(see the comment above `if(NOT DEFINED USE_TEXTURE_MISSING_DIALOG)` near the top of the root
`CMakeLists.txt`, or `MbmCoreFeatureDefines` in `platform-msvs/core_mbm/mbm-core-flags.props` for
MSVS) because it's genuinely useful for interactive desktop/editor use — locating a moved texture
via the picker. **For any headless/CI/agent-driven build dir (testLib, mini-mbm, Lua scripts),
configure with `-DUSE_TEXTURE_MISSING_DIALOG=0`** so a missing texture falls back to a solid white
texture (`"#FFFFFFFF"`, recognized by `TEXTURE::load`/`loadSolidColor`) instead of hanging forever.
Check `CMakeCache.txt` in the build dir if unsure which mode it's configured with — if it's unset
or `1`, the dialog is live.

Even with the flag off, still add asset paths up front rather than relying on the fallback — a
render silently coming out solid white instead of erroring is easy to mistake for "it worked":

- **C++**: call `util::addPath("dir/containing/textures")` (`include/core_mbm/util-interface.h`)
  for every asset directory the mesh/sprite/etc. under test needs, before loading it.
- **Lua**: call `mbm.addPath("dir")` in `onInitScene()` before loading anything that references
  a texture — `game-template/main.lua:22-28` shows the standard set of subfolders
  (`assets`, `scenes`, `scenes/textures`, etc.) to add.

If a test run seems to hang with no log output past a mesh/sprite/texture load line, first check
whether the build dir has `USE_TEXTURE_MISSING_DIALOG` on — that dialog is almost certainly why.

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

Before writing the test script, check `docs/lua-api.md` for the actual signature of the
function(s) under test rather than guessing from the C++ side or an old memory of the API —
`mbm.*` argument shapes are frequently more permissive than they look (many take either a table
or expanded numeric args, see e.g. `mbm.setAmbientLight`/`mbm.setPointLight` in §3.16) and new
bindings land there as they're added (e.g. §3.16 Lighting, and `mesh:loadAsync` in §7.2). If the
doc and the code under test disagree, or the doc is silent on something `src/lua-wrap/**` clearly
exposes, that's a `doc-drift-check` skill situation — fix the doc as part of the same change
instead of testing against a description you already know is wrong.

---

## Path C — Lua ImGui editor widget in isolation

A new function inside `editor_utils.lua` or one of the `editor/*.lua` tools (a drawing/math
widget, a new panel, an interaction handler) does not need the full editor tool running to be
verified. Reaching it through the real tool (e.g. `mesh_debug.lua`) usually requires navigating
a file dialog or clicking through menus first — mouse interaction an agent in this sandbox cannot
perform (no `xdotool`/`xte`/`pynput`/`python-xlib` available; confirmed by checking, don't assume).
Instead, write a **throwaway scene script** that calls the new function directly with mock data.

### Why the scratch scene must live next to the module it requires

`require "editor_utils"` / `require "lang.language"` work from inside `editor/*.lua` files with no
explicit `package.path` setup because the engine auto-adds the currently-running `--scene` file's
own directory to `package.path` when it loads it (`src/lua-wrap/manager-lua.cpp`, the
`package.path = package.path .. ';' .. path_lua` blocks). That mechanism only fires for the
directory of the file passed to `--scene` — so put the scratch scene **inside `editor/`** (e.g.
`editor/_tmp_<feature>_smoketest.lua`) to get the same free module resolution production code
gets, then delete it when done (`git status` should come back clean). Don't try to work around
this with a hand-rolled `LUA_PATH`; it's more moving parts for no benefit.

### The smoke-test pattern

```lua
tImGui = require "ImGui"
tUtil  = require "editor_utils"

local mockState = { azimuth = 0.3, elevation = 0.3 }   -- whatever the function under test needs
local start_time, errMsg = nil, nil

function onInitScene() start_time = mbm.getTimeRun() end

function onLoop(delta)
    local ok, err = pcall(function()
        if tImGui.Begin('Smoketest', false, 0) then
            tUtil.yourNewWidget(mockState)   -- call the real function, not a reimplementation
        end
    end)
    tImGui.End()   -- always, OUTSIDE the pcall — see warning below
    if not ok then errMsg = err end
    if start_time and (mbm.getTimeRun() - start_time) >= 4 then
        if errMsg then print('error', 'red', 'SMOKETEST FAIL: ' .. tostring(errMsg))
        else print('info', 'green', 'SMOKETEST OK') end
        mbm.quit()
    end
end
```

Wrapping the call in `pcall` and printing an explicit `SMOKETEST OK`/`FAIL` sentinel matters —
without it a Lua runtime error inside an ImGui callback can scroll past in the log without ever
failing the process's exit code. Run it the same way as any Path B script
(`--disable_select_monitor`, `timeout -s KILL` backstop), then `grep` the log for the sentinel
and for `error|traceback|attempt to|nil value` as a second pass.

**`tImGui.End()` must be outside the `pcall`, never inside it.** If the widget call between
`Begin`/`End` errors, a Lua error unwinds via `longjmp` straight past anything else in that same
`pcall`'d function — including an `End()` written after it — leaving ImGui's internal window
stack unbalanced. On the *next* frame ImGui's own error-recovery asserts `"Missing End()"` and
**aborts the whole process** (`imgui.cpp: ErrorRecoveryTryToRecoverState`). This is not
hypothetical: a real editor bug (`scene_editor3d.lua` passing `texInfo.id`, a nil field, to
`ImageButton` instead of `texInfo` itself) hit exactly this, and reproducing it here — even with
`End()` correctly moved outside the `pcall` — **still aborted the process**, because a widget call
that partially executes before erroring (e.g. `ImageButton` typechecking its texture argument
after already touching internal ImGui state) can corrupt more than just the Begin/End counter.
`pcall` reliably catches the error for *logging* (the `SMOKETEST FAIL` sentinel above did print),
but it is not a crash-safety net for ImGui state — don't treat a passing pcall-wrapped smoke test
as proof a given call is safe if it still throws. The only real fix is removing the erroring call
so it never throws in the first place, not defending around it after the fact.

### Visually verifying the render, without mouse input

This sandbox has a real X display (`echo $DISPLAY`, `/tmp/.X11-unix` — check, don't assume it's
headless) and ImageMagick's `import`/`convert` are installed, even though no input-automation tool
is. That's enough to *see* drawing-code output, just not to *click* on it:

```sh
timeout -s KILL 10 ./bin/debug/linux_x86/mini-mbm --scene editor/_tmp_foo_smoketest.lua \
    --disable_select_monitor --nosplash -w 500 -h 400 -x 50 -y 50 > run.log 2>&1 &
BGPID=$!
sleep 2.5   # let it render at least one frame before the deadline fires
import -display :1 -window root screenshot.png
wait $BGPID
convert screenshot.png -crop <WxH+X+Y> -resize 400% zoom.png   # crop to the widget, upscale
```

Then `Read` the cropped PNG directly — the Read tool renders images. This is not a gimmick: doing
this on a new orbit-navigation gizmo caught a real inverted painter's-algorithm bug (near/far dot
sizing and draw order were backwards) that code review alone had missed — the math looked
plausible on paper but was visibly wrong once rendered.

**Honest limit:** this verifies rendering and catches runtime errors across many frames, but it
does **not** exercise click/drag interaction (no synthetic mouse input exists here). Say so
explicitly rather than claiming full interactive verification — e.g. "I confirmed the gizmo
renders and updates state correctly in isolation; I couldn't automate an actual click/drag, so
give it one manual pass in the running editor."

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
| Loading a mesh/sprite/texture without `util::addPath`/`mbm.addPath` for its directory first, or building a test/CI dir without `-DUSE_TEXTURE_MISSING_DIALOG=0` | By default, a missing texture opens a blocking native file-picker dialog that no timeout can interrupt. Configure headless/agent build dirs with `-DUSE_TEXTURE_MISSING_DIALOG=0` (falls back to a solid white texture instead), and always add the asset path first regardless |
| Trying to click through testLib's menu to load a specific mesh for an agent-driven check | Pass it as `testLib <seconds> <mesh_file> <world>` instead — no mouse interaction needed |
| Trying to reach a new editor widget through the full editor tool (file dialogs, menu clicks) | Path C: isolate it in a throwaway scene under `editor/` that calls the function directly with mock data |
| Assuming a Lua widget "looks right" from code review alone, or claiming interactive UI is verified when only rendering was checked | Screenshot it (Path C) when the logic involves geometry/drawing/depth — and say plainly if click/drag interaction wasn't actually exercised |
| Leaving a scratch scene file behind under `editor/` after a Path C smoke test | `rm` it before finishing — `git status` should show only the real feature files changed |
