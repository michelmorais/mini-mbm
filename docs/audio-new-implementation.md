# Audio System — Architecture & New Backend Implementation Guide

This document describes the complete audio subsystem of the mini-mbm engine:
how it is structured, how it flows from Lua script or C++ through to the
platform audio API, what invariants must be preserved at every layer, and the
exact checklist to follow when adding a new backend (e.g. Xbox/PlayStation).

---

## 1. File Map

| File | Purpose |
|---|---|
| `include/core_mbm/audio-interface.h` | `AUDIO_INTERFACE` base class + `AUDIO_MANAGER_INTERFACE` base class |
| `include/core_mbm/audio.h` | Backend-neutral `AUDIO` concrete class declaration + `AUDIO_MANAGER` class declaration. Backend state is hidden behind `AUDIO::BackendData` and implemented only in the selected `audio-*.cpp`. |
| `src/core_mbm/audio-manager.cpp` | Platform-agnostic lifecycle: `load()`, `destroy()`, `destroyNow()`, `update()`, `release()`. It delegates backend init/final/update through private hooks implemented by the selected backend. |
| `src/core_mbm/audio-interface.cpp` | `AUDIO_INTERFACE` constructor/destructor (tiny). |
| `src/core_mbm/audio-direct-sound.cpp` | Backend: DirectSound 8 (Windows). Guard: `AUDIO_ENGINE_DIRECT_SOUND_8` |
| `src/core_mbm/audio-portaudio.cpp` | Backend: PortAudio (Linux default / cross-platform). Guard: `AUDIO_ENGINE_PORT_AUDIO` |
| `src/core_mbm/audio-opensl-android.cpp` | Backend: OpenSL ES (Android, current). Guard: `AUDIO_ENGINE_ANDROID_OPENSL` |
| `src/core_mbm/audio-avfoundation.mm` | Backend: AVFoundation (macOS / iOS). Guard: `AUDIO_ENGINE_AVFOUNDATION` |
| `src/core_mbm/audio-jni-android.cpp` | *(removed)* — was the legacy Android JNI / MediaPlayer backend. |
| `src/core_mbm/audio-none.cpp` | Stub backend (no audio). Guard: `AUDIO_ENGINE_NONE` |
| `src/lua-wrap/audio-lua.cpp` | Lua binding layer. Registers `audio` global, manages Lua userdata lifetime. |
| `assets/logic/audio_manager.lua` (game) | High-level Lua audio manager: sound pools, music/sfx distinction, `release()`. |

---

## 2. Class Hierarchy

```
AUDIO_MANAGER_INTERFACE   (abstract — interface only)
    └── AUDIO_MANAGER     (audio-manager.cpp — platform-neutral lifecycle)

AUDIO_INTERFACE           (abstract — interface only)
    └── AUDIO             (audio.h declared, selected audio-*.cpp implements methods and BackendData)
```

`AUDIO_MANAGER` owns a `std::vector<AUDIO*> audios` (live) and
`std::vector<AUDIO*> audiosToDelete` (deferred deletion queue).
Backend-specific manager setup/teardown and per-frame polling live behind
`AUDIO_MANAGER::initializeBackend()`, `finalizeBackend()`, and `updateBackend()`.

---

## 3. CMake Backend Selection

Pass `-DAUDIO=<value>` at configure time:

| `-DAUDIO=` value | Preprocessor macro defined | Default on |
|---|---|---|
| `opensl` | `AUDIO_ENGINE_ANDROID_OPENSL` | Android |
| `avfoundation` | `AUDIO_ENGINE_AVFOUNDATION` | macOS, iOS |
| `portaudio` | `AUDIO_ENGINE_PORT_AUDIO` | Linux, Windows |
| `none` | `AUDIO_ENGINE_NONE` | explicit opt-out |
| ~~`jni`~~ | ~~`AUDIO_ENGINE_JNI`~~ | *(removed)* |

The selection logic lives in `src/core_mbm/CMakeLists.txt` (lines 58–82 and 214–254).

---

## 4. Object Lifecycle — The Golden Rules

These rules apply to **every** backend. Violating them is the source of the
majority of audio bugs encountered in the engine history.

### 4.1 Creation path

```
Lua: audio:new(file, inMemory, play, loop)
  → audio-lua.cpp: onNewAudioLua()
      → AUDIO_MANAGER::load(file, loop, inMemory)
          → searches audios[] for a stopped, same-filename instance (reuse)
          → searches audiosToDelete[] for a stopped, same-filename instance (resurrect)
          → if none: new AUDIO(idScene)  ← constructor initializes per-instance backend state
          → calls AUDIO::load(file, loop, inMemory)
              → returns false  →  delete my_audio  ← ~AUDIO() frees backend resource
              → returns true   →  audios.push_back(my_audio)
          → returns AUDIO* (or nullptr)
      → wraps AUDIO* in a Lua table with metatable "_mbmAudio"
      → stores AUDIO** as raw userdata at table[1]
```

### 4.2 Deferred deletion path (normal GC / scene-change)

```
Lua GC or scene unload:
  → __gc metatable → onDestroyAudioLua()
      → onReleaseAudioLua()
          → deletes USER_DATA_AUDIO_LUA (Lua callback refs)
          → AUDIO_MANAGER::destroy(audio)
              → stop()
              → moves audio from audios[] → audiosToDelete[]
  → AUDIO_MANAGER::update() [called every frame]
      → if audio->idScene is gone AND scene no longer exists:
          → delete audio  ←  ~AUDIO() frees backend resource
```

### 4.3 Immediate deletion path (audio:destroy() within same scene)

```
Lua: tSound:destroy()
  → audio-lua.cpp: onForceDestroyAudioLua()
      → getAudioUDFromRawTable() — gets AUDIO**
      → AUDIO_MANAGER::setPersist(audio, false)
      → onReleaseAudioLua()  [cleans up Lua side]
          → AUDIO_MANAGER::destroy(audio)  [moves to audiosToDelete[]]
      → AUDIO_MANAGER::destroyNow(audio)
          → removes from audios[] and audiosToDelete[]
          → delete audio  ←  ~AUDIO() frees backend resource IMMEDIATELY
      → *ud = nullptr  ←  poisons the userdata pointer so __gc is a no-op
```

### 4.4 The invariant that must NEVER be broken

> `~AUDIO()` is the **one and only place** that calls backend resource teardown
> (e.g. `opensl_release_engine()`, `(*playerObj)->Destroy()`, etc.).
>
> `AUDIO::load()` failure paths must **never** call backend teardown.
> `AUDIO_MANAGER::load()` calls `delete my_audio` on failure, which triggers
> `~AUDIO()`, which does the teardown. Calling teardown inside `load()` as
> well causes a double-decrement / double-free.

---

## 5. Audio Lifecycle Flow (visual)

```
  [Lua script]
       │
       │ audio:new(file, inMemory, play, loop)
       ▼
  [audio-lua.cpp] onNewAudioLua()
       │
       │ AUDIO_MANAGER::load()
       ▼
  [audio-manager.cpp]
  ┌────────────────────────────────────────────┐
  │  Search audios[]        → reuse if stopped │
  │  Search audiosToDelete[] → resurrect       │
  │  new AUDIO(idScene)                        │
  │  AUDIO::load(file, loop, inMemory)         │──→ false → delete → ~AUDIO() cleans backend
  │  audios.push_back(audio)                   │
  └────────────────────────────────────────────┘
       │
       │ returns AUDIO*
       ▼
  [audio-lua.cpp] wraps in Lua table {metatable="_mbmAudio", [1]=AUDIO**}
       │
       │ Lua script holds reference
       ▼
  [AUDIO::play(loop)] / [AUDIO::stop()] / [AUDIO::pause()] / [AUDIO::setVolume()]
       │  (delegated to backend)
       ▼
  [BACKEND] (OpenSL ES / AVFoundation / PortAudio / DirectSound)


  Deletion (normal scene change):
  [Lua GC __gc] → onReleaseAudioLua → AUDIO_MANAGER::destroy() → audiosToDelete[]
  [update() loop] → delete audio when scene gone → ~AUDIO() → backend cleanup

  Deletion (immediate, same scene):
  [audio:destroy()] → destroyNow() → delete audio immediately → ~AUDIO() → backend cleanup
```

---

## 6. Backend-Specific Notes

### 6.1 OpenSL ES (Android — `audio-opensl-android.cpp`)

- **Engine singleton**: `g_engineObj / g_engineIf / g_outputMixObj` are
  global, ref-counted via `g_refCount`. One engine is shared by all `AUDIO`
  instances.
- **Player cap**: `g_playerCount` / `MAX_PLAYERS = 32`. OpenSL ES has a hard
  system limit of ~32 concurrent `CreateAudioPlayer` objects. Exceeding it
  returns `SL_RESULT_MEMORY_FAILURE`. `AUDIO::load()` returns `false` when the
  cap is reached; the Lua side falls back to `newFakeAudio()`.
- **EOF replay**: `SL_DATALOCATOR_ANDROIDFD` keeps the FD at EOF after first
  play. `AUDIO::play()` always does `SetPlayState(STOPPED)` → `SetPosition(0)`
  → `SetPlayState(PLAYING)` to rewind before every play.
- **Asset lifetime**: `AAsset*` is stored in `OSLPlayer::asset` and kept open
  for the lifetime of the player. `AAsset_close()` is called in `~AUDIO()`.
- **`SL_IID_SEEK` is required** (not optional). It is listed in the
  `reqs[]` array with `SL_BOOLEAN_TRUE` so that `CreateAudioPlayer` fails
  cleanly if seek is unavailable, rather than returning a player with no seek
  support.

### 6.2 AVFoundation (macOS / iOS — `audio-avfoundation.mm`)

- Uses `AVAudioEngine` + `AVAudioPlayerNode`. No hard player-count limit beyond
  available memory.
- WAV, AIFF, CAF, AU, MP3, AAC/M4A, FLAC are native. OGG/Vorbis uses
  `stb_vorbis` to decode to PCM, then wraps in `AVAudioPCMBuffer`.
- Shared `AVAudioEngine` lifecycle is handled by the backend manager hooks.
  Each `AUDIO` instance stores its `AVAudioPlayerNode` and buffer inside
  `AUDIO::BackendData` in `audio-avfoundation.mm`.
- No EOF rewind issue — `AVAudioPlayerNode` handles seek internally.

### 6.3 PortAudio (Linux / Windows — `audio-portaudio.cpp`)

- Each `AUDIO` instance holds a `std::unique_ptr<PA_INTERFACE>` inside
  `AUDIO::BackendData` in `audio-portaudio.cpp`.
- Format dispatch in `load()` by file extension:
  - `.ogg` / `.oga` → `PA_OGG` (decodes via `stb_vorbis_decode_filename`, always in-memory)
  - everything else → `PA_WAVE` (WAV, supports `inMemory` flag for streaming vs RAM)
- Sample-format mapping in `TranslateFormatType(sampleFormat, bitsPerChannel)`:
  - PCM 8-bit → `paUInt8`, 16-bit → `paInt16`, 24-bit → `paInt24`, 32-bit → `paInt32`
  - IEEE float → `paFloat32`
- Bug fixes applied:
  1. Correct byte-count in stream callbacks (`m_bytesPerSample * frameCount`, not `* channels * frameCount`)
  2. Type-safe volume scaling per `PaSampleFormat`
  3. Linear stereo pan model via `applyPan()`
  4. `m_finished` atomic set before `paComplete`; polled from the backend
     `updateBackend()` hook on the main thread to fire `onEndStreamCallBack`
  5. `stop()` always rewinds via `setPosition(0.0)` before returning
  6. `PA_DATA_FILE::setPosition` / `getPosition` fully implemented (seek by byte offset from `m_dataStart`)
  7. `PA_DATA_MEMORY::setPosition` corrected: `m_index = pos * size` (was inverted)
- `~AUDIO()` destroys `BackendData`, which destroys the `PA_INTERFACE` unique_ptr.

### 6.4 DirectSound 8 (Windows legacy — `audio-direct-sound.cpp`)

- Each `AUDIO` holds a `std::unique_ptr<WaveFile>` and DirectSound buffer inside
  `AUDIO::BackendData` in `audio-direct-sound.cpp`.
- The backend `updateBackend()` hook drives the double-buffer stream pump.
- WAV only.
- `~AUDIO()` destroys `BackendData`, which releases the DirectSound buffer and
  wave reader.

### 6.5 JNI / MediaPlayer (removed)

The legacy JNI / MediaPlayer backend (`audio-jni-android.cpp`) has been
removed. It was superseded by OpenSL ES. See Section 9 for details.

---

## 7. Lua Binding Layer (`audio-lua.cpp`)

### 7.1 Lua object structure

Each `audio:new()` returns a **Lua table** with:
- metatable `_mbmAudio` (has `__gc` = `onDestroyAudioLua`)
- methods: `play`, `stop`, `pause`, `setVolume`, `setPan`, `setPitch`,
  `isPlaying`, `isPaused`, `getVolume`, `getPan`, `getPitch`, `reset`,
  `getLen`, `setPosition`, `onEnd`, `destroy`, `getName`, `setPersistent`,
  `isPersistent`
- raw slot `[1]` = userdata holding `AUDIO**`

The **double pointer** `AUDIO**` is essential: `onForceDestroyAudioLua` sets
`*ud = nullptr` after `destroyNow()` so subsequent `__gc` calls are safe
no-ops even if Lua holds multiple references to the same table.

### 7.2 Key functions

| Function | Triggered by | Behaviour |
|---|---|---|
| `onNewAudioLua` | `audio:new(...)` | Calls `AUDIO_MANAGER::load()`, wraps result |
| `onDestroyAudioLua` | Lua GC `__gc` | Calls `onReleaseAudioLua`; guards against `*ud == nullptr` |
| `onForceDestroyAudioLua` | `tSound:destroy()` | Calls `destroyNow()`, nulls `*ud` |
| `onReleaseAudioLua` | Both above | Cleans up `USER_DATA_AUDIO_LUA`, calls `AUDIO_MANAGER::destroy()` |

### 7.3 `userData` field on `AUDIO`

`AUDIO_INTERFACE::userData` is **exclusively** owned by the Lua binding. It
stores a `USER_DATA_AUDIO_LUA*` which holds the Lua function reference for the
`onEnd` callback. Do not use this field for any other purpose in a new backend.

---

## 8. Game-Level Lua Audio Manager (`audio_manager.lua`)

The game wraps the engine's `audio` global in a higher-level manager:

| Concept | Impl | Notes |
|---|---|---|
| Music | `loadMusic(file, play, loop)` | `in_memory=false`, 1 instance, `isMusic=true` |
| Sound effect pool | `loadSound(file, play)` | `in_memory=true`, up to `sound_pool` instances, `isSound=true` |
| Play | `play(file)` | Finds a non-playing pool slot; loads more if under `sound_pool` limit |
| Stop | `stop(file)` | Stops all pool slots for that file |
| Stop all | `stopAll()` | Iterates all loaded audio |
| Immediate free | `release(file)` | Calls `destroy()` on all slots → `destroyNow()` frees OpenSL slot now |
| Free all | `releaseAll()` | Calls `release()` on every entry |

**`sound_pool` sizing**: With `MAX_PLAYERS=32` on OpenSL ES, and typical music
occupying 4 slots (one per scene's background track), `sound_pool=3–5` leaves
enough slots for ~5–8 simultaneous distinct sound effects. The pool prevents
the same sound being played past `sound_pool` concurrent instances.

**When to call `release()`**: Before showing end-of-battle score / achievement
screens, release the heavy battle SFX that are no longer needed. This frees
their OpenSL player slots so achievement sounds can load in the same scene.

---

## 9. JNI Backend Removal (completed)

The legacy JNI / MediaPlayer backend has been fully removed:
1. `git rm src/core_mbm/audio-jni-android.cpp`
2. Removed `AUDIO=jni` blocks from `src/core_mbm/CMakeLists.txt`.
3. Removed `AUDIO_ENGINE_ANDROID_JNI` private members from `audio.h`.
4. Removed `indexJNI` field from `audio-interface.h` (OpenSL ES does not use it).
5. Removed `streamStopped()` virtual chain from `audio-interface.h`, `audio-manager.cpp`,
   `specific-opengl_es.h`, and `specific-android.cpp`.
6. Removed `streamStopped` JNI callback from `platform-android/main.cpp` and `main-lua.cpp`.
7. Removed `jni` references from root `CMakeLists.txt` help message.

---

## 10. Checklist for a New Backend (e.g. PlayStation / XAudio2 / FMOD)

Follow every step below. Each step maps to a real bug encountered historically.

### Step 1 — Choose a macro name
```
AUDIO_ENGINE_<NAME>  (e.g. AUDIO_ENGINE_XAUDIO2)
```

### Step 2 — Create the source file
`src/core_mbm/audio-<name>.cpp`

Wrap the entire content:
```cpp
#if defined(AUDIO_ENGINE_<NAME>)
// ... implementation
#endif
```

### Step 3 — Implement every `AUDIO` method
The complete set required by `audio.h`:

| Method | Notes |
|---|---|
| `AUDIO(int idScene)` | Constructor — create `BackendData` and initialize per-instance defaults. If a required resource cannot be acquired, set a flag so all methods are no-ops. |
| `~AUDIO()` | **Destructor — the ONLY place to free backend resources.** Must be safe to call even if constructor failed / load() never called. |
| `bool load(file, loop, inMemory)` | Loads the audio asset. Return `false` on any error — DO NOT call any teardown here; `~AUDIO()` handles it. Set `fileName` on success. |
| `bool play(loop)` | Start or restart playback. Handle the EOF-after-first-play problem if your API has it (see Section 6.1). |
| `bool stop()` | Stop playback. Set `state = AUDIO_STOPPED`. |
| `bool pause()` | Pause playback. Set `state = AUDIO_PLAYING` (the "was playing" marker used by `resume()`). |
| `bool resume()` | Resume only if `state == AUDIO_PLAYING`. |
| `bool isPlaying()` | Query backend. Update `state`. |
| `bool isPaused()` | Query backend. |
| `bool setVolume(float)` | Range 0.0–1.0. |
| `bool setPan(float)` | Range -1.0 (left) to +1.0 (right). |
| `bool setPitch(float)` | Range 0.5–2.0, default 1.0. |
| `float getVolume()` | |
| `float getPan()` | |
| `float getPitch()` | |
| `int getLength()` | Duration in milliseconds. |
| `bool reset()` | Seek to position 0 without changing play state. |
| `bool setPosition(int ms)` | Seek to position in milliseconds. |
| `bool isLoaded()` | Return `fileName.size() > 0`. |
| `const char* getFileName()` | Return `fileName.c_str()`. |
| `void setOnEndstream(cb)` | Store `cb` in `onEndStreamCallBack`. |
| `OnEndStreamCallBack getOnEndstream()` | Return `onEndStreamCallBack`. |
| `bool updateBackend()` | Backend per-frame hook. Return whether the backend performed work. Use it for stream pumps or main-thread completion callbacks. |
| `const char* AUDIO_ENGINE_version()` | Return a short identifier string. |

### Step 4 — Per-instance state in the backend `.cpp`

Define `AUDIO::BackendData` inside `src/core_mbm/audio-<name>.cpp`:
```cpp
struct AUDIO::BackendData
{
    // Backend handles, readers, buffers, counters, etc.
};
```
Do not add backend-specific includes, fields, or `#if defined(AUDIO_ENGINE_*)`
blocks to `include/core_mbm/audio.h`.

### Step 4.1 — Manager backend hooks

Every backend source must implement these private `AUDIO_MANAGER` hooks, even if
they are no-ops:

```cpp
void AUDIO_MANAGER::initializeBackend();
void AUDIO_MANAGER::finalizeBackend();
void AUDIO_MANAGER::updateBackend();
```

Use them for shared backend state such as a DirectSound device, AVFoundation
engine pre-warm/release, or main-thread polling of finished streams.

### Step 5 — Handle resource limits

If your backend has a hard cap on concurrent sound objects:
- Add a `static int g_<name>_count` and `constexpr int MAX_<NAME> = <cap>`.
- Check the cap at the start of `load()`, before allocating. Return `false` if at cap.
- Increment only after full successful setup.
- Decrement in `~AUDIO()`, before calling teardown.
- If the engine/context is shared (like OpenSL ES), use a ref-count for the engine singleton and reset the player count to 0 when the engine is torn down.

### Step 6 — Handle EOF / replay

If your API leaves a cursor at EOF after first play (common with file-descriptor-based APIs):
- In `play()`: always stop → seek-to-0 → set loop → start.
- Request seek capability as **required** (not optional) during player creation.
  If seek is unavailable, return `false` from `load()`.

### Step 7 — `inMemory` flag

`AUDIO_MANAGER::load()` passes this flag from Lua (`loadSound` uses `true`,
`loadMusic` uses `false`). Implement it if your API supports the distinction:
- `inMemory=true`: decode and buffer entirely in RAM (low-latency for SFX).
- `inMemory=false`: stream from storage (large music files).

If the backend has no such distinction (like OpenSL ES's FD locator), ignore it
with `(void)inMemory;` and document the fact.

### Step 8 — CMake wiring

In `src/core_mbm/CMakeLists.txt`:
```cmake
elseif(${AUDIO} STREQUAL "<name>")
    add_definitions(-DAUDIO_ENGINE_<NAME>)
    target_link_libraries(core_mbm <backend_lib>)
    message("Using <name> audio")
```

In the root `CMakeLists.txt`:
- Add a default assignment under the target platform block if appropriate.
- Update the help message string at the `message("-DAUDIO=...")` line.

### Step 9 — async end-of-stream handling

If your backend needs an async end-of-stream callback:
- Do not add backend-specific virtuals or guard macros to `audio-interface.h`.
- Store thread-visible completion state inside `AUDIO::BackendData`.
- Poll that state from `AUDIO::updateBackend()` or `AUDIO_MANAGER::updateBackend()`
  on the main thread, then call `onEndStreamCallBack`.

### Step 10 — Test with the engine's `destroyNow()` path

`AUDIO_MANAGER::destroyNow()` immediately calls `delete audio` without waiting
for scene change. Your `~AUDIO()` must be safe to call at any time, including:
- Before `load()` was ever called (in case constructor partially succeeded).
- While the sound is currently playing (must stop first — call `stop()` internally or ensure the destructor does it).
- After the engine/context singleton was already torn down (guard with a `nullptr` check).

### Step 11 — Verify the lifecycle against Section 4

Run through the four scenarios in Section 4 mentally against your implementation:
1. Normal load → play → GC → deferred delete.
2. `load()` failure → automatic teardown via destructor.
3. `audio:destroy()` within same scene → `destroyNow()` → immediate teardown.
4. Scene change → `update()` loop → deferred teardown.

---

## 11. Thread Safety Notes

- `audio-manager.cpp` (`AUDIO_MANAGER` methods) is called **only from the main
  game thread** (Lua runs single-threaded in the engine).
- `audio-opensl-android.cpp` uses `g_engineMutex` to protect the engine
  singleton from destruction racing with player setup. New backends with a
  shared engine singleton must do the same.
- The `onEndStreamCallBack` is called from a background thread in some backends.
  Do not call `AUDIO_MANAGER` methods directly from it — post to the main thread
  (see `onEndStreamCallBackFromSceneThread()` in `audio-lua.cpp`).

---

## 12. Platform Matrix (Current State)

| Platform | Build flag | Backend | Format support |
|---|---|---|---|
| Android (current) | `-DAUDIO=opensl` | OpenSL ES | WAV, OGG, MP3 (via Android codec) |
| Android (legacy) | ~~`-DAUDIO=jni`~~ | ~~JNI/MediaPlayer~~ | *(removed)* |
| Linux | `-DAUDIO=portaudio` | PortAudio | WAV + OGG |
| Windows | `-DAUDIO=portaudio` | PortAudio | WAV + OGG |
| Windows (alt) | `-DAUDIO=dsound` | DirectSound 8 | WAV only |
| macOS | `-DAUDIO=avfoundation` | AVFoundation | WAV, AIFF, CAF, MP3, AAC, FLAC, OGG\* |
| iOS | `-DAUDIO=avfoundation` | AVFoundation | same as macOS |
| Any | `-DAUDIO=none` | Stub | (silence) |

\* OGG decoded via `stb_vorbis` → PCM → `AVAudioPCMBuffer`
