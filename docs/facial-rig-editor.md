# Facial Rig Editor

The Lua Facial Rig Editor authors facial surface controls, generated regions, morph targets, named
expressions, and facial clips for an assigned MSH. The editor can create and sculpt targets without
Blender. Faceit is a workflow reference; its add-on and SDK are not required to edit or export an
MSH.

## Morph Targets

The **Morph Targets** worktree has four modes:

| Mode | Current action |
| --- | --- |
| Add Target | Create a zero-delta target with a unique name, then open it in Edit Target. A new editor-authored target starts with the `Manual` convention. |
| Edit Target | Change name, convention, and default weight; duplicate or delete; apply translation, pivot rotation, or reset to the chosen vertices; inspect expression and clip references. |
| Paint Target | Paint local Move or Smooth delta changes on the preview surface, optionally restricted to a mask; use local undo and redo. |
| Mirror Target | Analyze neutral-vertex correspondence across an editable face X plane, inspect diagnostics and a temporary deformation preview, then explicitly create the mirrored target. |

The convention field stores a label: `ARKit`, `FACS`, or `Manual`. It does not convert channels or
change the deformation. FACS uses facial Action Unit names; the editor does not implement an
automatic Action Unit solver. Existing imported convention labels remain intact until changed.

Edit Target can apply a batch transform to all vertices, a selected vertex, or a facial selection
mask. Masked translation and rotation can use a soft edge over adjacent connected triangle rings.
The rotation pivot can be positioned in the 3D viewport. The transform controls show a draft before
Apply; Apply writes deltas to the candidate target. Reset to neutral clears the selected deltas.
Paint Target's Move brush uses the entered X, Y, and Z offset with radius and strength. Smooth
averages neighboring deltas; a zero target needs a Move or batch transform to gain displacement.
The brush edits canonical frame-0 vertex indices while picking against the deformed preview.

Mirror Target reports matched vertices, active source vertices without partners, and ambiguous
matches. It previews a temporary target without modifying the editor candidate. An approximate
match requires explicit opt-in and visual inspection. Create mirrored target commits the new target;
the source target is retained. Optional viewport guides show the chosen source vertex, its reflected
position, and the match radius.

## Facial landmarks, surface controls, and regions

The facial authoring flow starts in **Landmarks**. The older cyan face guide (outline, crosses,
and axes) is a landmark placement reference, not a vertex mask. It appears in **Face Position**
and can be shown on demand with **Show landmark guide** in **Landmarks**; it starts hidden there
so facial action shapes are clear.

**Side editing mode** appears at the top of Landmarks. **Independent sides** edits each side
separately. **Synchronized sides** mirrors paired brow shapes and side landmarks around the nose's
X position, or the face origin until the nose is placed. Enabling synchronization uses the
currently selected brow as the source. Subsequent gizmo drags, right drags, surface adjustments,
and explicit template refits update the opposite brow. Mirroring changes its point offsets while
keeping its mesh vertex references and colors; the operation participates in Undo.

The author selects a pre-defined facial action mask and adapts its controls directly over the mesh:

The templates are FACS-inspired authoring references, not an automatic FACS solver. FACS describes
visually discernible facial movement through Action Units; the editor maps those actions to editable
mesh controls. See the [Paul Ekman Group FACS overview](https://www.paulekman.com/facial-action-coding-system/)
and the [visual reference used by the team](https://coralievogelaar.com/Facial-Action-Coding-System).

| Kind | Meaning | Initial authoring behavior |
| --- | --- | --- |
| polygon | Area of influence, such as a brow or cheek region | Closed surface shape with editable contour and depth-adjustable anchors |
| polyline | Line of action, such as a lip border, furrow, or wrinkle | Open surface line with editable points and depth-adjustable anchors |
| point | Localized landmark or controller | Explicitly references the polygons/polylines it controls; it is not limited to a single vertex |

The first template slice contains left and right brow masks. Each brow mask has an area polygon,
an action-line polyline, a group controller, and a separate action-line controller. The templates are initialized with
12 contour anchors for the brow area and five anchors for the action line, then adapted to the
current mesh after the face origin is placed. Initial fitting uses the mesh subset containing the
face origin and prioritizes the front view contour over guessed depth. This avoids snapping brow
points to a separate hair subset and reduces jumps toward the eyelid. The green group controller
moves the complete mask. The yellow action-line controller moves only the action lines and itself;
individual area and line anchors still move one point at a time. The action-line controller is
placed above the line so its pick target stays distinct from the line anchors.
In a previously saved project, **Refit selected brow to face** replaces that brow's points with
the current template; the operation is undoable and never runs automatically on existing edits.
Opening an older brow project adds only a missing action-line controller and its relationships;
it is placed near the already edited action line. Existing contours, offsets, and the
group-controller position are preserved.

All mask outlines remain visible. The shared default palette uses cyan for influence areas and
orange for action lines on both sides and for future facial templates. Previously saved stock
right-brow pink and older action-line accents are upgraded on load; custom colors are preserved.
The group controller is a larger green square
with a cross; the action-line controller is a yellow square with a cross. Only the selected mask
displays square anchors for editing.
In the viewport, left drag on empty space orbits the camera. Clicking a square anchor or an outline
selects its point (a segment selects its nearer endpoint) and displays a 3D translation gizmo.
Dragging the red X, green Y, or blue Z axis moves that point freely in 3D. Dragging the green
controller's gizmo moves every point in its mask group by the same world-space displacement;
dragging the yellow controller moves only the action lines and itself. Right drag still snaps an
anchor or the corresponding controller's set of points to mesh vertices. The editor should extend this
template catalog to the remaining FACS-inspired facial actions without requiring the user to create
each shape from an empty control.

The current authoring slice exposes the facial-action mask selector at the top of **Landmarks**.
The user selects a shape by clicking its points or outline in the viewport. The **Edit shape**
selector and **Depth** slider are no longer shown; the gizmo edits X, Y, and Z directly. Previously
saved depth offsets remain visible and are retained until the selected shape is adjusted to the
surface. Each anchor keeps its mesh vertex index and an optional 3D offset, which is saved in the
project and can be undone.
**Snap selected shape to face surface** projects every point of the shape selected in the viewport
along face Z, using the mesh subset of that point. It solves the surface Z independently for each
point, preserves each X/Y position, and applies the shape only when all points can be projected.
The operation is undoable; the other shapes in the mask remain unchanged. The separate
landmark button below continues to raycast a selected individual landmark onto the surface.

**Masks / Regions** consumes the authored controls. It generates or refreshes derived regions and
vertex weights from the selected polygon, polyline, or point relationships. Generated regions retain
the source-control identity and topology signature, and become stale when the authored control or
source topology changes. The user must explicitly regenerate and review them.

Legacy vertex masks remain a compatibility representation for old projects and existing workflows.
They are not the authoring surface for the new facial workflow, and the editor must not silently
convert a legacy mask into a surface control. Their viewport visualization starts disabled and is
kept hidden outside **Masks / Regions**, so it cannot be confused with the FACS-inspired controls.
It can still be explicitly enabled while editing a legacy mask in that workspace for compatibility
review.

In the 3D viewport, left drag orbits and middle drag pans. Right click picks a vertex in Edit Target;
right drag applies the selected brush in Paint Target. The selected target has a preview weight and
an optional displacement overlay. The overlay is a visual aid; the textured mesh can be shown again
without discarding the selected weight. Viewport mesh visibility controls the selected surface.

## Project and MSH persistence

Saving the facial project stores editor metadata, facial action masks and their surface controls, legacy compatibility masks,
expressions, clips, target selection, and preview weight. Generated-region metadata will be added
when the derivation stage is implemented. Target edits
are stored as sparse changes relative to the assigned source MSH.
Reopening validates source target identity and topology before applying those changes. If the
assigned MSH already contains the exact edited target set and deltas, the editor treats the project
patch as incorporated and can save the project without the redundant patch. Other source mismatches
are reported as errors.

Exporting an MSH is an explicit operation. The MSH stores dense target position deltas, optional
normal deltas, convention and default weight, and facial clip data. The editor preview weight is a
project preference, not an MSH target default. After exporting to a new path, save the project to
retain the updated assigned MSH path. Mesh Debug and a runtime mesh instance can read the exported
targets. A new project opened on an MSH that already has targets does not remove them.

The current format and runtime contracts are in [Mesh V11 format](mesh-v11-format.md#facial-sections-45-46-version-1),
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md#facial-composition-on-the-same-mesh),
and the [Lua API](lua-api.md#facial-targets-and-clips-mesh). Remaining work is listed in the
[facial animation plan](realtime-facial-animation-future-work-plan.md).
