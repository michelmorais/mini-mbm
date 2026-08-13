# Skeletal Animation Editor

Status: **Exclusive worktree shell available; Skin Weight Lab, bind diagnostics, and runtime GLES2 LBS/DQS preview implemented**
Last updated: **2026-08-12**

## 1. Purpose

The Skeletal Animation Editor is the standalone Mini MBM tool for inspecting and editing skeletal
mesh data. Its currently implemented workspace, **Skin Weight Lab**, repairs canonical type-42
frame-1 vertex weights without expanding Mesh Debug into a general animation editor.

For canonical skeletal meshes within the GLES2 palette limit, the preview can play the same
per-instance LBS or rigid-DQS deformation path used by the runtime. Non-GLES backend delivery
remains in the [Real-Time Skinning Animation Plan](realtime-skinning-animation-plan.md).

The editor is organized into five mutually exclusive worktrees: **Bind Pose Contract**,
**Runtime Skeletal Preview**, **Skin Weight Lab**, **Create / Edit Animations**, and
**Paint Weights**. The last two are currently reserved and explain their future scope. Their product
boundaries, the audited relationship to Mesh Debug's Bones node, and the migration sequence are defined in the
[Skeletal Animation Editor Plan](skeletal-animation-editor-plan.md).

## 2. Opening the editor

Choose **Skeletal Animation Editor** from the Mini MBM launcher, or start it directly:

```sh
./bin/debug/linux_x86/mini-mbm --scene editor/skeletal_animation_editor.lua
```

Use **File > Open Mesh** to load a `.msh` file. The mesh should contain a frame-1 skeleton and
canonical vertex skin weights for all bone-dependent workflows. Meshes without bones or weights may
still be inspected through AABB and material-subset selection.

After loading a skeleton, open **Bind Pose Contract** to inspect the canonical conversion without
editing the source asset. The panel reports global-to-local TRS reconstruction error, bind-identity
error, fatal/warning diagnostics, stable bone IDs, local quaternion TRS, and the local, global, and
inverse-global bind matrices. Root parent indices are displayed as `0`; stable IDs are hexadecimal
strings so their full 64-bit identity is preserved through Lua.

Bones are navigated as their actual parent/child hierarchy rather than as a flat source-order list.
Multiple roots are shown as separate top-level nodes, nodes with diagnostics are marked in orange,
and **Expand all** opens the complete hierarchy. Selecting a node highlights its joint and incoming
parent-to-child bone segment in cyan in the bind-pose gizmo, and updates one separate technical panel with that bone's identity, parent,
local TRS, radius/length, and bind matrices. The selected-bone panel permits an explicit rename.
Empty or duplicate names are rejected transactionally; weights and animation tracks continue
targeting the unchanged stable bone ID. Rename and reparent create a one-level whole-asset rollback
snapshot before committing; failed mutations discard their staged snapshot and preserve the previous
history entry. **Revert** reloads skeleton, weights, clips, preview, hierarchy, and gizmos together.
Root nodes highlight only their joint because they have no incoming parent segment.
The hierarchy has its own scroll region, so expanding a large rig does not clip its lower branches
or push the selected-bone panel out of reach.

The selected-bone panel supports reparenting to another bone or to root. **Preserve global bind
pose** is enabled by default and recalculates local TRS so the bone does not jump; disabling it keeps
local TRS and intentionally lets the subtree move. Self-parenting and hierarchy cycles are rejected,
and the tree is rebuilt only after the complete canonical candidate validates.

**Edit local bind TRS** exposes parent-relative translation, quaternion rotation, scale, radius, and
length. Applying normalizes the quaternion and transactionally recompiles and validates the complete
canonical asset. Because this is a local bind correction, the selected bone and its descendants move;
child transforms are not silently compensated. Invalid input leaves the asset unchanged, and the
successful edit can be reverted through the shared one-level history.

Direct mouse manipulation is reserved for the editor-refinement phase. The numeric fields remain
available for exact values; future viewport translation/rotation/scale gizmos will feed the same
transactional bind-edit operation rather than maintaining separate skeleton state.

**Add bone** creates a root or child using a unique name and parent-relative translation. New bones
start with identity rotation/scale and inherit the selected bone's authoring radius/length; those
values can then be corrected through the local-bind fields. Addition allocates a new stable ID,
preserves existing weights/tracks, validates the complete canonical asset, selects the new bone, and
participates in the shared rollback history.

**Remove bone** first displays direct-child, weighted-vertex, and animation-track counts. The first
safe policy removes only a leaf absent from both the weight palette and all tracks, and requires an
explicit confirmation. Referenced bones remain blocked rather than silently reparenting children,
redistributing weights, or discarding animation. Successful removal selects the former parent when
available and can be reverted through the shared history.

The panel and bind-pose gizmo read the detached canonical-first bind report. The editor accepts its
bone snapshot only when `canonical == true`; it does not fall back to `getTotalBone/getBone` or
manufacture a legacy skeleton. Assets containing only exploratory skeletal sections must be
re-imported from FBX.

Open **Runtime Skeletal Preview** to select a canonical clip, play or restart it, pause/resume,
seek by time, or explicitly return the mesh to bind pose. Choose Auto, LBS, or rigid DQS in the same panel;
changing it rebuilds the preview so the method is selected before mesh loading and shader creation.
Auto selects DQS only if bind and all clips use unit scale; otherwise it selects LBS and shows the
reason. The panel reports requested/resolved methods and explains the limits directly: how many bones this mesh requires and the
maximum accepted by the current device for one mesh draw. Multiple mesh instances are evaluated
separately; the capacity is not a combined scene-wide bone budget. Bind restoration stops
the active player; it does not assume that time zero of an authored clip is the bind pose.
The slider is a lightweight playback scrubber, not the future Animation-node
timeline: it does not expose tracks or edit keys. The mesh deformation uses the runtime player and
matching GLES2 LBS or DQS palette. The bind-only diagnostic gizmo is hidden in this worktree so it
is not mistaken for either evaluated runtime instance.

Enable **Compare LBS / DQS pose stress** to replace the single preview with two runtime instances:
LBS on the left and rigid DQS on the right. Both receive the same clip, restart, pause/resume, seek,
and bind-restoration commands; the right instance is re-seeked to the left instance's time each
frame to avoid drift. The camera reframes both meshes automatically. This comparison is read-only,
and a DQS pose rejection is reported while the LBS instance remains visible.

The editor supports **Save**, **Save As**, and one-level **Revert Last Weight Operation**. Revert is
available only for the latest weight-changing operation in the current editor session.

## 3. Interface workflow

Only one worktree is open at a time. Opening another automatically closes the previous one and
updates the viewport. **Show Mesh** is shared. Skeleton visualization is contextual: Bind Pose
Contract displays the bind skeleton automatically, Skin Weight Lab provides **Show Skeleton** and
depth behavior, and Runtime Skeletal Preview hides the bind-only gizmo. Drawing a skeleton there
would require a separately evaluated gizmo for each animated LBS/DQS instance. Skin Weight Lab
preserves its state while closed, but its AABB, proximity capsule,
heatmap, analyzed markers, transition diagnostics, highlights, and editing controls are hidden and
inactive outside that worktree. Runtime LBS/DQS comparison geometry is likewise shown only in the
Runtime Skeletal Preview worktree.

Inside **Skin Weight Lab**, the controls remain organized in three numbered groups.

### 3.1 Visualization

- **Show Mesh** controls the mesh preview.
- **Show Skeleton** displays the stored bind skeleton inside Skin Weight Lab.
- **Skeleton Always on Top** keeps the skeleton visible through the mesh.
- **Analyzed Markers Always on Top** controls depth behavior for analyzed markers and diagnostic
  lines.
- **Heatmap for Analyzed Bone Weight** colors analyzed vertices by the selected bone's stored
  influence. Disabling it returns markers to the operation/diagnostic colors; red in that mode is
  not a weight value.
- **Highlight** places an always-on-top sphere on the selected analysis, proximity, or rigid-target
  bone where that control is available. Each role uses a distinct color.

The heatmap follows the conventional cold-to-hot scale:

```text
blue (0) -> cyan -> green -> yellow -> red (1)
```

Only positive stored weights contribute to the heatmap. Bone joint position alone does not imply
that the bone influences the analyzed vertices.

### 3.2 Selection and analysis

Choose one selection method, configure it, then press **Analyze Selection**. The resulting vertex
set is cached until selection geometry or another relevant analysis input changes.

#### AABB volume

The box selects vertices geometrically. It does not cut the mesh, create subsets, or duplicate
vertices.

- Drag the box directly in the 3D view or edit its minimum and maximum coordinates.
- **Size X/Y/Z** expands or contracts both opposing faces around the current center.
- **Drag sensitivity** controls numeric-edit speed; **Auto** restores a bounds-derived value.
- Each of the six faces has an independent transition enable and width.
- A disabled face is a hard selection boundary. Transition corners respect every crossed face, so
  a disabled crossed face blocks that corner's falloff.

The red inner box is the rigid core. The orange outer region is the transition shell used by Rigid
Bind with transition. A nonzero enabled face width means that face has a transition.

#### Material subset

Selects all vertices belonging to one stored material subset. Subset indices and vertex counts come
from frame 1.

#### Bone proximity

Selects vertices near one bone segment using an independent, scale-aware radius. The orange capsule
shows the active segment and radius. The optional nearest-segment filter excludes vertices for which
another bone segment is closer.

The **Proximity Bone** defines geometry selection. It is independent from the **Analyzed Bone** used
by the heatmap.

#### Analysis report

The report includes selected vertices, rigid-core/transition counts, non-normalized sums, unknown
bone references, and references excluded by the current allowed-bone filter. A heatmap warning about
zero selected influence does not invalidate geometric analysis or full-vector transition diagnosis.

## 4. Operations

Choose the intended operation after analyzing the selection. Operation-specific controls appear
only in their relevant context.

### 4.1 Inspect transitions

**Diagnose Abrupt Transitions** compares complete normalized weight vectors across triangle-adjacent
vertices. The threshold is the minimum vector difference considered abrupt.

- Magenta lines are abrupt edges whose endpoints are both inside the selection.
- Orange lines cross from the selection to an external neighbor.
- Internal and boundary edge/vertex counts are reported separately.
- Maximum difference is a property of the diagnosed data; changing only the threshold need not
  change it.

Adjacency follows stored triangle indices inside each subset. It does not currently weld duplicate
positions across UV seams or material-subset boundaries. This diagnoses stored-weight discontinuity,
not deformation in an animated pose.

### 4.2 Rigid Bind

Choose one **Target Bone** and press **Apply Rigid Bind**. Every analyzed core vertex receives weight
`1.0` for that bone.

With an enabled AABB transition shell, core vertices remain rigid while shell vertices blend the
target influence into their existing weights. Linear and Smooth falloff are available. Vertices
outside the outer shell are not written.

Use this for geometry that must behave as one rigid part, such as a mechanical component or the
hollow abdominal cavity in the alien-rat test mesh. A transition is usually required where rigid
and deformable surfaces share topology.

### 4.3 Normalize and Limit

For every analyzed vertex, this operation:

1. removes invalid, non-positive, or unusable entries;
2. merges duplicate bone references;
3. retains the four strongest effective influences;
4. normalizes their sum to `1.0`.

Vertices with no effective influence are intentionally skipped. The editor never invents a bone
assignment for them. The persistent report separates analyzed, corrected, already-valid, skipped,
and failed vertices. Running the operation again on cleaned data should report zero corrections.

### 4.4 Smooth selection

This operation performs triangle-adjacency smoothing over the complete analyzed selection.

- **Strength** controls how far each pass moves weights toward neighboring values.
- **Iterations** controls how many passes are applied.
- **Restrict Allowed Bones** filters which influences may survive smoothing.
- **Allow All**, **Clear All**, persistent highlighting, and hover highlighting help configure a
  large skeleton without invalidating the cached geometric analysis.

For joint work, inspect relevant bones individually with the heatmap. Permit every bone showing a
meaningful influence in the selected region; do not include progressively more distant limb bones
merely to hide a local tear, because that can stretch the limbs.

### 4.5 Smooth detected transitions

After diagnosing transitions, this operation smooths only the internal magenta vertex set. Orange
external endpoints remain read-only.

Before the write, the editor captures all four raw name/weight slots for every unique external
neighbor. The report then shows:

- external boundary neighbors verified;
- external boundary neighbors modified;
- external audit failures.

Any external modification or audit read failure is an operation error. A successful operation
automatically refreshes both internal and boundary diagnostics. Revert restores the pre-operation
snapshot.

## 5. Camera and 3D interaction

The camera frames itself from the loaded mesh bounds. Use WASD movement and orbit controls in the
camera panel; position and focus are editable. Camera movement and AABB numeric sensitivity scale
with the mesh, while the exposed AABB drag-sensitivity control can override an inconvenient default.

The engine manages renderable `z` values as part of render ordering. Diagnostic objects use explicit
always-on-top/depth behavior instead of treating a renderable's changing `z` as mesh-space geometry.
Weight calculations use stored vertex and skeleton coordinates, not the diagnostic marker render
order.

## 6. Recommended workflows

### Rigid cavity or mechanical region

1. Select the rigid interior with an AABB.
2. Keep unrelated geometry, such as the chin, outside the box.
3. Enable transition only on faces that meet deformable body geometry.
4. Analyze the selection.
5. Choose Rigid Bind and its owning torso bone.
6. Apply, save under a new name, export to FBX, and test a torso-turning animation.
7. Narrow or widen individual transition faces based on the observed boundary, not the rigid core.

### Neck or joint smoothing

1. Select the joint and a small amount of geometry on both sides.
2. Inspect the spine, neck, head, shoulders, and only the arms that visibly influence the region.
3. Enable allowed-bone restriction and select those locally relevant bones.
4. Diagnose abrupt transitions.
5. Start with low strength and one iteration.
6. Prefer Smooth Detected Transitions when the magenta set isolates the defect.
7. Compare diagnostic counts, revert if necessary, then validate the exported pose externally.

Diagnostics are guidance, not an automatic quality score. Fewer abrupt edges can still produce a
worse animation if inappropriate bones are introduced.

## 7. Persistence and FBX validation

Weight changes are stored in the mesh-v11 vertex skin-weight section. Saving the `.msh` preserves
the skeleton and edited weights. See [Mesh v11 Format](mesh-v11-format.md) for the binary contract
and [Bones, Armatures, Skin Weights, and FBX](bones-armatures-and-fbx.md) for import/export behavior.

The editor does not upload to or invoke Mixamo. Export through the existing FBX workflow, animate
the result in Mixamo or Blender, and compare the same animation and timestamps before and after an
edit. Preserve the source mesh and save experiments under new names.

## 8. Validation fixtures

The initial workflow was validated with the following assets. Availability is stated explicitly so
a historical validation artifact is not mistaken for a currently versioned fixture:

| Fixture | Availability | Purpose |
|---|---|---|
| `T-BONE-rato-scale-100-from-mixamo.msh` | Versioned under `src/test-lib/` | Main 41-bone alien-rat fixture for AABB, proximity, heatmap, smoothing, rigid cavity, persistence, and FBX tests. |
| `T-BONE-rato-scale-100-one-unweighted-vertex.msh` | Historical validation artifact; not currently versioned | Normalize exceptional case: one vertex must be skipped without receiving an invented influence. Recreate it deterministically before using it as an automated acceptance fixture. |
| `Crate.msh` | Versioned under `src/test-lib/` | Mesh without skeleton/weights and two-subset isolation (`192 + 24 = 216` vertices). |

Accepted normalization results for the one-unweighted-vertex fixture were `179` corrected,
`35,969` already valid, `1` skipped, and `0` failures. Boundary-safe targeted smoothing verified
five external neighbors with zero modifications and zero audit failures.

## 9. Current limitations and future work

The following are not defects in the delivered Skin Weight Lab:

- runtime preview is currently GLES2 only; there is no non-GLES backend selector;
- the diagnostic skeleton gizmo remains in bind pose during runtime preview;
- no protected/exclusion volumes;
- no topology-ring selection expansion;
- no welded/coincident-vertex adjacency across seams;
- no automatic heavy whole-mesh weight generation;
- no custom-tail animation generation;
- one-level revert rather than general undo/redo.

Future animation authoring/timeline, richer pose-stress overlays, antipodality tooling, and non-GLES
backend delivery remain in the
[Real-Time Skinning Animation Plan](realtime-skinning-animation-plan.md).
Skeleton and animation authoring/import remain in the product plan. Mesh Debug's legacy Bone
node/window has been retired; canonical bind inspection and weight repair belong to this editor.
