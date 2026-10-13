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

# Box2D physics and optional fluids

Mini MBM uses one Box2D 2.4.1 backend in `plugins/box2d`, with LiquidFun 1.1.0
particles ported locally into `third-party/box2d-2.4.1`. This is a local extension,
not an official LiquidFun release for Box2D 2.4.1. Source files retain their
upstream copyright and license notices.

## Build profiles

All CMake builds require `PLAT` and Lua for the physics plugin.

| Profile | CMake options |
| --- | --- |
| Rigid bodies | `-DUSE_LUA=1 -DUSE_BOX2D=1 -DUSE_BOX2D_PARTICLES=OFF` |
| Rigid bodies and fluids | `-DUSE_LUA=1 -DUSE_BOX2D=1 -DUSE_BOX2D_PARTICLES=ON` |

`USE_BOX2D_PARTICLES` defaults OFF. `USE_ALL=1` defaults it ON and respects an
explicit OFF or a value already stored in the CMake cache. Particles require both
Lua and Box2D; enabling `USE_BOX2D_LIQUID_FUN` is an error. Use the options above.

Use distinct build directories and absolute `MBM_OUTPUT_ROOT` paths for ON/OFF
profiles so their executables and libraries cannot overwrite each other. Rebuild
all consumers when changing profiles: `MBM_BOX2D_PARTICLES` controls both sources
and C++ declarations.

OFF excludes particle solver sources, particle state and fluid Lua bindings.
ON includes them, but creates particle systems only when requested. An empty or
paused system is not equivalent to destroying it: lifecycle and wrapper work can
remain. Use `destroyFluid` when a fluid is no longer needed. Measure representative
scenes with the [regression tools](../src/test-lib/physics-compat/README.md) when
choosing performance budgets.

Visual Studio uses `/p:MbmBox2dParticles=0` or `1` (default). Rebuild when switching
profiles. See the [Windows build instructions](../platform-msvs/README.md).

## Lua modules and ownership

`require "box2d"` loads the backend in either profile. With particles ON,
`require "box2dLiquidFun"` exposes its compatibility factory through the same
backend. With particles OFF, requiring the fluid module produces an explicit
configuration error. `box2d:supportsParticles()` reports the compiled capability.

Each factory call creates a separate world. Bodies, joints and fluids belong to
their creating world; loading both module names does not make worlds interact.
`box2dLiquidFun:new()` preserves its compatibility defaults and spring-joint
methods. `box2d:new()` uses the modern defaults. Desktop builds provide a forwarding
library; embedded builds register the compatibility entry point in the backend.

The fluid extension supplies immediate or gradual emission, lifetimes, pause,
rigid-body category filtering, queries and particle rendering. The simulation is
XY; `is3d=true` renders planar particle quads in a dedicated depth-tested pass
with depth writes disabled. It does not create a volumetric simulation.

See [the Lua API](lua-api.md#11-plugin-box2dliquidfun) for signatures, shape formats,
units, joint compatibility, ownership and rendering limits, and
[the binding index](lua-plugin-symbols.md#box2d-world-and-factory) for source locations.

## Regression and version updates

The [physics test suite](../src/test-lib/physics-compat/README.md) covers native
OFF/ON invariants, Lua contracts, emission, filtering, fluid geometry, rendering,
and repeated creation/destruction. Run both profiles when changing the backend,
bindings or build configuration. Platform manifest checks do not replace native
builds and runtime tests on each target.

The standalone harness also accepts an external LiquidFun source tree for solver
comparisons. That source is never linked into the engine. Keep generated results,
logs, screenshots and extracted sources in a build or temporary directory.
