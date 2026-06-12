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
| `TEXTURE` backend handle | Converted | `TEXTURE` stores the GPU handle behind `BackendData`; backend code uses helper methods for integer or pointer handles. |
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
| `include/core_mbm/core-manager.h` | `device`. Scene initialization, scene-change, Caps Lock, and window restore options are hidden behind `CORE_MANAGER::Impl`. |
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
| `include/core_mbm/shader.h` | `BUFFER_GL::BUFFER_SPECIFIC *bs` | Done: stored in `BUFFER_GL::BackendData`, with helper accessors for backend code. |
| `include/core_mbm/shader.h` | `SHADER::void *ptrShaderSpecific` | Done: stored in `SHADER::BackendData`, with helper accessors for backend code. |
| `include/core_mbm/texture-manager.h` | `TEXTURE::idTexture/ptrTexture` union | Done: stored in `TEXTURE::BackendData`, with helper accessors for backend code. |
| `include/core_mbm/renderizable.h` | `RENDERIZABLE_TO_TARGET::void *specificConfig` | Done: stored in `RENDERIZABLE_TO_TARGET::BackendData`, with helper accessors for backend code. |
| `include/core_mbm/specific-*.h` | Concrete GL/EGL/X11, D3D9, Metal/Cocoa implementation layouts | Done: concrete backend layouts moved to `src/core_mbm/private/`; public backend headers now keep only forward declarations or narrow bridge APIs. |

This can follow the audio pattern: public class owns `std::unique_ptr<BackendData>`, the incomplete struct is defined in the active backend `.cpp/.mm`, and common code talks through narrow private hooks.

#### Backend header leakage audit

The backend handle fields above are now hidden from the main public class layouts, and the concrete backend implementation layouts have moved out of the public header surface. The remaining public backend headers are compatibility/bridge headers: they forward-declare backend context types or expose narrow opaque bridge APIs instead of concrete SDK-owned layouts.

This completed the most useful PIMPL direction for the original goal: isolate platform-specific implementation details without forcing accessor churn for every strongly typed gameplay field.

| Header | What leaks today | Current external consumers | Cleanup direction |
|---|---|---|---|
| `src/core_mbm/private/specific-opengl_es.h` | EGL, GLES2, and GL debug macros remain, but the header is now private to OpenGL ES backend implementation files. `RENDER2TARGET_GLES`, `BUFFER_SPECIFIC`, `GLES_PS_VS`, and all concrete OpenGL ES context layouts have moved to private OpenGL ES backend headers. | OpenGL ES backend files and private GLES resource/context headers. | Done for public-header isolation. Keep Android bridge APIs in `include/core_mbm/android-bridge.h` for Android platform/file/Lua users that need them. |
| `include/core_mbm/specific-directx9.h` | No concrete DirectX9/Win32 backend layout remains; it now only forward-declares `SPECIFIC_AUX_CONTEXT_DEVICE`. `RENDER2TARGET_DIRECTX9`, `BUFFER_SPECIFIC`, `D3D_PS_VS`, `D3D_VERTEX_CONVERTER`, HRESULT logging, and the DirectX9 context layout have moved to private backend headers. The texture pixel-copy helper is file-local. Direct dependencies on `core-manager.h`, `shader.h`, `primitives.h`, D3D9, Win32 platform, and D3DX headers have been removed. | DirectX9 backend files and Win32 platform helper files include the private context header when concrete D3D/window fields are required. | Done for DirectX9 context layout. The broader Win32 platform header remains separate platform integration surface. |
| `include/core_mbm/specific-metal.h` | No concrete Metal backend layout remains; it now only forward-declares `SPECIFIC_AUX_CONTEXT_DEVICE` and exposes narrow opaque `void *` frame bridge functions for the ImGui Metal plugin. `RENDER2TARGET_METAL`, `BUFFER_SPECIFIC`, and the Metal context layout have moved to private Metal backend headers. | Metal backend files include the private context header. The ImGui Metal bridge uses the opaque frame bridge instead of reading the context layout directly. | Done for Metal context layout. |
| `include/core_mbm/specific-dummy.h` | No concrete dummy backend layout remains; it now only forward-declares `SPECIFIC_AUX_CONTEXT_DEVICE`. `RENDER2TARGET_DUMMY`, `BUFFER_SPECIFIC`, and the dummy context layout have moved to private dummy backend headers. | Dummy backend and dummy Lua wrappers can include it as a compatibility shim, but only backend files include the concrete private context header. | Done for dummy; use this as the lowest-risk pattern for future platform bridge splits. |
| `include/core_mbm/platform-win32.h` | Win32 window/event integration types. The MinGW D3DX9 shim has moved to private `src/core_mbm/private/d3dx9-mingw.h`. | Windows launcher, Win32 platform code, Lua wrappers, and DirectX9 backend code. | Treat Win32 platform integration separately from renderer PIMPL. The D3DX9 compatibility shim is now private to the DirectX9 backend selector. |

Recommended order:

1. Do not add accessors for public gameplay/core flags only because they are public and strongly typed. Accessors are useful when they hide layout, preserve invariants, or replace repeated direct backend access.
2. Move backend-only render resource structs private first. `RENDER2TARGET_METAL`, `RENDER2TARGET_DIRECTX9`, `RENDER2TARGET_GLES`, backend `BUFFER_SPECIFIC` structs, `GLES_PS_VS`, `D3D_PS_VS`, and `D3D_VERTEX_CONVERTER` are done.
3. Add narrow platform bridge methods before hiding concrete `SPECIFIC_AUX_CONTEXT_DEVICE` fields. The Android asset/window/JNI bridge and Metal opaque frame bridge are done.
4. Move the full `specific-*.h` layouts to `src/core_mbm/` or a private include area and leave only forward declarations or stable bridge APIs in public headers. This is done for dummy, DirectX9, Metal, and OpenGL ES.
5. Reorganize the private backend headers into `src/core_mbm/private/` and update internal include paths/build include directories. This is done as the structure-only follow-up after backend isolation passed Linux, Android, macOS, MinGW, MSVS, and iOS validation.

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

Milestone 42 implementation note:

- Added `TEXTURE` backend-handle compatibility helpers: `getBackendTextureId()`, `setBackendTextureId()`, `getBackendTextureIdAddress()`, `getBackendTexturePointer()`, `setBackendTexturePointer()`, and `getBackendTexturePointerAddress()`.
- Migrated core texture managers, shader binding paths, ImGui texture helpers, and Lua texture-info access from direct `idTexture` / `ptrTexture` reads or writes to the helper methods.
- The public `TEXTURE::idTexture/ptrTexture` union remains in place for source compatibility in this milestone; remaining direct code-side handle access is isolated to the helper implementation and the compatibility union declaration.
- Updated backend-porting guidance so new code uses the helper path instead of direct union access.

Milestone 43 implementation note:

- Moved the `TEXTURE::idTexture/ptrTexture` compatibility union out of the public header and into private `TEXTURE::BackendData`.
- `TEXTURE` now follows the same public-header shape as the audio precedent: an incomplete `BackendData` plus `std::unique_ptr<BackendData>`.
- Existing backend helpers remain the only handle access path, so OpenGL ES still gets a `uint32_t *` for `GLGenTextures()` / `GLDeleteTextures()` and pointer-backed backends still get `void *` / `void **`.

Milestone 44 implementation note:

- Added `BUFFER_GL::getBackendBuffer()` and `BUFFER_GL::setBackendBuffer()` as the compatibility helper path for `BUFFER_GL::bs`.
- Migrated non-owner backend consumers from direct `pBufferId->bs` / `pGl->bs` access to `getBackendBuffer()`, covering shader render paths and mesh-debug readback helpers.
- The public `BUFFER_GL::bs` member remains in place for source compatibility in this milestone; remaining direct code-side access is isolated to `BUFFER_GL` owner implementation methods and will be the next cleanup before moving storage behind `BackendData`.

Milestone 45 implementation note:

- Cleaned up owner-side `BUFFER_GL::bs` usage in OpenGL ES, DirectX9, Metal, and dummy backend `BUFFER_GL` methods.
- Constructor/destructor paths now use `setBackendBuffer()` / `getBackendBuffer()`, and functions that use the backend buffer more than once store it in a local `BUFFER_SPECIFIC *backendBuffer` first.
- Revisited the Milestone 44 render-path migration to follow the accessor reuse rule: repeated `pBufferId->getBackendBuffer()` use is now localized once per render function.
- Remaining direct `BUFFER_GL::bs` code-side access is limited to the compatibility helper implementation and the public compatibility member declaration.

Milestone 46 implementation note:

- Moved the `BUFFER_GL::bs` compatibility member out of the public header and into private `BUFFER_GL::BackendData`.
- `BUFFER_GL::getBackendBuffer()` and `BUFFER_GL::setBackendBuffer()` remain the only backend-buffer access path, so backend destructors still own and delete their concrete `BUFFER_SPECIFIC` allocation exactly as before.
- Used a custom private deleter for the incomplete `BackendData` holder so backend-specific `BUFFER_GL` destructor definitions do not need the private storage definition.

Milestone 47 implementation note:

- Added `SHADER::getBackendShaderSpecific()` and `SHADER::setBackendShaderSpecific()` as the compatibility helper path for `SHADER::ptrShaderSpecific`.
- Migrated non-owner shader-variable registration paths in animation, shader effects, line mesh, particle, and steered particle code to use the helper.
- `SHADER::update()` now stores the backend shader-specific pointer once in a local variable before updating pixel and vertex shader variables, following the accessor reuse rule.
- The public `SHADER::ptrShaderSpecific` member remains in place for source compatibility in this milestone; remaining direct code-side access is isolated to backend owner implementation methods and the compatibility helper implementation.

Milestone 48 implementation note:

- Cleaned up owner-side `SHADER::ptrShaderSpecific` usage in OpenGL ES, DirectX9, and Metal backend `SHADER` methods.
- Constructors now initialize backend shader-specific objects through `setBackendShaderSpecific()` where a backend allocation is needed, while the public compatibility field has a default `nullptr` initializer.
- Destructors, restore/release paths, compile paths, load checks, and render paths now store `getBackendShaderSpecific()` once in a local `void *backendShaderSpecific` before casting or passing it on.
- Remaining direct `ptrShaderSpecific` code-side access is limited to parameter names, comments, the compatibility helper implementation, and the public compatibility member declaration.

Milestone 49 implementation note:

- Moved the `SHADER::ptrShaderSpecific` compatibility member out of the public header and into private `SHADER::BackendData`.
- `SHADER::getBackendShaderSpecific()` and `SHADER::setBackendShaderSpecific()` remain the only shader-specific backend access path, so backend destructors still own and release their concrete objects exactly as before.
- Used a custom private deleter for the incomplete `BackendData` holder so backend-specific `SHADER` destructor definitions do not need the private storage definition.

Milestone 50 implementation note:

- Added `RENDERIZABLE_TO_TARGET::getRenderTargetSpecificConfig()` and `RENDERIZABLE_TO_TARGET::setRenderTargetSpecificConfig()` as the compatibility helper path for `RENDERIZABLE_TO_TARGET::specificConfig`.
- Migrated render-target backend config users in OpenGL ES, DirectX9, and Metal core-manager, texture-manager, and render-to-texture implementation files to the helper path.
- Constructors now initialize backend render-target config through `setRenderTargetSpecificConfig()`, and destructors store `getRenderTargetSpecificConfig()` once in a local `void *renderTargetSpecificConfig` before deleting the concrete backend object.
- This milestone only touches render-to-texture backend handle access; gameplay-facing `RENDERIZABLE` fields remain untouched.
- The public `RENDERIZABLE_TO_TARGET::specificConfig` member remains in place for source compatibility in this milestone; remaining direct code-side access is limited to the compatibility helper implementation and the public compatibility member declaration.

Milestone 51 implementation note:

- Moved the `RENDERIZABLE_TO_TARGET::specificConfig` compatibility member out of the public header and into private `RENDERIZABLE_TO_TARGET::BackendData`.
- `RENDERIZABLE_TO_TARGET::getRenderTargetSpecificConfig()` and `RENDERIZABLE_TO_TARGET::setRenderTargetSpecificConfig()` remain the only render-target backend config access path, so backend destructors still own and release their concrete config objects exactly as before.
- Used a custom private deleter for the incomplete `BackendData` holder so backend-specific `RENDERIZABLE_TO_TARGET` destructor definitions do not need the private storage definition.
- This milestone still avoids gameplay-facing `RENDERIZABLE` fields.

Milestone 52 implementation note:

- Added a backend header leakage audit to separate the PIMPL goal from broad accessor churn.
- Confirmed the next useful boundary is the public `specific-*.h` header set, not gameplay-facing `RENDERIZABLE` state or strongly typed public flags by default.
- Classified each public backend header by leaked platform/resource types, current non-backend consumers, and likely cleanup direction.
- Updated backend-porting guidance so new backend work treats public `specific-*.h` layouts as legacy compatibility and keeps backend-only structs in backend implementation/private headers when practical.
- This milestone is documentation-only and does not change engine code, object layout, or `RENDERIZABLE`.

Milestone 53 implementation note:

- Moved `RENDER2TARGET_METAL` out of the public `include/core_mbm/specific-metal.h` header and into the private backend header `src/core_mbm/specific-metal-render-target.h`.
- Updated the Metal render-target owner/destructor path, Metal render-to-texture save path, Metal render-target texture creation, and Metal render-to-target pass code to include the private header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE` and `BUFFER_SPECIFIC` in `specific-metal.h` at that point because platform/bootstrap, shader, mesh, device, and ImGui bridge code still needed the concrete Metal context/buffer layout. Milestone 56 later moves Metal `BUFFER_SPECIFIC`.
- This is the first backend-header leakage pilot and does not change runtime ownership, object layout outside the private backend config, gameplay API, or `RENDERIZABLE`.

Milestone 54 implementation note:

- Moved `RENDER2TARGET_DIRECTX9` out of the public `include/core_mbm/specific-directx9.h` header and into the private backend header `src/core_mbm/specific-directx9-render-target.h`.
- Updated the DirectX9 render-to-target pass, render-target texture creation, and render-to-texture owner/save paths to include the private header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE`, `BUFFER_SPECIFIC`, and `D3D_PS_VS` in `specific-directx9.h` for now because window/bootstrap, shader, texture, blend, device, and platform helper code still need those concrete layouts.
- This repeats the backend-header leakage pattern proven by Metal and does not change runtime ownership, gameplay API, or `RENDERIZABLE`.

Milestone 55 implementation note:

- Moved `RENDER2TARGET_GLES` out of the public `include/core_mbm/specific-opengl_es.h` header and into the private backend header `src/core_mbm/specific-opengl_es-render-target.h`.
- Updated OpenGL ES render-target release, common render-to-target pass, render-target texture creation, and render-to-texture owner/save paths to include the private header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE`, `BUFFER_SPECIFIC`, `GLES_PS_VS`, and GL debug/helper macros in `specific-opengl_es.h` at that point because Android/platform, shader, mesh, file/asset, audio, and Lua wrapper code still needed those concrete definitions. Milestones 58 and 59 later move the buffer and shader structs.
- This completes the first render-target backend-header cleanup pass for Metal, DirectX9, and OpenGL ES without changing runtime ownership, gameplay API, or `RENDERIZABLE`.

Milestone 56 implementation note:

- Moved Metal `BUFFER_SPECIFIC` out of the public `include/core_mbm/specific-metal.h` header and into the private backend header `src/core_mbm/specific-metal-buffer.h`.
- Updated Metal buffer release, shader buffer allocation/render paths, and Metal mesh-debug readback to include the private buffer header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE` in `specific-metal.h` for now because device, core-manager, platform, blend, texture, render-target, and ImGui bridge code still need the concrete Metal context layout.
- This is the first buffer backend-header cleanup pilot and does not change `BUFFER_GL::BackendData`, buffer ownership, runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 57 implementation note:

- Moved DirectX9 `BUFFER_SPECIFIC` out of the public `include/core_mbm/specific-directx9.h` header and into the private backend header `src/core_mbm/specific-directx9-buffer.h`.
- Updated DirectX9 shader buffer ownership and render paths to include the private buffer header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE` and `D3D_PS_VS` in `specific-directx9.h` at that point because window/bootstrap, shader, texture, blend, device, and platform helper code still needed those concrete layouts. Milestone 60 later moves `D3D_PS_VS`.
- This repeats the buffer backend-header cleanup pattern after Metal and does not change `BUFFER_GL::BackendData`, buffer ownership, runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 58 implementation note:

- Moved OpenGL ES `BUFFER_SPECIFIC` out of the public `include/core_mbm/specific-opengl_es.h` header and into the private backend header `src/core_mbm/specific-opengl_es-buffer.h`.
- Updated OpenGL ES shader buffer ownership/render paths and OpenGL ES mesh-debug readback to include the private buffer header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE`, `GLES_PS_VS`, and GL debug/helper macros in `specific-opengl_es.h` at that point because Android/platform, shader, mesh, file/asset, audio, and Lua wrapper code still needed those concrete definitions. Milestone 59 later moves `GLES_PS_VS`.
- This completes the buffer backend-header cleanup pass for Metal, DirectX9, and OpenGL ES without changing `BUFFER_GL::BackendData`, buffer ownership, runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 59 implementation note:

- Moved `GLES_PS_VS` out of the public `include/core_mbm/specific-opengl_es.h` header and into the private backend header `src/core_mbm/specific-opengl_es-shader.h`.
- Updated OpenGL ES shader-specific ownership, uniform upload, load checks, and render paths to include the private shader header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE` and GL debug/helper macros in `specific-opengl_es.h` for now because Android/platform, file/asset, audio, Lua wrapper, and general GLES backend code still need those definitions.
- This removes the last OpenGL ES render-resource struct from the public backend header without changing `SHADER::BackendData`, shader ownership, runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 60 implementation note:

- Moved `D3D_PS_VS` out of the public `include/core_mbm/specific-directx9.h` header and into the private backend header `src/core_mbm/specific-directx9-shader.h`.
- Updated DirectX9 shader-specific ownership, uniform lookup/upload, release, compile, load checks, and render paths to include the private shader header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE`, DirectX helper functions, `D3D_VERTEX_CONVERTER`, and Win32 integration types in `specific-directx9.h` for now because window/bootstrap, shader conversion, texture, blend, device, and platform helper code still need those definitions.
- This removes the main DirectX9 shader resource struct from the public backend header without changing `SHADER::BackendData`, shader ownership, runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 61 implementation note:

- Moved `D3D_VERTEX_CONVERTER` out of the public `include/core_mbm/specific-directx9.h` header and into the private backend header `src/core_mbm/specific-directx9-vertex.h`.
- Updated DirectX9 shader vertex-buffer conversion paths to include the private vertex helper header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE`, DirectX helper functions, and Win32 integration types in `specific-directx9.h` for now because window/bootstrap, texture, blend, device, and platform helper code still need those definitions.
- This removes the last shader-only helper type from the public DirectX9 backend header without changing vertex conversion behavior, buffer ownership, gameplay API, or `RENDERIZABLE`.

Milestone 62 implementation note:

- Removed the `copy_pixels_per_row_Pitch` declaration from the public `include/core_mbm/specific-directx9.h` header.
- Made `copy_pixels_per_row_Pitch` file-local inside `src/core_mbm/texture-manager-directx9.cpp`, where all current call sites already live.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE`, the HRESULT logging helper, and Win32 integration types in `specific-directx9.h` at that point because window/bootstrap, device, and platform helper code still needed those definitions. Milestone 63 later moves the HRESULT helper.
- This removes a texture-only DirectX9 helper from the public backend header without changing texture upload behavior, gameplay API, or `RENDERIZABLE`.

Milestone 63 implementation note:

- Moved the DirectX9 HRESULT logging helper declaration and `CHECK_AND_LOG_HRESULT_DX` macro out of the public `include/core_mbm/specific-directx9.h` header and into the private backend header `src/core_mbm/specific-directx9-hresult.h`.
- Updated the DirectX9 core-manager render path and helper implementation file to include the private HRESULT header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE` and Win32 integration types in `specific-directx9.h` for now because window/bootstrap, device, and platform helper code still need those definitions.
- This removes the last free helper declaration from the public DirectX9 backend header without changing render begin error handling, gameplay API, or `RENDERIZABLE`.

Milestone 64 implementation note:

- Removed direct `core-manager.h`, `shader.h`, and `primitives.h` includes from the public `include/core_mbm/specific-directx9.h` header.
- Added forward declarations for `CORE_MANAGER` and `FVF_PROVIDE_BY_ENGINE`, which are enough for the remaining DirectX9 context method signatures.
- Kept `platform/win32-platform.h`, `d3d9.h`, and the DirectX compatibility shim in `specific-directx9.h` because the public context layout still stores concrete Win32 and D3D9 fields.
- This reduces transitive public header dependencies without changing DirectX9 context ownership, runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 65 implementation note:

- Removed the DirectX9 D3DX compatibility shim include from the public `include/core_mbm/specific-directx9.h` header.
- Initially added direct `core_mbm/d3dx9-mingw.h` includes to the DirectX9 backend files/private headers that actually use `D3DX*` symbols. Milestone 66 corrects this because `d3dx9-mingw.h` is only for MinGW; MSVS must keep using standard `d3dx9.h`.
- Kept `d3d9.h` in `specific-directx9.h` because the public context layout still stores concrete D3D9 COM pointer fields.
- This reduces another transitive public backend dependency without changing DirectX9 shader, texture, render begin, gameplay API, or `RENDERIZABLE` behavior.

Milestone 66 implementation note:

- Added private DirectX9 D3DX selection header `src/core_mbm/specific-directx9-d3dx.h`.
- The private header now includes the private MinGW shim locally only for MinGW/Cygwin and includes standard `d3dx9.h` for MSVS/other Windows compilers, with an MSVC `d3dx9.lib` pragma.
- Updated DirectX9 backend files/private headers that use `D3DX*` symbols to include `specific-directx9-d3dx.h` instead of referring to the MinGW shim directly.
- Kept D3DX out of public `specific-directx9.h`, preserving the public-header cleanup while restoring the intended MSVS vs MinGW split.

Milestone 67 implementation note:

- Fixed the MSVS compile failure in `src/core_mbm/specific-directx9.cpp` after the Milestone 64 include trim.
- Added a direct `shader.h` include to `specific-directx9.cpp` because `SPECIFIC_AUX_CONTEXT_DEVICE::getFVF` switches on concrete `FVF_PROVIDE_BY_ENGINE` enum values.
- Kept `shader.h` out of public `include/core_mbm/specific-directx9.h`; the public header still only needs the forward declaration for the method signature.

Milestone 68 implementation note:

- Added MSVC-only C4251 suppression macros to `include/core_mbm/core-exports.h`.
- Applied the suppression narrowly around `SHADER::backendData`, the exported-class PIMPL `std::unique_ptr` member that MSVS reports repeatedly as needing a DLL interface.
- This kept the opaque `SHADER::BackendData` PIMPL design without changing MinGW/GCC/Clang behavior, ABI layout, runtime behavior, gameplay API, or `RENDERIZABLE`. Milestone 69 replaces this temporary suppression with method-level exports.

Milestone 69 implementation note:

- Replaced `class API_IMPL SHADER` with `class SHADER` plus explicit `API_IMPL` on public `SHADER` methods and static matrix members.
- Removed the MSVC C4251 suppression macros from `core-exports.h` and removed the suppression wrapper around `SHADER::backendData`.
- Kept `SHADER::BackendData` private and opaque while avoiding class-level export of the private `std::unique_ptr` member.
- Added explicit export to `SHADER::modelView` and `SHADER::mvpMatrix` because render code and plugins use those static members through the public header.
- This is the structural MSVC warning fix for `SHADER` and does not change runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 70 implementation note:

- Moved `RENDER2TARGET_DUMMY` out of the public `include/core_mbm/specific-dummy.h` header and into the private backend header `src/core_mbm/specific-dummy-render-target.h`.
- Updated the dummy render-to-texture implementation to include the private dummy render-target header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE` and `BUFFER_SPECIFIC` in `specific-dummy.h` for now because dummy device, shader, audio, and Lua wrapper/template code still include that public template header.
- This updates the dummy backend template to follow the proven private render-target pattern without changing runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 71 implementation note:

- Moved dummy `BUFFER_SPECIFIC` out of the public `include/core_mbm/specific-dummy.h` header and into the private backend header `src/core_mbm/specific-dummy-buffer.h`.
- Updated the dummy shader/buffer implementation to include the private dummy buffer header explicitly.
- Kept `SPECIFIC_AUX_CONTEXT_DEVICE` in `specific-dummy.h` because dummy device, core-manager, audio, and Lua wrapper/template code still include that public context surface.
- This completes the dummy backend resource-struct split without changing runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 72 implementation note:

- Removed the now-unused `primitives.h` include from public `include/core_mbm/specific-dummy.h`.
- After Milestones 70 and 71, the public dummy header only exposes the minimal dummy `SPECIFIC_AUX_CONTEXT_DEVICE` template and the Win32 window dependency needed by that context on Windows.
- This trims dummy public-header dependencies while preserving the dummy backend template context surface.

Milestone 73 documentation note:

- Refreshed the remaining-work plan after the backend resource-handle milestones.
- Added a pre-audit for `TEXTURE_MANAGER`, `MESH_MANAGER`, `ANIMATION_BACKUP`, and `EFFECT_SHADER` focused on the primary PIMPL goal: hiding explicit OS/backend dependencies from public headers.
- Confirmed these four candidates do not currently expose DirectX/OpenGL ES/Metal/Win32/macOS SDK handles directly in their public manager layouts; their remaining value is mostly header/ABI cleanup, not backend isolation.
- Kept the platform bridge headers as the main remaining backend-isolation problem because `SPECIFIC_AUX_CONTEXT_DEVICE` still exposes concrete backend/platform context layouts.

Milestone 74 implementation note:

- Moved the concrete dummy `SPECIFIC_AUX_CONTEXT_DEVICE` layout out of public `include/core_mbm/specific-dummy.h` and into the private backend header `src/core_mbm/specific-dummy-context.h`.
- The public dummy header now only forward-declares `SPECIFIC_AUX_CONTEXT_DEVICE`, matching the bridge direction used by `DEVICE`.
- Updated dummy backend, dummy Win32 bridge, and DirectSound dummy-window users to include the private context header only where the concrete `window`, `idIcon`, or `release()` members are required.
- This removes the last concrete dummy backend context layout from the public core header without changing runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 75 implementation note:

- Moved the concrete DirectX9 `SPECIFIC_AUX_CONTEXT_DEVICE` layout out of public `include/core_mbm/specific-directx9.h` and into the private backend header `src/core_mbm/specific-directx9-context.h`.
- The public DirectX9 header now only forward-declares `SPECIFIC_AUX_CONTEXT_DEVICE`, so it no longer exposes `d3d9.h`, `platform/win32-platform.h`, `IDirect3D*`, `IDirect3DVertexDeclaration9`, `WINDOW`, `DWORD`, or Win32 event/joystick bridge fields.
- Updated DirectX9 backend resource headers, backend implementation files, DirectSound, and the Win32 platform helper to include the private context header only where the concrete D3D/window layout is required.
- Kept `platform-win32.h` separate because it is a platform integration surface, not a renderer resource ownership type.
- This removes the DirectX9 backend context layout from the public core header without changing runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 76 implementation note:

- Moved the concrete Metal `SPECIFIC_AUX_CONTEXT_DEVICE` layout out of public `include/core_mbm/specific-metal.h` and into the private backend header `src/core_mbm/specific-metal-context.h`.
- The public Metal header now only forward-declares `SPECIFIC_AUX_CONTEXT_DEVICE` and declares three exported opaque frame bridge functions: `mbm_metal_get_current_pass_descriptor()`, `mbm_metal_get_current_command_buffer()`, and `mbm_metal_get_current_encoder()`.
- Updated Metal backend files and private Metal resource headers to include the private context header only where concrete `id<MTL*>`, `CAMetalLayer`, Cocoa, or UIKit fields are required.
- Updated the ImGui Metal bridge plugin to use the opaque frame bridge instead of reading `DEVICE::getSpecificContextDevice()->currentPassDescriptor/currentCommandBuffer/currentEncoder` directly.
- This removes the Metal backend context layout from the public core header without changing runtime behavior, gameplay API, or `RENDERIZABLE`.

Milestone 77 implementation note:

- Moved the concrete Windows OpenGL ES `SPECIFIC_AUX_CONTEXT_DEVICE` layout out of public `include/core_mbm/specific-opengl_es.h` and into the private backend header `src/core_mbm/specific-opengl_es-windows-context.h`.
- The Windows branch of the public OpenGL ES header now only forward-declares `SPECIFIC_AUX_CONTEXT_DEVICE`, so it no longer exposes `WINDOW`, `DWORD`, `WIN_EVENT_BY_PASS`, `WIN_JOYSTICK_BY_PASS`, `plusWindows`, `joystick-win32`, `core-manager.h`, or `platform/win32-platform.h`.
- Updated Windows OpenGL ES backend files, DirectSound, and the Win32 platform helper to include the private context header only where the concrete window/EGL callback layout is required.
- Kept Android and Linux/macOS OpenGL ES context layouts public for now because they have many platform and asset/file bridge call sites that need a separate staged migration.
- This removes the Windows-specific OpenGL ES context layout from the public core header without changing runtime behavior, gameplay API, Android/Linux/macOS GLES behavior, or `RENDERIZABLE`.

Milestone 78 implementation note:

- Moved the concrete X11/Linux/macOS OpenGL ES `SPECIFIC_AUX_CONTEXT_DEVICE` layout out of public `include/core_mbm/specific-opengl_es.h` and into the private backend header `src/core_mbm/specific-opengl_es-x11-context.h`.
- The Linux/macOS branch of the public OpenGL ES header now only forward-declares `SPECIFIC_AUX_CONTEXT_DEVICE`, so it no longer exposes `X11/Xlib.h`, `X11/Xutil.h`, `X11/XKBlib.h`, `Window`, `Display`, `window_x11`, `display_x11`, `eglConfig`, `make_x_window()`, or `recreateEGLSurface()`.
- Updated shared OpenGL ES core-manager/device code and the X11 OpenGL ES backend file to include the private context header only where concrete X11/EGL fields are required.
- Kept the Android OpenGL ES context layout public for now because Android platform entry points, Lua wrappers, file/asset helpers, and lsqlite3 asset package code still reach into JNI/asset/window fields.
- This removes the X11 OpenGL ES context layout from the public core header without changing runtime behavior, gameplay API, Android GLES behavior, Windows GLES behavior, or `RENDERIZABLE`.

Milestone 79 Android OpenGL ES context bridge audit:

The Android branch is the last concrete `SPECIFIC_AUX_CONTEXT_DEVICE` layout still exposed by public `include/core_mbm/specific-opengl_es.h`. Do not move it in one step. It currently mixes platform bootstrap, JNI class cache, asset/file helpers, native-window EGL state, and texture filter state.

Current Android context responsibilities:

| Context area | Public fields / methods currently used | Current consumers | Bridge direction |
|---|---|---|---|
| App paths | `absPath`, `apkPath`, `addPathDroid()` | `platform-android/main.cpp`, `platform-android/main-lua.cpp`, `platform-android/main-native-activity.cpp`, `src/core_mbm/file-util.cpp`, `third-party/lsqlite3/asset-pkg.cpp`, Android Lua wrappers | Add path setters/getters on an Android-private bridge, e.g. `mbm_android_set_paths(absPath, apkPath)`, `mbm_android_get_abs_path()`, and `mbm_android_add_asset_path(path)`. |
| Asset manager | `assetManager`, `existFileOnAssets()`, `copyFileFromAsset()`, `getImageDataFromDroid()`, `fopenAsset()` | Android bootstrap, `file-util.cpp`, `audio-opensl-android.cpp`, `specific-android.cpp`, `lsqlite3` asset package | Keep file/asset operations as bridge functions so non-render modules do not read the context layout. Add `mbm_android_get_asset_manager()` only for backend files that must call NDK APIs directly. |
| Native window / EGL | `nativeWindow`, `eglDisplay`, `eglSurface`, `eglContext`, `eglConfig`, `release()` | `platform-android/main-native-activity.cpp`, `core-manager-opengl_es_android.cpp`, `device-opengl_es.cpp` | Move to private `src/core_mbm/specific-opengl_es-android-context.h` only after adding native-window setters and EGL helpers used by Android platform/core-manager code. |
| JNI thread/env | `jenv`, temporary reassignment around callbacks | `platform-android/main.cpp`, `platform-android/main-lua.cpp`, `platform-android/main-native-activity.cpp`, Android Lua wrappers | Add scoped or explicit bridge calls for setting/restoring the active `JNIEnv*`, e.g. `mbm_android_get_jni_env()`, `mbm_android_set_jni_env(env)`, and later a scoped helper if the codebase accepts it. |
| Java class cache | `jclassDoCommandsJniEngine`, `jclassFileJniEngine`, `jclassKeyCodeJniEngine`, `initClassLoader()`, `cacheJavaClasses()` | Android platform entry points and Android Lua wrappers | Add narrow wrappers for doCommands, key code/name, dialogs, vibration, and file dialog calls. Do not expose cached `jclass` fields outside private Android code after migration. |
| UTF string lifetime | `get_safe_string_utf()` | Android Lua wrappers | Keep as a private helper behind JNI bridge functions. Lua wrappers should pass C strings to bridge functions, not allocate `jstring` directly through context fields. |
| Texture filter cache | `filter_GL_TEXTURE_*` | `core-manager-opengl_es_android.cpp`, `device-opengl_es.cpp` | Can stay backend-private with the Android context once `device-opengl_es.cpp` includes the private Android context header on Android. |

Suggested Android migration order:

1. Add Android bridge declarations to the public GLES header, but only as narrow functions. Keep the concrete context public during this step.
2. Convert non-render file/asset users first: `file-util.cpp`, `audio-opensl-android.cpp`, and `third-party/lsqlite3/asset-pkg.cpp`.
3. Convert Android Lua JNI users next: `framework-android-lua.cpp`, `manager-android-lua.cpp`, and any Android-only Lua helpers. These should call bridge functions for doCommands, key mapping, dialogs, and string safety.
4. Convert Android platform bootstrap: `platform-android/main.cpp`, `platform-android/main-lua.cpp`, and `platform-android/main-native-activity.cpp` should set paths, asset manager, native window, class loader, and active `JNIEnv*` through bridge functions.
5. Move the concrete Android context layout to `src/core_mbm/specific-opengl_es-android-context.h`, include it only from Android backend/platform implementation files, and leave only forward declarations plus bridge functions in `include/core_mbm/specific-opengl_es.h`.

Validation requirement:

- Each Android bridge group should be a separate milestone and must be tested with the full Android command before proceeding.
- Linux/MSVS/macOS should also be checked after the public header changes because `specific-opengl_es.h` is shared by all GLES builds.

Milestone 80 implementation note:

- Added the first narrow Android GLES bridge functions to public `include/core_mbm/specific-opengl_es.h`: `androidGetAbsPath()`, `androidAbsPathEndsWithSlash()`, `androidAddPath()`, `androidCopyFileFromAsset()`, and `androidGetAssetManager()`.
- Implemented those bridge functions in `src/core_mbm/specific-android.cpp`, where they still delegate to the current concrete Android `SPECIFIC_AUX_CONTEXT_DEVICE` layout.
- Migrated non-render path/asset consumers away from direct Android context layout access:
  - `src/core_mbm/file-util.cpp`
  - `third-party/lsqlite3/asset-pkg.cpp`
  - `src/core_mbm/audio-opensl-android.cpp`
- Removed now-unneeded `device.h` includes from those consumers where they were only used to reach `DEVICE::getSpecificContextDevice()`.
- Kept Android JNI class-cache, `JNIEnv*`, native-window/EGL, and texture-filter context access unchanged for later milestones.
- This reduces direct Android context coupling in file/asset code without moving the Android context layout yet.

Milestone 81 implementation note:

- Added Android Lua/JNI bridge functions beside the existing Android path/asset bridge:
  - `androidRequestQuit()`
  - `androidGetKeyCode()`
  - `androidGetKeyName()`
  - `androidGetIdiom()`
  - `androidGetUserName()`
  - `androidSaveFile()`
  - `androidRequestOpenFile()`
  - `androidShowMessageBox()`
  - `androidOpenFolder()`
- Implemented those functions in `src/core_mbm/specific-android.cpp`, where JNI class-cache, `JNIEnv*`, `jstring`, and Android configuration details still belong.
- Migrated `src/lua-wrap/framework-android-lua.cpp` away from direct Android context/JNI layout access for Lua quit, key mapping, idiom, username, save/open dialog, message box, open folder, path source, and include-file asset copy.
- Left Android EGL/native-window/bootstrap context access unchanged for later milestones.
- This milestone keeps Lua behavior stable while removing the Lua wrapper's dependency on Android-specific context fields.

Milestone 82 implementation note:

- Added Android EGL/native-window bridge functions:
  - `androidReleaseGraphicsContext()`
  - `androidEnsureEGLSurface()`
  - `androidSwapBuffers()`
  - `androidStoreTextureFilters()`
- Moved Android EGL display/config/surface/context creation and resume logic from `src/core_mbm/core-manager-opengl_es_android.cpp` into `src/core_mbm/specific-android.cpp`.
- Migrated Android `CORE_MANAGER::ReleaseGraphics()`, `initGraphics()`, and Android `swapBuffers()` away from direct EGL/native-window/filter context field access.
- Kept plugin subscription JNI handle access and Android platform bootstrap context population unchanged for later milestones.
- This isolates Android EGL/native-window lifecycle from core-manager code while preserving the current NativeActivity behavior.

Milestone 83 implementation note:

- Added `androidGetPluginSubscribeHandle()` as the Android plugin subscription bridge.
- Migrated the Android branch in `CORE_MANAGER::addPlugin()` away from direct `SPECIFIC_AUX_CONTEXT_DEVICE` and `JNIEnv*` access.
- The value passed to `PLUGIN::onSubscribe()` remains the current Android `JNIEnv*` handle for compatibility, but `CORE_MANAGER` no longer depends on the Android context layout.
- Kept platform bootstrap context population unchanged for a later, separate milestone.

Milestone 84 implementation note:

- Added NativeActivity bootstrap bridge functions:
  - `androidSetRuntimePaths()`
  - `androidSetAssetManager()`
  - `androidSetNativeWindow()`
  - `androidAttachNativeActivityThread()`
  - `androidCreateActivityGlobalRef()`
  - `androidDeleteGlobalRef()`
  - `androidCallActivityDoCommands()`
- Migrated `platform-android/main-native-activity.cpp` away from direct `SPECIFIC_AUX_CONTEXT_DEVICE` field writes for paths, asset manager, native window, JNI env, class loader setup, and global activity reference deletion.
- `android_command_handler()` now calls the Android bridge instead of using `JNIEnv*` directly.
- The exported `Java_com_mini_mbm_MbmActivity_nativeOnCallBackCommands()` callback still has JNI parameters by ABI requirement, but it does not access the Android context layout.
- Legacy Android entry points (`platform-android/main.cpp`, `platform-android/main-lua.cpp`, and `platform-android/scene-1.cpp`) still need a separate audit if they remain supported.

Milestone 85 implementation note:

- Added legacy Android JNI/context bridge helpers:
  - `androidGetJNIEnv()`
  - `androidSetJNIEnv()`
  - `androidCacheJavaClasses()`
- Migrated legacy Android entry points away from direct `SPECIFIC_AUX_CONTEXT_DEVICE` access:
  - `platform-android/main.cpp`
  - `platform-android/main-lua.cpp`
  - `platform-android/scene-1.cpp`
- Kept JNI function signatures and Java native registration unchanged because those files are JNI ABI entry points.
- Current CMake Android builds use `platform-android/main-native-activity.cpp`; these legacy files are not part of the active Android target, so validation is by static scan plus the active Android build.

Milestone 86 implementation note:

- Moved the concrete Android OpenGL ES `SPECIFIC_AUX_CONTEXT_DEVICE` layout out of public `include/core_mbm/specific-opengl_es.h` into private `src/core_mbm/specific-opengl_es-android-context.h`.
- Removed Android-only `jni.h`, `android/asset_manager.h`, `android/native_window.h`, and `std::string` requirements from the public GLES header.
- Kept public Android bridge APIs in `specific-opengl_es.h` with backend-neutral pointer signatures where platform handles cross the boundary.
- Added `androidGetImageDataFromDroid()` and migrated `TEXTURE::loadFromAndroid()` away from direct Android context layout access.
- `src/core_mbm/specific-android.cpp` and `src/core_mbm/device-opengl_es.cpp` are now the Android implementation files that include the private Android context layout.
- The new private header has been staged immediately because it is a new backend-private implementation file.

Milestone 87 audit note:

- Confirmed the Android public-header context split from Milestone 86: public `include/core_mbm/specific-opengl_es.h` now forward-declares Android `SPECIFIC_AUX_CONTEXT_DEVICE` and no longer includes Android-only `jni.h`, `android/asset_manager.h`, `android/native_window.h`, or `std::string`.
- Confirmed Android texture loading now uses `androidGetImageDataFromDroid()` instead of direct concrete context access.
- Remaining public backend-header leakage is not Android context layout anymore:
  - `specific-opengl_es.h` still intentionally exposes EGL/GLES types/macros as the OpenGL ES backend utility header.
  - `d3dx9-mingw.h` was still a public DirectX/MinGW compatibility shim at this point. Milestone 89 moves it behind the private DirectX9 selector.
  - `time-control.h` still includes `windows.h` under `_WIN32`; this is a small public Windows dependency unrelated to Android and is addressed by Milestone 88.
- At this point, moving all backend-private headers to `src/core_mbm/private/` was intentionally deferred until Windows/macOS reviewed the Android split. Milestone 94 completes that structure-only move after the platform matrix passed.

Milestone 88 implementation note:

- Removed the public `windows.h` include from `include/core_mbm/time-control.h`.
- The Windows timing branch only needs `_timeb` and `_ftime_s`, which are declared by `sys/timeb.h`; no Win32 API type is required in this header.
- This keeps `DEVICE` and any other `TIME_CONTROL` consumer from inheriting an avoidable Windows SDK dependency through a public core header.
- This is a header-boundary cleanup only; timing storage, behavior, and public API remain unchanged.

Milestone 89 implementation note:

- Moved the MinGW-only D3DX9 compatibility shim from public `include/core_mbm/d3dx9-mingw.h` to private `src/core_mbm/d3dx9-mingw.h`.
- Updated the private DirectX9 D3DX selector `src/core_mbm/specific-directx9-d3dx.h` to include the shim locally only for MinGW/Cygwin.
- Updated the MSVS project and filter references to the new private file location so the solution does not point at a removed public header.
- MSVS behavior remains unchanged: `_MSC_VER` still includes the standard `d3dx9.h` and links `d3dx9.lib`; the compatibility shim remains MinGW/Cygwin-only.

Milestone 90 implementation note:

- Added public `include/core_mbm/android-bridge.h` for Android bridge functions that do not require consumers to include the OpenGL ES backend utility header.
- `include/core_mbm/specific-opengl_es.h` includes the Android bridge on Android for source compatibility, but it no longer owns the `android*` bridge declarations directly.
- Migrated Android platform entry points, Android Lua wrappers, file/asset helpers, OpenSL Android audio, Android texture loading, and the lsqlite3 asset package helper to include `android-bridge.h` when they only need Android bridge functions.
- Removed an unnecessary GLES-header include from the Android sample scene that did not use any GLES or Android bridge symbols.
- This reduces accidental EGL/GLES exposure in Android non-render code while leaving GL wrapper macros and real GLES backend files untouched.

Milestone 91 implementation note:

- Removed unnecessary `specific-opengl_es.h` includes from non-render Lua framework wrappers:
  - `src/lua-wrap/framework-linux-lua.cpp`
  - `src/lua-wrap/framework-windows-lua.cpp`
  - `src/lua-wrap/framework-directx-lua.cpp`
- The Linux wrapper now includes `strings.h` directly for `strcasecmp()` instead of relying on unrelated include chains.
- The Windows and DirectX wrappers now include `platform/mismatch-platform.h` for the Win32/string-compatibility surface they actually use, instead of pulling in EGL/GLES through the OpenGL ES backend utility header.
- This keeps Lua platform wrappers tied to platform APIs only, not to a renderer backend header.

Milestone 92 implementation note:

- Moved the OpenGL ES backend utility header from public `include/core_mbm/specific-opengl_es.h` to private `src/core_mbm/specific-opengl_es.h`.
- Updated CMake to keep both public `include/core_mbm` and private `src/core_mbm` on the internal engine include path.
- Updated the Android private context include and MSVS project/filter references to the new private header location.
- After Milestones 90 and 91, the moved header is only used by OpenGL ES backend implementation files and private GLES resource/context headers, so EGL/GLES types and GL debug macros are no longer exposed through a public core header.

Future private-header organization note:

- The backend-private headers now live under `src/core_mbm/private/`.
- CMake and MSVS private include paths are updated together so internal engine files outside `src/core_mbm/`, such as `src/render/` and `src/platform/`, can include private backend headers by name instead of fragile relative paths.

Milestone 93 validation/status note:

- User confirmed the current branch builds on MSVS, macOS, MinGW, and iOS after Milestone 92.
- Android arm64-v8a validation had already passed during the Android/OpenGL ES split.
- Public SDK include/concrete-layout scan is clean for `include/core_mbm`, `include/render`, and `include/lua-interface`. The broad symbol scan only finds the `plugin-callback.h` documentation comment that explains the opaque `void * renderDevice` ABI.
- Backend/platform public-header isolation is complete for the original PIMPL goal. Remaining work is private-header organization or optional ABI/header hygiene, not Android/OpenGL ES public leakage.

Milestone 94 implementation note:

- Moved backend-private headers from flat `src/core_mbm/` into `src/core_mbm/private/`: all private `specific-*.h` headers plus the MinGW-only `d3dx9-mingw.h` shim.
- Kept public compatibility/bridge headers in `include/core_mbm/`.
- Added `src/core_mbm/private/` to internal CMake and MSVS include paths.
- Replaced relative private-header includes in `src/platform/win32-platform.cpp` and render-to-texture backend files with private include-path based includes.
- This is a structure-only cleanup after the backend PIMPL split; it should not change engine API or runtime behavior.

Milestone 95 scope/closure note:

- User confirmed the `src/core_mbm/private/` version builds on macOS, iOS, Windows, Android, and Linux.
- The original backend/OS isolation scope is closed: isolate OS/backend SDK types, backend-owned handles, and concrete platform/backend layouts from public headers.
- After reviewing the remaining public layout, the next PIMPL scope is expanded to ABI/header hygiene for selected manager/helper classes.
- This second scope is not about hiding direct DirectX/OpenGL ES/Metal/Win32/macOS SDK types. It is about reducing public class layout churn, STL-heavy public includes, and unnecessary recompilation when manager internals change.

Milestone 96 planning note:

- Reopen `TEXTURE_MANAGER`, `MESH_MANAGER`, `EFFECT_SHADER`, and maybe `ANIMATION_BACKUP` for future PIMPL work, but do it as staged ABI/header hygiene, not as one broad migration.
- Start with `TEXTURE_MANAGER`: its singleton/cache/path/capability state is contained, and `TEXTURE` already has the `BackendData` precedent.
- Consider `ANIMATION_BACKUP` next if we want a contained cleanup inside `animation.h`; its nested backup structs are private implementation detail and good candidates for an opaque `Impl`.
- Treat `EFFECT_SHADER` as a later accessor-policy milestone because it exposes gameplay/editor-visible state such as `statusFx`, `typeAnim`, `ptrCurrentShader`, and `timeAnimation`.
- Treat `MESH_MANAGER` as later and higher risk. Moving only the manager cache helps some ABI hygiene, but most header weight is in `MESH_MBM` and mesh file/debug data, so this needs a separate compatibility review.

Milestone 97 implementation note:

- Moved `TEXTURE_MANAGER` singleton implementation state behind `TEXTURE_MANAGER::Impl`.
- The public texture manager header no longer exposes the texture cache `std::unordered_map`, path buffer, or texture capability counters.
- Added narrow private helper methods for backend/common texture manager translation units: cache lookup, cache store, and max texture size lookup.
- Updated common, DirectX9, dummy, OpenGL ES, and Metal texture manager implementations to use those helpers instead of direct manager field access.
- Preserved all public `TEXTURE_MANAGER` method signatures and did not change `TEXTURE` backend handle ownership.

Milestone 98 implementation note:

- Moved `ANIMATION_BACKUP` implementation state behind `ANIMATION_BACKUP::Impl`.
- Removed nested backup structs and vectors from the public `animation.h` layout: `VAR_SHADER_BACKUP`, `FX_BACKUP`, `ANIMATION_STATE`, `lsAnimationState`, `lsFxBackup`, and backup `indexCurrentAnimation`.
- Kept backup/restore behavior and public `ANIMATION_MANAGER` fields unchanged.
- Exported the `ANIMATION_BACKUP` constructor/destructor because the public class now owns an out-of-line `std::unique_ptr<Impl>`.
- This is ABI/header hygiene only; it does not change animation playback API or `RENDERIZABLE`.

Milestone 99 implementation note:

- Moved `EFFECT_SHADER`'s private shader cache map behind `EFFECT_SHADER::Impl`.
- Removed the public-header dependency on `<map>` from `animation.h`.
- Kept public effect state fields unchanged: `statusFx`, `typeAnim`, `ptrCurrentShader`, and `timeAnimation`.
- This is the safe first `EFFECT_SHADER` ABI/header hygiene step; hiding public effect state still requires an accessor policy and call-site migration.

Milestone 100 implementation note:

- Moved only `MESH_MANAGER` singleton cache state behind `MESH_MANAGER::Impl`: mesh cache and fake-release list.
- Removed the public-header dependency on `<unordered_map>` from `mesh-manager.h`.
- Kept `MESH_MBM` and `MESH_MBM_DEBUG` layouts unchanged because those are larger source-compatibility surfaces used by renderers, debug tools, Lua bindings, and mesh file compatibility code.
- This is the lowest-risk `MESH_MANAGER` ABI/header hygiene step; any future mesh cleanup should review `MESH_MBM` separately.

Milestone 101 implementation note:

- Moved the `ANIMATION_MANAGER` restore backup object behind `ANIMATION_MANAGER::Impl`.
- Kept public animation state fields unchanged: `indexCurrentAnimation`, `onEndAnimation`, `onEndFx`, and `lsAnimation`.
- This hides the restore-only backup storage from the public manager layout without changing animation playback, Lua callbacks, render-type update code, or plugin/editor direct animation access.
- Hiding the remaining `ANIMATION_MANAGER` fields still requires a separate accessor migration because many render types, Lua bindings, and the tiled editor use them directly.

Milestone 102 implementation note:

- Moved `CORE_MANAGER` window restore options behind `CORE_MANAGER::Impl`: window border and resize-enabled state.
- Added private `CORE_MANAGER` helpers for backend/common code to store and reuse those options during device restore.
- Kept `LUA_MANAGER` source compatibility by giving it its own launch-option fields for Lua startup argument parsing.
- At this milestone, `device`, `changeScene`, `__sceneWasInit`, and `keyCapsLockState` still remained public because platform/Lua compatibility code read them directly.
- This is a narrow `CORE_MANAGER` layout cleanup and does not change window creation arguments, lost-device restore behavior, or gameplay API.

Milestone 103 implementation note:

- Moved `CORE_MANAGER::changeScene` behind `CORE_MANAGER::Impl`.
- Added private `CORE_MANAGER` helpers for common/backend code to read and update the scene-change flag.
- Kept `device`, `__sceneWasInit`, and `keyCapsLockState` public because platform, Lua, and plugin compatibility code still reads them directly.
- This is a narrow scene-transition state cleanup and does not change scene loading, unloading, or swap-step behavior.

Milestone 104 implementation note:

- Moved `CORE_MANAGER::keyCapsLockState` behind `CORE_MANAGER::Impl`.
- Added exported `CORE_MANAGER::isKeyCapsLockOn()` for Lua/plugin consumers and a private setter for backend event code.
- Migrated Lua framework and ImGui plugin Caps Lock reads to the getter.
- Kept `device` and `__sceneWasInit` public because platform/Lua compatibility code still reads them directly.
- This is a narrow input-state layout cleanup and does not change Caps Lock behavior.

Milestone 105 implementation note:

- Moved `CORE_MANAGER::__sceneWasInit` behind `CORE_MANAGER::Impl`.
- Added exported `CORE_MANAGER::isSceneInitialized()` for platform/Lua consumers and a private setter for core/backend code.
- Migrated common event dispatch, backend joystick-info dispatch, and Lua scene-loading checks to the getter.
- Kept `device` public because platform startup, mobile bridges, and Lua/platform code still use it directly.
- This is a narrow scene-lifecycle state cleanup and does not change scene initialization, loading, or finalize behavior.

Milestone 106 implementation note:

- Added exported `CORE_MANAGER::getDevice()` as the compatibility accessor for the remaining public device pointer.
- Migrated the launcher library's repeated `LUA_MANAGER::device` reads to a local `DEVICE *device = luaCore.getDevice();`.
- Kept `CORE_MANAGER::device` public because backend startup, Android/iOS bridges, Lua manager code, and platform glue still use it directly.
- This is a prep milestone before any future broad migration of direct device field access.

Milestone 107 implementation note:

- Migrated Lua manager direct `device` reads to `CORE_MANAGER::getDevice()` or existing local `DEVICE *device` variables.
- Kept the three `pLuaManager->device = DEVICE::getInstance()` constructor assignments because they initialize the compatibility field.
- Updated Lua scene transition helpers to store `device->getScene()` once in a local `SCENE *scene` when the same scene object is reused.
- This is a contained Lua-manager compatibility cleanup and does not hide `CORE_MANAGER::device` yet.

Milestone 108 implementation note:

- Migrated Android Lua bridge direct `game->device` reads in `platform-android/main-lua.cpp` to `game->getDevice()`.
- Stored `device->getScene()` once in `MiniMbmEngine_onCallBackCommands()` and reused the local `scene` pointer for validation and callback dispatch.
- Left the non-Lua Android C++ bridge and native-activity bridge for separate milestones.
- This is a platform glue cleanup only; it does not change Android initialization, JNI paths, or scene callback behavior.

Milestone 109 implementation note:

- Migrated Android non-Lua bridge direct `game->device` reads in `platform-android/main.cpp` to `game->getDevice()`.
- Stored `device->getScene()` once in `MiniMbmEngine_onCallBackCommands()` and reused the local `scene` pointer for validation and callback dispatch.
- Left Android native-activity and iOS bridge direct device reads for separate milestones.
- This is a platform glue cleanup only; it does not change Android JNI initialization, resize handling, or callback behavior.

Milestone 110 implementation note:

- Migrated Android NativeActivity direct `s_game->device` reads in `platform-android/main-native-activity.cpp` to `s_game->getDevice()`.
- Stored `device->getScene()` once in the native callback command bridge and reused the local `scene` pointer for callback dispatch.
- Android platform entry files no longer directly read `CORE_MANAGER::device`.
- Left iOS bridge direct device reads for a separate milestone.
- This is a platform glue cleanup only; it does not change NativeActivity input, window, restore, or callback behavior.

Milestone 111 implementation note:

- Migrated iOS bridge direct `s_game->device` reads in `platform-ios/MetalViewController.mm` to `s_game->getDevice()`.
- Stored `device->getScene()` once in the Swift callback command bridge and reused the local `scene` pointer for callback dispatch.
- Android and iOS platform entry files no longer directly read `CORE_MANAGER::device`.
- This is a platform glue cleanup only; it does not change iOS Metal view setup, render loop, resize handling, or callback behavior.

Milestone 112 implementation note:

- Migrated `CORE_MANAGER::onLoop()` internal device reads to one local `DEVICE *device = this->getDevice();`.
- Added a local `getInitializedScene` helper inside `onLoop()` so repeated event dispatch reads `device->getScene()` once per dispatch path.
- Kept `CORE_MANAGER::device` public because constructors and backend initialization still assign/read the compatibility field directly.
- This is an internal accessor-consistency cleanup only; it does not change loop, resize, plugin, or event dispatch behavior.

Milestone 113 implementation note:

- Migrated `CORE_MANAGER::onStopCoreManager()` and `CORE_MANAGER::update()` internal device reads to local `DEVICE *device = this->getDevice();` variables.
- Kept render-list iteration, pause-state capture, FPS update, camera scale cache, and plugin `onLoop()` behavior unchanged.
- This is an internal accessor-consistency cleanup only; it does not change stop, update, plugin, or render-list behavior.

Milestone 114 implementation note:

- Migrated `CORE_MANAGER::updateAudio()` and `CORE_MANAGER::updatePhysis()` internal device reads to local `DEVICE *device = this->getDevice();` variables.
- Stored `device->getScene()` once in `updatePhysis()` before using the scene id for physics filtering.
- Kept audio-manager update, physics count iteration, scene filtering, and physics delta behavior unchanged.
- This is an internal accessor-consistency cleanup only; it does not change audio or physics update behavior.

Milestone 115 implementation note:

- Migrated remaining `CORE_MANAGER::render()` internal device reads to the existing local `device` pointer.
- Kept render-list references, frustum preparation, camera update, plugin render callbacks, and render counters unchanged.
- This is an internal accessor-consistency cleanup only; it does not change render ordering or render-to-target behavior.

Milestone 116 implementation note:

- Migrated `CORE_MANAGER::_updateDimFrustum()` and `CORE_MANAGER::adjustScaleScreen2d()` internal device reads to local `DEVICE *device = this->getDevice();` variables.
- Kept camera reference handling, back-buffer reads, frustum probing, and 2D scale/stretch calculations unchanged.
- This is an internal accessor-consistency cleanup only; it does not change camera, frustum, or screen-scale behavior.

Milestone 117 implementation note:

- Migrated `CORE_MANAGER::reinitTimers()`, `CORE_MANAGER::enableRender()`, and `CORE_MANAGER::disableRender()` internal device reads to local `DEVICE *device = this->getDevice();` variables.
- Kept timer reset/resume behavior and render-list enable/disable filtering unchanged.
- This is an internal accessor-consistency cleanup only; it does not change scene render visibility behavior.

Milestone 118 implementation note:

- Migrated `CORE_MANAGER::logic()` internal device reads to a local `DEVICE *device = this->getDevice();` variable.
- Stored `device->getScene()` once in a local `SCENE *scene` and refreshed it after `device->setScene(...)` can change the current scene.
- Kept scene finalize, plugin destroy, scene transition, loading, init, end-loading, and per-frame scene loop behavior unchanged.
- This is an internal accessor-consistency cleanup only; it does not change scene lifecycle behavior.

Milestone 119 implementation note:

- Migrated `CORE_MANAGER::onLostDevice()` internal device reads to one local `DEVICE *device = this->getDevice();` variable.
- Reused that local device for camera restore state, render-list restore batches, restore progress callbacks, and final resume handling.
- Kept the restore step machine, object restore order, progress percentage behavior, and swap-buffer behavior unchanged.
- This is an internal accessor-consistency cleanup only; it does not change lost-device restore behavior.

Milestone 120 implementation note:

- Migrated `CORE_MANAGER::forceRestore()` and touch coordinate helpers to local `DEVICE *device = this->getDevice();` variables.
- Reused those local devices for back-buffer/window-position restore arguments and `CAMERA` scale reads in `onTouchDown()`, `onTouchUp()`, `onTouchMove()`, and `onDoubleClick()`.
- Kept forced restore looping, event coordinate scaling, event types, and event queue behavior unchanged.
- This is an internal accessor-consistency cleanup only; it does not change forced restore or input event behavior.

Milestone 121 implementation note:

- Migrated `CORE_MANAGER::initEnableRenders()` internal device reads to a local `DEVICE *device = this->getDevice();` variable.
- Reused that local device for the 3-D, 2-D screen, and 2-D world render-list initialization passes.
- Kept the render-list order and `enableRender = false` initialization behavior unchanged.
- This is an internal accessor-consistency cleanup only; it does not change render visibility initialization behavior.

Milestone 122 implementation note:

- Migrated `CORE_MANAGER::setScene()` internal device write to a local `DEVICE *device = this->getDevice();` variable.
- Kept the `DEVICE::setScene(currentScene)` assignment path unchanged.
- This is an internal accessor-consistency cleanup only; it does not change scene ownership or scene transition behavior.

Milestone 123 implementation note:

- Migrated the `CORE_MANAGER::pushEvent(EVENT_KEY*)` and `CORE_MANAGER::pushEvent(INFO_JOYSTICK_INIT_PLAYER*)` scene guards to local `DEVICE *device = this->getDevice();` variables.
- Kept the scene-initialized checks, event coalescing, joystick-info queueing, and mutex behavior unchanged.
- This is an internal accessor-consistency cleanup only; it does not change event filtering or queue behavior.

Milestone 124 implementation note:

- Migrated OpenGL ES `CORE_MANAGER::renderToTargets()` device reads to a local `DEVICE *device = this->getDevice();` variable.
- Stored `device->getCamera()` once in a local `CAMERA &camera` for repeated projection restore updates.
- Kept render-target iteration, framebuffer binding, viewport restore, camera restore dimensions, and failure behavior unchanged.
- This is an internal accessor-consistency cleanup only; it does not change OpenGL ES render-to-target behavior.

### Phase 3 - Hide renderer backend handles

Order:

1. `TEXTURE::idTexture/ptrTexture` - done
2. `BUFFER_GL::bs` - done
3. `SHADER::ptrShaderSpecific` - done
4. `RENDERIZABLE_TO_TARGET::specificConfig` - done
5. `DEVICE::specificContextDevice` - done

This phase removes most backend leakage while keeping gameplay-facing APIs mostly unchanged.

### Phase 4 - PIMPL manager/helper internals

This phase is now a future ABI/header hygiene scope. It is separate from the completed backend/OS isolation scope.

Do not move a manager only because it has private STL state if the change adds risk without a clear benefit. Move it when hiding the private layout reduces public dependency weight, ABI churn, or accidental coupling to implementation details.

Manager backend-leakage pre-audit:

| Candidate | Exposes explicit OS/backend SDK types in the public header? | Current issue | Suggested priority |
|---|---|---|---|
| `TEXTURE_MANAGER` | No direct DirectX/OpenGL ES/Metal/Win32/macOS types. `TEXTURE` backend handles are already behind `BackendData`. | Manager cache/path/capability storage moved behind `Impl`. Public TTF API still exposes `stbtt_aligned_quad` by signature. | Done for manager private layout. Further texture header cleanup would require a public TTF API decision. |
| `ANIMATION_BACKUP` | No OS/backend SDK types. | Backup vectors and nested backup structs moved behind `Impl`. | Done for backup private layout. |
| `EFFECT_SHADER` | No OS/backend SDK types. | Private shader cache map moved behind `Impl`; public state fields remain visible and are used by core/plugins/editor. | Partly done. Hiding public effect state still requires accessor policy for `statusFx`, `typeAnim`, `ptrCurrentShader`, and `timeAnimation`. |
| `MESH_MANAGER` | No direct DirectX/OpenGL ES/Metal/Win32/macOS SDK types. It still carries legacy GLES-named engine value types such as `MATERIAL_GLES`, but those are engine/file-format structs, not backend handles. | Manager cache/fake-release state moved behind `Impl`. Header still exposes large `MESH_MBM` and `MESH_MBM_DEBUG` layouts. | Done for manager private layout. Any future mesh cleanup must review `MESH_MBM`/debug compatibility separately. |

Future ABI/header hygiene could move private containers and counters into `Impl` for:

- `TEXTURE_MANAGER`, done for manager cache/path/capability layout.
- `ANIMATION_BACKUP`, done for backup nested structs/vectors.
- `EFFECT_SHADER`, private shader cache map done; public effect state requires accessors before hiding.
- `MESH_MANAGER`, done for singleton cache/fake-release layout; `MESH_MBM` and debug layouts remain future work.
- `ANIMATION_MANAGER`, restore backup object done; animation list/index/callback fields remain public pending accessor migration.
- `CORE_MANAGER`, window restore options, scene-change flag, Caps Lock state, scene-initialized flag, and early lifecycle/update/audio/physics/render/camera/timer/render-enable/render-init/scene-assignment/event-queue/logic/restore/input-coordinate/OpenGL-ES-render-target accessor cleanup done; `getDevice()` compatibility accessor added, Lua manager reads migrated, and Android/iOS platform reads migrated before any broader `device` field migration.

This mainly improves header hygiene and ABI layout. It is intentionally separate from the completed backend/OS isolation scope.

Current stop rule:

- Backend/OS isolation is complete when a public header has no direct OS/backend SDK type, no concrete backend-owned handle, and no concrete platform/backend layout.
- ABI/header hygiene can continue separately for selected managers/helper classes when hiding the private layout has clear value.
- Do not migrate public gameplay ergonomics only to make the code "more PIMPL".

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

## Future work to complete strict PIMPL

This is future work only. It is not required for the completed OS/backend isolation goal.

1. Define a compatibility policy: decide whether public fields remain supported or whether a breaking API cleanup is acceptable.
2. Move selected backend-neutral manager/helper internals behind `Impl`, with `TEXTURE_MANAGER`, `ANIMATION_BACKUP`, the private `EFFECT_SHADER` shader cache, `MESH_MANAGER` singleton cache, the `ANIMATION_MANAGER` restore backup object, selected `CORE_MANAGER` flags, and early `CORE_MANAGER::getDevice()` compatibility migrations done.
3. Add complete accessor/mutator coverage for `EFFECT_SHADER`, `RENDERIZABLE`, `SCENE`, `ANIMATION_MANAGER`, remaining `CORE_MANAGER` state, and any manager state that external code currently reads.
4. Migrate engine internals, Lua bindings, plugins, examples, editor tools, and platform samples from direct field access to the stable API.
5. Move remaining backend-neutral private containers into `Impl` only after call sites no longer depend on their layout.
6. Split public and private headers further where STL-heavy internals still leak compile dependencies.
7. Revisit plugin ABI separately if `PLUGIN::onSubscribe(void *context, void *renderDevice)` ever needs a stable cross-version ABI instead of opaque backend handles.
8. Decide whether value-heavy inheritance such as `DEVICE : public TIME_CONTROL, public FRUSTUM` should remain source-compatible or move behind composition.

## Not worth PIMPL first

These can stay as normal public value/API types unless there is a specific ABI goal:

- `VEC2`, `VEC3`, `MATRIX`, `COLOR`, and other primitive math/value structs.
- Small enums such as `TYPE_CLASS`, `BLEND_STATE`, `TYPE_ANIMATION`.
- File-format structs in `header-mesh.h`, unless the mesh format itself is being redesigned.
- `PLUGIN` virtual interface, unless plugin ABI stability becomes a formal goal.

## Summary

The core has completed the original backend/OS PIMPL scope: backend resource-handle cleanup, public backend/platform header isolation, and private backend-header organization without touching gameplay-facing `RENDERIZABLE` state.

Current decision:

1. Continue with selected ABI/header hygiene even when the header has no direct DirectX/OpenGL ES/Metal/Win32/macOS SDK type.
2. `TEXTURE_MANAGER` manager internals are now behind `Impl`.
3. `ANIMATION_BACKUP` backup internals are now behind `Impl`.
4. `EFFECT_SHADER` private shader cache is now behind `Impl`; public effect state remains pending accessor policy.
5. `MESH_MANAGER` singleton cache/fake-release internals are now behind `Impl`; `MESH_MBM` and debug mesh layouts remain separate future work.
6. `ANIMATION_MANAGER` restore backup storage is now behind `Impl`; public animation list/index/callback state remains pending accessor migration.
7. `CORE_MANAGER` window restore options, scene-change flag, Caps Lock state, and scene-initialized flag are now behind `Impl`; `getDevice()` exists and Lua/Android/iOS platform reads plus early lifecycle/update/audio/physics/render/camera/timer/render-enable/render-init/scene-assignment/event-queue/logic/restore/input-coordinate/OpenGL-ES-render-target helpers use it, but `device` remains a public compatibility field.
8. Keep direct gameplay public fields as convenience API unless a deliberate future strict-PIMPL cleanup is chosen.

For the original PIMPL goal of hiding OS/backend dependencies from public headers, the work is complete. The next work is ABI/header hygiene, not backend isolation.
