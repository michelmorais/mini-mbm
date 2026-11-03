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

The first Morph Editor worktree is **Surface shapes**. It follows the interaction model of the
FACS mask editor while keeping the data generic: the user chooses a shape, places it on the mesh,
and adapts its handles to the object. Its operation modes are **Add shape**, **Edit shape**, and
**Remove shape**; each mode exposes only the controls needed for that operation.

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
- Save and reopen the authoring project as a Lua `.morph.lua` sidecar.

The project stores the mesh path, a topology signature, shape type and mode, canonical surface
anchors, the selected shape, and rebuildable vertex selections. The derived selection is binary at
this stage. Falloff settings are retained by the shared region data model but do not yet affect the
selection. A changed topology is reported and disables shape editing. To author against a
different topology, open that mesh as a new project; the existing point indices are not
automatically transferred. The current project schema is version 3; older Morph Editor project
files are intentionally rejected while this editor is being developed.

The project also persists the editor preferences used to continue the same authoring session:
language, Add/Edit/Remove mode, selector size, surface-snap axis, new-shape type and Free flag,
the draft shape name, mesh visibility, and the Morph viewport camera. These settings are restored
when the project is opened and are part of the project dirty state when changed.

## Editing a shape

1. Open an MSH mesh and create a named shape.
2. Create a Square, Circle / ellipse, or Free polygon; it is placed at the viewport center when
   possible. A Free polygon starts with a four-point contour. If the center ray does not hit the
   mesh, click the mesh to begin placing its surface points.
3. In locked mode, drag the center or one of the shape-specific resize handles. Enable Free when
   the contour should be edited point by point.
4. For a free polygon, add at least three points and close the contour.
5. Rebuild the selection to update the list of covered mesh vertices.
6. Save the project. Reopen the `.morph.lua` file to continue authoring.

Opening or closing a free contour and removing an anchor invalidate the derived vertex selection
without scanning the mesh immediately. Use **Rebuild selection** when the contour is ready; this is
the explicit operation that performs the potentially expensive mesh scan.

Adjusting a shape to the surface also invalidates its derived vertex selection. The operation only
changes the authored point offsets; it does not regenerate the covered-vertex list automatically.

Contours can be concave. The region uses the polygon selection algorithm from the shared surface
region modules; triangulation is not exposed as the meaning of the influence field. Morph target
selection, weight painting, smoothing, transforms, mirroring, MSH export, and runtime playback
belong to milestone 2.3 of the [facial animation future work plan](realtime-facial-animation-future-work-plan.md).
