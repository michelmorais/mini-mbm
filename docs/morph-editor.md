# Morph Editor

The Morph Editor is a generic tool for authoring surface shapes on any MSH mesh. It does not
require a face origin, facial landmarks, facial roles, or FACS data. The launcher entry is
`Morph Editor`, implemented by `editor/morph_region_editor.lua`.
Its topology, shape geometry, and region data come from `morph_surface_topology.lua`,
`morph_surface_polygon.lua`, `morph_surface_shapes.lua`, and `morph_surface_regions.lua`.
Linux and macOS builds with Lua also create the `morph_editor` shell alias in `shortcuts.sh`.
On launch the editor opens to an empty workspace. Choose **File > Open Mesh** or **File > Open
Project** when ready; no file dialog appears automatically. **File > Quit** exits the editor, and
**Options > Language** switches between English and Portuguese (Brazil).

The Morph Editor currently presents two sequential worktree nodes: **Surface shapes** and
**Morph targets**. The first worktree follows the interaction model of the
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

## Shape modes

The new-shape selector provides:

- **Square**: starts as a square. In locked mode it has a center controller and three resize
  handles: uniform size, width, and height. The uniform handle preserves a square; the width and
  height handles can turn it into a rectangle without enabling Free.
- **Circle / ellipse**: starts as an ellipse. In locked mode it has a center controller and
  three resize handles: uniform size, width, and height.
- **Free polygon**: starts with a visible four-point contour when the viewport can hit the mesh and
  exposes each anchor for independent editing.

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

The project stores the mesh path, a topology signature, shape type and mode, canonical surface
anchors, the selected shape, morph-target selections, and rebuildable vertex selections. The
derived region selection is binary at
this stage. Falloff settings are retained by the shared region data model but do not yet affect the
selection. A changed topology is reported and disables shape editing. To author against a
different topology, open that mesh as a new project; the existing point indices are not
automatically transferred. The current project schema is version 4; older Morph Editor project
files are intentionally rejected while this editor is being developed.

The project also persists the editor preferences used to continue the same authoring session:
language, selected worktree, Add/Edit/Mirror/Remove modes, selector size, surface-snap axis,
new-shape type and Free flag, mirror-shape name/axis, target name and translation drafts, target
mirror name/axis/tolerance options, the draft shape name, mesh visibility, target preview weight,
target displacement visibility, and the Morph viewport camera. These
settings are restored when the project is opened and are part of the project dirty state when changed.

## Editing a shape

1. Open an MSH mesh and create a named shape.
2. Create a Square, Circle / ellipse, or Free polygon; it is placed at the viewport center when
   possible. A Free polygon starts with a four-point contour. If the center ray does not hit the
   mesh, click the mesh to begin placing its surface points.
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
   create a target.
3. Switch to **Edit target** to inspect the source fingerprint, topology status, selected vertex
   count, authored delta count, and the target preview.
4. In **Edit target**, choose **Translate**, **Rotate**, or **Reset to neutral**. Translation and
   rotation use the source selection weights; rotation uses the selection's weighted center as
   its pivot. Press **Apply edit** to commit the chosen operation.
5. Use **Preview weight** to inspect the neutral mesh at `0` and the full target contribution at `1`.
   **Show target displacement** selects the visualization material: enabled uses the dynamic
   displacement shader, while disabled keeps the displaced geometry visible with its original
   texture and material when available, without the custom shader. The source mesh remains
   unchanged and the target is updated in memory.
6. Switch to **Paint target**, choose **Move** or **Smooth**, adjust the brush radius and strength,
   then right-drag over the displaced target surface. Release the button to commit one stroke;
   left-drag still orbits and the middle button pans.
7. Switch to **Mirror target**. The new target name is initially suggested as
   `<source_name>_mirror`. The selected target's source polygon and target are temporarily mirrored
   across the origin plane of the selected axis (X by default). The source polygon remains visible
   as a reference while the candidate is shown in orange. Adjust the name, axis, and match radius,
   review the candidate and diagnostics, then press **Create mirrored target** to persist the new
   polygon and target. The source remains unchanged. To repeat the operation with another target,
   select it, choose X/Y/Z, and use **Analyze mirror** before confirming.
8. If the source shape changes, use **Rebuild target selection** explicitly. A stale target is not
   edited or silently regenerated.

While **Morph targets** is active, **Edit target** remains navigation-only. **Paint target** uses
right-drag on the displaced surface to apply local Move or Smooth edits; left-drag orbits and the
middle button pans. Shape anchors are editable only in **Surface shapes**. Selecting a target
selects its source shape for inspection without changing the source geometry.

This initial target worktree stores generic authoring data only. Its editor preview uses a dynamic
in-memory surface overlay and does not modify or regenerate the source MSH while the preview weight
changes. Paint strokes are accumulated in memory and committed when the right button is released;
the preview rebuilds once per completed stroke. Smoothing, transforms beyond translation, MSH export,
and runtime reload remain later Milestone 2.3 steps; mirroring is available through the explicit
Analyze/Create workflow described above.

Opening or closing a free contour and removing an anchor invalidate the derived vertex selection
without scanning the mesh immediately. Use **Rebuild selection** when the contour is ready; this is
the explicit operation that performs the potentially expensive mesh scan.

Adjusting a shape to the surface also invalidates its derived vertex selection. The operation only
changes the authored point offsets; it does not regenerate the covered-vertex list automatically.

Contours can be concave. The region uses the polygon selection algorithm from the shared surface
region modules; triangulation is not exposed as the meaning of the influence field. Runtime target
playback, richer smoothing, MSH export, and runtime reload remain in milestone 2.3 of the [facial
animation future work plan](realtime-facial-animation-future-work-plan.md).
