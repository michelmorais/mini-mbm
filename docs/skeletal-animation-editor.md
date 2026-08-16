# Skeletal Animation Editor

Status: **Bind/weight/runtime workflows implemented; canonical clip/track/key inspection started**
Last updated: **2026-08-14**

## 1. Purpose

The Skeletal Animation Editor is the standalone Mini MBM tool for inspecting and editing skeletal
mesh data. Its currently implemented workspace, **Skin Weight Lab**, repairs canonical type-42
frame-1 vertex weights without expanding Mesh Debug into a general animation editor.

For canonical skeletal meshes within the GLES2 palette limit, the preview can play the same
per-instance LBS or rigid-DQS deformation path used by the runtime. Non-GLES backend delivery
remains in the [Real-Time Skinning Animation Plan](realtime-skinning-animation-plan.md).

The editor is organized into six mutually exclusive worktrees: **Bone Editor**, **Bind Pose Contract**,
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

When a loaded static mesh has no canonical skeleton, Bind Pose Contract offers **Create initial
skeleton**. Its default root is placed at the center of the mesh's AABB base, with scale-aware radius
and length suggestions. Creation writes only the canonical skeleton; it does not fabricate weights
or clips. The new root can immediately be extended with the ordinary add/edit/reparent workflow and
the whole initialization can be reverted.

Inside **Add bone**, **Add chain** creates `prefix1..prefixN` beneath the chosen parent. Every bone
uses the same local translation step and becomes parent of the next; rotation/scale start at identity
and radius/length inherit the selected bone. The chain is validated and committed as one operation,
selects its last bone, and produces one rollback entry—partial chains are never retained.

**Mirror subtree** previews the number of duplicated bones, global X/Y/Z reflection plane, and name
prefix. On confirmation it mirrors full global bind matrices—not only joint positions—then derives
valid locals while preserving the copied hierarchy and assigning new IDs. Existing weights are not
guessed or mirrored. The current helper is limited to assets without clips; animation-aware mirroring
remains a later refinement. The mirrored root is selected and the complete operation is reversible.

After a local skeleton has been created but before any canonical weights exist, the selected-bone
panel offers **Initialize skin weights**. The impact preview states the complete frame-zero vertex
count and the selected bone. Explicit confirmation creates type-42 coverage by rigidly binding
every vertex to that bone with weight `1.0`; it does not infer envelopes or proximity. The operation
is transactional, participates in the whole-asset rollback, and opens **Skin Weight Lab**
immediately so influences can be redistributed with its analysis and blending tools. Existing
weights are never replaced by this action.

**Remove bone** first displays direct-child, weighted-vertex, and animation-track counts. The first
safe policy removes only a leaf absent from both the weight palette and all tracks, and requires an
explicit confirmation. Referenced bones remain blocked rather than silently reparenting children,
redistributing weights, or discarding animation. Successful removal selects the former parent when
available and can be reverted through the shared history.

A referenced leaf exposes **Transfer weights to**. Choosing an explicit replacement transfers its
palette entry; overlapping influences on the same vertex are summed rather than duplicated. Tracks
are never retargeted as if their local transforms belonged to another bone: removal remains disabled
until **Discard this bone's animation tracks** is explicitly checked. Child-bearing removal follows
the separate policy below.

For a child-bearing bone in an asset without clips, **Reparent children and preserve global bind**
promotes its direct children to the removed bone's parent, or to roots when removing a root. Their
local TRS is derived again so each global bind remains unchanged. With canonical clips, promoted
children receive full-TRS tracks baked at the union of their own and the removed bone's authored key
times, plus clip boundaries. Global poses are preserved at those samples; the editor warns that
continuous interpolation between samples can differ. A composition requiring shear rejects the
whole operation.

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

Open **Create / Edit Animations** to inspect the canonical type-43 structure before editing is
enabled. The node selects a named clip and displays its stable ID, duration, looping policy, tracks,
target bone identity, T/R/S channel mask, and every key's time, local quaternion TRS, easing, and
Cubic-Bezier controls. Selecting a track synchronizes the editor's selected bone index. This first
Milestone-6 surface is deliberately read-only: it has no timeline, key insertion/removal, or clip
mutation yet, so imported animation cannot be changed accidentally while the authoring transaction
model is still being introduced.

The same node can create an empty clip and update the selected clip's name, duration, and loop
policy. Clip IDs remain unchanged when properties are edited. A duration reduction that would
exclude an existing key is rejected rather than truncating or moving animation data. Clip removal
requires explicit confirmation because it removes all contained tracks and keys; removing the final
clip also removes canonical type-43 storage. These operations use the shared whole-asset rollback
boundary. Track/key mutation and timeline authoring remain unavailable in this slice.

Track-container authoring is now available for the selected clip. The editor lists only bones that
do not already have a track there, accepts any nonempty T/R/S channel combination, and creates the
track with one key at time zero copied from the bone's local bind TRS. This seed prevents an invalid
empty-track intermediate and initially evaluates to bind pose. Existing track channel masks may be
changed transactionally; stored key values remain intact and are revalidated under the enabled
channels. Track removal requires confirmation because all of that track's keys are removed with it.
Key-value editing and the timeline remain the next authoring layer.

Keyframe authoring is available inside each expanded track. A new key time must be unique and lie
inside the clip; insertion samples the existing clip and captures that bone's evaluated local TRS,
so merely adding a key preserves the current curve. Expanded keys expose editable time,
translation, quaternion rotation, scale, easing, and Cubic-Bezier controls. Applying normalizes the
quaternion, reorders a moved key by time, and validates the complete type-43 collection. Removal
requires confirmation and the final key cannot be removed because canonical tracks may not be
empty. Every insertion, update, and removal uses whole-asset rollback. A graphical timeline and
pose-oriented viewport controls remain subsequent Milestone-6 work.

The Animation worktree now also has the shared in-memory pose contract needed by those controls.
Its time scrubber evaluates the current unsaved clip directly from `meshDebug`, installs the packed
LBS/DQS palette on the preview instance without saving or reloading, and rebuilds the visible
skeleton from the same evaluated global transforms. Consequently the mesh and skeleton show the
same pose while editing. This slice deliberately stops at evaluation/preview: mouse picking,
translation/rotation gizmos, auto-key policy, and the graphical timeline are the next interaction
layer; numeric key fields are diagnostic/fallback controls rather than the intended primary UX.
Bone selection is now viewport-driven as well as tree/track-driven. Clicking either an evaluated
joint or its parent-to-child segment performs a nearest-hit ray test and selects the child bone;
dragging empty viewport space continues to orbit the camera. This establishes the selection
semantics that translation and rotation gizmos will consume without yet mutating the pose.

The selected animation bone now displays world-space X/Y/Z translation handles. Dragging a handle
computes displacement along that axis, converts the world delta through the inverse parent basis,
and feeds the resulting local translation back into the one-bone in-memory override contract on
every move. Mesh, evaluated skeleton, and gizmo update together. The result is explicitly marked
temporary and can be discarded; mouse release does not silently create or overwrite a key. This
deliberate boundary lets the next slice define auto-key versus explicit commit without conflating
viewport mechanics with persistence policy.

Temporary translation can now be committed explicitly with **Create / update translation key**.
The atomic operation finds or creates the selected bone's track, enables its translation channel,
and creates or updates the key at the current authoring time. It validates the complete canonical
animation before replacement and participates in whole-asset rollback. Mouse release still
performs no persistence, and Auto Key remains deliberately disabled.

If the editable pose cannot be installed on the runtime preview, the editor reports the failed
contract clause instead of a generic incompatibility. Diagnostics distinguish unavailable skeletal
GPU input, palette capacity, resolved-method mismatch, row/bone counts, non-finite data, and the
first ordered stable-bone identity mismatch. Assets without canonical runtime weights or whose
unsaved skeleton order differs from the file-backed preview are therefore actionable rather than
appearing as an unexplained gizmo failure.
In particular, a canonical skeleton and clips can animate the bone gizmo without being capable of
deforming a mesh: type-42 canonical vertex weights are also required. This condition is reported as
`invalid-canonical-data (canonical vertex weights are missing)` rather than being treated as an
identity or shader problem.

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

## Bone Editor

**Bone Editor** is the mouse-first bind-construction surface for ordinary users; **Bind Pose
Contract** remains the advanced diagnostic and matrix-oriented surface. Both edit the same canonical
skeleton. In the Bone Editor presentation, one bone is shown as a head joint, a tail joint, and the
segment between them. The persisted representation remains one canonical transform node, but now
also stores its explicit second joint as a bone-local tail offset. This is authoring geometry, not
an unrelated second skeleton, and it follows the bone's evaluated global pose.

The first delivered slice provides X/Y/Z, a positive **Bone length**, and **Add Bone**. With no
prior selection it creates an independent root whose head is at that position and whose derived
tail is initialized at that length along local +Y. Length defaults to 1. **Add Joint** creates only
the hierarchy transform and therefore has no selectable tail or segment; **Add Bone** creates the
explicit endpoint and segment. Imported FBX bones retain Blender's actual head and tail even when
coordinate conversion changes the visual aim axis.

Extending a selected explicit tail creates a child whose local head is exactly that tail offset and
sets `connectedToParent=true`. This explicit constraint is what later joint dragging will use to
move the shared parent tail and child head together; ordinary parenthood does not imply connection.
The integer immediately before **Extend Selected Tail** chooses between 1 and 256 new segments; the
whole chain is committed atomically. Every new tail continues the selected segment's parent-local
direction. When the selected segment has a parent, its own length is inherited by every new segment;
for a root, the **Bone length** field supplies the repeated length. This includes bones whose
imported aim axis is not local `+Y`.

The **Snap X/Y/Z** controls constrain joint and segment dragging in global space. With one or more
axes checked, only those coordinates receive the mouse displacement; unchecked coordinates remain
at their gesture-start values. With all three unchecked, dragging remains freely projected on the
camera-facing plane. A selected endpoint/segment also exposes always-on-top red X, green Y, and blue
Z handles; dragging one handle temporarily constrains only that gesture without changing the
checkboxes. **Snap step** quantizes global displacement to the requested positive interval, while
zero disables quantization. Endpoint/segment picking has priority over handles, only the outer 65%
of a handle is pickable, and camera-near-parallel axes do not capture input. The active axis remains
fully colored while the other two dim during the gesture.

Dragging an explicit tail uses a camera-facing plane through the endpoint. The resulting world-space
point is converted back through the bone's global bind basis into `tailOffset`; the head remains
fixed, length is recomputed, and explicitly connected child heads follow the shared joint. One
rollback snapshot covers the complete gesture rather than producing one history entry per frame.
During the gesture the editor requests a lightweight bind report that omits weight/track impact
counts and caps expensive gizmo reconstruction to roughly 30 Hz; the complete report is restored
on release. Tail-only mutation also skips redundant full weight/clip validation because it cannot
change IDs, palettes, vertex influences, clip IDs, or track targets.

Picking treats a parent tail and every child head marked `connectedToParent` as one logical joint;
clicking or dragging any coincident member highlights and moves the whole group through the owning
parent tail. Coincident endpoints without an explicit connection remain independent. Repeated
clicks cycle deterministically through candidates at the nearest depth, so zoom is not required to
reach an obscured tail. Cycling occurs only on left-button release when the pointer moved no more
than three pixels. On press, an already highlighted candidate remains locked; moving beyond that
threshold starts its drag and release does not change the selection.

An independent head can also be dragged on the same camera-facing plane. Its new world point is
converted into parent-local translation, its explicit tail remains fixed in global bind space, and
the segment is therefore stretched from its initial endpoint. Moving a connected logical joint
instead edits the owning parent tail; connected child heads follow while each child's opposite tail
is preserved, reshaping both adjacent segments rather than translating the child segment wholesale.
**Preserve other joints** is enabled by default: descendants outside the joint being manipulated
have their local bind TRS compensated so their global joints stay in place. Disabling it restores
ordinary hierarchical propagation, allowing the edited joint to carry the descendant subtree.
Dragging the body of a selected segment translates its head and tail together, retaining its length
and orientation. A connected child head follows the displaced tail; the same preservation checkbox
decides whether the rest of the hierarchy stays globally fixed or follows that movement.
The explicit **Move/Rotate** segment tool removes interaction ambiguity. In **Rotate**, dragging the
segment keeps its head and displayed global length fixed and changes only the tail direction;
connected heads follow that tail, and Preserve other joints retains its existing compensation
meaning. Global X/Y/Z constraints also apply to the rotated endpoint before conversion back to
bone-local space. The required local tail magnitude is derived through the complete bind linear
basis, so non-uniform ancestor scale cannot visually stretch or shrink the rotating segment.
When Rotate is selected, a yellow camera-facing orbit guide is drawn around the fixed head with the
current visual segment length as its radius. A cross marks the fixed head, a radial line connects it
to the tail, and the XYZ gizmo is anchored at that head. The center and radial markers are offset a
small amount toward the camera so the always-on-top skeleton does not occlude them. Moving translates
both endpoints; rotating keeps the head at the guide center and constrains the tail to that circle.
In this tool mode, clicking an owned head, tail, or shared joint is interpreted as selecting its
owning segment and immediately starts segment rotation; Move mode retains direct joint manipulation.
The XYZ axis renderables keep origin-relative line geometry and place the renderable itself at the
selected pivot, ensuring the always-on-top pass sorts them at their real world location while editing.
The Animation-worktree translation gizmo follows the same rule and updates its persistent line
objects in place while previewing an unsaved pose, avoiding destroy/recreate trails during dragging.
The Animation worktree now offers explicit Move and Rotate tools. Rotate draws persistent local
X/Y/Z rings at the evaluated joint; dragging a ring applies a normalized quaternion override to the
shared in-memory pose, so the deformed mesh and evaluated skeleton update together without changing
the bind pose or clip. Mouse release remains temporary. The user must explicitly create/update the
rotation key (channel R), or discard the temporary pose.
During interactive Move/Rotate preview, Animation uses persistent joint shapes and parent-child line
segments. Pose refresh updates their positions and dynamic line buffers in place; it does not destroy
and recreate the complete skeleton for every mouse event. A structural selection/load change still
performs the normal complete rebuild.
Auto Key is explicitly opt-in and defaults off. When enabled, releasing a Move or Rotate gesture
that actually changed the evaluated pose commits one undoable key for only that gesture's channel
(T or R). A click without movement creates no key. When disabled, mouse release retains the
temporary override for explicit commit or discard.
The graphical timeline occupies a separate horizontal window along the bottom of the editor. Its
left edge follows the actual right edge of the resizable worktree panel and its right edge reaches
the screen boundary, leaving the panel unobstructed. It draws a time ruler, one
scrollable row per canonical track, wide bone/channel labels, key markers, and the current authoring playhead. Clicking empty ruler/row space seeks
the shared in-memory pose. Clicking a key selects its track and bone and seeks exactly to its stored
time. Only vertically visible track rows submit labels, lines, and key draw commands, avoiding
per-frame work for off-screen tracks. A selected key can be dragged horizontally: the marker and
playhead preview the clamped time in memory, collision with another key in the same track is shown
in red and rejected, and mouse release performs one backend-validated update plus one rollback entry.
For usable pointer feedback, entering an eight-pixel zone around another marker snaps the preview to
that key's exact time and activates collision state; the dragged marker is drawn last so its larger
red indicator cannot be hidden by the destination marker.
The key's T/R/S/easing payload is preserved while canonical ordering is recomputed by the backend.
Timeline selection is distinct from mutation. A normal click selects one key; `Ctrl+click` toggles
independent keys across tracks, selected markers remain yellow, and the window reports/clears the
selection count. Pressing a selected marker without Ctrl preserves the group and drags every selected
key by one shared time delta. Preview clamps the group by its earliest/latest key and checks every
moved key against unselected keys in its track. Release calls one candidate-copy backend operation;
all affected tracks reorder and validate together or none changes.
The track canvas consumes the complete remaining vertical area of the freely resizable timeline
window, so increasing window height reveals more rows before scrolling. Dragging from empty timeline
space draws a translucent selection rectangle and selects every visible marker inside it on release;
without Ctrl it replaces the selection, while Ctrl adds the rectangle result to the current set. A
stationary empty-space click retains its seek behavior.
The detached canonical clip/track/key report is cached while the asset is unchanged. Skeleton or
animation mutation invalidates it through the shared bind/report refresh boundary. This avoids
rebuilding and garbage-collecting the complete nested Lua report every frame while Animation is idle.
The timeline window receives its panel-relative bottom position and remaining width only on first
appearance; afterward it is freely movable and resizable by the user.
Timeline authoring playback advances `authoringTime` through the same in-memory pose evaluator used
by seek and gizmos; it does not start a separate runtime player. Play/Restart begins at zero,
Pause/Resume preserves time, Stop returns to zero, speed is clamped to `0.05x..4x`, and clip loop is
honored. Non-looping playback stops at duration. Timeline seeking or beginning a transform gesture
pauses playback before editing, and playback itself never creates or changes keys.
For the selected explicit segment, the panel reports its normalized local direction, current length,
inclination from local `+Y`, and azimuth in the local `XZ` plane. These values update during drag.
Roll is deliberately reported as undefined: head and tail determine an axis but cannot determine
twist around that axis, and this endpoint tool does not silently rewrite the canonical bind quaternion.
During any Bone Editor head, tail, or segment gesture, `Esc` or right-click reloads the complete
pre-mouse-down skeletal snapshot. Cancellation restores the prior modified state, keeps the selected
stable bone when it still exists, creates no history entry, and rebuilds reports, preview, and gizmos.
The selected segment also draws a short yellow always-on-top arrow from its head toward its tail.
The arrow follows the endpoint continuously during rotation, providing spatial direction feedback
directly at the joint instead of relying only on numeric angles. One persistent line object is
updated in place during the gesture, preventing stale arrow geometry from accumulating between
render frames.
For a selected non-root bone, **Connect head to parent tail** establishes the explicit shared-joint
constraint against its current parent. The head snaps to that tail while the selected segment's
global tail is preserved; Preserve other joints controls compensation of the remaining hierarchy.
**Disconnect from parent tail** only clears the constraint and moves no geometry. These actions do
not change parenthood: choosing another parent remains a distinct structural operation.

The selected bone's **Remove Bone** section exposes the existing transactional deletion contract in
the mouse-first workspace. It reports child, weighted-vertex, and animation-track impact; strict
unreferenced leaves need no remap, while referenced/branching bones require an explicit replacement,
global-preserving child promotion, and track-discard confirmation where applicable. The last bone
cannot be removed. While the removal section is open, the selected weight-transfer target is
highlighted in green across its joint, tail, and segment; the bone being removed retains the blue
selection color. Success selects the surviving parent and creates one rollback entry.

**Joint radius** edits the selected bone's positive visual/picking radius, optionally applying one value to
its complete descendant subtree. The nonnegative change is atomic, refreshes viewport geometry and
selection tolerance immediately, and creates one rollback entry. Radius remains authoring metadata:
it does not create envelopes or modify canonical vertex weights. The radius field uses a
scale-proportional `DragFloat` interaction for quick visual adjustment.
This makes a bone visible even on a static mesh that began without any skeleton. Connected extension
and mouse manipulation of joints/segments are intentionally the next slices.

Viewport picking distinguishes the initial joint, final joint, and bone segment. A selected joint
highlights only that endpoint; selecting the segment highlights the segment and both endpoints.
Clicking empty viewport space clears the selection and remains available for camera orbit. This
slice records selection intent only and does not yet move bind data.

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
- Runtime Skeletal Preview deliberately hides its bind-only diagnostic gizmo; the Animation
  worktree instead displays the evaluated in-memory pose skeleton;
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
