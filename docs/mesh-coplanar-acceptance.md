# Coplanar simplification: final acceptance

Status: **completed at 7.289.0 (M1-M8)**. The final audit adds validation and
clarifies scope; it adds no engine algorithm, public API or editor behavior and
therefore does not bump the feature version. Further algorithms are optional,
asset-driven follow-ups, not milestones required to finish this delivery.

The [design and delivery history](mesh-coplanar-optimization-plan.md) records the
individual milestones. [Mesh Simplification](mesh-simplification.md#coplanar-modes-72800)
is the current operational contract.

## Accepted scope and evidence

Test filenames below are under `src/test-lib/`; numerical helpers live under
`src/core_mbm/private/`. Engine results refer to Linux Debug / OpenGL ES.

| Original requirement | Implementation / accepted behavior | Validation |
|---|---|---|
| Preserve existing QEM; optional Coplanar before QEM or alone | Native modes default to QEM; combined target uses original scope count and skips QEM when already reached | `mesh_simplify_smoke.lua`, `mesh_coplanar_smoke.lua`, `image_mesh_coplanar_smoke.lua` |
| Connected planar/near-planar regions, controlled tolerance | Fixed seed plane, distance bound using fixed connected-domain scale, source and replacement face angle checks; no accumulated pairwise normal drift | `mesh_coplanar_smoke.lua`: dense, inclined, near-planar and bent geometry |
| External contours, concavities, holes, winding and connectivity | Directed loops, incidence, vertex links, Euler/coverage and area checks; at most 16 disjoint internal holes | `mesh_coplanar_holes_smoke.lua`, `mesh_coplanar_boundary_smoke.lua`: coverage, boundaries, orientation, edge crossings and geometric T-junctions |
| No assumption that a planar region always becomes two faces | Every boundary sample stays by default; optional exact straight-boundary coordination only removes jointly certified samples | Dense grid 128 -> 30 by default or 2 with coordination; hole 120 -> 40 or 8; hole/notch/junction fixtures |
| Subsets/materials, UV seams and normal discontinuities | Separate attribute charts, original retained corner values, whole-frame neighbor context; no seam welding | Boundary, hole, normal and imported-asset suites; selected and whole-frame scopes |
| Interior interpolation, not only boundary attributes | All source corners must fit the UV affine residual limit; raw normals constant or exactly affine on exactly planar generic regions | `mesh_coplanar_normals_smoke.lua` samples output interiors and rejects nonlinear/normalized fields; raw non-unit values retained |
| Avoid new cracks, degenerate faces or intersections | Candidate topology certificates, coordinated dependencies, indexed obstacle context, strict projected/3D separation and previous certified boundary contacts | Hole/boundary/obstacle/spatial suites; standalone index and interval-separation tests |
| Explicit deformation policy | Skeletal/canonical, articulated and multiframe assets skip the generic Coplanar stage; QEM remains available | Existing multiframe/QEM fixtures; loaded `Lorekeeper-walk.msh` remains unchanged with `planarSkipped=true` |
| Preserve the source when validity cannot be certified | Region fallback, optional boundary-stage rollback; combined QEM failure rolls back the transaction | Attribute/topology/obstacle rejection, failed-QEM rollback, no-op and coordination-limit fixtures |
| Engine limits, progress, cancel and complete publication | uint16 output checks; 2,048 boundary vertices, 16 holes, 2,000,000 budgeted operations/region; bounded boundary coordination; detached result and commit gate | 65,536-source-vertex compaction, 17-hole rejection, 4,097-region fallback, cancel/retry/late cancel/progress/undo tests |
| Both general editor flows; separate curved workflow | Shared native modes/settings in Mesh Debug and Image Mesh; curved-specific generation retains its exact plateau/locks policy | `mesh_coplanar_cancel_smoke.lua`, `image_mesh_coplanar_smoke.lua`, `image_mesh_curved_simplify_smoke.lua` |
| No expensive idle processing | Mesh Debug resumes only active simplification coroutines; Image Mesh rebuild checks dirty/drag/edit state and uses geometry cache | Source audit of `simplifyResume`, `rebuildImpl` and job launch paths; Image Mesh idle-cache regression |
| Counts, geometry, attributes and appearance | Numeric and structural fixtures plus checker/lighting before/after render comparisons | `mesh_coplanar_visual_smoke.lua`; comparisons pass and spatial-island images inspected |

The geometric/UV bounds describe the Coplanar intermediate result. Subsequent QEM
approximation has its own contract. Attribute preservation concerns stored data and
standard interpolation; arbitrary nonlinear custom vertex shaders are not certified.

## Files loaded and imported during closure

`mesh_coplanar_imported_smoke.lua` loads real repository assets, checks retained
corner attributes and texture names per subset, verifies unchanged buffers on
no-op, and saves/reloads each result. It also writes an OBJ/MTL fixture, parses it
through the actual `tiny_obj_loader` module, builds its two adjacent UV charts,
recalculates normals as the editor import flow does, saves to MSH and reloads it
before simplification. This exercises the parser/native path, not a mouse-driven
import dialog or the complete Blender/FBX pipeline.

| Asset | Original faces | Coplanar, boundaries retained | Coplanar, straight-boundary reduction |
|---|---:|---:|---:|
| `Crate.msh` | 108 | 108 | 108 |
| `base.msh` | 60 | 60 | 60 |
| `building_A.msh` | 828 | 828 | 828 |
| `Lorekeeper-walk.msh` | 10,738 | 10,738 (skipped) | 10,738 (skipped) |
| Imported OBJ with two dense adjacent UV charts | 256 | 60 | 4 |

No reduction is a supported result, not a guarantee that a mesh is already globally
minimal. These tests make no claim that every imported asset will benefit.

## Numerical portability and execution limits

- GCC and Clang pass both private helper suites at native `long double` precision
  and at binary64 precision. The latter uses temporary source copies substituting
  `double` and matching literals. This avoids the host C library ABI mismatch of
  `-mlong-double-64`; it does **not** emulate Windows/MSVC or macOS/Metal.
- The separation suite checks 10,000 integer-reference interval enclosures and
  shared-vertex pairs, cancellation arithmetic, actual crossings, touches, margin
  rejection, scale/axis changes and interrupted axis work. The index suite checks
  1,200 inclusive queries against exhaustive candidate selection.
- Both compilers confirm the new 3D interval certificate disables itself under
  fast-math. Supported builds require strict arithmetic; this targeted fallback
  test does not certify the entire simplifier for arbitrary fast-math flags.
  No explicit fast-math setting was found in the inspected repository CMake or
  MSVS project/property files. Custom toolchain flags remain outside this evidence.
- M8 also passed AddressSanitizer/UndefinedBehaviorSanitizer for its private
  predicate, with leak detection disabled because of sandbox ptrace.
- Binary64 checks cover the helpers, not full-region grouping/retriangulation.
  Floating-point precision can change conservative acceptance and resulting face
  counts. Bit-identical results across compilers/platforms are not promised.
- Windows DirectX and macOS Metal execution, native mouse interactions and
  allocation-failure injection were not performed. Full-pipeline budget exhaustion
  specifically inside the newest 3D certificate was not forced; helper interruption,
  worker cancellation and existing budget fallback were exercised.

## Repeatable validation

From the repository root, with the documented Linux build configured and the real
X display available (disable missing-texture dialogs for automated runs):

```sh
cmake --build build --target mini-mbm -j 4
python3 src/test-lib/run-coplanar-validation.py --output /tmp/coplanar-close-gcc
python3 src/test-lib/run-coplanar-validation.py --compiler clang++ --structural-only --output /tmp/coplanar-close-clang
```

The runner compiles the native/binary64 helpers, checks the fast-math fallback and
runs all 13 engine scenes unless `--structural-only` is supplied. Every scene must
emit its success sentinel; exit code zero alone is insufficient. Each process has
a 90-second timeout. Logs and a machine-readable `results.json` go in the output
directory, with temporary precision-test sources and binaries. Existing Lua scenes
also write their documented `/tmp` mesh/image fixtures. Run the engine suite serially
because fixtures share names. The compiler option assumes GCC/Clang CLI flags;
this runner is not a Windows/MSVC validation harness.

Closure execution: GCC complete suite and Clang structural suite **passed**.
No production-code defect was found in this audit. No additional algorithm is
required to close the accepted scope. Deformation certificates, non-affine attribute
remeshing, approximate contour changes, broader contacts and diagnostic overlays
remain [optional follow-ups](future-features.md#coplanar-region-follow-up).
