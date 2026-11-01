# Facial Rig Editor

The Lua Facial Rig Editor authors facial surface controls, morph targets, named
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
separately. **Synchronized sides** mirrors paired facial shapes and side landmarks around the nose's
X position, or the face origin until the nose is placed. Enabling synchronization uses the
currently selected mask as the source. Subsequent gizmo drags, right drags, surface adjustments,
and explicit template refits update the opposite side. Mirroring changes its point offsets while
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

The stock catalog contains paired masks for brows (AU1, AU2, AU4), eyes (AU5, AU7, AU41-AU46),
cheeks (AU6), mouth corners (AU12, AU15), and nasolabial folds (AU11). It also has upper-lip
(AU10), lower-lip (AU16), chin (AU17), jaw (AU26, AU27, AU29-AU31), and nose/nostril
(AU9, AU38, AU39) masks. These AU labels identify related actions; a mask does not solve or
generate those movements. The
[Ekman and Friesen AU table](https://www.paulekman.com/wp-content/uploads/2013/07/Measuring-Facial-Movement.pdf)
is the naming reference. Each mask has an area polygon, one or more action-line polylines, a group
controller, and a separate action-line controller. The brow masks start with 12 contour anchors
and five action-line anchors; the other masks provide their own pre-authored contours. All are adapted to the
current mesh after the face origin is placed. Initial fitting uses the mesh subset containing the
face origin and prioritizes the front view contour over guessed depth. This avoids snapping brow
points to a separate hair subset and reduces jumps toward the eyelid. The green group controller
moves the complete mask. The yellow action-line controller moves only the action lines and itself;
individual area and line anchors still move one point at a time. The action-line controller is
placed apart from the line so its pick target stays distinct from the line anchors.
New projects load all 15 masks when the face origin is placed. Saved projects keep their authored
mask catalog and are not expanded automatically. The point landmark list already includes the
chin and both jaw hinges; they are optional rig references and do not create surface masks. The
jaw polygon is a separate influence region. **Refit selected mask to face** replaces only the
selected mask's points with its current template; it is undoable. Refitting an older eye template
also adds its new lower-eyelid action line. Refit never runs automatically on existing edits.
Opening an older brow project adds only a missing action-line controller and its relationships;
it is placed near the already edited action line. Existing contours, offsets, and the
group-controller position are preserved.

All mask outlines remain visible. The shared default palette uses cyan for influence areas and
orange for action lines on both sides and for future facial templates. Previously saved stock
right-brow pink and older action-line accents are upgraded on load; custom area and action-line
colors are preserved. Every group controller is green, including those in older projects.
The group controller is a larger green square
with a cross; the action-line controller is a yellow square with a cross. Only the selected mask
displays square anchors for editing.
In the viewport, left drag on empty space orbits the camera. Clicking a square anchor or an outline
selects its point (a segment selects its nearer endpoint) and displays a 3D translation gizmo.
Dragging the red X, green Y, or blue Z axis moves that point freely in 3D. Dragging the green
controller's gizmo moves every point in its mask group by the same world-space displacement;
dragging the yellow controller moves only the action lines and itself. Right drag still snaps an
anchor or the corresponding controller's set of points to mesh vertices. Further facial actions
can extend the catalog without requiring the user to create each shape from an empty control.
Middle drag pans the whole viewport, as in **Face position**, without editing facial points.

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

**Masks / Regions** now generates a named region from the selected facial mask's closed area
polygon. Generation is explicit and preserves the source points. It projects the outline in face
X/Y, selects vertices from the source mesh subset within a local front-surface depth band based
on the anchor vertices (so lifting the contour in Z does not switch mesh layers), and
stores binary vertex weights. The stored region records its source group, source control,
source fingerprint (including the face scale used for the depth band), and mesh topology
signature. Changing an anchor, face scale, or topology makes it
stale; the editor reports that state and requires explicit regeneration before the region can be
used. **Copy to Morph Target mask** creates an independent, editable snapshot of a current
region's selected vertices and opens it for visual review. The copy does not remain linked to the
source. The existing legacy and interim polygon-mask editors remain available. Surface falloff,
automatic rig-weight assignment, and Blender bridge transport of generated regions remain future
work.

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

Saving the facial project stores editor metadata, facial action masks and their surface controls,
generated regions, legacy compatibility masks, expressions, clips, target selection, and preview
weight. Target edits
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
