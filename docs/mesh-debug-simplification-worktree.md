# Mesh Debug simplification work tree

Status: delivered in **7.290.0**.

Implementation plan (recorded before changes): move the complete simplification
panel from Frame into a sibling Simplification work tree. Keep method, frame/subset
scope, shared-frame policy, QEM/coplanar settings, progress, cancel, reports and undo.
Frame retains its frame/subset editing and split-capture operations.

Add an opt-in side-by-side comparison of the last successful operation's source
and result. Keep a result snapshot paired with the existing undo source so subsequent
edits/settings/no-op jobs cannot silently relabel another mesh as that result.
Comparison uses static geometry from the operation's reference frame (base geometry,
not animated/skinned pose playback); retain per-subset UVs, normals and textures.
Offer independent original/result visibility, wireframe and fit-to-view. Display-only
position offsets must never enter saved/exported geometry. Hide/release comparison
renderables on selection/preview changes; discard snapshots on undo/replacement/removal.
Build geometry/wire buffers only on request, cache them during idle drawing, and keep
heavy processing out of the per-frame path. Reuse Image Mesh wire rendering where safe.

Validation: real editor scene exercises QEM/coplanar jobs, tree separation, snapshot
counts/attributes, wire toggles/cache, selection/tree changes, no-op, cancel/failure,
undo and cleanup. Verify visual output and document native input/platform limitations.

## Delivery and validation

The new `mesh_debug_simplification.lua` module owns comparison snapshots, display
objects, cached frame/subset metadata and panel rendering. The original simplify
worker, algorithms and undo transaction remain in place. Frame no longer calls
`showSimplifyGeometry`; its sibling Simplification tree does. Existing target scope,
QEM/coplanar settings, reports, progress, cancellation and undo are retained.

Comparison is opt-in after a successful change. The source is the undo file and the
result is a separate immutable snapshot, so a no-op or editing inputs cannot relabel
the previous result. Display copies contain the operation's static reference frame;
shared-frame jobs show frame 1. A label explicitly excludes animated/skinned playback.
World-X offsets separate the copies, with a shared camera and lighting. Fit encloses
both; camera controls remain available. Vertex/triangle counts and percentage reduction
refer to this stored pair. Saving/exporting continues to use the actual edited mesh.

Filled and wire views share cached objects. Geometry/wire buffers are constructed
only on explicit comparison/wire requests. Leaving the tree hides the pair; preview
changes release runtime objects but keep snapshots for a later request. Undo, replacing
the last successful result, mesh removal, Clear All and Quit-menu paths discard the
pair. No-op/cancel/failure retain it. Abrupt process termination/window-manager exit
may leave temporary files, as with other editor temp/undo paths; this change does not
add a general scene-finalization mechanism.

The reused Image Mesh wire builder now uses pair keys instead of `a*65536+b`, avoiding
edge-key collisions when concatenated subset vertices exceed 65,536. The comparison
visibility guard also handles Mesh Debug's transform panel restoring the primary
preview each frame, avoiding a third superimposed mesh.

Validation on Linux Debug / OpenGL ES:

- Lua `loadfile` checks for changed editor/module/language/test files and engine build pass.
- `mesh_debug_simplification_smoke.lua` calls the real Frame and Simplification trees
  and verifies the settings moved. It exercises Coplanar and QEM, original 120 vs
  result 8 triangles, frame snapshot save/load, preview-only offsets, independent
  visibility, filled/wire toggles, idle cache reuse, primary preview suppression,
  cancellation, no-op, failed QEM, undo and preview/snapshot cleanup. A multiframe
  QEM case confirms the comparison uses selected frame 2 and leaves frame 1 intact.
- Wire and filled comparisons rendered to `/tmp/md-simplify-comparison.png` and
  `/tmp/md-simplify-comparison-filled.png` were visually inspected.
- Existing Coplanar cancellation/undo, Image Mesh modes/preview/export/project/idle,
  curved-specific sync/async/cancel and generic QEM regression scenes pass.
- No native click/drag automation or non-GLES runtime testing was performed. Complex
  animated material/shader parity and allocation-failure injection were not tested.

Run the new scene from the repository root after building the engine:

```sh
timeout -s KILL 60 ./bin/debug/linux_x86/mini-mbm --scene src/test-lib/mesh_debug_simplification_smoke.lua --disable_select_monitor --nosplash -w 1100 -h 750
```

Require the `MESH DEBUG SIMPLIFICATION WORKTREE / COMPARISON / WIREFRAME / IDLE / CANCEL / NOOP / UNDO / CLEANUP OK`
sentinel, not only process exit status. The test writes only disposable `/tmp` assets.
