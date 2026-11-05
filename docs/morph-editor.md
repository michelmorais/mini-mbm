# Morph Editor

The Morph Editor is a generic tool for authoring surface shapes on any MSH mesh. It does not
require a face origin, facial landmarks, facial roles, or FACS data. The launcher entry is
`Morph Editor`, implemented by `editor/morph_region_editor.lua`.
Its topology, shape geometry, and region data come from `morph_surface_topology.lua`,
`morph_surface_polygon.lua`, `morph_surface_shapes.lua`, and `morph_surface_regions.lua`.
Linux and macOS builds with Lua also create the `morph_editor` shell alias in `shortcuts.sh`.
On launch the editor opens to an empty workspace. Choose **File > Open Mesh** or **File > Open
Project** when ready; no file dialog appears automatically. **File > Quit** exits the editor, and
**Options > Language** switches between English and Portuguese (Brazil). **Ctrl+N** starts a blank
project, **Ctrl+O** opens a project, and **Ctrl+S** saves the current project. **Ctrl+Z** and
**Ctrl+Y** undo and redo completed paint strokes.

The Morph Editor currently presents five sequential worktree nodes: **Surface shapes**,
**Morph targets**, **Expressions**, **Clips**, and **Preview**. The first worktree follows the interaction model of the
FACS mask editor while keeping the data generic: the user chooses a shape, places it on the mesh,
and adapts its handles to the object. Its operation modes are **Add shape**, **Edit shape**,
**Mirror shape**, and **Remove shape**; each mode exposes only the controls needed for that
operation. Mirror shape previews a reflected copy across the origin plane of the selected X/Y/Z
axis, keeps the source unchanged, and creates the new shape only after explicit confirmation.

**Morph targets** is enabled only after at least one non-empty surface selection has been rebuilt.
Each worktree is a separate editor node; the Add/Edit/Remove radios belong to the currently
opened worktree and are not worktree selectors.
When **Morph targets** is active, the source shapes remain visible only as inactive reference
geometry: their handles cannot be selected or edited, and the active point selection is cleared
when changing worktrees.

**Expressions** is enabled after at least one ready Morph Target exists. It stores named generic
poses as normalized weights referencing targets by stable target ID. It has Add, Edit, and Remove
modes. Add mode edits a temporary weight draft and creates the expression explicitly. Edit mode
changes the selected expression name and weights; Remove mode deletes only the expression. FACS
Action Units, profiles, anatomy, and muscle relationships are not evaluated in this worktree.
The viewport composes the referenced target deltas in memory and applies the existing preview-weight
slider from neutral at `0` to the complete expression at `1`. The source MSH and target records are
not changed by preview. Surface-shape contours and handles are hidden while this worktree is active.
The target-weight slider uses a fixed, wider table column so its value can be edited precisely.
Missing or stale target references remain visible in the expression status.

**Clips** is enabled after at least one valid Expression exists. A Morph clip is a generic timeline
of stable Morph Target IDs and normalized weights. Add mode creates a clip with neutral keys at
time zero and at the end of its initial duration. Edit mode inserts Expression or neutral keys,
changes duration, frame rate, looping, and interpolation, and previews the sampled pose in memory.
Remove mode deletes only the selected clip. The clip worktree does not evaluate FACS Action Units,
does not change Expressions or Morph Targets, and hides surface-shape contours and handles. Its
timeline is shown in a separate window, matching the Facial Rig Editor workflow.

## Shape modes

The new-shape selector provides:

- **Square**: starts as a square. In locked mode it has a center controller and three resize
  handles: uniform size, width, and height. The uniform handle preserves a square; the width and
  height handles can turn it into a rectangle without enabling Free.
- **Circle / ellipse**: starts as an ellipse. In locked mode it has a center controller and
  three resize handles: uniform size, width, and height.
- **Free polygon**: starts with a visible four-point contour when the viewport can hit the mesh and
  exposes each anchor for independent editing.

Adding a shape always assigns a unique name. If the entered name already exists, the editor adds a
numeric suffix. After a shape is added, the editor automatically enters **Edit shape** for the new
selection.

The **Free** flag is available for Square and Circle / ellipse. A free shape keeps its initial
template points but then behaves as an editable polygon, so each anchor can be dragged to create
an arbitrary contour. Locked Square and Ellipse shapes are always closed; free polygons can be
opened or closed explicitly.

The center controller moves the complete shape. Resize controllers change only the shape
dimensions. All shape controls are drawn in the current camera plane and the selected worktree
shows the handles over the selected surface contour.

## Current scope

- Open a mesh and create named Square, Circle / ellipse, or Free polygon shapes.
- Creating a Square or Circle / ellipse places it at the center of the current viewport when the
  mesh is hit there; drag its center or resize handles afterward.
- Enable Free to drag individual anchors, insert points on a closed contour, and remove points.
- Orbit with the left or right mouse button, pan with the middle button, and zoom with the wheel.
  Horizontal orbit follows the same direction as the Facial Rig Editor.
- Use the Morph viewport window to frame the mesh or selected region, toggle mesh visibility,
  orbit with the camera gizmo, and inspect the camera distance and navigation help.
- Rebuild the shape's derived vertex selection and inspect the list of covered vertices.
- Adjust the selected shape to the mesh surface, independently for every point. The default test
  axis is Z; X and Y are available for generic objects. Both directions of the selected axis are
  tested and the nearest surface is used.
- In **Mirror shape**, select a source shape, review the temporary orange mirrored contour, adjust
  its suggested `<source_name>_mirror` name or mirror axis, and press **Create mirrored shape**.
  The new shape is selected in **Edit shape** afterward for small adjustments.
- Save and reopen the authoring project as a Lua `.morph.lua` sidecar.
- The main editor header shows the project name and selected mesh path. It intentionally does not
  show the mesh vertex count there.
- In **Morph targets**, create an independent target selection from a rebuilt shape selection,
  inspect its source and topology status, regenerate it explicitly after source changes, and apply
  weighted Translate, Rotate, or Reset to neutral operations to its authored deltas. Rotate mode
  shows three colored rings and three axis handles: rings edit the rotation draft and axes move the
  persisted pivot. Soft edge rings include neighboring triangle-connected vertices with gradual
  influence. Edit mode also previews the selected target in the viewport with a neutral-to-full
  contribution slider. Mirror target uses the selected target's source polygon, reflects it across
  the origin plane of the selected X/Y/Z axis, and shows a temporary polygon and target preview.
  It reports unmatched or ambiguous pairs; only confirmation creates the new polygon and target.
  The source target is never modified.
- In **Expressions**, create a named expression from one or more Morph Target weights. Edit the
  target sliders to inspect the composed preview, then create the expression explicitly. Select an
  existing expression in Edit mode to change its name or weights. Remove mode deletes the selected
  expression without changing its targets. Expressions remain project-only data; their weights can
  be captured into Morph clip keys for runtime export.
- In **Clips**, create a named timeline after at least one valid Expression exists. Insert the
  selected Expression or a neutral pose at the current time, edit duration, frame rate, looping,
  and Step/Linear interpolation, then use Play, Stop, and the time slider to inspect the sampled
  composition. Clip preview uses the same in-memory target overlay and does not rewrite the source
  MSH. Clips remain persisted in the project and can optionally be embedded as native facial clips
  when exporting an MSH.

The project stores the mesh path, a topology signature, shape type and mode, canonical surface
anchors, the selected shape, morph-target selections, named expressions, Morph clips, and rebuildable vertex selections. The
derived region selection is calculated from neutral mesh vertices near the resolved polygon plane; an authoring
offset changes the polygon point but does not force its original neutral vertex into the selection. The derived region selection is binary at
this stage. Falloff settings are retained by the shared region data model but do not yet affect the
selection. A changed topology is reported and disables shape editing. To author against a
different topology, open that mesh as a new project; the existing point indices are not
automatically transferred. The current project schema is version 6; older Morph Editor project
files are intentionally rejected while this editor is being developed.

The project also persists the editor preferences used to continue the same authoring session:
language, selected worktree, Add/Edit/Mirror/Remove modes, selector size, surface-snap axis,
new-shape type and Free flag, mirror-shape name/axis, target name and translation drafts, target
mirror name/axis/tolerance options, the draft shape name, mesh visibility, target preview weight,
target displacement visibility, expression mode/name draft, clip mode/name draft/source expression,
selected clip key, clip preview weight, Preview mode and source selections, the MSH export clip
option, and the Morph viewport camera. These settings are restored when the project is opened and
are part of the project dirty
state when changed.

## Editing a shape

1. Open an MSH mesh and create a named shape.
2. Create a Square, Circle / ellipse, or Free polygon; it is placed at the viewport center when
   possible. A Free polygon starts with a four-point contour. If the center ray does not hit the
   mesh, click the mesh to begin placing its surface points. The new shape receives a unique name
   and opens automatically in **Edit shape**.
3. In locked mode, drag the center or one of the shape-specific resize handles. Enable Free when
   the contour should be edited point by point.
4. For a free polygon, add at least three points and close the contour.
5. Rebuild the selection to update the list of covered mesh vertices.
6. To duplicate a shape symmetrically, switch to **Mirror shape**, review the temporary orange
   contour, and press **Create mirrored shape**. The source remains unchanged and the new shape
   opens in **Edit shape**.
7. Save the project. Reopen the `.morph.lua` file to continue authoring.

## Morph targets

1. Rebuild a closed shape selection until its status is ready.
2. Open the **Morph targets** worktree, keep **Add target** selected, choose the source shape, and
   create a target. Selecting a source shape suggests `<source_name>_target` as the target name;
   the user can change it before creation.
3. Switch to **Edit target** to inspect the source fingerprint, topology status, selected vertex
   count, authored delta count, and the target preview.
4. In **Edit target**, choose **Translate**, **Rotate**, or **Reset to neutral**. Translation and
   rotation use the source selection weights; rotation uses the selection's weighted center as
   its pivot. A non-zero Translate or Rotate draft is shown immediately with the textured/default
   material, including soft edge weights, but remains in memory. Press **Apply edit** to commit the
   chosen operation; changing back to zero or applying the edit restores the selected preview mode.
   After applying, the current preview weight remains active and the preview is rebuilt from the
   updated target deltas. **Undo edit** and **Redo edit** immediately after **Apply edit** restore
   or reapply the last committed Translate, Rotate, or Reset operation. The batch selection is
   recalculated from the source region for each operation, so a previous feathered edit does not
   grow the next selection cumulatively.
   Soft edge rings are intentionally additional influence beyond the polygon: use `0` when the
   polygon must be the strict limit. If a target was already authored with extra rings, apply a
   new edit or rebuild its target selection after changing the ring count to discard those old
   feather vertices.
   If the editor reports the source region as stale after this selection algorithm change, rebuild
   the region first and then rebuild the target selection; this removes stale anchor-derived
   vertices from an older project state.
5. Use **Preview weight** to inspect the neutral mesh at `0` and the full target contribution at `1`.
   **Show target displacement** selects the visualization material: enabled uses the dynamic
   displacement shader, while disabled keeps the displaced geometry visible with its original
   texture and material when available, without the custom shader. The source mesh remains
   unchanged and the target is updated in memory.
6. Switch to **Paint target**, choose **Move** or **Smooth**, adjust the brush radius and strength,
   then right-drag over the displaced target surface. In **Smooth**, choose the number of surface
   smoothing passes. **Protect surface creases** is enabled by default: it prevents averaging
   across sharp triangle-normal changes, which helps preserve lip openings, eyelids, nostrils, and
   other narrow folds. Adjust **Max surface angle** for the mesh or disable the protection when
   smoothing across a crease is intentional. The operation follows triangle adjacency and remains
   inside the target selection. The stroke is accumulated while the right button is held, and the
   selected target preview is updated when the stroke is released, matching the facial editor
   workflow. Release the button to commit one stroke, or leave it unchanged when the stroke
   produces no delta. **Undo brush stroke** and **Redo brush stroke** restore completed strokes for
   the selected target. Left-drag still orbits and the middle button pans.
7. Switch to **Mirror target**. The new target name is initially suggested as
   `<source_name>_mirror`. Choose the mirror axis in this mode, immediately before **Analyze mirror**.
   The selected target's source polygon and target are temporarily mirrored across the origin plane
   of the selected axis (X by default). The source polygon remains visible
   as a reference while the candidate is shown in orange. Adjust the name, axis, and match radius,
   review the candidate and diagnostics, then press **Create mirrored target** to persist the new
   polygon and target. The source remains unchanged. To repeat the operation with another target,
   select it, choose X/Y/Z, and use **Analyze mirror** before confirming.
8. If the source shape changes, use **Rebuild target selection** explicitly. A stale target is not
   edited or silently regenerated.

## Expressions

1. Rebuild at least one surface shape and create a ready Morph Target.
2. Open the **Expressions** worktree. In **Add expression**, enter a name and set one or more target
   weights. The composed result is shown in memory through the preview-weight slider.
3. Press **Create expression** to persist the named pose. The expression stores target IDs and
   normalized weights; it does not copy or modify target deltas.
4. In **Edit expression**, select an expression to change its name or weights. In **Remove
   expression**, delete only the selected expression. Missing or stale target references are reported
   in the table and are not silently removed.

Expressions are generic Morph data. They do not evaluate FACS profiles and are not stored as a
separate runtime object in MSH. The Clips worktree consumes the saved expressions as keyframe
sources; exporting a clip serializes the resulting target weights as native facial clip keys.

## Clips

1. Create and save at least one valid Expression, then open the **Clips** worktree.
2. In **Add clip**, enter a unique name and create the clip. The initial clip contains neutral
   keys at time `0` and at the end of its one-second duration.
3. In the separate timeline window, choose an Expression source and insert it at the current
   time. **Capture current key** stores the currently sampled Morph pose; use **Reset Morph weights**
   before capturing when a neutral key is needed. Existing keys at the same time are replaced.
4. Adjust duration, frame rate, looping, and Step/Linear interpolation. The editor scales key
   times when duration changes so the authored timing remains proportional.
5. Use the separate clip timeline window to move the time slider or press **Play**. The preview samples target weights in memory and uses
   the clip preview weight to blend from neutral to the sampled pose. Stop returns to time `0`.
6. Use the key table to inspect and select keys. **Remove selected key** can remove any key,
   including an endpoint; the clip validation reports when the required time-zero or end key is
   missing. **Remove clip** deletes only the selected clip.

Clips retain stable Morph Target IDs and report missing or stale target references. They do not
introduce FACS metadata or modify saved Expressions or Morph Targets. The MSH export dialog can
optionally convert valid project clips to native facial clips; existing embedded clips with the
same name are replaced in the derived MSH.

## Preview

1. Create at least one valid Expression. The **Preview** worktree becomes available after the
   expression is ready; it is intentionally read-only with respect to authored Morph data.
2. Select **Preview expression** and choose a saved Expression. Inspect the composed pose with
   **Preview weight**, from neutral at `0` to the complete pose at `1`.
3. Select **Preview clip** and choose a saved Morph clip. Scrub **Time**, or use **Play** and
   **Stop** to inspect the sampled clip pose. Looping and interpolation follow the saved clip.
4. Toggle **Show displacement** to compare the displacement shader visualization with the textured
   displaced mesh. This toggle applies to the static overlay path; runtime comparison and skeletal
   playback use the textured runtime MSH directly. Surface shapes and their selectors remain hidden
   in this worktree.
5. Use **Runtime comparison** to inspect the loaded skeletal mesh with **facial CPU vs GPU** or
   **facial LBS vs DQS**. The editor creates a runtime copy with the project Morph Targets loaded
   as native facial targets, places the two resolved paths side by side, and reports the execution
   path and skinning method for each copy. The selected Expression/Clip pose and preview weight are
   applied to both copies. LBS/DQS is available only on backends that support DQS and only when the
   mesh resolves a skeletal method.
6. Use **Skeletal animation** to select a skeletal clip and press **Play**, **Pause**, **Resume**,
   **Stop**, or scrub its time. The Morph pose and skeletal animation are applied to the same runtime
   mesh, so they can be inspected together. In comparison mode, the selected skeletal clip and time
   are synchronized across both runtime meshes.
7. Save and reopen the project. The selected Preview mode, Expression/Clip source, preview weight,
   runtime comparison mode, skeletal clip selection, language, worktree, and other editor preferences
   are restored; playback itself is not persisted and restarts stopped.

Preview composes stable target IDs in memory and reports missing or stale references instead of
silently dropping them. During clip playback it updates the existing dynamic surface buffers, so
the preview does not generate a temporary mesh on every frame. Runtime comparison and skeletal
playback use a temporary runtime MSH generated from the source and project Morph Targets. The source
MSH and project data are not changed.

Use **File > Export morph targets and clips to MSH** to open the export window. Choose the normal policy and
whether to include project Morph clips, then choose the output path. The authoring mesh and Morph
Editor project remain unchanged. The
export copies the source mesh, replaces existing targets with the same names, materializes the
generic sparse deltas as dense position deltas, and appends new targets as `Manual` targets. All
targets must be ready before export. The derived file removes optional normal-map data because the
facial runtime uses the geometric normal path for meshes with morph targets. When such a facial MSH
is loaded, the runtime also ignores optional tangent batches, normal-map material settings, and
normal texture slots in its in-memory render copy; the source MSH is not rewritten.
Texture files are not copied; their source references are preserved in the derived MSH. **Preserve
existing** keeps normal deltas already stored in source targets and writes zero normal deltas for
new targets; **Recompute from surface** derives normal deltas from the deformed triangle surface
for every exported target.

When **Include project Morph clips in MSH** is enabled, the exporter validates every project clip,
maps its stable target IDs to the final target order in the derived MSH, and writes its keys through
the native facial clip format. Existing source clips are preserved unless a project clip has the
same name, in which case the project clip replaces it. Expressions themselves remain in the
`.morph.lua` project and are not embedded separately.

While **Morph targets** is active, **Edit target** remains navigation-only. **Paint target** uses
right-drag on the displaced surface to apply local Move or Smooth edits; left-drag orbits and the
middle button pans. Shape anchors are editable only in **Surface shapes**. Selecting a target
selects its source shape for inspection without changing the source geometry.

This initial target worktree stores generic authoring data. Its editor preview uses a dynamic
in-memory surface overlay and does not modify or regenerate the source MSH while the preview weight
changes. Paint strokes are accumulated in memory and committed when the right button is released;
the preview rebuilds once per completed stroke. The explicit MSH export copies the source mesh and
writes the generic targets as dense runtime morph targets without changing the authoring source;
runtime reload validation is complete for the exported MSH workflow, including project Morph clips,
smoothing now supports configurable surface-neighbor passes, and export supports explicit
normal-delta preservation or surface recomputation. Mirroring is available through the explicit
Analyze/Create workflow described above.

Opening or closing a free contour and removing an anchor invalidate the derived vertex selection
without scanning the mesh immediately. Use **Rebuild selection** when the contour is ready; this is
the explicit operation that performs the potentially expensive mesh scan.

Adjusting a shape to the surface also invalidates its derived vertex selection. The operation only
changes the authored point offsets; it does not regenerate the covered-vertex list automatically.

Contours can be concave. The region uses the polygon selection algorithm from the shared surface
region modules; triangulation is not exposed as the meaning of the influence field. Runtime target
playback and combined skeletal playback were validated in MeshDebug and in a runtime mesh instance.
The complete authoring workflow was also covered manually with the Megan mesh: locked and free
shapes, shape and target mirroring, target painting and smoothing, undo/redo, project reload, MSH
export, and both normal-delta policies. Expressions were then validated with project reload and
composed preview. The current implementation also includes the Clips worktree described above;
its visual acceptance and runtime MSH reload are covered by milestone 2.6 of the
[facial animation future work plan](realtime-facial-animation-future-work-plan.md).
