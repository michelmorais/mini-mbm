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

Choose **Vertex mask** or **FACS mask** once in the first workspace, **Setup**. The
selection is fixed for the project; start a new project to choose the other workflow. Older facial
project schemas are not loaded by this editor version. The workspace tree then follows the selected
path: **Rig** is available in Vertex mask, while **FACS Action Unit Profiles** is available
in FACS mask. **Morph Targets**, **Expressions**, and **Clips** are shared.
The Action Unit reference-table button appears in **Setup** only after selecting **FACS mask**; it is
also available from **FACS Action Unit Profiles**.
Loading a project with authored FACS controls does not upgrade, add, or recolor its template shapes.
Templates are seeded only when the FACS catalog is empty. If a template change alters saved control
data, bump the project schema; older schemas are rejected and must be recreated.

**Vertex mask** is the classic workflow for placing facial landmarks and editing vertex
masks. The cyan face guide (outline, crosses, and axes) is a landmark placement reference, not a
vertex mask. It appears in **Face Position** and can be controlled with **Show landmark guide** in
this workflow. The guide is shown by default and can be hidden while editing landmarks.

**FACS mask** shows the facial action templates and their surface controls. In the workspace tree,
the entry that is called **Landmarks** in the classic workflow is displayed as **FACS facial masks**.
That is the FACS equivalent of **Marcos faciais**: it is where the influence polygons and group
controllers are edited. The classic landmark controls, cyan guide,
and Rig workspace are hidden in this workflow. Its selected path is stored with the project.

**Side editing mode** appears below the workflow help and is available in both modes.
**Independent sides** edits each side separately. **Synchronized sides** mirrors paired facial
shapes and side landmarks around the nose's X position, or the face origin until the nose is placed.
Enabling synchronization uses the currently selected mask as the source. Subsequent gizmo drags,
right drags, surface adjustments,
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
| point | Group controller for one or more influence areas | Explicitly references the polygons it moves; it is not limited to a single vertex |

The stock catalog contains paired masks for brows (AU1, AU2, AU4), eyes (AU5, AU7, AU41-AU46),
cheeks (AU6), mouth corners (AU12, AU15), and nasolabial folds (AU11). It also has upper-lip
(AU10), lower-lip (AU16), chin (AU17), jaw (AU26, AU27, AU29-AU31), and nose/nostril
(AU9, AU38, AU39) masks. These AU labels identify related actions; a mask does not solve or
generate those movements. The
[Ekman and Friesen AU table](https://www.paulekman.com/wp-content/uploads/2013/07/Measuring-Facial-Movement.pdf)
is the naming reference. Each mask has an influence polygon and a green group controller. The brow
masks start with 12 contour anchors; the other masks provide their own pre-authored contours. All are
adapted to the current mesh after the face origin is placed and FACS mode is selected. Initial
fitting uses the mesh subset containing the face origin and prioritizes the front view contour over
guessed depth. This avoids snapping brow
points to a separate hair subset and reduces jumps toward the eyelid. The green group controller
moves its linked influence polygons at the stock weight of 1, along with itself. Individual area
anchors still move one point at a time.
New projects load all 15 masks when the face origin is placed while FACS mode is active. Saved
projects keep their authored mask catalog and are not expanded automatically. The point landmark
list already includes the chin and both jaw hinges; they are optional rig references and do not create surface masks. The
jaw polygon is a separate influence region.

All mask outlines remain visible. The shared default palette uses cyan for influence areas on both
sides and for future facial templates. Group controllers use green. The group controller is a
larger green square with a cross. Only the selected mask
displays square anchors for editing. Each square faces the camera in the view plane through its
polygon vertex, so its visible corners stay at the vertex's camera depth while the camera orbits.
The controller crosses use the same plane, and the 3D gizmo starts at the exact vertex position.
The engine preserves Z zero when registering 3D objects, so the contour and its
handles receive no implicit creation-order displacement.
In the viewport, left drag on empty space orbits the camera. Clicking a square anchor or an outline
selects its point (a segment selects its nearer endpoint) and displays a 3D translation gizmo.
Hold Ctrl while clicking to restrict the interaction to an axis on the currently selected point's
gizmo. Ctrl uses a wider axis hit area; clicking outside the gizmo neither selects another point
nor starts an orbit.
Dragging the red X, green Y, or blue Z axis moves that point freely in 3D. Dragging a controller's
gizmo moves its linked controls by the configured weights; stock green controllers move their mask
group. Right drag still snaps an
anchor or the corresponding controller's set of points to mesh vertices. Further facial actions
can extend the catalog without requiring the user to create each shape from an empty control.
Middle drag pans the whole viewport, as in **Face position**, without editing facial points.

Selecting a green point controller exposes **Related controls** in the FACS mask panel.
The list shows each linked control and its movement-link weight. Weight 1 follows the controller's
full displacement; weight 0 does not move that control. This weight affects editor control movement
only; it is not an Action Unit or morph-target contribution. The user can adjust a weight, remove a
link, or add links explicitly. **Suggest nearby links** proposes currently unlinked influence
polygons based on their distance from the controller in face X/Y. The review list can be edited
or cancelled; project links change only after **Apply selected links**. A controller moves only
its direct links, so links to another controller do not recursively move that controller's links.
Changes support Undo/Redo and persist in the project. In synchronized side mode, paired control
links and weights are mirrored where counterparts exist; independent mode edits only one side.

In **FACS masks** mode, the facial-action mask selector appears below the mode choice. The user
selects a shape by clicking its points or outline in the viewport. The **Edit shape**
selector and **Depth** slider are no longer shown; the gizmo edits X, Y, and Z directly. Previously
saved depth offsets remain visible and are retained until the selected shape is adjusted to the
surface. Each anchor keeps its mesh vertex index and an optional 3D offset, which is saved in the
project and can be undone.
**Snap selected shape to face surface** projects every point of the shape selected in the viewport
along face Z, using the mesh subset of that point. It solves the surface Z independently for each
point, preserves each X/Y position, and applies the shape only when all points can be projected.
The operation is undoable; the other shapes in the mask remain unchanged. Landmark placement and
its individual surface-adjustment controls appear only in **Vertex mask** mode.

In the **FACS mask** workflow, **Masks / Regions** generates a named region from the selected facial mask's closed area
polygon. Generation is explicit and preserves the source points. It projects the outline in face
X/Y, selects vertices from the source mesh subset within a local front-surface depth band based
on the anchor vertices (so lifting the contour in Z does not switch mesh layers), and
stores binary vertex weights. The stored region records its source group, source control,
source fingerprint (including the face scale used for the depth band), and mesh topology
signature. Changing an anchor, face scale, or topology makes it
stale; the editor reports that state and requires explicit regeneration before the region can be
used. **Copy to Morph Target mask** creates an independent, editable snapshot of a current
region's selected vertices and opens it for visual review. The copy does not remain linked to the
source. The **Vertex mask** workflow instead shows Rig role management and classic vertex
masks here; it does not show FACS region generation. Surface falloff,
automatic rig-weight assignment, and Blender bridge transport of generated regions remain future
work.

Generated regions also carry the generic region fields used by the upcoming Morph Editor:
polygon kind, canonical surface anchors, topology identity, and rebuildable vertex weights. Facial
generation retains its front-depth and mesh-subset rules; the generic polygon core is being prepared
for non-facial meshes separately. This first generic core derives binary vertex selections. Its
stored falloff settings do not affect weights yet.

Projects choose one authoring workflow in **Setup**: **Vertex mask** uses classic facial
landmarks, Rig roles, and vertex masks, while **FACS mask** uses surface-action templates, generated
regions, and target masks. The choice is fixed for the project; start a new project to choose the
other workflow. The workspace tree hides **FACS Action Unit Profiles** in the classic workflow and
hides **Rig** in the FACS workflow. **Masks / Regions** also filters its controls by workflow:
classic projects show Rig role management and classic masks; FACS projects show generated FACS
regions and target masks. **Morph Targets**, **Expressions**, and **Clips** are shared. Project
schema 21 stores the choice; earlier projects are rejected and must be recreated.

Classic Rig role masks remain an authoring representation in the **Vertex mask** workflow.
Rig and Morph Target selection masks use vertex selections in that workflow. In the FACS workflow,
surface polygon editing belongs only to **FACS facial masks** (the FACS version of **Marcos faciais**);
there is no representation selector in **Masks / Regions**. That workspace only generates regions
from the authored FACS area polygon and edits the resulting vertex selections. Classic masks are not
silently converted into FACS surface controls. Their viewport visualization starts disabled and
remains inside **Masks / Regions**. FACS target masks are created from generated regions or as
explicit vertex-selection masks in the FACS workflow.

In a **Vertex mask** project, **Rig** first materializes the missing facial bones and verified
vertex weights directly in the loaded MSH. Its optional **Generate facial armature preview in
Blender** section is open by default. Export the Blender bridge, then use **Generate preview
armature in Blender** to open a selected `.blend`, import the bridge automatically, and create the
`FacialRigPreview` armature. This Blender armature is for visual review and continued manual work;
it does not assign vertex weights or replace the rig materialized in the MSH.

In the 3D viewport, left drag orbits and middle drag pans. Right click picks a vertex in Edit Target;
right drag applies the selected brush in Paint Target. The selected target has a preview weight and
an optional displacement overlay. The overlay is a visual aid; the textured mesh can be shown again
without discarding the selected weight. Viewport mesh visibility controls the selected surface.

## FACS Action Unit profiles

The **FACS Action Unit Profiles** workspace maps semantic Action Units to facial targets already
present in the loaded MSH. A profile has a name, AU identifier, side (`bilateral`, `left`, or
`right`), normalized input range, preview intensity, and one or more target outputs. Each output
stores its contribution weight, response curve, optional per-output clamp, and a secondary
contribution marker for neighboring regions. The secondary marker is descriptive metadata; it
does not change the weight calculation.

The workspace is divided into three modes: **Add profile**, **Edit profile**, and **Delete profile**.
Only the controls belonging to the selected operation are shown. Editing changes profile metadata,
target outputs, composition, and preview; deleting uses the selected profile without mixing those
controls with the authoring form.

An Action Unit (AU) is a standardized label for an observable facial movement. For example, AU1
identifies the inner brow raiser. The profile's AU names the action; mapped target outputs define
which deformations the mesh receives. The combo shows each code with its standardized action name.
The reference window lists the 44 single facial AUs and marks which have a supplied surface-mask
template. A standardized AU without a template can still be profiled by manually mapping existing
morph targets. The editor does not infer mappings from an AU name.

The workspace does not infer target mappings from facial polygons. The user
chooses each target explicitly. Multiple enabled profiles can be composed with **Add, clamp at 1**
or **Maximum contribution**. Missing target references are reported and block preview or expression
capture. Applying the composed preview writes the resulting weights to the mesh; targets without
an output in the composition are set to zero. The composed-target table shows the resulting weight
and whether each mapped target has primary, secondary, or both contribution types.

**Capture as expression** stores the composed weights in the existing named-expression model. That
expression can be used in the existing facial clip editor and MSH export workflow. AU profiles are
saved in the facial project (schema 21); they do not add a runtime or MSH data section. The profile
authoring layer does not model muscle anatomy or provide live capture input. The standard action
names follow the [CMU FACS reference](https://www.cs.cmu.edu/afs/cs/project/face/www/facs.htm) and
the [44 single-AU catalog](https://pub.h-brs.de/frontdoor/deliver/index/docId/3118/file/brsu_techreport_04_2017_TeenaHassan.pdf).

The FACS Profiles workspace keeps an always-visible applied-composition summary below the profile
controls. It shows the final target weights after enabled profiles, output contributions, response
curves, clamps, and the selected composition rule are evaluated. This summary is the preview source
used by **Apply composed preview to mesh** and **Capture as expression**.

The workspace list is a toolbox, not a required checklist. A FACS profile requires facial targets
to exist first. In a **FACS mask** project, adapt templates in **Landmarks**, generate **Regions**
when reusable vertex selections are needed, create or import targets in **Morph Targets**, then
create a profile here, select its AU and side, and map its target outputs. Adjust the profile
intensities, apply the composed preview or capture it as an expression, then use that expression in
**Clips**. Profile edits, including **Include in preview**, take effect after clicking **Apply profile
details**.

In the facial clip timeline, **Reset facial weights** returns the current mesh preview to neutral
without changing saved keys. Use **Capture current key** afterward to store that neutral pose at
the selected time, for example as the first key before inserting a composed FACS expression at the
end of the clip.

The optional assisted Blender target-generation section is available only in the classic **Vertex
mask** workflow. It uses the classic rig bridge and landmark roles to create starter Shape Keys;
it does not consume FACS surface polygons or Action Unit profiles. FACS projects
create or import Morph Targets in the **Morph Targets** workspace and export the runtime MSH
without the classic skeleton and vertex-weight materialization step. The Blender bridge export is
also hidden in the FACS workflow until a FACS-specific interchange is implemented.

## Project and MSH persistence

Saving the facial project stores the selected authoring workflow, editor metadata, facial action
masks and their surface controls, generated regions, classic vertex masks, FACS Action Unit profiles,
expressions, clips, target selection, and preview weight. Target edits
are stored as sparse changes relative to the assigned source MSH.
Reopening validates source target identity and topology before applying those changes. If the
assigned MSH already contains the exact edited target set and deltas, the editor treats the project
patch as incorporated and can save the project without the redundant patch. Other source mismatches
are reported as errors.

Exporting an MSH is an explicit operation. The MSH stores dense target position deltas, optional
normal deltas, convention and default weight, and facial clip data. The editor preview weight is a
project preference, not an MSH target default. After exporting to a new path, save the project to
retain the updated assigned MSH path. Facial export removes optional 3D normal-map material and
tangent batches from the exported copy because the current facial renderer does not accept normal
mapping; geometric vertex normals remain available for facial normal deltas. Mesh Debug and a
runtime mesh instance can read the exported targets. A new project opened on an MSH that already
has targets does not remove them.

The current format and runtime contracts are in [Mesh V11 format](mesh-v11-format.md#facial-sections-45-46-version-1),
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md#facial-composition-on-the-same-mesh),
and the [Lua API](lua-api.md#facial-targets-and-clips-mesh). Remaining work is listed in the
[facial animation plan](realtime-facial-animation-future-work-plan.md).
