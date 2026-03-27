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
| `include/core_mbm/audio.h` | `AUDIO` concrete class declaration + `AUDIO_MANAGER` class declaration. All per-engine private members are declared here under `#if defined(AUDIO_ENGINE_*)` guards. |
| `src/core_mbm/audio-manager.cpp` | Platform-agnostic lifecycle: `load()`, `destroy()`, `destroyNow()`, `update()`, `release()`. **Never holds any engine-specific code.** |
| `src/core_mbm/audio-interface.cpp` | `AUDIO_INTERFACE` constructor/destructor (tiny). |
| `src/core_mbm/audio-audiere.cpp` | Backend: Audiere (Linux / Windows fallback). Guard: `AUDIO_ENGINE_AUDIERE` |
| `src/core_mbm/audio-direct-sound.cpp` | Backend: DirectSound 8 (Windows). Guard: `AUDIO_ENGINE_DIRECT_SOUND_8` |
| `src/core_mbm/audio-portaudio.cpp` | Backend: PortAudio (Linux default / cross-platform). Guard: `AUDIO_ENGINE_PORT_AUDIO` |
| `src/core_mbm/audio-opensl-android.cpp` | Backend: OpenSL ES (Android, current). Guard: `AUDIO_ENGINE_ANDROID_OPENSL` |
| `src/core_mbm/audio-avfoundation.mm` | Backend: AVFoundation (macOS / iOS). Guard: `AUDIO_ENGINE_AVFOUNDATION` |
| `src/core_mbm/audio-jni-android.cpp` | Backend: Android JNI / MediaPlayer (legacy, unused). Guard: `AUDIO_ENGINE_JNI` |
| `src/core_mbm/audio-none.cpp` | Stub backend (no audio). Guard: `AUDIO_ENGINE_NONE` |
| `src/lua-wrap/audio-lua.cpp` | Lua binding layer. Registers `audio` global, manages Lua userdata lifetime. |
| `assets/logic/audio_manager.lua` (game) | High-level Lua audio manager: sound pools, music/sfx distinction, `release()`. |

---

## 2. Class Hierarchy

```
AUDIO_MANAGER_INTERFACE   (abstract — interface only)
    └── AUDIO_MANAGER     (audio-manager.cpp — platform-neutral lifecycle)

AUDIO_INTERFACE           (abstract — interface only)
    └── AUDIO             (audio.h declared, one of the audio-*.cpp implemented)
```

`AUDIO_MANAGER` owns a `std::vector<AUDIO*> audios` (live) and
`std::vector<AUDIO*> audiosToDelete` (deferred deletion queue).

---

## 3. CMake Backend Selection

Pass `-DAUDIO=<value>` at configure time:

| `-DAUDIO=` value | Preprocessor macro defined | Default on |
|---|---|---|
| `opensl` | `AUDIO_ENGINE_ANDROID_OPENSL` | Android |
| `avfoundation` | `AUDIO_ENGINE_AVFOUNDATION` | macOS, iOS |
| `portaudio` | `AUDIO_ENGINE_PORT_AUDIO` | Linux |
| `audiere` | `AUDIO_ENGINE_AUDIERE` | Windows (MinGW/MSVC) |
| `none` | `AUDIO_ENGINE_NONE` | explicit opt-out |
| `jni` | `AUDIO_ENGINE_JNI` | (legacy Android, do not use) |

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
          → if none: new AUDIO(idScene)  ← constructor calls backend init
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
  [BACKEND] (OpenSL ES / AVFoundation / PortAudio / Audiere / DirectSound)


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
- No ref-counted engine singleton — each `AUDIO` instance holds its own
  `std::unique_ptr<AVFAudioData>`. `~AUDIO()` destroys it via the unique_ptr.
- No EOF rewind issue — `AVAudioPlayerNode` handles seek internally.

### 6.3 PortAudio (Linux — `audio-portaudio.cpp`)

- Each `AUDIO` instance holds a `std::unique_ptr<PA_WAVE>`.
- WAV only (via `third-party/portaudio/` wave reader).
- `~AUDIO()` destroys the `PA_WAVE` unique_ptr.

### 6.4 Audiere (Windows / Linux fallback — `audio-audiere.cpp`)

- Each `AUDIO` holds an `audiere::OutputStreamPtr` (reference-counted smart
  pointer from the Audiere API).
- Supports WAV, OGG, MP3, FLAC, and more.
- `~AUDIO()` releases the `OutputStreamPtr`.
- `AUDIO_MANAGER` holds a static `audiere::AudioDevicePtr`.

### 6.5 DirectSound 8 (Windows legacy — `audio-direct-sound.cpp`)

- Each `AUDIO` holds a `std::unique_ptr<WaveFile>` and a double-buffer stream.
- WAV only.
- `~AUDIO()` destroys the wave reader.

### 6.6 JNI / MediaPlayer (Android legacy — `audio-jni-android.cpp`)

- Each `AUDIO` holds an `indexJNI` (int handle into a Java-side pool).
- All operations are JNI calls to `AudioManagerJniEngine.java`.
- `~AUDIO()` calls `onDestroyAudioJniEngine(indexJNI)` via JNI.
- **Status**: superseded by OpenSL ES. Do not use for new development.
  See Section 9 for removal.

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

## 9. Removing `audio-jni-android.cpp`

`audio-jni-android.cpp` is **safe to delete**. It is only compiled when
`AUDIO_ENGINE_JNI` is defined, which requires `-DAUDIO=jni`. This is never set
by any active build configuration (Android now always uses `-DAUDIO=opensl`).

Steps to remove:
1. `git rm src/core_mbm/audio-jni-android.cpp`
2. In `src/core_mbm/CMakeLists.txt`, remove the two blocks that reference `jni`:
   - Line ~61: `elseif (${AUDIO} STREQUAL "jni") add_definitions(-DAUDIO_ENGINE_ANDROID_JNI)`
   - Line ~218-220: `elseif(${AUDIO} STREQUAL "jni") ... add_definitions(-DAUDIO_ENGINE_JNI)`
3. In `include/core_mbm/audio.h`, remove the `AUDIO_ENGINE_ANDROID_JNI` private member block.
4. In `include/core_mbm/audio-interface.h`, the `indexJNI` field is declared
   under `#ifdef ANDROID`. If JNI is the only user, remove it too
   (but first verify `audio-opensl-android.cpp` does not use `indexJNI` —
   it does not; it uses `oslPlayer`).
5. In `CMakeLists.txt` (root), remove the `jni` references in the help message (line 57).

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
| `AUDIO(int idScene)` | Constructor — init or acquire backend resource. If resource cannot be acquired, set a flag so all methods are no-ops. |
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
| `const char* AUDIO_ENGINE_version()` | Return a short identifier string. |

### Step 4 — Per-instance state in `audio.h`

Add a private member under a new guard block in `audio.h`:
```cpp
#elif defined(AUDIO_ENGINE_<NAME>)
    std::unique_ptr<MyBackendData> backend_data;  // or struct/handle
```
Keep one member only — complexity belongs in the `.cpp` file.

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

### Step 9 — audio-interface.h additions (if needed)

The `AUDIO_MANAGER_INTERFACE` has a `streamStopped(int indexJNI)` virtual
under `#ifdef ANDROID`. If your backend needs an async end-of-stream callback:
- Add a new virtual under your own guard macro.
- Implement it in `audio-manager.cpp` to forward to the correct `AUDIO` instance.

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
| Android (legacy) | `-DAUDIO=jni` | JNI/MediaPlayer | WAV, OGG, MP3 (do not use) |
| Linux | `-DAUDIO=portaudio` | PortAudio | WAV only (stb_vorbis possible) |
| Linux (alt) | `-DAUDIO=audiere` | Audiere 1.9.4 | WAV, OGG, MP3, FLAC, MOD |
| Windows (MinGW/MSVC) | `-DAUDIO=audiere` | Audiere 1.9.4 | WAV, OGG, MP3, FLAC, MOD |
| macOS | `-DAUDIO=avfoundation` | AVFoundation | WAV, AIFF, CAF, MP3, AAC, FLAC, OGG\* |
| iOS | `-DAUDIO=avfoundation` | AVFoundation | same as macOS |
| Any | `-DAUDIO=none` | Stub | (silence) |

\* OGG decoded via `stb_vorbis` → PCM → `AVAudioPCMBuffer`
