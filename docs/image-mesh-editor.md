# Image Mesh Editor

An offline editor for turning image regions into textured 3D modules. It supports
image-derived relief, manually drawn heights, brush corrections, holes, editable
contours, simplification, and individual or batch export.

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

Lighting affects only the 3D scene. Ambient color, directional-light color and
direction, and light reset are available. Camera, light, and wireframe changes do
not regenerate geometry or alter exported data. Changing properties on the same
module preserves its camera view; selecting another module provides a new framing.

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
- Ctrl+click extends the selection. Duplicate and Delete affect the selection.
- Applying generation parameters affects all selected regions; name, crop, and
  contour changes affect the primary selection only.
- Project defaults provide inherited parameters. Resetting a selection to the
  defaults clears its overrides.

**Edit project defaults** switches the property groups below to project-wide
settings and opens **Volume and relief**. Module-specific fields such as the name
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

## Volume and relief

**Volume and relief** controls world dimensions, base depth, relief amplitude,
and fixed border height. **Preserve image aspect ratio** derives world height
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
| Image | Processed image values, followed by brush corrections |
| Manual | A normalized base height plus enabled manual areas and brush corrections; image colors do not determine height |
| Image + manual areas | Processed image values overridden by manual areas, followed by brush corrections |

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

In Manual or mixed mode, add rectangles/ellipses or draw polygons/freehand areas.
Adding the first area in Image mode selects mixed mode and adaptive refinement.
Each area has a name, enabled flag, normalized target height, and inward transition
width. World height is the normalized target multiplied by relief amplitude.
For example, base 0.4, raised area 0.8, and groove 0.2 define three independent levels.

Move areas by dragging inside them; use handles or width/height fields to resize.
Areas stay within the crop but may extend outside the module contour, which clips
the final geometry together with holes. Later areas override earlier ones; reorder,
duplicate, disable, or remove them in the list. Disabled areas remain saved.

**Line/groove** creates an open path of 2–128 points with editable width and rounded
caps/joins. The target height is absolute, not an accumulated displacement.
Self-crossings are allowed and do not add depth. Transition proceeds inward; if it
exceeds half the width, the center cannot reach the full target height.

A module supports 32 areas, with up to 128 points each. Coordinates and line width
are normalized to follow crop resizing. Rasterization has a 64-million edge-evaluation
budget. Areas are saved and duplicated with the module; presets do not copy them.
Brush corrections follow area composition, and fixed outer-border treatment comes last.

### Height painting

Enable painting for the selected module to apply pending settings and show its
height map. Raise, Lower, Flatten, and Smooth operate within a brush radius shown
in image pixels. Strength ranges from 0.01 to 1; Flatten uses a target height in 0–1.

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

## Geometry, normals, and budgets

**Follow image shapes (adaptive)** refines relief according to sampled interpolation
error and aligns edges with processed height transitions. Relief tolerance is a
fraction of amplitude; columns/rows constrain sampling density. This is a sampled
criterion, not a maximum error guarantee at every pixel. Fine details may require
higher resolution. The output always consists of triangles, not a QUAD mesh.

With adaptive geometry and Two heights, plateau faces receive priority when
computing normals shared with steep transitions. This preserves flat top/bottom
shading without duplicating vertices. Other vertices use adjacent-face averaging;
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

Repeated textures can add UV seams and geometry. Materials participate in preview,
wireframe, simplification, and export. Image transparency is preserved; stretched
outer-edge segments crossing transparent pixels can sample a more opaque pixel
inside the crop. Solid side colors are opaque.

## Simplification and comparison

Enable **Simplify after generation** to use the same simplifier as Mesh Debug over
the whole generated frame, including its materials:

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
individual simplification comparison with all modules and their current materials.

Set grid columns, X/Y spacing, and Arrange to distribute modules in project order.
Cells use the largest module dimensions. Select a module to change its zero-based
row/column, depth offset, or visibility. Fit Assembly centers the view. Base front
planes align at Z=0 before relief and per-module depth offsets.

Placement uses numeric controls; it is a session preview, not saved/exported scene
layout or project history. Disabling it restores the individual view. Edit mode
hides the assembly; opening another project clears it.

Updates prepare all modules before replacing the visible assembly. Failure or
cancellation preserves the previous assembly, camera, and framing, labels it as
outdated, and discards partial results. Only a complete successful update publishes
new geometry and counts. Layout, selection, and visibility changes reuse ready meshes.

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
- `image_mesh_simplify_smoke.lua`: preview, comparison, export, history, and cache reuse.
- `image_mesh_map_async_editor_smoke.lua`: diagnostic-map replacement/cancellation.
- `image_mesh_simplify_cancel_smoke.lua`: cancellation and export preservation.
- `image_mesh_assembly_cancel_smoke.lua`: assembly failure, cancellation, and retry.

Run engine tests using the repository's
[engine-testing instructions](../.agents/skills/engine-testing/SKILL.md).
The public scripting contract is documented in
[Lua API: image-based mesh generation](lua-api.md#image-based-mesh-generation).
