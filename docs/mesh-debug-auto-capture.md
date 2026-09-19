# Mesh Debug automatic capture

In **Frame -> Split**, manual capture still uses **Start Capture** to position a box. Unchecking
it displays **Capture Results** immediately below that control, before the automatic-capture block.

**Auto capture** detects connected triangle groups throughout the checked frames/subsets without
using the box. It skips groups already captured in the current editor session. Each accepted island
becomes its own subset when **Apply Capture** is pressed. Original subset/material boundaries remain
separate; this feature does not merge different source subsets.

## Connectivity and thresholds

| Mode | Connection between faces |
|---|---|
| Vertex indices | A shared vertex index; duplicated vertices at UV/normal seams remain separate. |
| Vertex positions | At least one shared position, allowing the configured distance tolerance. |
| Shared edges (default) | Two shared edge endpoints, allowing the configured position tolerance. |

**Position tolerance** is measured in mesh coordinates. Zero means exact positions; index mode
ignores it. Nearby vertices form transitive position clusters, so a chain of close vertices can
connect more distant endpoints. This changes detection only: positions, normals, UVs and weights
are not welded or moved. Increasing tolerance can join separate objects, so inspect the results
before applying. In edge mode, tolerance does not discard a real edge when both endpoints enter
the same position cluster.

**Minimum faces per island** defaults to 1. Smaller islands are excluded from capture but their
triangles remain together in the uncaptured remainder of their source subset; they are not deleted.
The result table reports detected islands, captured faces, skipped islands and the number of
captured subsets to create. Optional center markers use cyan for accepted islands and orange for
islands left in the remainder.

Hover the connectivity selector for an explanation of the selected mode. The tolerance and
minimum-face inputs also have tooltips explaining units, ignored settings and how skipped faces
are preserved. The minimum is a triangle-count filter, not a target number of subsets.

After automatic capture, orange translucent boxes surround each accepted island by default,
previewing the bounds of the subsets to create. They use the same style as the manual capture
box, but are read-only. Uncheck **Show subset boxes** to hide them; **Show island centers** remains
independent. Skipped islands do not receive boxes. Preview geometry is cached and cleaned up when
the analysis is discarded or applied; it is not rebuilt continuously while idle.

Changing automatic-capture parameters invalidates the previous automatic result. Click **Auto
capture** again to analyze. No island detection runs continuously while the editor is idle. The
connectivity combo has a fixed 240-pixel width; tolerance and minimum-face inputs use 120 pixels.

## Apply and revert

Capture validates source signatures and rebuilds a detached mesh before replacing the editor's
object. It preserves source vertex attributes and texture roles, remaps canonical skeletal weights
to the new frame-global vertex order, and checks the result. **Revert Last Capture** restores the
snapshot from before the latest capture. The source file is only updated by an explicit save.

Manual capture continues to combine its accepted islands into one captured subset per source
subset. Automatic capture creates a separate captured subset for each accepted island.

## Six-block regression fixture

The user-provided `n1-v01-ee31480b-msh01.msh` has one frame, one subset and 2,861 triangles. Its six
visually distinct blocks contain 17 exact-position islands: several decorative details are detached
from the surfaces of the same block. Index connectivity alone finds 1,357 islands because many
vertices are duplicated.

For this fixture, use **Vertex positions**, **Position tolerance = 3**, and **Minimum faces = 1**.
The result is six subsets with 758, 683, 588, 328, 300 and 204 triangles. Comparing triangle multisets,
including positions, normals and UVs, matches the six subsets in
`n1-v01-ee31480b-msh01-manual-splited.msh` exactly, before and after save/reload. The subset order
can differ. This tolerance is fixture-specific, not an assumption that all meshes contain six parts.

The fixture and its textures are external to the repository. From the repository root, run:

```sh
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/mesh_debug_islands_test.lua

timeout -s KILL 20 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/mesh_debug_auto_capture_smoke.lua \
  --disable_select_monitor --nosplash -w 1100 -h 850

MBM_AUTO_SPLIT_FIXTURE_DIR=/path/to/test-auto-split-subset \
MBM_AUTO_SPLIT_OUTPUT=/tmp/auto-split-result.msh \
timeout -s KILL 25 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/mesh_debug_auto_capture_reference_smoke.lua \
  --disable_select_monitor --nosplash -w 1200 -h 900
```

The general engine smoke test covers indexed/non-indexed geometry, minimum-size filtering,
texture/UV/weight preservation, manual capture, revert, stale-analysis rejection and idle behavior.
The reference smoke test checks exact group membership against the manual six-subset mesh and
rechecks the saved result. It also checks the six preview boxes, their positions and dimensions,
independent box/center toggles, cleanup and absence of idle reconstruction. The reference output
path must be a disposable path, not an input file.
