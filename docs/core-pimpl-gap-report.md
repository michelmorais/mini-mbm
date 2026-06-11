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
| `DEVICE::specificContextDevice` | Opaque by type only | The concrete type is forward-declared in `device.h`, but the pointer is public and engine/platform code reaches through it directly. |
| Shader resources | Partly hidden | Built-in shader code is hidden behind functions, but runtime shader/buffer objects still expose backend handles. |
| Manager caches | Private but not PIMPL | Several managers keep private containers in public headers. That hides access, but still exposes layout and forces container includes. |

## Main gaps

### 1. Public data members are the largest blocker

Several core classes expose mutable state as part of the public API. Hiding these fields without a compatibility layer would break a large amount of engine code, platform samples, Lua wrapping, and likely external game code.

High-impact examples:

| Header | Public state that blocks strict PIMPL |
|---|---|
| `include/core_mbm/device.h` | `verbose`, `run`, `backBufferWidth`, `backBufferHeight`, `colorClearBackGround`, `camera`, render counters, `cfg`, global dynamic vars, `specificContextDevice`, `scene`, `orderRender`, window position. |
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
| `include/core_mbm/device.h` | `SPECIFIC_AUX_CONTEXT_DEVICE *specificContextDevice` | `DEVICE::BackendData` or `DEVICE::Impl`, with backend helper functions in `.cpp` files. |
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

- `DEVICE`: camera, screen size, run/error flags, dynamic globals, render stats, background clear color.
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
