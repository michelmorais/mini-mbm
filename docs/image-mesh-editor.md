# Image Mesh Editor

An offline editor for turning image regions into textured 3D modules. It supports
image-derived and manual relief, curved target hierarchies, interior transitions,
automatic faceting, height finishing with areas and brushes, holes, editable
contours, simplification with comparison, and individual or batch export.

The image supplies colors and optional height data; the editor does not infer the
semantic shape of the depicted object. Painted shadows can become geometry when
image brightness is used as height. Manual relief or a separate height image gives
more direct control.

## Launch and workflow

Choose **Image Mesh Editor** in the launcher, or run from the repository root:

```sh
bin/debug/linux_x86/mini-mbm --scene editor/image_mesh_editor.lua --disable_select_monitor --nosplash -w 1440 -h 900
```

The editor requires Lua and ImGui. The documented test coverage uses Linux/OpenGL ES;
launcher integration is also present for Windows and macOS.

1. Open an image from the File menu.
2. Create regions for the modules, manually or with the region grid.
3. Set dimensions, depth, relief, materials, and geometry limits.
4. Apply the properties and switch from edit mode to the 3D view.
5. Inspect lighting, wireframe, simplification, or neighboring modules.
6. Save the editable `.imesh` project and export the finished `.msh` modules.

**Apply** commits property edits. Saving a project also validates and commits
pending properties. Invalid properties prevent overwriting the project.

## Workspace and navigation

The left **Regions** panel contains tools, the module list, and the selected
module's properties. Shape creation and the module list precede the budget diagnostics
and region-grid controls. Budget diagnostics appear only after an image is loaded.
The right-side **Editor view** panel selects **Editing** or **Viewing** and contains
the corresponding camera controls. Lighting appears in the 3D view.
The image and mesh are displayed directly in the engine scene.

| Mode | Scene controls |
|---|---|
| Edit mode | In Select/Move, left-drag a shape to move it or empty space to pan. Handles take priority. Right/middle-drag also pans. The wheel zooms around the cursor. Fit Image resets the view. |
| 3D view | Left-drag to orbit; use the wheel for distance. Camera controls provide numeric orbit/focus values and reset. Wireframe displays triangle edges instead of textured surfaces. |

Drawing tools use their own gestures. Switching modes cancels unfinished drawing
or dragging; apply pending property fields before switching. GUI clicks do not
manipulate scene objects. Resize handles grow with zoom within screen-space limits.

**Options > Background Color** offers the default editor background plus white,
black, red, green, blue, cyan, yellow and magenta, with color swatches as in Mesh
Debug. The choice affects the scene background in editing and 3D viewing, persists
for the session, and does not change materials, project data or exported meshes.
It does not regenerate geometry. Default restores the editor's original dark color.

Lighting affects only the 3D scene. Ambient color, directional-light color and
direction, and light reset are available. Camera, light, and wireframe changes do
not regenerate geometry or alter exported data. Changing properties on the same
module preserves its camera view; selecting another module provides a new framing.

## Manual curved relief

In **Relief and grooves**, select **Manual curved**. Set **Contour thickness** and
**Target thickness** in final mesh units, then **Apply**. For a saw blade, use 1 and 8.
These are total thicknesses, independent of the saved Depth and Relief settings.
The point/circle controls below describe the initial radial profile; the following
sections cover target hierarchies, interior transitions and automatic faceting.
Shared height finishing is described under [Height painting](#height-painting).

- Center X/Y are normalized within the region. Radius 0 gives a point; a positive
  radius gives a circular flat plateau. Radius is measured in the final mesh plane,
  so a non-square crop does not turn the target into an ellipse in the mesh.
- **Move center / resize target** applies pending properties and activates canvas
  handles. Drag the center handle to move it, or the right circle handle to resize.
  Release commits one Undo step; Escape cancels the drag.
- **Swap thickness values** reverses the growth. **Symmetric thickness** distributes
  half the total thickness on each side of Z=0. With it disabled, the native back
  plane is fixed at half the smaller endpoint thickness; the front receives the
  full thickness variation. The editor rotates the native mesh for its front view.
- With automatic faceting disabled, the profile is linear along rays from the center to the actual contour, including
  teeth and recesses. The center must see the entire contour from inside, and the
  circle must remain strictly inside without touching it. Invalid configurations
  report an error; generation does not silently select another algorithm.
- Holes cut the curved surface without changing its thickness profile (7.271.0).
  The back uses the source texture and the same openings; horizontal
  back UV mirroring and side texture modes remain available. Other saved back modes,
  image height adjustments and the general simplifier are
  inactive. Switching modes retains their saved settings/data.
- The height map shows total thickness divided by the larger endpoint thickness.
  It shares the native field with mesh generation. There is no groove overlay.

Before optional hole cuts, the mesh includes the point or the circular target boundary explicitly. Columns/rows
set initial sampling; the generator refines rings until midpoint and centroid height
samples meet Height tolerance, as a fraction of the endpoint thickness difference.
This is a sampled error target, not a certified global bound. The circular boundary
is represented by chords. Vertex/triangle budgets can stop refinement with a diagnostic.
Equal endpoint values produce a uniform solid. Thickness values must be at least 0.001.
The legacy point/circle controls remain linear. Extended target shapes and profiles
are available after explicit conversion to the hierarchy below. Optional constrained
simplification is available since 7.270.0 (see below).

### Interior transition for concave shapes (7.274.0)

In **Manual curved**, enable **Transition through the interior**, then open
**Edit targets and local regions**. In this mode, a new hierarchy starts empty.
Select the root and add a **Polyline** target. Its initial short segment is placed
inside the piece. Expand **Vertices (advanced)** to insert vertices and edit the
path along the U/S shape; the canvas also exposes its vertices. The path stays open.
Set the contour thickness and the target's thickness, then Apply. A point or straight
line target is also supported. An empty hierarchy stays flat at the contour thickness.

This mode calculates a smooth-shaded surface through the piece's internal mesh,
without radial visibility requirements or connections across external gaps. All
outer and hole borders retain the contour thickness in the base surface, before
optional height finishing. Targets cannot cross the
contour, holes or themselves; invalid vertex edits produce a message and keep the
previous geometry. A hole influences nearby heights, unlike the post-cut behavior
of radial relief. **Columns/rows** set the resolution; the result is a discrete
approximation and can change with resolution. The transition need not be linear.

The interior transition accepts a single root point/line/polyline. Closed targets,
additional hierarchy levels, local regions and faceting are unsupported. Saved
Bézier/smooth profiles and simplification remain inactive. Adaptive tolerance does
not drive the interior base solve; optional finishing adds local refinement afterward.
Existing incompatible hierarchies are preserved and generation reports the reason;
remove their targets or use another module to author the interior target. Changing
back to radial mode also preserves the polyline, which must then be removed or
replaced with a supported radial target. Save/reopen, undo/redo, height maps, symmetric
or flat back, materials and mesh export are supported.

### Automatic faceting for diamonds (7.273.0)

Select **Manual curved** on a module using the point/circle profile, then enable
**Automatic faceting**. Start with an ellipse, **Minimum sectors = 8** and
**Transition rings = 1**. Set the contour and target thicknesses, choose a positive
radius for a central table (or zero for a peak), then Apply.

The mode generates planar triangular faces with independent normals. It supports
convex outlines; polygon corners are preserved and may require extra sectors.
Ellipses use the sector count for their polygonal silhouette, replacing the saved
ellipse segmentation while the mode is active. Increasing ring count divides the
linear transition; bands can remain coplanar. This first milestone is a basic
sector/ring cut, not a gemological brilliant-cut preset.

The center can move, thicknesses can be swapped, and both symmetric and flat backs
are supported. Holes cut the existing planes and build inner walls. Height maps
represent those same planes. Grid resolution, adaptive tolerance and simplification
are inactive in this mode; the editor disables their controls and retains their
saved values. Facet seams duplicate vertices, which count toward the geometry budget.

Target chains are also supported: use **Edit targets and local regions** to convert
or edit the hierarchy, then add nested convex targets with their own thicknesses.
For example, contour 1, intermediate polygon 5, inner polygon 8 creates two slopes
and a flat central table. A terminal point creates a peak instead. An empty hierarchy
is flat. Local regions and line targets are unsupported; their add buttons are disabled
while faceting is active. Existing incompatible hierarchies produce a generation error.

A shared radial origin is the arithmetic mean of the innermost polygon's vertices,
or the terminal point. Rays through every polygon corner preserve all target borders;
minimum sectors adds rays and transition rings subdivides each target connection.
Moving a target can change the triangulation throughout the chain. Bézier/smooth
profiles remain saved but are inactive; transitions are linear between thicknesses,
and the profile panel explains this while faceting is enabled. Settings participate
in Apply, undo/redo, `.imesh`, asynchronous generation and export. The mode is off by
default for existing projects. Export preserves the hard facet normals.

### Targets and local regions (7.267.0)

Use **Edit targets and local regions** to explicitly convert the current point/circle
profile. Old projects retain their previous geometry until this action. Conversion
is undoable; circles become 32-point polygonal ellipses and use the hierarchy's
nearest-target interpolation, so the result need not match the legacy radial surface.

Use **Back to relief** at the top of the hierarchy panel to apply pending edits and
return to the compact relief controls. This exits canvas target editing and preserves
the entire hierarchy. **Edit targets and local regions** opens it again and immediately
activates target editing on the canvas; there is no second activation button. A status
message identifies the active mode and explains why the module contour cannot be
manipulated until **Back to relief** is used. Selecting
another module also closes the panel; the hierarchy remains active in generation.

Select the outer contour or a closed node and choose a new shape. **Set shape target**
defines its thickness destination; **Create detail region** creates an independent
area that can receive its own target. Hover either button for an explanation and
example. Each owner admits one target. Point and line targets
are terminal; a closed target without a target is flat. A local region inherits the
underlying border and is neutral until it has a target or local children. Two local
regions can control independent details side by side. Their order has no effect.

The canvas center handle moves a node with all descendants; the lower-right corner
resizes a closed subtree. The default numeric fields edit the whole shape's center
and horizontal/vertical half-size, including descendants. Circles/ellipses and
rectangles retain their shape; explicitly convert them to polygons to edit vertices.
Line endpoints and polygon vertices are available under **Edit vertices (advanced)**,
with fields labeled **Selected vertex X/Y**. Numeric edits and canvas drags reject
non-finite/out-of-bounds coordinates, collapsed edges, self-intersection, non-convex
targets, invalid containment/visibility and overlapping independent regions. A message
at the end of the panel explains the rejection; numeric edits also show a temporary
warning with the field, rejected value and reason, without taking keyboard focus or
moving the controls. The previous geometry is retained. Validation
runs on edits, not during idle frames. Native generation keeps its own validation.
Removing a node removes its
descendants; undo/redo restores the complete hierarchy. Changes use Apply and the
existing generation/export queue. Labels precede numeric fields to fit narrow panels.

Closed targets must be convex and fit entirely in the parent's visibility kernel.
Local regions may be concave, but independent regions must not touch, overlap or
contain one another. Nesting must be explicit. Generation reports invalid placements;
the editor lets you adjust them. Limits: 32 nodes, 128 points per node, depth 8.
A flat back is fixed at Z=0 for the hierarchy. In radial mode, polyline targets and blending overlapping regions are unsupported.

### Transition profiles per target (7.268.0)

Select a target under **Edit targets and local regions**. **Transition from owner to
this target** chooses Linear, Smooth or Bézier for that specific incoming transition.
Omitted profiles remain Linear, preserving existing projects. Local regions use the
profile of their target; selecting the region itself does not expose a profile.

Smooth uses a smoothstep profile. Since 7.269.0, Bézier offers radio buttons for
**2, 3 or 4 internal control points**, in addition to the fixed border/target endpoints.
Each yellow handle is draggable vertically and has a slider. Horizontal positions
are evenly spaced. The controls are independent in [0,1] and may cross vertically;
3 or 4 controls can create waves, while thickness stays between the border and target
values. The graph shows normalized progress toward the target thickness, including
when that thickness is lower than the border. It is not a literal mesh cross-section.
Increasing the control count uses degree elevation and preserves the current curve.
Reducing it resamples the control polygon and approximates the profile, so the shape
may change. Existing projects default to two controls with the same cubic evaluation.
The graph updates immediately; Apply uses the existing preview/export workflow.
Each link in a chain can use a different profile. Profiles participate in undo/redo,
project persistence, duplication, maps and asynchronous generation. The graph samples
are cached and rebuilt only when the profile or controls change. Smooth endpoint
slopes do not remove geometric corners or guarantee smoothness across inherited borders.

## Regions and contours

Create rectangles, circles/ellipses, triangles, regular polygons with 3–32 sides,
or custom polygons. Primitives can be inserted near the visible image center
with numeric dimensions. Rectangles and ellipses can also be drawn by dragging;
polygons are drawn by clicking vertices and finishing the contour.

The region grid takes columns, rows, margins, and spacing in pixels and adds
regions without replacing existing ones. Individual crops remain editable.

- Drag a region to move it; use resize handles or numeric crop fields for its size.
- Polygon vertices can be moved, inserted, or removed. Moving a vertex beyond the
  crop expands it within the image while keeping the other vertices in place.
- **Lock module in scene**, below the selected module name, keeps a module visible
  but excludes it from scene picking and mouse edits, including painting, holes,
  height areas and UV handles. Clicks can reach unlocked modules beneath it.
  Select a locked module in the ImGui list to edit its properties or unlock it.
  Locking is saved in the project and supports Undo/Redo; it does not affect mesh
  generation or export. Existing projects load unlocked, and duplicates start unlocked.
- Ctrl+click extends the selection. Duplicate and Delete affect the selection.
- Applying generation parameters affects all selected regions; name, crop, and
  contour changes affect the primary selection only.
- Project defaults provide inherited parameters. Resetting a selection to the
  defaults clears its overrides.

**Edit project defaults** switches the property groups below to project-wide
settings and opens **Volume**. Module-specific fields such as the name
and contour are hidden. **Apply project defaults** commits the settings for new
modules and inherited parameters of existing ones; individual overrides remain.
Uncheck the option to return to the selected module's properties.

Contours may be concave but must be simple: crossings, self-contact, duplicate
points, and degenerate areas are rejected. Circles are editable ellipses; later
edits can change their proportions. The project supports 256 regions. Undo/Redo
(Ctrl+Z/Ctrl+Y) retains 40 operations; a completed drag is one operation. Esc
cancels an unfinished gesture.

### Freehand and automatic contours

**Freehand contour** closes a drawn stroke and presents an editable preview.
Point-reduction tolerance is measured in image pixels (0.1–20, default 1.5).
Confirm to create the polygon, draw again to replace the preview, or cancel.
Strokes accept up to 4,096 samples and resulting polygons up to 128 vertices.
The same workflow can create a hole in the selected module.

**Automatic contour** selects a connected image area around a clicked seed:

- Transparency uses an alpha threshold.
- Background color also excludes pixels within an RGB tolerance of a chosen or
  sampled color. Tolerance uses the largest channel difference.

Connectivity uses horizontal/vertical neighbors. Review the contour, adjust point
reduction, and confirm it as a new module or a hole. Internal cavities do not
become holes automatically. Hole detection is restricted to the module crop;
new-module detection can optionally use that crop too.

Enclosed cavities are excluded before tracing the outer boundary, so contacts
between internal color patches do not invalidate an otherwise valid silhouette.
Diagonal background connections to the exterior remain open. Ambiguous pixel
corners are trimmed by one quarter of a sampling cell to keep the traced boundary
from touching itself; other invalid contours are still rejected. On fully opaque images the alpha threshold has
no effect: use background-color tolerance to adjust the selection.

Detection uses the original pixels, not height filters or painting. Fully opaque
images need background-color separation if the desired shape is not the whole
connected crop. The search grid is limited to 262,144 cells and reports its sampling
step; smaller crops preserve finer details. Limits also apply to boundary segments
(65,536), corners before reduction (4,096), and final vertices (128). Ambiguous,
crossing, or touching contours are rejected. Changing detection settings requires
a new seed click; cancellation discards the unfinished search or preview.

### Holes

The **Holes** controls add a rectangle, a 16-point circle, or a manually drawn
polygon. Added primitives search for a free initial position. Move or resize an
existing hole, or draw a smaller one, if no valid position is found.

Pink contours mark real openings. Drag their interiors to move them or their
handles to reshape them. Circular holes can retain an elliptical shape: the right
handle changes width, the bottom changes height, and the diagonal handle restores
a circle while keeping its center fixed. Disable shape preservation to edit
individual vertices.

Each module accepts up to 16 holes with 3–128 points each. Holes must lie strictly
inside the outer contour and cannot touch, overlap, cross, or contain one another.
Invalid edits are rejected when the gesture ends.

Holes remove front/back faces and create inner walls at the local relief height.
They do not flatten neighboring relief. Fixed border height applies only to the
outer contour. Inner walls use the side material; with an inner contour band,
they stretch the hole-edge texture instead of using the outer band's inset.
Holes can increase back-face triangulation and geometry usage. They are saved,
duplicated, scaled with the module, and included in undo/redo and export.

### Holes in Manual curved mode (7.271.0)

Use the same **Holes** panel while editing a module with Manual curved relief.
Add a circle for a saw's central opening, or a rectangle/polygon for another cutout.
The hole removes material through the whole thickness and builds inner walls with
the existing side material. It does not change the surrounding thickness profile.

Targets and local regions remain virtual surface controls: a target can be inside
a hole, and a hole can cross a target line or a region boundary. Moving a hole does
not move those controls. The usual containment/visibility rules for the controls
still refer to the complete outer contour. Hole contours must remain strictly inside
that contour and separate from one another.

Symmetric relief, flat backs, protected simplification, undo/redo, project files,
preview and export share the same cuts. Simplification preserves hole borders.
The height map is transparent inside openings. Fine cuts can increase geometry:
the dense surface and intermediate cuts must fit the budget before simplification.

## Volume and relief

**Volume** controls world dimensions and base depth.
**Relief and grooves** contains relief amplitude, fixed border height, and its transition width,
shown when border locking is enabled. **Preserve image aspect ratio** derives world height
from world width using the crop's pixel-center spans, `(h - 1) / (w - 1)`, with
minimum one-pixel spans. Disable it for independent width and height.

The editor's front faces +Z, matching Mesh Debug's initial view. The origin is at
the center of the base volume. The underlying `mbm.generateImageMesh` API faces
-Z; the editor rotates positions and normals before preview/export, preserving
UVs and winding.

### Height sources and tonal controls

Choose the relief mode in **Relief and grooves**:

| Source | Height definition |
|---|---|
| Image | Processed image values, followed by enabled areas and brush corrections |
| Manual | A normalized base height plus enabled manual areas and brush corrections; image colors do not determine height |
| Image + manual areas | Processed image values, followed by enabled areas and brush corrections; retained as the mixed mode |
| Manual curved | Total thickness from the radial hierarchy or interior field, optionally followed by areas and brush corrections; faceting keeps finishing inactive |

Image modes can use luminance, red, green, blue, or alpha. An image without alpha
supplies alpha 1. The channel changes height sampling, not the displayed material.

A separate height image can align to the whole source image or to the whole
module crop. Different resolutions use bilinear sampling; the resulting field
uses the source crop's resolution. A separate height image is a project dependency,
not an exported material. Reset to the original image to remove it. Manual mode
ignores this configuration without deleting it.

**Height levels and curve** provides black/white points and a response curve:

- Values at/below black map to 0; values at/above white map to 1.
- Equal points produce a step. Black cannot exceed white.
- Curve 1 is neutral; values above 1 lower intermediate heights and values below
  1 raise them. The allowed range is 0.1–10. Reset restores 0/1/1.

These adjustments precede inversion and filtering. They do not change material
colors or the target heights of manual areas.

### Grooves, filtering, and diagnostic views

**Mode** selects the original image, height map, or detected grooves in blue.
The height map shows normalized final heights, including border attenuation,
before world relief amplitude. Blue marks automatic groove classification, not
holes. Manual mode hides the blue view and image-filter controls.

Use the groove threshold to classify low values. **Two heights (bars/grooves)**
levels upper/lower areas and uses transition width for the ramp between them.
Edge-preserving smoothing offers zero to four local passes. It reduces small
variations but cannot distinguish painted shadows from depth.

| Control | Height map | Detected grooves |
|---|---|---|
| Inversion and smoothing | Applied | Applied |
| Groove threshold | Used with Two heights | Applied |
| Two heights and transition width | Applied when enabled | Not used by this diagnostic view |
| Adaptive geometry and relief tolerance | Affect the 3D mesh, not map pixels | Affect the 3D mesh, not map pixels |

Irrelevant controls are hidden without resetting their values. Original-image
view hides the diagnostic modifiers. Generation controls remain available in 3D
and when editing defaults, according to their dependencies.

**Preview adjustments** renders draft settings without committing them. Apply
commits the settings; Save Project persists them. Maps are overlays on the selected
crop and never replace the source or exported texture. Their background task has
progress/cancellation; outdated requests cannot replace newer results. Previous
maps are identified while updates run or after a failure. Request another preview
to retry. Panning and zooming reuse the map.

### Drawn height areas and lines

In **Height painting**, select **Areas** to add rectangles/ellipses or draw
polygons/freehand areas. Available for Image, Manual, mixed and curved relief;
faceting keeps finishing inactive. Adding an area enables adaptive refinement
without changing the base relief mode.
Each area has a name, enabled flag, operation (Flatten, Raise or Lower), normalized
height/amount and inward transition width. Flatten sets a target; Raise and Lower
add or subtract the amount from the previous result, clamped to 0–1. World height is the normalized target multiplied by relief amplitude.
For example, base 0.4, raised area 0.8, and groove 0.2 define three independent levels.

Move areas by dragging inside them; use handles or width/height fields to resize.
Areas stay within the crop but may extend outside the module contour, which clips
the final geometry together with holes. Areas apply in list order; reorder,
duplicate, disable, or remove them in the list. Disabled areas remain saved.

**Line/groove** creates an open path of 2–128 points with editable width and rounded
caps/joins. Flatten uses an absolute target; Raise and Lower use a relative amount.
Self-crossings are allowed and do not add depth. Transition proceeds inward; if it
exceeds half the width, the center cannot reach the full target height.

A module supports 32 areas, with up to 128 points each. Coordinates and line width
are normalized to follow crop resizing. Rasterization has a 64-million edge-evaluation
budget. Areas are saved and duplicated with the module; presets do not copy them.
Brush corrections follow area composition, and fixed outer-border treatment comes last.

### Height painting

Since 7.277.0 this panel groups **Brush** and **Areas** as final height editing.
Unchecking **Apply height finishing** bypasses all areas and brush strokes without erasing
anything. **Clear areas and brush strokes** restores the base and is undoable.
The order is base relief, enabled areas in list order, then brush strokes.
Turning off brush input alone does not disable existing edits. Switching base
modes keeps edits and reapplies them, including in Image mode; curved finishing
still requires its explicit opt-in. Fixed-border treatment in non-curved modes
continues to apply after finishing.

Enable painting for the selected module to apply pending settings and show its
height map. Raise, Lower, Flatten, and Smooth operate within a brush whose **Diameter (%)**
is relative to the selected module's shorter side (0.2–100%, default 16%).
At 100%, the circle spans that side, not twice its size. The same setting scales
with the selected module; the effective diameter has a 1 image-pixel minimum.
Stored strokes still use normalized radius and keep their original size. Strength ranges from 0.01 to 1; Flatten uses a target height in 0–1.

Drag inside the module and release to commit a stroke. Each stroke is one undo
operation; Esc discards it. Clear Painting is also undoable. Empty space pans the
camera, and choosing another tool disables painting.

Strokes are stored separately from the source texture and replayed on the floating-
point height field. The preview and generator use that field directly, without a
PNG roundtrip. Normalized brush coordinates follow crop changes. Changing automatic
height settings replays the same corrections over the new base field.

Painting accepts up to 4,096 samples per module and 64 million pixel operations
per generation. Adaptive geometry refines changed areas locally; otherwise the
chosen grid resolution controls detail. Geometry budgets still apply, and optional
simplification can approximate painted details afterward.

In **Manual curved** mode (7.276.0), open **Height painting** while editing the
module and enable **Apply finishing over curve**, then choose **Brush** or **Areas**.
For freehand brush strokes, enable **Paint selected module**.
This is explicitly opt-in, so old strokes previously inactive in curved mode do
not change existing projects. The panel shows the thickness interval: normalized
height 0 is its minimum and 1 its maximum. Painting is a final edit and may alter
targets and borders; holes remain cut out. **Clear areas and brush strokes**
restores the curve; clearing only brush strokes retains the areas. Unchecking
finishing retains both areas and strokes for later use. Equal thicknesses provide
no interval to paint; the panel explains how to make one. Faceting keeps painting
inactive to preserve planar faces. Radial hierarchies and interior transitions
are supported; local refinement captures fine strokes. Curve simplification and
its original/result comparison both use the painted surface.

## Geometry, normals, and budgets

**Follow image shapes (adaptive)** refines relief according to sampled interpolation
error and aligns edges with processed height transitions. Relief tolerance is a
fraction of amplitude; columns/rows constrain sampling density. This is a sampled
criterion, not a maximum error guarantee at every pixel. Fine details may require
higher resolution. The output always consists of triangles, not a QUAD mesh.

With adaptive geometry and Two heights, plateau faces receive priority when
computing normals shared with steep transitions. This preserves flat top/bottom
shading without duplicating vertices. Automatic faceting instead duplicates vertices
and gives each triangle its geometric normal. Since 7.274.1, radial curved hierarchies
regularize their constrained triangulation during refinement and use corner-angle
weighted normals. This removes artificial wrinkles caused by long thin triangles;
control edges, holes and plateau preference remain protected. Other vertices use adjacent-face averaging;
front, back, and side boundaries retain their intended normal separation.
Exports preserve generated normals. Uniform recomputation in Mesh Debug replaces
this specialized shading; see [normal processing](mesh-debug-normals.md).

The vertex budget can be lower than the engine's fixed 65,535-vertex limit. The
editor derives the triangle budget as `2 * maxVertices`; neither is a target count.
Budgets include the front, back, walls, and UV/normal splits. Simplification runs
after generation and cannot bypass the generator's budget.

Face counts use compact notation such as `4K`; tooltips show exact totals. In edit
mode, counts are requested with **Calculate faces** or by entering 3D. Dragging
regions or height areas does not regenerate geometry just to count faces. The last
calculated module can be reused for its 3D preview until its inputs change.

Budget diagnostics show counts and percentages for the parameters that produced
the result, with a warning at 90% usage. Simplified results show both source and
final counts. Pending edits and old previews are not presented as current results.
Errors identify the exceeded resource and processing stage and suggest relevant
adjustments to density, contours, relief, painting, or side-texture seams.

## Materials: back and sides

The front uses the source-image crop. Back geometry/material choices are:

| Back mode | Result |
|---|---|
| Flat | Flat back using the original crop |
| Copy front relief | Final front heights, including painting and border treatment, reflected outward onto the back |
| No back | Front and walls only, leaving the rear open |
| Flat + UV remap | Flat back using another crop of the same image |
| Solid color | Flat back with an opaque RGB material |
| External texture | Flat back using a separate image, fitted to the module shape |

Eligible flat backs are automatically triangulated from the original contour corners,
without copying the front's refinement samples: a quadrilateral back uses two triangles.
The walls retain all front relief samples and are retriangulated to meet the reduced
back without T-junctions. This happens during generation, before optional simplification,
and also applies to the direct generation API. Front geometry and normals are unchanged.
Back UV mapping and materials are retained; inner-band wall UVs keep their front/rear
mapping, with interpolation following the new wall triangles. Holed modules, copied
back relief, repeated side textures, transparent edge-texture crops, and contours or
strips that cannot be safely reduced keep the existing topology.

Horizontal back-texture mirroring is independent of copied relief. A relief back
requires additional geometry; all of it counts toward the budget. Solid colors
are stored as `#RRGGBBFF` and need no texture file.

For UV remapping, enable back-crop editing in 2D. The purple contour follows the
front shape but can be moved/resized within the image. Numeric fields and the
reset-to-front-crop button provide the same controls. This changes only back UVs,
not geometry or height. Each module retains its own back crop when applying presets.
An external back texture defaults to the original crop if no file is selected.

| Side mode | Texture mapping |
|---|---|
| Stretched edge | Extends contour pixels across the wall depth |
| Solid color | Opaque RGB material |
| Repeated texture | Repeats the original crop or an external image, with perimeter/depth repetition controls |
| Inner contour band | Maps the strip between the outer contour and an editable inner contour onto the walls |

The inner band has a minimum width of one pixel; the maximum is computed to avoid
collapsed or crossing contours. Rectangles shrink by side, circles stay concentric,
and polygons use offset edges. Some narrow contours cannot support a valid band.
The green handle changes inset size only. **Invert band UV** swaps its depth direction:
by default, the outer contour meets the front and the inner contour meets the back.
The band mapping selector offers **Follow inner contour** (the default for existing
projects) and **Perpendicular to edge**. Perpendicular mapping translates texture
samples inward without shifting their position along each edge. It is useful for
continuing masonry joints onto side walls without the convergence caused by a
smaller inner contour. Each edge has its own band; the green preview uses separate
inner segments, which can overlap at corners. At crop limits the normal translation
is shortened. Width is limited to half the smaller crop span. UV inversion works
with either mapping. This setting is saved in projects and generation presets.
The original drawing still determines the sampled joints; this mode does not detect
or straighten them automatically.

Repeated textures can add UV seams and geometry. Materials participate in preview,
wireframe, simplification, and export. Image transparency is preserved; stretched
outer-edge segments crossing transparent pixels can sample a more opaque pixel
inside the crop. Solid side colors are opaque.

## Constrained curved simplification (7.270.0)

In Manual curved mode, open **Simplification** and enable **Simplify curved relief**.
Set **Faces to keep** (0.5 requests half the front triangles) and **Additional error**
(default 0.01, or 1% of the thickness range), then Apply. The panel reports before/after
front face counts and the conservative error bound. A partial-reduction notice means
constraints, tolerance or the bounded processing pass prevented the requested ratio.

Contour vertices, target/local-region boundaries, the legacy peak/circle ring and
sampled local extrema remain fixed. Interior vertices may be removed; front/back
and sides are then generated together with fresh normals. Error is bounded against
the dense generated surface, not directly against the analytic field; initial surface
sampling still has its own `heightTolerance`. Dense generation must fit the vertex
budget before this pass can run. Maps continue to show the analytic thickness field.

This option is off by default, including in old projects with a saved generic
simplification setting. Apply, undo/redo, `.imesh`, asynchronous generation and
export use the same options. Each generation starts from the dense surface, so
repeated Apply does not accumulate losses. The pass runs only during generation,
supports cancellation and adds no recurring work while the editor is idle.
Since 7.275.0, the 3D preview also offers **Comparison / Side by side** in this
section, with the same visibility and wireframe controls as generic simplification.
The original uses identical options with only `curvedSimplify=false`; the result
uses the selected reduction. Comparison counts refer to the complete meshes, while
the curved reduction/error summary above refers to the front surface. No generic
simplifier error is substituted for the curved error bound.

Preparing a curved comparison requires an additional asynchronous generation for
the reference. An explicitly requested statistics calculation caches both meshes;
switching to 3D reuses them. Visibility, wireframe and comparison toggles do not
regenerate geometry. Cancelling reference generation does not install a partial
comparison. Export generates only the selected result, without the reference.

## Simplification and comparison

General Method offers **QEM** (default) and **Coplanar (CGAL external)**.
Settings are saved per project/default/region and apply to preview, statistics,
comparison, assembly and export. QEM runs in the engine; coplanar reduction runs
in the independently configured [mbm-cgal](https://github.com/michelmorais/mbm-cgal)
executable. No CGAL library is linked into the engine.

CGAL exposes **Coplanar angle** (0..60 degrees, default 0.05) and independent
**Plane distance** (0..10% of the whole exported input diagonal, default 0.00001%).
The saved fields are `planarAngle` and `planarTolerance` (distance fraction).
These controls affect CGAL region/corner detection. Its reported surface error
is sampled, not a certified bound. UV seams and materials are retained; existing
normals are reconstructed as face normals. The ratio, Preserve Details and
boundary-collapse controls apply only to QEM. No simplification runs on idle frames.

For image/manual/mixed relief, enable **Simplify after generation**. Both editors
use the chosen backend over the whole generated frame, including its materials.
QEM controls:

- Triangle ratio: 0.001–0.95, default 0.9; the fraction to retain.
- Preserve details: enabled by default; penalizes collapses near geometric detail.
- Boundary-collapse threshold: 0–0.25; zero locks open boundaries, while larger
  values allow eligible boundary collapses relative to the mesh diagonal.

The panel shows estimated triangles, actual before/after counts, reduction, and
reported error. These are geometry metrics, not an assessment of image quality.
If constraints prevent the requested reduction, the editor reports failure rather
than silently exporting the unsimplified mesh. Disabling simplification regenerates
the source geometry, so repeated edits do not accumulate simplification losses.

Side-by-side comparison shares camera and lighting. The meshes are spaced using
their bounds; checkboxes control each version's visibility and indicate its side.
Wireframe follows those controls. Comparison reuses cached results and does not
change the exported version: export uses the simplified mesh even if only the
original is visible. Leaving comparison restores the previous viewing distance.

Generation and simplification show progress and support **Cancel** or **Esc**.
Cancelling preserves the previous preview, identifies it as outdated, and prevents
export of the cancelled result. In a batch, remaining modules stop while completed
exports remain. Retry explicitly by applying settings or returning from edit mode
to 3D. See [Mesh Simplification](mesh-simplification.md) for the shared algorithm.

## Assembly preview

In 3D, enable **Show assembled modules** in the assembly controls. This replaces
individual simplification comparison with a configurable set of preview objects
and their current materials. Initially there is one object per project module.
Since 7.278.0, select **Preview object** and choose its **Source module** independently.
**Add copy** adds another object using that source; **Remove object** removes only
the preview instance. A project with a single module can therefore test two or more
copies side by side without duplicating the editable module.

Set grid columns, X/Y spacing, and Arrange to distribute objects in preview order.
Cells use the largest displayed module dimensions. Select a preview object to change
its zero-based row/column, depth offset, or visibility, independently of other copies. Fit Assembly centers the view. Base front
planes align at Z=0 before relief and per-module depth offsets.

Placement uses numeric controls; it is a session preview, not saved/exported scene
layout or project history. Disabling it restores the individual view. Edit mode
hides the assembly; opening another project clears it.

Updates generate each distinct source module once, then load independent preview
instances before replacing the visible assembly. Objects referencing deleted source
modules are removed on rebuild; newly created modules can be chosen as sources. Failure or
cancellation preserves the previous assembly, camera, and framing, labels it as
outdated, and discards partial results. Only a complete successful update publishes
new geometry and counts. Layout, object selection, and visibility changes reuse
ready meshes. Adding/removing objects or changing their source invalidates the
assembly; its controls are disabled while a generation task is running.

## Projects, presets, and export

### Projects and presets

Save Project (Ctrl+S) writes a versioned `.imesh` Lua data file and displays a
confirmation. Projects store regions, inherited/overridden properties, holes,
manual heights, painting, and presets. Files are loaded in an empty environment
and validated. Undo history, camera/light settings, and GPU resources are not saved.

Images under the project directory use relative references. If the source image
is missing, Locate Image accepts a replacement with matching dimensions. Reselect
missing external height, side, or back images in their respective controls.
Each decoded source/height/material image is limited to 16,777,216 pixels.

Generation presets capture displayed settings, including unapplied values, without
generating geometry. They can be created, updated, renamed, deleted, and applied to
selected modules or project defaults. Application preserves module identity, crops,
contours, holes, manual areas, and committed painting. Presets contain generation
parameters, not camera settings or region-specific shape data.

Presets are saved inside the project; there is no separate automatic library.
Export/import `.imeshpreset` files to share them between projects. Import adds a
preset without applying it and rejects duplicate names. A project accepts 128
presets, with unique nonempty names up to 128 bytes; preset files are limited to
64 KiB. Preset operations participate in undo/redo.

### Mesh and portable texture export

Export the selected module or all modules to `.msh` v11. Generated filenames
include the module ID. Plain mesh export references its existing texture files;
keep them available on the engine's asset paths.

The **mesh + textures** commands produce portable meshes and PNGs in one directory.
Move that directory together. By default, export preserves the entire source image's
dimensions and RGBA pixels, transparency, and original UVs. Power-of-two dimensions
remain unchanged. This decodes/re-encodes PNG data rather than copying source bytes.

Optional texture cropping exports each material's UV-covered rectangle with a
four-pixel replicated border and remaps UVs. Crops are not rounded to powers of two.
A material shared by front/back uses bounds covering both. Padding reduces filtering
bleed but does not guarantee isolation at every mip level.

Within one export, materials/modules using the same source image share a PNG.
Cropped exports also require identical UV bounds to share it. Different source
files are not deduplicated by pixel content. There is no shared texture atlas.
Solid colors stay in the mesh; a separate height image is not copied as a material.
Texture filenames in the mesh are limited to 63 bytes.

Portable export overwrites destination files. It prepares a module's temporary
outputs before replacement, keeps backups during replacement, and attempts to
restore them if replacement fails. Failures are reported; completed batch modules
remain exported. Project inputs are not changed. Previously exported files that
are no longer referenced are not automatically deleted.

## Verification and API reference

The editor updates geometry, diagnostic maps, and wireframe caches when their
inputs change or an operation is requested, not continuously while idle.

Representative automated coverage in `src/test-lib/` includes:

- `image_mesh_smoke.lua`: geometry, validation, export/reload, and rendering.
- `image_mesh_relief_smoke.lua`: sampled relief interpolation and adaptive geometry.
- `image_mesh_areas_smoke.lua` and `image_mesh_height_line_smoke.lua`: manual heights.
- `image_mesh_facets_smoke.lua`: planar field, hard normals, sectors/rings, holes, materials, map agreement, export, vertex budget, snapshots and cancellation.
- `image_mesh_facets_editor_smoke.lua`: ImGui controls, history, save/reopen, export/map and idle behavior.
- `image_mesh_finishing_smoke.lua`: areas and brushes in all height modes, operation order, bounded results, disabling, maps and curved geometry.
- `image_mesh_finishing_editor_smoke.lua`: area operations/history, clear-and-undo, persistence, original/result comparison, export and idle UI.
- `image_mesh_curved_paint_smoke.lua`: bounded brushes, targets/borders, holes, interior transition, simplification, maps, budgets and asynchronous snapshots.
- `image_mesh_curved_paint_editor_smoke.lua`: real enable control, stroke history, persistence, painted original/result comparison, export and idle behavior.
- `image_mesh_curved_quality_smoke.lua`: saw-like outline, Bézier hierarchy, thin-triangle regression, target-edge heights, hole/manifold checks and angle-weighted normals.
- `image_mesh_curved_holes_smoke.lua`: curved through-cuts, target/line intersections, 16 holes, concavity, watertightness, Euler/area/volume, side modes, maps, async snapshots and cancellation.
- `image_mesh_curved_holes_editor_smoke.lua`: move/resize, invalid edits, history, persistence, preview/export, ImGui guidance and idle behavior.
- `image_mesh_simplify_smoke.lua`: preview, comparison, export, history, and cache reuse.
- `image_mesh_map_async_editor_smoke.lua`: diagnostic-map replacement/cancellation.
- `image_mesh_simplify_cancel_smoke.lua`: cancellation and export preservation.
- `image_mesh_assembly_cancel_smoke.lua`: assembly failure, cancellation, and retry.

Run engine tests using the repository's
[engine-testing instructions](../.agents/skills/engine-testing/SKILL.md).
The public scripting contract is documented in
[Lua API: image-based mesh generation](lua-api.md#image-based-mesh-generation).

Interior-transition smoke tests: `src/test-lib/image_mesh_interior_smoke.lua` and
`src/test-lib/image_mesh_interior_editor_smoke.lua`.

### External CGAL simplification

General simplification now includes **Coplanar (CGAL external)**. Configure the
standalone `mbm-cgal-planar` executable under **Options > CGAL executable** and
click **Save path**. Mesh Debug shares this preference; project files store the
`cgal` mode and angle/distance, not the machine-specific executable path.

Preview and export run the external tool asynchronously, preserving UV seams and
materials while reconstructing face normals. Smooth authored normals are not
retained. The generated result must fit the project's vertex budget and the
engine's 65,535-vertex frame limit. Cancellation or process/import failure retains
the previous preview. The dedicated curved-relief simplifier is unchanged.
See [MBM CGAL](https://github.com/michelmorais/mbm-cgal)
for build instructions, supported geometry, timeouts and report details.
