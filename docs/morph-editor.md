# Morph Editor

The Morph Editor is a generic tool for authoring polygons on MSH mesh surfaces. It works with an MSH mesh
without a face origin, facial landmarks, facial roles, or FACS data. The launcher entry is
`Morph Editor`, implemented by `editor/morph_region_editor.lua`.
Its topology, polygon geometry, and region data come from `morph_surface_topology.lua`,
`morph_surface_polygon.lua`, and `morph_surface_regions.lua`.
Linux and macOS builds with Lua also create the `morph_editor` shell alias in `shortcuts.sh`.

## Current scope

- Open a mesh and create named polygon regions on its surface.
- Add points by clicking the mesh, drag existing points to another visible surface vertex,
  insert a point on a closed contour, and remove points or polygons.
- Orbit with the right mouse button, pan with the middle button, and zoom with the wheel.
- Rebuild the polygon's derived vertex selection and inspect its point and vertex counts.
- Save and reopen the authoring project as a Lua `.morph.lua` sidecar.

The project stores the mesh path, a topology signature, polygon names, canonical surface
anchors, the selected polygon, and rebuildable vertex selections. The derived selection is
binary at this stage. Falloff settings are retained by the shared region data model but do not
yet affect the selection. A changed topology is reported and disables polygon editing. To author
against a different topology, open that mesh as a new project; the existing point indices are not
automatically transferred.

This milestone authors surface regions and exposes their contours and derived selection counts.
Morph target selection, weight painting, smoothing, transforms, mirroring, MSH export, and runtime
playback belong to milestone 2.3 of the [facial animation future work plan](realtime-facial-animation-future-work-plan.md).

## Editing a polygon

1. Open an MSH mesh and create a polygon with a name.
2. Click the mesh to add surface points. Existing points appear as handles; drag one to move it.
3. Add at least three points, then close the contour.
4. Rebuild the selection to update the list of covered mesh vertices.
5. Save the project. Reopen the `.morph.lua` file to continue authoring.

Contours can be concave. The region uses the polygon selection algorithm from the shared surface
region modules; triangulation is not exposed as the meaning of the region.
