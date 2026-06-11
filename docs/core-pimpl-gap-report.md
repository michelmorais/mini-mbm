# Core MBM PIMPL Gap Report

Date: 2026-06-11

This report checks what is still missing for the main `core_mbm` API to move toward a PIMPL-style design. It focuses on the public engine boundary under `include/core_mbm/` and the implementation files under `src/core_mbm/`.

## Target

A strict PIMPL-style core API would mean:

- Public headers expose stable classes, constructors, destructors, API methods, enums, and value types only.
- Implementation state lives behind opaque `Impl`, `BackendData`, or equivalent pointers.
- Backend-specific graphics/audio/window objects are not visible in public headers.
- Users, Lua bindings, render classes, and plugins use methods instead of reading or mutating internal fields directly.
- Adding a backend or changing manager internals does not require changing the public object layout.

The current codebase is not designed this way yet. It is mostly a public-data engine API with some backend state already isolated. A PIMPL migration should be staged.

## Existing PIMPL-style work

| Area | Current state | Notes |
|---|---|---|
| `AUDIO` | Partly converted | `include/core_mbm/audio.h` uses `struct BackendData; std::unique_ptr<BackendData> backend;`. Backend state is now in backend implementation files. |
| `AUDIO_MANAGER` | Partly converted | Shared manager code delegates setup, teardown, and update through private backend hooks. |
| `DEVICE::specificContextDevice` | Converted | The pointer is now stored behind `DEVICE::Impl`; backend code reaches it through `DEVICE::getSpecificContextDevice()`. |
| Shader resources | Partly hidden | Built-in shader code is hidden behind functions, but runtime shader/buffer objects still expose backend handles. |
| Manager caches | Private but not PIMPL | Several managers keep private containers in public headers. That hides access, but still exposes layout and forces container includes. |

## Main gaps

### 1. Public data members are the largest blocker

Several core classes expose mutable state as part of the public API. Hiding these fields without a compatibility layer would break a large amount of engine code, platform samples, Lua wrapping, and likely external game code.

High-impact examples:

| Header | Public state that blocks strict PIMPL |
|---|---|
| `include/core_mbm/device.h` | No direct public data members remain; gameplay-facing state is accessor-backed. |
| `include/core_mbm/renderizable.h` | `position`, `scale`, `angle`, `bounding_AABB`, `alwaysRenderize`, `isObjectOnFrustum`, `enableRender`, dynamic vars, `isRender2Texture`, `userData`, `blend`, `fileName`, `__distFromView`. |
| `include/core_mbm/core-manager.h` | `device`, `changeScene`, `__sceneWasInit`, key/window flags. |
| `include/core_mbm/animation.h` | `ANIMATION` frame state, `fx`, `ANIMATION_MANAGER::indexCurrentAnimation`, callbacks, vector of animations, backup object. |
| `include/core_mbm/scene.h` | `endScene`, `wasUnloadedScene`, `nextScene`, `goToNextScene`, `userData`. |

A broad scan for direct member access on the main exposed state returned more than 2,000 hits across `include/`, `src/`, `plugins/`, `platform-*`, and `editor/`. That number is only a sizing signal, but it confirms this cannot be a single mechanical header edit.

What is missing:

- Accessors and mutators for the current public fields.
- Internal call-site migration from direct access to methods.
- A compatibility policy for external source code. Either keep legacy public fields for one cycle or accept a breaking API cleanup.
- Clear classification of which fields are intentionally part of gameplay scripting ergonomics and which are pure internals.

### 2. Backend handles are still exposed through public core types

The strongest PIMPL candidate is backend-owned rendering state. These fields should move behind backend-local data structures before attempting the broader public-data cleanup.

| Header | Exposed backend state | Suggested destination |
|---|---|---|
| `include/core_mbm/device.h` | `SPECIFIC_AUX_CONTEXT_DEVICE *specificContextDevice` | Done: stored in `DEVICE::Impl`, with backend helper functions in `.cpp` files. |
| `include/core_mbm/shader.h` | `BUFFER_GL::BUFFER_SPECIFIC *bs` | `BUFFER_GL::BackendData`. |
| `include/core_mbm/shader.h` | `SHADER::void *ptrShaderSpecific` | `SHADER::BackendData`. |
| `include/core_mbm/texture-manager.h` | `TEXTURE::idTexture/ptrTexture` union | `TEXTURE::BackendData`, with backend-only accessors in implementation files. |
| `include/core_mbm/renderizable.h` | `RENDERIZABLE_TO_TARGET::void *specificConfig` | `RENDERIZABLE_TO_TARGET::BackendData`. |
| `include/core_mbm/specific-*.h` | GL/EGL/X11, D3D9, Metal/Cocoa public headers | Move to private backend include area or `src/core_mbm/` once public users no longer need these concrete structs. |

This can follow the audio pattern: public class owns `std::unique_ptr<BackendData>`, the incomplete struct is defined in the active backend `.cpp/.mm`, and common code talks through narrow private hooks.

### 3. Public headers include too much implementation detail

Many headers pull STL containers or subsystem headers only because private layout is visible.

Examples:

- `core-manager.h` includes `<map>`, `<list>`, `<mutex>`, and stores event queues/plugin lists in the class layout.
- `device.h` includes `shader-cfg.h`, `order-render.h`, `frustum.h`, `camera.h`, and `time-control.h` because it inherits from value-heavy classes and exposes objects directly.
- `renderizable.h` includes `blend.h`, `shader.h`, `<map>`, and `<string>` because transform/state/cache members are visible.
- `texture-manager.h` includes STB font types in public method signatures and exposes texture handle storage.

What is missing:

- A public/private header split for implementation-only types.
- Forward declarations where signatures allow it.
- A decision on value-type inheritance. `DEVICE : public TIME_CONTROL, public FRUSTUM` prevents fully hiding those base subobjects.

### 4. Core managers are private-access clean, but ABI-layout dirty

Some classes already keep data private, but not behind PIMPL. This prevents source misuse, but every private field still affects class size, header dependencies, compile times, and ABI.

Good candidates:

- `CORE_MANAGER`: event queues, plugin list, mutex, restore progress, name, event state.
- `TEXTURE_MANAGER`: cache map, path buffer, texture capability fields.
- `MESH_MANAGER`: mesh cache and fake-release list.
- `ANIMATION_BACKUP`: backup vectors and nested backup structs.
- `EFFECT_SHADER`: shader map and current shader state.

What is missing:

- `struct Impl; std::unique_ptr<Impl> impl;`
- Out-of-line destructors in `.cpp` files.
- Deleted or explicitly defined copy/move semantics.
- Migration of private helper functions that directly touch moved fields.

### 5. `RENDERIZABLE` is a special compatibility problem

`RENDERIZABLE` is central to gameplay and editor ergonomics. `position`, `scale`, `angle`, `enableRender`, and `blend` are intentionally convenient today. Moving them behind PIMPL improves ABI hygiene but worsens direct game-code ergonomics unless the replacement API is carefully designed.

Recommended approach:

- Do not start the migration here.
- First add methods such as `setPosition`, `getPosition`, `setScale`, `setAngle`, `setEnabled`, `isEnabled`, `setBlendState`, and `getBlendState`.
- Migrate engine internals to those methods.
- Keep public fields temporarily, or explicitly choose a breaking API cleanup.
- Hide internal-only fields first: `fileName`, `__distFromView`, `isObjectOnFrustum`, `isRender2Texture`, and dynamic-var storage.

### 6. Plugin and Lua boundaries need an explicit policy

Plugins currently receive raw `context` and `renderDevice` pointers in `PLUGIN::onSubscribe`. That is not PIMPL-friendly, but it may be intentional for low-level plugins.

What is missing:

- A stable public plugin context type, or a documented rule that plugins may access backend-native handles.
- Lua binding updates once fields move behind methods.
- A compatibility layer for existing Lua/editor code that assumes direct state names.

### 7. Tests/build coverage must be staged by backend

This refactor touches object layout and backend dispatch, so validation should be target-specific rather than only a full repo build.

Minimum validation per phase:

- Linux OpenGL ES: `core_mbm`, launcher, one render sample.
- Linux `-DAUDIO=none` target for quick interface checks.
- Windows DirectX9: render-to-texture and texture binding paths.
- macOS/iOS Metal: texture, render target, shader, and device context ownership.
- Android OpenGL ES/OpenSL: asset manager access after `DEVICE` context hiding.
- Lua/editor smoke check after any `RENDERIZABLE`, `DEVICE`, `SCENE`, or animation API change.

## Recommended migration phases

### Agreed safe-start scope

The first implementation milestone should be deliberately narrow:

- Touch `AUDIO_MANAGER` only.
- Move only private manager storage into an `Impl`.
- Keep `pauseAudioOnPauseGame` public during this milestone.
- Keep `AUDIO` unchanged; its existing `BackendData` pattern is the precedent, not part of this first manager cleanup.
- Do not touch `RENDERIZABLE`.
- Do not touch `DEVICE` or `CORE_MANAGER` until `AUDIO_MANAGER` compiles cleanly with the new pattern.

This keeps the first step focused on proving the PIMPL mechanics without changing gameplay-facing API or platform render behavior.

### Phase 0 - Decide compatibility target

Choose one:

1. Hybrid PIMPL: hide backend and manager internals, keep gameplay convenience fields public for now.
2. Strict PIMPL: hide all mutable state and accept a broad breaking API migration.

Recommendation: start with Hybrid PIMPL. It gives most backend isolation benefit with much less disruption. The first concrete target is `AUDIO_MANAGER`, not `RENDERIZABLE`.

### Phase 1 - Standardize the pattern

- Create a short architecture note with the accepted pattern:
  - `struct BackendData` for backend-only state.
  - `struct Impl` for backend-neutral private state.
  - `std::unique_ptr<...>` members.
  - Out-of-line destructors.
  - No backend headers in public core headers.
- Use `AUDIO::BackendData` as the concrete precedent.

### Phase 2 - PIMPL `AUDIO_MANAGER`

Move the private manager-owned storage into `Impl`:

- `audios`
- `audiosToDelete`

Keep unchanged:

- `pauseAudioOnPauseGame` remains public.
- `AUDIO` remains as-is.
- Backend hooks remain as-is.
- Singleton behavior remains as-is.

This is the proof step for the local PIMPL pattern.

Milestone 1 implementation note:

- `AUDIO_MANAGER::Impl` now owns `audios` and `audiosToDelete` in `src/core_mbm/audio-manager.cpp`.
- Backend update implementations call the private `updateManagedAudiosBackend()` helper instead of reading the manager vector directly.
- `pauseAudioOnPauseGame` remains public.
- `AUDIO`, `RENDERIZABLE`, `DEVICE`, and `CORE_MANAGER` remain untouched by this milestone.

Milestone 2 implementation note:

- `CORE_MANAGER::Impl` now owns the private event queue state: key pressed map, event list, joystick-info event list, event mutex, and last event.
- The duplicated backend destructors moved to the common implementation so `CORE_MANAGER::Impl` can be destroyed where the type is complete.
- Public `CORE_MANAGER` flags such as `__sceneWasInit`, `keyCapsLockState`, `windowBorder`, and `enableResizeWindow` remain public for compatibility.
- `RENDERIZABLE` and `DEVICE` remain untouched by this milestone.

Milestone 3 implementation note:

- `CORE_MANAGER::Impl` now owns the private plugin list.
- Backend `addPlugin()` implementations use private helper methods (`getTotalPlugins()`, `getPlugin()`, `appendPlugin()`) instead of reading the plugin vector directly.
- Plugin subscribe/destroy ordering remains unchanged.
- `RENDERIZABLE` and `DEVICE` remain untouched by this milestone.

Milestone 4 implementation note:

- `CORE_MANAGER::Impl` now owns private loop/bootstrap and device-restore bookkeeping: `loopVariablesInitialized`, pause-before-stop state, restore phase, restore list selector, restore index, restore batch size, and restore progress values.
- `getStepRestore()` remains public but is implemented out-of-line so it can read hidden state.
- Backend constructors rely on `Impl` defaults instead of repeating restore-state initialization.
- `nameApplication`, public compatibility flags, `RENDERIZABLE`, and `DEVICE` remain untouched by this milestone.

Milestone 5 implementation note:

- `CORE_MANAGER::Impl` now owns the stored application name.
- Backend `initGraphics()` implementations call the private `setNameApplication()` helper, and restore paths read it through `getNameApplication()`.
- Native window initialization still receives the original `initGraphics()` argument where it did before.
- Public compatibility flags, `RENDERIZABLE`, and `DEVICE` remain untouched by this milestone.

Milestone 6 implementation note:

- `DEVICE::Impl` now owns private accessor-backed state: app return code, audio manager interface pointer, and paused-state flag.
- `CORE_MANAGER` uses `DEVICE::getAudioManagerInterface()` instead of reading the audio interface pointer directly.
- Render lists, `specificContextDevice`, camera, scene, pixel-perfect backend state, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 7 implementation note:

- `DEVICE::Impl` now owns the private 2D camera scale cache used by screen/world coordinate helpers.
- `CORE_MANAGER` updates that cache through a private `DEVICE::setCamera2dScaleCache()` helper instead of writing hidden fields directly.
- The scale calculation timing remains unchanged in the normal update path and lost-device restore path.
- Render lists, `specificContextDevice`, camera, scene, pixel-perfect backend state, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 8 implementation note:

- `DEVICE::Impl` now owns the private pixel-perfect rendering active flag.
- OpenGL ES, DirectX9, Metal, and dummy backend filtering methods toggle that flag through a private `DEVICE::setPixelPerfectRenderingActive()` helper.
- The public `DEVICE::isPixelPerfectRendering()` behavior remains unchanged.
- Render lists, `specificContextDevice`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 9 implementation note:

- `DEVICE::Impl` now owns the private physics list.
- `DEVICE::addPhysics()`, `DEVICE::removePhysics()`, and object-removal cleanup paths use the hidden physics list internally.
- `CORE_MANAGER::updatePhysis()` iterates through private `DEVICE` helpers instead of reading the vector layout directly.
- Render lists, `specificContextDevice`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 10 implementation note:

- `DEVICE::Impl` now owns the private render-to-texture target list.
- `DEVICE::addObjectRender2Texture()`, `DEVICE::removeObjectRender2Texture()`, and `DEVICE::stopRender2Texture2()` use the hidden target list internally.
- OpenGL ES, DirectX9, Metal, and dummy `CORE_MANAGER::renderToTargets()` implementations iterate through private `DEVICE` helpers instead of reading the vector layout directly.
- `src/core_mbm/new-backend-instructions.md` now documents the helper-based render-target iteration pattern.
- Main 2D/3D render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, `specificContextDevice`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 11 implementation note:

- `DEVICE::Impl` now owns the private 3-D render object list.
- `DEVICE::addRenderizable()`, `DEVICE::removeRenderizable()`, `DEVICE::removeObjectByIdSceneScene()`, and `DEVICE::disableAllButThis()` use the hidden 3-D list internally.
- `CORE_MANAGER` render preparation, render enable/disable, stop, and lost-device restore paths access the 3-D list through a private `DEVICE::getRender3DList()` helper.
- 2-D world/HUD render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, `specificContextDevice`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 12 implementation note:

- `DEVICE::Impl` now owns the private 2-D world render object list.
- `DEVICE::addRenderizable()`, `DEVICE::removeRenderizable()`, `DEVICE::removeObjectByIdSceneScene()`, and `DEVICE::disableAllButThis()` use the hidden 2-D world list internally.
- `CORE_MANAGER` render preparation, render enable/disable, stop, and lost-device restore paths access the 2-D world list through a private `DEVICE::getRender2DWList()` helper.
- 2-D screen/HUD render list, `RENDERIZABLE_TO_TARGET::specificConfig`, `specificContextDevice`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 13 implementation note:

- `DEVICE::Impl` now owns the private 2-D screen/HUD render object list.
- `DEVICE::addRenderizable()`, `DEVICE::removeRenderizable()`, `DEVICE::removeObjectByIdSceneScene()`, and `DEVICE::disableAllButThis()` use the hidden 2-D screen list internally.
- `CORE_MANAGER` render preparation, render enable/disable, stop, and lost-device restore paths access the 2-D screen list through a private `DEVICE::getRender2DSList()` helper.
- All three main render lists are now outside the visible `DEVICE` layout; `RENDERIZABLE_TO_TARGET::specificConfig`, `specificContextDevice`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 14 implementation note:

- Added `DEVICE::getSpecificContextDevice()` as the compatibility accessor for backend device context access.
- Added a private `DEVICE::setSpecificContextDevice()` helper for backend context creation/destruction.
- OpenGL ES, DirectX9, Metal, and dummy `DEVICE` backend implementation files now use the helper path for their own context ownership and local backend operations.
- `specificContextDevice` remains public in this milestone to avoid a breaking source change; `CORE_MANAGER`, texture, shader, platform, and render-target call sites can migrate backend-by-backend before the pointer moves behind `DEVICE::Impl`.
- `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 15 implementation note:

- Android-specific core and platform call sites now use `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated Android file/asset helpers, Android texture loading, OpenSL asset loading, GLES Android manager context access, and Android platform entry points.
- `specificContextDevice` remains public for compatibility while the remaining non-Android backend, texture, shader, and render-target call sites migrate in later milestones.
- `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 16 implementation note:

- OpenGL ES common and X11 `CORE_MANAGER` paths now use `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated EGL swap/reset/surface queries, plugin subscription handle selection, X11 event/window handling, and OpenGL ES/X11 release paths.
- OpenGL ES Windows, DirectX9, Metal manager/shader/texture/render-target, and remaining non-Android call sites are left for later backend-specific milestones.
- `specificContextDevice` remains public for compatibility; `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 17 implementation note:

- OpenGL ES Windows `CORE_MANAGER` paths now use `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated Win32 window event handling, EGL display/surface/context setup, joystick callback wiring, texture filter cache reads, and OpenGL ES Windows release paths.
- DirectX9, Metal manager/shader/texture/render-target, and remaining non-OpenGL ES call sites are left for later backend-specific milestones.
- `specificContextDevice` remains public for compatibility; `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 18 implementation note:

- DirectX9 `CORE_MANAGER` paths now use `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated Win32 event/window handling, Direct3D device creation/reset, render begin/end/present, render-to-target backbuffer access, plugin subscription handles, and min/max window sizing.
- DirectX9 shader, texture, and render-target implementation files are left for later backend-specific milestones.
- `specificContextDevice` remains public for compatibility; `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 19 implementation note:

- DirectX9 texture-manager and render-to-texture implementation files now use `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated DirectX9 dynamic texture creation, file texture loading, render-target texture creation, and render-target pixel readback device access.
- The larger DirectX9 shader implementation remains separate for a later backend-specific milestone.
- `specificContextDevice` remains public for compatibility; `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 20 implementation note:

- DirectX9 shader and blend-state implementations now use `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated DirectX9 vertex/index buffer creation, shader parameter upload, render paths, particle render paths, FVF declaration lookup, and blend operation state changes.
- This completes the currently identified DirectX9 direct context-access migration; `specificContextDevice` remains public until remaining non-DirectX backends and compatibility users are migrated.
- `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 21 implementation note:

- Metal manager, shader, blend-state, texture-manager, and render-to-texture implementation files now use `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated macOS/iOS Metal manager context lookup and release, plugin subscription handles, Metal device lookup, blend-state tracking, and render-target Metal device/command queue access.
- This completes the currently identified Metal direct context-access migration; `specificContextDevice` remains public until remaining compatibility users are migrated.
- `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 22 implementation note:

- Remaining non-Lua/non-plugin compatibility users now use `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated DirectSound cooperative-level window lookup, dummy backend release, Win32 icon/dialog helpers, a stale launcher comment, and the Metal backend instructions snippet.
- Remaining direct users are intentionally isolated to Lua framework wrappers and the ImGui Metal bridge for later compatibility-focused milestones.
- `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 23 implementation note:

- Lua framework wrappers now use `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated Android and DirectX Lua wrapper context lookups for quit, path/include handling, key code/name helpers, idiom/user-name queries, file dialogs, message boxes, and folder dialogs.
- Remaining direct users are intentionally isolated to the ImGui Metal bridge plugin for a later plugin-focused milestone.
- `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 24 implementation note:

- The ImGui Metal bridge plugin now uses `DEVICE::getSpecificContextDevice()` instead of reading `DEVICE::specificContextDevice` directly.
- Migrated ImGui Metal frame setup and draw-data rendering context lookups, and updated bridge/backend instruction comments to document the accessor path.
- This completes the currently identified direct `specificContextDevice` call-site migration; the pointer remains public until it is moved behind `DEVICE::Impl` in a later milestone.
- `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 25 implementation note:

- `DEVICE::specificContextDevice` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- `DEVICE::setSpecificContextDevice()` and `DEVICE::getSpecificContextDevice()` preserve the existing backend ownership boundary for context creation, release, and lookup.
- Updated the current-state report and backend instructions so new backend code does not depend on direct `DEVICE` layout access.
- `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 26 implementation note:

- `DEVICE` render counters are now stored behind `DEVICE::Impl` instead of being public `DEVICE` data members.
- Added read-only public getters for render statistics and private writer/increment helpers used by `CORE_MANAGER::render()`.
- Migrated Lua render-stat reads and backend guide snippets to the accessor/helper path.
- `specificContextDevice`, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 27 implementation note:

- `DEVICE` 3D frustum dimension cache is now stored behind `DEVICE::Impl` instead of being public `DEVICE` data members.
- `CORE_MANAGER::_updateDimFrustum()` writes the cache through private `DEVICE` helpers, while existing external reads continue through `DEVICE::getDimFromFrustum()`.
- Render code already used `getDimFromFrustum()`, so no render surface or `RENDERIZABLE` behavior changed.
- `specificContextDevice`, render counters, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 28 implementation note:

- `DEVICE::clearBackGround` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added public `DEVICE::setClearBackGround()` / `DEVICE::isClearBackGroundEnabled()` so Lua and core render logic no longer mutate/read the layout directly.
- Migrated scene-transition clear-state writes, render clear checks, and the Lua `mbm.enableClearBackGround` path to the accessor methods.
- `specificContextDevice`, render counters, frustum dimension cache, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 29 implementation note:

- `DEVICE::bOnErrorStopScript` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added public `DEVICE::setStopScriptOnError()` / `DEVICE::isStopScriptOnErrorEnabled()` as the compatibility path for this script-error behavior flag.
- Migrated the Lua `mbm.stopFlag` path to the setter; no current engine read path was changed.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 30 implementation note:

- `DEVICE::__swapBackBufferStep` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::resetSwapBackBufferStep()`, `DEVICE::incrementSwapBackBufferStep()`, and `DEVICE::getSwapBackBufferStep()` for the scene-transition flow.
- Migrated `CORE_MANAGER::logic()` and Lua loading reset code to the helper methods.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 31 implementation note:

- `DEVICE::windowPositionX` and `DEVICE::windowPositionY` are now stored behind `DEVICE::Impl` instead of being public `DEVICE` data members.
- Added public `DEVICE::setWindowPosition()`, axis-specific setters, and read-only getters so platform backends and Lua startup argument parsing keep the same behavior without direct layout access.
- Migrated DirectX9, OpenGL ES, Metal, X11 move tracking, forced device restore, and Lua initialization paths to the accessor methods.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 32 implementation note:

- `DEVICE::colorClearBackGround` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::setColorClearBackGround()` and `DEVICE::getColorClearBackGround()` so backend clear operations, Lua background color setup, and test scenes no longer depend on direct `DEVICE` layout access.
- Left `RENDERIZABLE_TO_TARGET::colorClearBackGround` untouched because render-to-texture target clear color is separate render target state.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 33 implementation note:

- `DEVICE::verbose` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::setVerbose()` and `DEVICE::isVerbose()` so backend diagnostics, Lua argument parsing, startup warnings, and the launcher wrapper no longer depend on direct `DEVICE` layout access.
- Kept the external launcher method `set_verbose()` unchanged; it still maps to the existing command-line behavior.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, clear color, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 34 implementation note:

- `DEVICE::run` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::setRun()` and `DEVICE::isRunning()` so core loops, backend close handlers, Lua quit paths, and mobile platform loops no longer depend on direct `DEVICE` layout access.
- Updated the macOS Metal quit/menu and window delegate helpers to store a `DEVICE *` and call `setRun(false)` instead of retaining a raw pointer to the hidden boolean.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, clear color, verbose flag, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 35 implementation note:

- `DEVICE::backBufferWidth` and `DEVICE::backBufferHeight` are now stored behind `DEVICE::Impl` instead of being public `DEVICE` data members.
- Added `DEVICE::setBackBufferSize()`, `DEVICE::setBackBufferWidth()`, and `DEVICE::setBackBufferHeight()` while keeping the existing read getters.
- Migrated core render/resize/restore logic, backend projection/init paths, Metal/macOS and Metal/iOS size handling, Android platform setup, Lua display metrics, and `testLib` layout reads to the accessor methods.
- Mesh file header fields named `backBufferWidth` / `backBufferHeight` remain unchanged because they are serialized asset metadata, not `DEVICE` state.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, clear color, verbose flag, run flag, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 36 implementation note:

- `DEVICE::ptrManager` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::setCoreManager()` and `DEVICE::getCoreManager()` so Android setup, Lua framework calls, plugin registration, display metrics, scene loading, and ImGui caps-lock checks no longer depend on direct `DEVICE` layout access.
- This does not change `CORE_MANAGER` ownership or lifetime; it only hides the back-reference storage.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, clear color, verbose flag, run flag, backbuffer size, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 37 implementation note:

- `DEVICE::cfg` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::getShaderConfig()` const/non-const accessors for shader config loading, sorting, shader lookup, Lua shader-list editing, plugin shader lookup, and `testLib` shader-menu reads.
- This hides the `DEVICE` layout slot only; the shader config object remains reachable because Lua/editor/plugin code still needs direct list and lookup access.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, clear color, verbose flag, run flag, backbuffer size, core manager pointer, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 38 implementation note:

- `DEVICE::orderRender` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::getOrderRender()` const/non-const accessors for scene-transition z-order reset and background z-order allocation.
- Internal renderizable z-order allocation now reads the hidden `Impl` state directly from `DEVICE::addRenderizable()`.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, clear color, verbose flag, run flag, backbuffer size, core manager pointer, shader config, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 39 implementation note:

- `DEVICE::lsDynamicVarGlobal` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::getDynamicVars()` const/non-const accessors so core manager command refs, Lua globals, Lua launcher setup, and Lua startup argument parsing keep the same map semantics without direct `DEVICE` layout access.
- Dynamic variable ownership cleanup remains in `DEVICE::~DEVICE()`, now deleting entries from the hidden `Impl` map.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, clear color, verbose flag, run flag, backbuffer size, core manager pointer, shader config, render order, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, scene, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 40 implementation note:

- `DEVICE::scene` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::setScene()` and `DEVICE::getScene()` so core scene transitions, event dispatch, audio scene lookup, Lua scene/user-data access, render constructors, plugin physics wrappers, and Android/iOS native callback routing no longer depend on direct `DEVICE` layout access.
- `CORE_MANAGER::setScene()` remains the main lifecycle entry point and now delegates to the hidden `DEVICE` scene slot.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, clear color, verbose flag, run flag, backbuffer size, core manager pointer, shader config, render order, dynamic globals, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, camera, remaining public fields, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 41 implementation note:

- `DEVICE::camera` is now stored behind `DEVICE::Impl` instead of being a public `DEVICE` data member.
- Added `DEVICE::getCamera()` const/non-const accessors so core projection, frustum updates, render matrices, Lua camera bindings, plugins, platform samples, and test scenes keep the existing mutable camera behavior without direct `DEVICE` layout access.
- Updated C++ usage examples and platform-port templates from `device->camera` to `device->getCamera()`.
- `specificContextDevice`, render counters, frustum dimension cache, clear-background state, script-error stop flag, swap-back-buffer state, window position, clear color, verbose flag, run flag, backbuffer size, core manager pointer, shader config, render order, dynamic globals, scene pointer, render lists, `RENDERIZABLE_TO_TARGET::specificConfig`, and `RENDERIZABLE` remain untouched by this milestone.

Milestone 41 review cleanup note:

- Replaced repeated line-by-line `getCamera()` use inside hot/core functions with local `CAMERA &camera` or `const CAMERA &camera` references.
- This keeps the `DEVICE` layout hidden while preserving the old direct-camera ergonomics inside a function scope.
- The local reference is intentionally scoped to the current function only; no code should cache the `DEVICE` camera reference as persistent object state.
- Project coding rules now generalize this pattern: repeated accessor-backed object use in one function should be stored once in a local variable/reference.

### Phase 3 - Hide renderer backend handles

Order:

1. `TEXTURE::idTexture/ptrTexture`
2. `BUFFER_GL::bs`
3. `SHADER::ptrShaderSpecific`
4. `RENDERIZABLE_TO_TARGET::specificConfig`
5. `DEVICE::specificContextDevice`

This phase removes most backend leakage while keeping gameplay-facing APIs mostly unchanged.

### Phase 4 - PIMPL remaining manager internals

Move private containers and counters into `Impl` for:

- `CORE_MANAGER`, after the audio manager pattern is validated.
- `TEXTURE_MANAGER`
- `MESH_MANAGER`
- `ANIMATION_BACKUP`
- `EFFECT_SHADER`

This mainly improves header hygiene and ABI layout.

### Phase 5 - Add public accessors for gameplay state

Before hiding public fields, add and use methods for:

- `DEVICE`: compatibility wrappers around gameplay-facing state, if direct mutable-reference access should be narrowed later.
- `RENDERIZABLE`: transform, visibility, blend, user data, dynamic vars.
- `SCENE`: scene transition state and user data.
- `ANIMATION_MANAGER`: animation list/index/callback access.

Keep direct fields during transition if source compatibility matters.

### Phase 6 - Hide or deprecate public fields

Only after engine internals, Lua bindings, plugins, examples, and editors use the methods:

- Move remaining state into `Impl`.
- Remove public backend fields.
- Deprecate or remove legacy direct fields depending on the compatibility decision from Phase 0.

## Not worth PIMPL first

These can stay as normal public value/API types unless there is a specific ABI goal:

- `VEC2`, `VEC3`, `MATRIX`, `COLOR`, and other primitive math/value structs.
- Small enums such as `TYPE_CLASS`, `BLEND_STATE`, `TYPE_ANIMATION`.
- File-format structs in `header-mesh.h`, unless the mesh format itself is being redesigned.
- `PLUGIN` virtual interface, unless plugin ABI stability becomes a formal goal.

## Summary

The core is already moving in the right direction in audio, but the main engine is not close to strict PIMPL yet. The missing work is mostly:

1. Hide backend handles in texture, shader, buffer, render target, and device classes.
2. Move private manager containers into `Impl`.
3. Add accessor APIs before hiding public gameplay fields.
4. Decide whether direct public fields remain a supported convenience API.
5. Update Lua, plugins, examples, and docs after each staged cleanup.

The highest-value first implementation step is not `RENDERIZABLE`; it is proving the pattern safely in `AUDIO_MANAGER`. After that compiles cleanly, the next large cleanup target should be backend handle isolation in `DEVICE`, texture, shader, buffer, and render-target code.
