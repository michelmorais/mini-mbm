<!--
┌──────────────────────────────────────────────────────────────────────────────┐
│ MIT License                                                                 │
│ Copyright (c) 2026 Michel Braz de Morais                                      │
│                                                                            │
│ Permission is hereby granted, free of charge, to any person obtaining a copy │
│ of this software and associated documentation files (the "Software"), to    │
│ deal in the Software without restriction, including without limitation the │
│ rights to use, copy, modify, merge, publish, distribute, sublicense, and/or │
│ sell copies of the Software, and to permit persons to whom the Software is │
│ furnished to do so, subject to the following conditions:                   │
│ The above copyright notice and this permission notice shall be included in│
│ all copies or substantial portions of the Software.                        │
│ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR │
│ IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  │
│ FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL   │
│ THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER│
│ LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING   │
│ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER       │
│ DEALINGS IN THE SOFTWARE.                                                  │
└──────────────────────────────────────────────────────────────────────────────┘
-->

# Physics regression tests

These tests exercise the [Box2D backend](../../../docs/box2d.md) and optional fluid
extension. Run commands from the repository root. Store generated results in
`build/` or a temporary directory, not in `docs/`.

## Native solver checks

```sh
cmake -S src/test-lib/physics-compat -B build/physics-tests -DPLAT=Linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/physics-tests -j6
ctest --test-dir build/physics-tests --output-on-failure
```

Use the matching `PLAT` and generator for the target host. For multi-configuration
generators, pass `--config Release` to the build and `-C Release` to CTest.
The default suite contains three tests: rigid-only, particles enabled, and rigid
OFF/ON equivalence. They cover contacts, joints, queries, containment, lifetimes
and destruction. `-DPHYSICS_SANITIZERS=ON` enables ASan/UBSan with GCC or Clang.

For comparison against a separate LiquidFun tree, configure the harness with
`-DPHYSICS_LEGACY_SOURCE=/absolute/path/to/liquidfun-source`. That directory must
contain `Box2D/Box2D.h` and its implementation sources. This adds a fourth native
test and a separate `physics_liquidfun` executable; it does not affect the engine.

## Build configuration checks

```sh
python3 src/test-lib/physics-compat/check-cmake.py --build-root build/physics-config-check
python3 src/test-lib/physics-compat/check-msvs.py
```

The CMake checker configures Linux profiles, verifies source inclusion and facade
dependencies, and expects errors for invalid flag combinations. The MSVS checker
verifies source manifests, conditional exclusions and the forwarding DLL reference.
Neither replaces compiling the engine or running it on the target platform.

## Lua and rendering contracts

Build the engine and facade with particles ON and OFF into separate output roots.
For unattended builds, pass `-DUSE_TEXTURE_MISSING_DIALOG=0` to avoid a blocking
asset picker. A working graphics session is required. For MSVC, use
`/p:MbmCoreFeatureDefines=AUDIO_ENGINE_PORT_AUDIO` for this test configuration.

```sh
python3 src/test-lib/physics-compat/run-lua-contracts.py --engine /absolute/path/to/on/mini-mbm --profile on --output build/physics-contracts-on
python3 src/test-lib/physics-compat/run-lua-contracts.py --engine /absolute/path/to/off/mini-mbm --profile off --output build/physics-contracts-off
```

Use the native executable name (for example `mini_mbm.exe` on MSVC). The runners
support `.so`, `.dll` and `.dylib`, select the corresponding module directory,
require success markers and exit status, and record binary hashes.

ON runs six contracts: modern factory, compatibility factory, gradual emission,
collision filters, convex contours and fluid depth. OFF runs the rigid API contract
and verifies that fluid loading is rejected. Shared scripts cover joint compatibility,
world ownership, handle invalidation and garbage collection.

## Lifecycle and client scenarios

```sh
python3 src/test-lib/physics-compat/run-ratworks.py --engine /absolute/path/to/on/mini-mbm --seconds 600 --output build/physics-lifecycle
```

Without `--game`, this runs the asset-free `ratworks-reduced.lua` scene: repeated
emission, pause/resume, containment, lifetime and destruction cycles. It uses a
fixed simulation step; simulated time and wall-clock time are different. The runner
records RSS where supported, but RSS alone does not prove the absence of leaks.
Use an engine build with sanitizers for memory diagnostics.

`--game /absolute/path/to/Ratworks` selects the real client scenario, which depends
on that game's scripts and assets. `--legacy` selects its compatibility comparison
mode for an external engine build. Neither is required by the self-contained suite.

## Documentation examples

```sh
python3 src/test-lib/physics-compat/inventory.py --documentation /path/to/mbm-documentation --examples-only --output build/physics-examples
python3 src/test-lib/physics-compat/run-examples.py --inventory build/physics-examples/inventory.json --documentation /path/to/mbm-documentation --engine /absolute/path/to/on/mini-mbm --output build/physics-examples-on
```

For an OFF build, select `--plugin box2d`. The runner extracts self-contained
construction examples; isolated calls and fragments remain classified as requiring
context. A passing run proves runtime smoke coverage, not interactive or visual
equivalence. Review those aspects with the intended scene inputs.

`inventory.py --upstream-box2d /path/to/box2d-2.4.1` also inventories local changes
against a matching upstream source tree, helping identify port changes to carry
forward when updating Box2D.

## Measurements

```sh
python3 src/test-lib/physics-compat/measure.py --build build/physics-tests --runs 5 --output build/physics-measurements.json
```

The collector defaults to `rigid` and `candidate` (particles OFF and ON). To include
the external comparison executable, use `--backends rigid liquidfun candidate`.
Point `--build` at a directory containing both the CMake cache and executables.
The collector records source/binary hashes, warmup, sample counts and timings for
native simulation only; Lua and rendering are not included. `--cpu N` pins Linux
runs to one CPU. Compare equivalent builds and workloads over repeated runs.
