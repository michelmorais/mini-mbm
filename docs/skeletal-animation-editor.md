# Skeletal Animation Editor

Status: **Armature Templates, Bind, Bone Editor, canonical weight repair, OpenGL ES/DirectX 9/Metal GPU runtime preview, explicit CPU LBS/DQS preview, local animation and offline same-topology clip import, Paint Weights, transient composition, per-bone layer masks, and multiple wearable follower previews implemented**
Last updated: **2026-08-20**

## 1. Purpose

The Skeletal Animation Editor is the standalone Mini MBM tool for inspecting and editing skeletal
mesh data. Its implemented worktrees cover reusable armature application, bind diagnostics, direct bone editing, canonical type-42
weight repair, runtime LBS/DQS preview, and local clip/track/key/timeline authoring without expanding
Mesh Debug into a general animation editor.

For canonical skeletal meshes within the active backend's measured palette limit, the preview can
play the same per-instance GPU LBS or rigid-DQS deformation path used by the runtime. It can also
select the explicit CPU execution path for resolved LBS or rigid DQS. OpenGL ES, DirectX 9, and
Metal are delivered. Their palette contracts, recorded validation evidence, and current capability
boundaries are documented in
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md).
Paint Weights uses backend-native heatmap and brush shaders on all three delivered backends;
Metal keeps generated skeletal deformation active when a fragment-only editor shader is applied.

The editor is organized into six mutually exclusive worktrees: **Armature Template**, **Bone Editor**, **Bind Pose Contract**,
**Runtime Skeletal Preview**, **Create / Edit Animations**, and **Paint Weights**. Create / Edit
Animations and direct brush-based weight authoring are active.
No worktree is selected initially. Loading or replacing an asset returns to this neutral state,
enables the ordinary textured mesh, and keeps skeletons, heatmaps, cursors, capture volumes, and
diagnostic overlays hidden until the user explicitly opens a worktree.

### Armature Template worktree

**Armature Template** starts a new rig directly in the same editor where it will be adjusted,
weighted, and animated. The user selects the built-in **No Fingers (23)** template or imports a
canonical Armature Lua file, then explicitly confirms
application. The editor recalculates the template skeleton's real vertical extent from every bone
head and oriented tail, then fits that measured height to the loaded mesh with one uniform scale and
bottom-center alignment. The adapted skeleton therefore has the target mesh height; the mesh
geometry is not stretched or otherwise changed.

Applying a template replaces all existing canonical skeletal data, including weights and clips, as
one Undoable transaction. Validation or API failure restores the complete pre-application snapshot,
so a partial hierarchy is never left behind. Application deliberately does not guess skin weights.
The worktree points to **Bone Editor**, where the user can adjust the fitted bones and explicitly run
the existing automatic initial-weight generator, then continue to **Create / Edit Animations**.
This is armature authoring for one target mesh, not animation retargeting between two live skeletons.
**Extract Armature...** writes a sandbox-loadable `mini-mbm-armature-1` Lua table from the current
canonical skeleton. It contains the parent-first hierarchy, parent-local bind TRS, explicit local
tail offsets, connection flags, radius, and length. It deliberately excludes geometry, materials,
textures, type-42 vertex weights, and type-43 animation clips. **Import Armature...** accepts only
that validated table schema, fits it through the same height/anchor rule, and uses the existing
confirmation, Undo, and rollback transaction. Lua files execute with an empty environment while
loading. Invalid, non-finite, duplicate-name, non-parent-first, or singular-scale data is rejected
before mutation.

The built-in **No Fingers (23)** entry now comes from the canonical Lorekeeper MSH supplied for
this correction. Unlike its legacy Euler/length-only predecessor, it retains the imported local
bind quaternion and explicit `tailOffset` for every bone. For example, `upperarm.l` now owns the
endpoint that coincides with `lowerarm.l` instead of reconstructing a misleading local-`+Y`
segment.
The four remaining legacy presets were removed because their global Euler/length-only records do
not preserve canonical explicit tails. Additional presets should be created with **Extract
Armature...** and consumed with **Import Armature...**, so the ComboBox never offers data known to
be structurally incomplete.

Leaving **Runtime Skeletal Preview** automatically stops its base/layer playback and restores the
shared preview to bind pose. Consequently, entering Armature Template can show the useful static
bind skeleton without carrying an animated pose or advancing playback in the background.
The runtime and Runtime Skeletal Preview expose transient two-clip Absolute and Additive composition.
Its product boundaries and relationship to the canonical runtime are defined in
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md).

Paint Weights is the primary day-to-day weight-authoring surface. The former Skin Weight Lab
worktree was retired after its useful nonredundant operations migrated under **Weight Tools** and
**Repair / Diagnostics** and passed brush, pose-safety, Undo, runtime-preview, and persistence
acceptance.
Canonical type-42 validation already guarantees normalized one-to-four-influence coverage, so a
Paint Weights normalize/limit/invalid-coverage panel would report no actionable state. Regional
AABB, subset, and topology-ring masks, complete-mask smoothing and Rigid Bind, connected-surface
painting, and direct rigid brush binding now provide the visual regional workflow without a separate
laboratory.

**Influence Distribution** is now available as an optional Repair / Diagnostics view. It maps a
normalized concentration score derived from each vertex's largest weight (`0.25 -> 0`, `1 -> 1`)
through the continuous heatmap: cooler colors mean influence is
more distributed, while warmer colors mean one bone dominates. The panel also reports dominant
minimum/average/maximum and vertex counts with one through four active influences. It can expose
unexpectedly rigid regions, unnecessary weak influences, and overly mixed areas, but it does not
judge deformation quality or replace pose-stress testing. Disabling it restores the unchanged
selected-bone heatmap.

**Weak Influence Contamination** is a second optional, mutually exclusive diagnostic. For each
vertex it sums positive influences strictly below the current Clean threshold and maps that sum
relative to the threshold through the heatmap. The panel reports affected vertices, weak influence
count, total weak weight, and maximum weak weight. Changing the threshold refreshes this view. It is
read-only; cleanup still requires an explicit **Clean Weak Influences** action using the same
threshold.

**Weight Health Summary** runs the principal weight checks over the complete mesh with one explicit
read-only action. It reports canonical contract validity, unweighted coverage, one-to-four active
influence counts, dominant-weight range, weak contamination at the current cleanup threshold,
abrupt triangle-edge transitions at the current classification threshold, and divergent connected
coincident seams. The summary does not assign a quality score: influence concentration is an
artistic choice, and abrupt transitions require inspection in representative poses. Mechanical
problems direct the user to the corresponding detailed diagnostic and explicit repair below.
The summary offers direct Undoable actions only for findings with deterministic mechanical repairs:
weak influences can be removed over the complete mesh at the reported threshold, and divergent
coincident-seam weights can be synchronized after explicit confirmation. It reruns the summary after
either action. It does not automatically reduce influence count, smooth abrupt transitions, create
missing weights, or repair an invalid skeleton because those operations require artistic intent or
a broader authoring decision.
All colored and disabled guidance in the summary uses the same width-aware wrapping path as ordinary
panel text, so resizing the editor panel reflows green, yellow, red, and gray messages consistently.

The ordinary Paint Weights surface keeps Weight Health, influence visualizations, contextual weight
tools, and brush authoring visible. The session-only **Options > Show
Advanced Diagnostics** toggle reveals the lower-frequency forensic tools without deleting their
state or changing the asset. Advanced controls are grouped by intent:

- viewport information and nearest-vertex pinning, followed by pinned geometry, bind topology,
  coincident-copy weights, and normals;
- whole-mesh coincident weight seams and their confirmed synchronization;
- whole-mesh incompatible-normal analysis/repair and coincident-normal smoothing;
- exact stored positions inside established source seam groups.

When enabled, a wrapped yellow notice marks the exact beginning of this advanced region in the
Paint Weights panel.
Disabling the option also hides advanced viewport overlays. It does not undo a repair already
committed; every mutating advanced action remains explicit, confirmed where applicable, and
Undoable.

Long disabled guidance around Bone Editor extension/selection and Paint Weights cached-geometry
feedback uses the same width-aware disabled wrapper as Weight Health. Runtime Preview lighting keeps
its panel text wrapped and uses explicit multiline tooltip formatting because tooltip windows have
no reliable automatic wrap width in the current ImGui binding.

The Paint Weights panel is contextual rather than showing every control at once. **Show Mesh** is
the first viewport control, immediately followed by **Show Skeleton**. Repair / Diagnostics follows because its radio selection
defines the rest of the panel in the same frame. Target Bone, Viewport Feedback, and History remain
contextual or visible as applicable. **Brush Operations** appears
only for Selected Bone Heatmap. Influence Distribution and Weak Influence Contamination are
read-only: the cursor is hidden and right-drag cannot start a stroke. Weak Contamination alone shows
its shared threshold and the contextual **Weight Tools / Clean Weak Influences** action below the
diagnostic statistics. Visualization is one explicit five-value radio group: Selected Bone Heatmap,
Influence Distribution, Weak Influence Contamination, Abrupt Weight Transitions, or Show Mask on
Original Mesh. The last mode restores the ordinary textured preview beneath the persistent orange
mask markers and builds no heatmap geometry, providing a read-only topology/material inspection view.

Target Bone and viewport bone picking are also contextual: they are exposed only in Selected Bone
Heatmap, where a particular bone actually drives visualization and brush edits. The three
whole-weight diagnostics hide Target Bone and leave left-drag exclusively to camera orbit; their
Skeleton section retains only the independent visibility control.
Hidden Paint Weights skeletons are also excluded from viewport hit testing. The current target bone
remains selected and can still be changed explicitly through the panel, but invisible joints and
segments cannot intercept a left click intended for orbiting or inspecting the mesh.
Likewise, a hidden mesh is excluded at the Paint Weights surface-picking boundary. Right-drag cannot
start or extend an invisible stroke, and hidden geometry does not feed the brush cursor, affected-
vertex preview, or nearest-vertex inspector. Re-enabling Show Mesh restores picking without changing
the selected bone or brush configuration.

**Abrupt Weight Transitions** is read-only and compares complete normalized weight vectors across
triangle-adjacent vertices using half their L1 distance, producing a bounded `0..1` value. Each
vertex displays its largest incident-edge difference. A configurable threshold classifies and
reports abrupt edges and unique affected vertices; the heatmap itself remains raw and unchanged
when only the threshold moves, so slider interaction updates cached statistics without rebuilding
geometry or rereading weights. The GUI therefore labels it as a classification threshold for
statistics only; it intentionally does not recolor the raw heatmap. Repair is a separate explicit
action described below; moving the classification threshold alone never mutates weights.
Distribution, Weak Influence Contamination, and Abrupt Weight Transitions are global or
mask-scoped diagnostics rather than target-bone views, so Paint Weights removes the cyan target
highlight while any of those modes is active. Returning to Selected Bone Heatmap restores it.

Abrupt Weight Transitions now exposes the contextual **Smooth Detected Transitions** repair. Only
vertices belonging to threshold-classified edges are editable. Configurable strength and 1-10
iterations blend their complete weight vectors toward triangle-neighbor averages; external
neighbors remain fixed boundaries during stable Jacobi passes. A separate maximum-change control
limits each final vertex by bounded half-L1 weight-vector distance. Candidate bone names are frozen
to the vertex's original one-ring neighborhood, preventing iterative propagation across multiple
topology rings in one repair. Every result deterministically normalizes and limits influences. The
final candidate commits through one canonical batch and one Undo entry, then the diagnostic rebuild
reports abrupt-edge counts and the maximum applied change.

When the selected runtime clip is available, repair also evaluates its start, quarter, midpoint,
three-quarter, and end poses through the canonical LBS palette. Every triangle incident to an edited
vertex is compared with its pre-repair posed area and orientation. If the full candidate would reduce
area below 25 percent or rotate the face normal by roughly 87 degrees or more, a deterministic binary
search reduces the complete batch until all sampled faces remain safe. The result reports the applied
pose-safety scale and how many unsafe face samples the unrestricted candidate would have produced.
The returned unsafe-triangle collection is retained for the safety overlay; an empty collection is
a valid result and does not interrupt the editor frame after a successful repair.

The repair also closes indexed topology seams without welding render data. Vertices are considered
seam copies only when their bind positions match within a mesh-extent-relative tolerance and their
separate triangle neighborhoods contain a matching geometric neighbor. This excludes merely
overlapping surfaces. Copies whose original weight vectors already agree receive one shared repair
candidate and are written atomically with identical weights. Pre-existing weight conflicts are left
unchanged and reported rather than silently forcing an ambiguous merge.

After a successful repair, **Show Last Repair Safety Overlay** visualizes the cached result without
reevaluating poses per frame. Translucent red filled faces identify unique triangles that failed at least one pose
sample for the unrestricted candidate and therefore contributed to safety scaling. Cyan crosses
identify coincident seam vertices written through a synchronized group. The report distinguishes
unique protected faces from total failed face/pose samples and states the seam vertex/group counts
on separate GUI lines.
The overlay is diagnostic only and is replaced by the next successful repair or cleared with the
loaded editor state. In Paint Weights, the shared **Show Mesh** control is the first viewport option,
immediately before **Show Skeleton**. Hiding the mesh does not hide the safety overlay, allowing the
protected faces and seam markers to be inspected in isolation.

Paint Weights now includes its first authoring slice. The user can select a target bone from the
panel or by clicking its joint/segment, inspect that bone's smoothly interpolated stored-weight
heatmap, hide or show the skeleton independently, adjust radius, strength, and linear/smooth
falloff, choose Paint/Add, Erase/Subtract, Smooth, or Rigid Bind through visible radio buttons, and drag the right mouse button over the mesh. Frame-zero vertices and
triangles are cached per loaded/restored mesh; separate local-space triangle and vertex BVHs narrow
surface ray intersection and radius queries. The heatmap rebuilds only when its target or canonical weights become
dirty. The heatmap stores each vertex's selected-bone weight in `UV.x`; the rasterizer interpolates
that value across each triangle and a dedicated pixel shader maps it through the continuous
blue-cyan-green-yellow-orange-red gradient. It retains normal depth testing, so hidden back faces
are not painted through the mesh. While this complete heatmap surface
exists, Paint Weights hides the ordinary textured preview instead of layering both copies; leaving
the worktree restores the shared preview normally. This visualization therefore exposes actual
stored interpolation rather than six clamped face-average buckets.

A Paint/Add stroke accumulates per-vertex alpha locally and normally samples between consecutive
surface hits at one quarter of the brush radius; an explicit per-event cap bounds pathological
cursor jumps. For each affected vertex, the selected bone
is blended toward weight one; other stored influences are reduced proportionally. Results are
sorted deterministically, limited to four influences, and normalized. The editor sends all changed
vertices through one atomic canonical type-42 batch only when the mouse is released. A successful
stroke creates one Undo entry and refreshes the heatmap; an empty, cancelled, or rejected stroke
does not mutate weights. `Esc` cancels the active stroke.

**Connected Surface Only** is enabled by default for Paint/Add, Erase/Subtract, and Smooth. Instead
of selecting every vertex inside a Euclidean sphere, each stamp runs a radius-bounded shortest-path
search over triangle edges seeded by the hit face. The brush therefore follows the mesh surface and
does not jump through empty space to the opposite side of a thin body part or another nearby limb.
Compatible coincident UV/material seam copies join the traversal through the same validated seam
groups used by repair. Disabling the option restores the original spatial-sphere behavior.

**Restrict to Hit Subset** is optional and applies to every brush operation, including Rigid Bind.
The first surface hit locks the material subset for the complete stroke. Vertices from other
subsets are excluded from both painting and **Show Affected Vertices**; leaving the locked subset
also breaks stroke interpolation so re-entering it cannot create a hidden bridge. A new stroke may
lock a different subset. This is useful for editing clothing near a body surface without changing
the body, while leaving the option disabled preserves cross-subset painting.

The session-only **Painted Mask** provides a persistent regional selection without changing stored
weights. **Add to Mask** and **Remove from Mask** use right-drag with the current radius, falloff,
connected-surface, and optional hit-subset scope; orange crosses show up to 500 sampled masked
vertices. **Limit Weight Brushes to Mask** constrains Paint/Add, Erase/Subtract, Smooth, and Rigid
Bind, and the exact affected-vertex diagnostic uses the same mask filter. **Clear Mask** removes the
selection. The mask is editor-session state, is reset when another mesh is loaded, and is never
written to the mesh file or added to Undo history because editing it does not mutate asset data.
Its persistent orange marker batch uses `alwaysRender` to bypass frustum rejection and
`alwaysOnTop` for overlay ordering, while ordinary workspace and mesh visibility still control it.

**Smooth Complete Weights in Mask** is the regional counterpart to the selected-bone Smooth brush.
For every masked vertex it averages the complete influence vector using only masked triangle and
compatible coincident-seam neighbors, then blends by the configured strength for the requested
iterations. Each iteration normalizes deterministically and retains at most four bones. Mask
boundaries therefore do not import weights from unmasked vertices. Before one atomic canonical
batch and Undo entry, the candidate passes the same sampled-pose face safety scaling used by
repairs; the status reports changed vertices, iterations, applied safety scale, and avoided unsafe
samples.

**Rigid Bind Complete Mask** applies the selected target bone regionally without repeated brush
strokes. With zero transition rings every masked vertex targets exclusive weight one. With one or
more rings, a topology breadth-first distance from the masked/unmasked boundary produces an
internal blend: boundary vertices receive `1 / (rings + 1)` of the rigid target, successive masked
rings increase linearly, and the deeper interior becomes rigid. Triangle and compatible coincident
seam adjacency share the distance field; unmasked vertices are never edited. Sampled-pose safety
may uniformly reduce the complete candidate before its single canonical batch and Undo entry.
After safety scaling, vertices whose final vector is numerically unchanged are excluded from the
batch. If the safe scale reaches zero, the operation reports that pose safety blocked it and creates
neither an asset mutation nor an Undo entry. Increasing transition rings is not guaranteed to raise
the safe scale: it changes more vertices and faces, and the shared global scale is limited by the
most sensitive sampled face in the complete region.

**Limit Diagnostics to Painted Mask** gives Influence Distribution, Weak Influence Contamination,
and Abrupt Weight Transitions a shared regional scope. Only masked vertices contribute to counts,
minimum/average/maximum values, weak mass, or abrupt edges; an abrupt edge requires both endpoints
inside the mask. Unmasked heatmap vertices use neutral gray rather than the diagnostic's zero-value
blue. The associated Weak Cleanup and Abrupt Repair actions consume the same scoped candidates, so
enabling the option cannot diagnose locally and then silently repair the complete mesh.

The first mask generator migrates material-subset selection into the visual workflow. After a
visible surface hit, **Replace Mask with Hit Subset**, **Add Hit Subset to Mask**, and **Remove Hit
Subset from Mask** operate on every frame-1 vertex belonging to that material subset. The editor
retains the last valid surface hit while the pointer moves into the GUI, so the buttons remain
usable without requiring an impossible simultaneous hover over mesh and panel. These commands
change only session mask membership and never mutate weights or create Undo history.

The painted mask can also **Grow** or **Shrink** by 1-10 topology rings on explicit button clicks.
Grow adds triangle-neighbor rings; Shrink removes selected vertices adjacent to an unselected
triangle neighbor, repeating from the updated boundary for each requested ring. **Cross Compatible
Seams** is enabled by default and treats the editor's already validated connected coincident copies
as one logical neighborhood, so selection does not stop merely at a UV or material split. Disabling
it follows stored triangle indices only. The operation uses cached adjacency, rebuilds mask markers
once after a real change, runs no analysis in idle frames, changes no weights, and creates no Undo
history because the mask remains transient session state.

**Start AABB Capture** follows Mesh Debug's Split capture boundary. Turning it on initializes a
quarter-size orange box at the mesh center, temporarily shows the original textured mesh even when
base-mesh visibility was off, and hides the heatmap, mask crosses, skeleton, brush cursor, and brush
feedback. Min X/Y/Z, Max X/Y/Z, and Size X/Y/Z rebuild only the visual box; Size preserves the
axis center, while Min/Max move one face. Left-dragging inside the volume translates it in the
camera plane, while dragging outside orbits the camera. The box uses `alwaysRender` to avoid
frustum loss but deliberately does not use `alwaysOnTop`, preserving front/behind depth cues
against the textured mesh. Painting and bone picking are blocked. No vertex query runs per frame
or per control edit.
**AABB sensitivity** controls every numeric drag step, defaults from the full mesh extent, and has
the same automatic-reset behavior as the retired Weight Lab. Hover feedback is prebuilt with the box rather
than allocated in the loop: X uses magenta, Y cyan, and Z lime; Min/Max highlights the corresponding
single face plus parallel axis edges, while Size highlights both opposing faces and those edges.
These transient feedback overlays use always-on-top only for legibility; the orange capture box
itself remains depth-tested.
Each hover-face triangle is emitted with both winding orders, making the overlay visible from
either camera side under GLES, DirectX 9, and Metal without changing shared culling state.
The capture box and hover overlays store centered local geometry plus a separate world position.
Viewport translation updates only their existing `setPos` values; it does not destroy or recreate
render objects on each mouse-move frame. Hover changes only visibility. Geometry is rebuilt solely
when Min, Max, or Size actually changes the volume dimensions.
Turning capture off performs one point-inside-box pass over the cached frame-1 vertices, destroys
the box, restores the previous editor visualization, retains the captured vertex set, and reports
its count plus the required apply step. Separate Replace/Add/Remove actions then apply that result
to the session mask without touching weights or Undo history.

Implementation trap: Lua's `condition and value_if_true or value_if_false` idiom cannot represent
`nil` as `value_if_true`. An expression such as `mode == "remove" and nil or true` always evaluates
to `true`, so a nominal removal silently adds the vertex back to the mask. Mask membership removal
must use an explicit branch that assigns `nil`; this rule applies to AABB, subset, brush, and future
mask generators.

**Show Brush Influence** displays a translucent brush-like disk oriented by the hit face. Its radial
alpha previews `strength * falloff` independently of the mesh triangulation: green represents
Paint/Add, red represents Erase/Subtract, and cyan represents Smooth. The disk communicates radius
and falloff, not the exact topology-clipped result. **Show Affected Vertices (Diagnostic)** is the
default-off exact candidate view; it runs the same spatial or connected-surface query as painting and
draws at most 500 white filled crosses in the brush plane. The crosses are independent batched
quads, not one continuous line strip, so no diagonal segments join their arms or neighboring
vertices. Both previews update only through the changed-pointer 30 Hz cursor
refresh and perform no idle query. The disk and radius circle use always-on-top priority 0, so the
whole flat brush remains visible over curved surfaces. Skeleton joints and segments use priority 1
and therefore remain visible above the brush. The disk winding follows the camera-facing hit normal
even when the surface is viewed from below.

**Inspect Nearest Vertex** is a read-only local inspector in Viewport Feedback. From the triangle
already returned by the throttled surface raycast, it deterministically chooses the face vertex
nearest to the brush hit, marks it with a yellow two-stroke cross oriented in the brush plane, and
lists its exact canonical global index,
subset, and up to four bone weights in descending order. It performs no global spatial query and
does not update while the pointer is stationary. The panel permanently reserves its header and four
influence rows, filling absent data with placeholders so entering the scrollbar does not collapse
the window content and remove the scrollbar itself. This complements the interpolated heatmap when an
artist needs to explain one precise vertex without leaving the visual Paint Weights workflow.
It remains available in Selected Bone Heatmap, Influence Distribution, Weak Influence
Contamination, and Abrupt Weight Transitions. Diagnostic modes keep the nearest-vertex marker and
complete influence list without enabling the brush cursor or weight mutation. In a global
diagnostic, a stationary left click pins the current vertex, yellow marker, and complete influence
list while subsequent pointer movement resumes ordinary surface probing without replacing the
pinned result; left-drag remains camera orbit. The explicit clear action returns to live inspection.
Pinning also resolves the editor's existing connected coincident-seam group for that global index.
The read-only report lists every copy by global index and subset with its normalized influences,
shows the maximum pairwise half-L1 weight divergence from 0 (identical) to 1 (disjoint), and marks
multi-vertex groups in cyan. A singleton explicitly reports that no connected coincident copy was
found. This makes seam-weight disagreement reproducible before any repair is authorized.
For a multi-copy group with nonzero divergence, **Synchronize This Seam's Weights** is an explicit,
confirmed local repair. It averages each bone weight across only the listed copies, deterministically
keeps the four strongest combined influences, normalizes once, and atomically writes that identical
result to every copy. The operation creates one Undo entry and immediately recomputes the pinned
report; success therefore reads as zero divergence without requiring a new pick.

**Analyze All Coincident Seams** extends the same definition to the complete mesh on demand, rather
than adding work to the editor loop. Its impact preview reports divergent groups versus all connected
coincident groups, unique affected vertices, maximum divergence, and positional tolerance. The
separately confirmed global synchronization applies the same per-group average independently to every
divergent group in one canonical batch and one Undo entry. It does not reduce the mesh to two
influences. Any later committed edit invalidates the audit and its confirmation.

Successful Paint Weights mutations and diagnostic-mask applications render their latest status in
yellow, while failures remain red and informational/no-change messages remain neutral. This keeps a
new repair result visually distinct from the surrounding white diagnostic text without changing the
operation's transactional semantics.

After each successful Paint/Add, Erase/Subtract, or Smooth stroke, Paint Weights performs a
non-mutating pose-safety diagnostic over only the triangles incident to changed vertices. It
compares the pre-stroke and candidate weights at the start, quarter, midpoint, three-quarter, and
end of the selected clip using the canonical LBS palette. The report shows changed vertices,
checked faces and poses, unsafe unique faces and face/pose samples, minimum posed-area ratio, and
maximum absolute orientation change, and minimum alignment between the new geometric normal and the
bind-face normal transformed by the same LBS palette and candidate weights. A face is unsafe when
its area falls below 25 percent of its pre-stroke value or when the stroke introduces an actual
normal-alignment sign inversion. Large absolute rotation remains informative but is not itself a
failure: a face may legitimately follow a different bone after painting. Pre-existing negative
alignment is reported by the metric but is not attributed to the stroke.
**Show Last Stroke Safety Overlay** displays unsafe faces in translucent red. This first slice is
diagnostic only: it never rejects, scales, or rewrites the committed stroke. If no usable clip is
selected, painting remains available and no stale diagnostic is retained.

The shared atomic type-42 batch boundary used by every Paint/Add, Erase/Subtract, Smooth, Clean,
and repair commit has executable save/reload acceptance. A deterministic fixture edits separate
four-influence and two-influence vertices, writes the mesh, reloads it, and verifies every occupied
and empty slot's bone name, order, and weight within the canonical tolerance.
Left-drag retains the editor-wide camera-orbit behavior; clicking a visible skeleton joint or
segment with the left button still selects its bone before an orbit begins.

Erase/Subtract uses the same sampling, transaction, and Undo boundary. It reduces only an existing
weight for the selected bone, then normalizes the remaining influences. Blue/zero-weight vertices
remain unchanged. If the selected bone is a vertex's sole influence, subtraction also leaves that
vertex unchanged because canonical type-42 data requires one to four positive influences summing
to one; the brush never fabricates a replacement bone.

Smooth uses triangle topology rather than spatial proximity to calculate the selected bone's
neighbor average for indexed meshes and ordinary non-indexed triangle lists. Strength and falloff interpolate the current selected-bone weight toward that
average; remaining influences are redistributed proportionally and the final record is normalized
and limited to four. The complete stroke still commits through one atomic batch and one Undo entry.
This is selected-bone weight smoothing, not geometry smoothing or an indiscriminate blur of every
bone channel. A configurable 1-10 iteration count repeats stable topology passes inside the painted
set, making the effect useful on dense meshes where one immediate-neighbor average is naturally
subtle. The default is three iterations.

Rigid Bind paints an exact selected-bone assignment inside a configurable `0..0.95` fraction of
the brush radius. Core vertices become `{selected bone: 1.0}`. The outer band blends from that
assignment back to the original normalized weights using Linear or Smooth falloff; brush strength
is intentionally hidden because it would make the core only approximately rigid. Repeated or
overlapping stamps retain the maximum spatial falloff rather than accumulating toward rigidity, so
sampling density does not shrink the transition band. The operation shares connected-surface
filtering, seam traversal, atomic commit, Undo, persistence, and last-stroke pose diagnostics with
the other brushes. Its preview color is yellow.

The first migrated **Weight Tools** operation is **Clean Weak Influences**. A configurable threshold
is applied to the complete mesh. Influences below it are removed, except that every vertex's
strongest influence is always preserved; the survivors are normalized and committed as one atomic
batch with one Undo entry. If no influence qualifies, the operation creates neither a snapshot nor
a mutation. Canonical weights are already normalized and limited to four influences, so a separate
Normalize All operation remains redundant.

Influence Distribution also exposes **Limit Maximum Influences** for an explicit `1..4` target.
Its impact preview counts vertices currently above the selected limit. After confirmation, the
operation keeps each affected vertex's strongest influences, removes the remainder, renormalizes,
and commits the complete mesh through one atomic batch and one Undo entry. A limit of four is a
no-op for valid canonical data. This tool does not claim that fewer influences are universally
better; the artist chooses the limit after inspecting the distribution and deformation.

The overlay reuses one vertex and UV per canonical frame-zero vertex through indexed geometry when
the complete mesh fits the shape API's 16-bit index limit. Larger meshes use an explicit
non-indexed fallback. Because the ordinary preview is hidden, no normal-offset duplicate is needed.
Surface picking remains event-driven through the cached BVH; cursor raycasts and cursor-object
rebuilds require a changed pointer position and are capped at 30 updates per second.

Paint Weights gives each visible segment the same mesh-independent identity used by Bone Editor:
a bone with an explicit canonical tail owns its own transformed `head -> tail` segment. Clicking that
segment therefore selects that bone, rather than the child whose head happens to end the segment.
The rule depends only on canonical skeleton data, never importer names or anatomical conventions.
Bones without an explicit tail remain selectable at their head joint and do not receive a guessed
segment.

## 2. Opening the editor

Choose **Skeletal Animation Editor** from the Mini MBM launcher, or start it directly:

```sh
./bin/debug/linux_x86/mini-mbm --scene editor/skeletal_animation_editor.lua
```

Use **File > Open Mesh** to load a `.msh` file. The mesh should contain a frame-1 skeleton and
canonical vertex skin weights for all bone-dependent workflows. Meshes without bones or weights may
still be inspected through AABB and material-subset selection.

The main menu places **Tutorial** between **File** and **Edit**. **Tutorial 1** opens a persistent
standalone guide that remains visible while the user changes
worktrees. Its data lives in `editor/skeletal_animation_tutorials.lua`: each registered tutorial has
a stable ID, localized menu/window keys, and an ordered list of localized steps with an optional
target worktree and focus anchor. Clicking a step selects its instructions; **Open the required
worktree** switches the editor through the ordinary workspace boundary, repeats the guidance in
status feedback, and on the following frame scrolls to the declared section. Collapsible anchored
sections open automatically.
Steps that require an asset disable navigation until a mesh is loaded. File-only steps remain
instructions rather than opening native dialogs implicitly. The module exposes `register()` so
future tutorials reuse the same menu, window, navigation, Previous/Next controls, and lifecycle
without adding tutorial-specific rendering branches to the main editor. A step may also declare an
optional `checkKey`; the window renders this after Previous/Next as a distinct expected-result and
diagnostic checklist rather than repeating the primary instruction.

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
The same read-only selection is available directly in the viewport: a left click on a bind joint or
parent-to-child segment selects the corresponding bone and synchronizes the hierarchy/details panel.
Clicking and dragging empty viewport space continues to orbit the camera; Bind Pose Contract does
not turn this selection path into direct manipulation.
Empty or duplicate names are rejected transactionally; weights and animation tracks continue
targeting the unchanged stable bone ID. Rename and reparent stage a whole-asset snapshot before
committing; failed mutations discard it without changing history. Undo/Redo restores skeleton,
weights, clips, preview, hierarchy, selection, and gizmos together.
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
successful edit participates in the shared bounded Undo/Redo history.

The separate Bone Editor provides direct joint/segment manipulation, constrained movement,
segment rotation, connection editing, radius editing, snapping, cancellation, and one transaction
per completed gesture. These numeric Bind Pose Contract fields remain available for exact values and
feed the same canonical skeleton state rather than maintaining a second representation.

Its **Weights and complete skeleton** group also owns explicit asset-level maintenance. With no
type-42 data, an explicit target-bone selector can rigidly initialize every frame-zero vertex and then
redistribute influences in Paint Weights. **Remove all weights** deletes only type 42, preserving
bones and clips. **Remove all bones and weights** reports bone, weighted-vertex, and clip counts,
then atomically deletes sections 41-43 because clips cannot remain valid without their skeleton.
Both destructive actions require separate confirmation and create one Undo entry.

**Generate automatic bone weights** is the non-rigid bootstrap. It scores every frame-zero vertex
against every explicit head-to-tail segment; a joint-only bone behaves as a zero-length segment.
Bone radius makes distance scale-aware, the four strongest candidates are normalized, and zero to
twelve configurable iterations diffuse 40 percent of each update through stored triangle adjacency
while retaining 60 percent of the current value. Each iteration prunes and renormalizes back to four
influences. The operation is deterministic, name/anatomy/importer independent, available only when
type 42 is absent, and commits one Undo entry. If initialization or the final canonical batch fails,
the staged asset snapshot is restored. This is an envelope-distance plus topology solver inspired by
automatic-weight workflows; it is not claimed to reproduce Blender's internal bone-heat solver.
Because duplicated seam copies have independent triangle neighborhoods, topology diffusion can make
weights that began equal diverge. After the final diffusion iteration and before the canonical batch,
the generator therefore resolves the established connected coincident-seam groups, averages each
group independently, retains and normalizes its strongest four combined influences, and assigns the
same result to every copy. The success status reports how many seam groups were synchronized.

For a pinned Paint Weights vertex, **Analyze Deformed Geometry Here** evaluates its incident
triangles with the canonical LBS palette for the selected authoring clip at the current playhead.
The read-only report exposes minimum deformed-to-bind area ratio, faces below the 25 percent collapse
limit, minimum alignment against the bind normal transformed by the same vertex weights, inverted
faces, and minimum/maximum edge-length ratios. Faces below 50 percent area or 0.25 normal alignment
are marked orange in bind space. Using a transformed reference normal distinguishes actual inversion
from an ordinary rigid rotation of the animated part.
The diagnostic owns an explicit time slider clamped to the selected authoring clip duration. Changing
the time invalidates the previous report and overlay; pressing Analyze evaluates exactly the displayed
time, independently of the Runtime Preview playback position or another worktree's playhead.

**Analyze Bind Topology Here** is a second read-only pinned diagnostic. It virtually welds all vertex
positions within the same mesh-scale tolerance used by seam analysis, builds undirected triangle-edge
incidence on those virtual roots, and classifies edges referenced by exactly one triangle as open.
It reports the complete open-edge count, distance from the pin to the nearest open edge, and the best
endpoint-matched gap to a non-adjacent opposing open edge. The candidate pair is marked red. Virtual
welding prevents ordinary UV/material duplicates from being mislabeled as holes and never changes the
stored geometry.

**Analyze Coincident-Copy Normals** compares every usable stored bind normal in the pinned seam group
and then applies the Compact LBS normal path at the explicit diagnostic time: the weighted palette
3x3 transform followed by normalization. The report gives the maximum pairwise angle in bind and
after deformation, plus normal coverage for the group. Angles above 5 degrees are presented as a
visible discontinuity candidate. This is read-only and does not toggle the engine-global lighting
state or force a mesh shader-variant reload.
Each valid copy can be expanded to inspect bind and deformed XYZ, both angles relative to the pinned
copy, and an area-weighted incident-face average with its angle to the stored bind normal. Incident
face cross products are sign-oriented toward that copy's stored normal before averaging, so the
engine's CW/CCW convention cannot create a false 180-degree mismatch. These details identify which
copies form one smooth group and which represent an intentional hard geometric boundary.
The report can be printed to the terminal as one machine-readable summary line followed by one line
per copy containing index, selection, bind/deformed vectors and angles, incident-face average, and
stored-to-geometric difference. Printing is read-only and avoids manually transcribing wide panels.
The confirmed local **Recompute Incompatible Normals Here** repair uses a configurable stored-to-
incident-face angle limit (30 degrees by default), previews affected copies, and replaces only those
stored normals with their normalized incident-face averages. Position, UV, weights, and compatible
copy normals remain unchanged. The edits use the original frame/subset/local-vertex identity, create
one Undo entry, rebuild the cached vertex data, and immediately rerun the pinned normal report.
The same test is available mesh-wide through **Analyze All Incompatible Normals** without requiring a
pin. It reports affected versus usable normals, unavailable normals, maximum difference, and the
active limit, with orange markers for affected vertices. Separately confirmed **Recompute All
Incompatible Normals** applies only that cached impact set, in one Undoable operation, then rebuilds
the geometry cache and reruns the audit. Changing the limit or committing another edit invalidates
both the audit and its confirmation. This remains distinct from aesthetic smoothing below the limit.

**Coincident Normal Smoothing** is that separate aesthetic operation. For each connected coincident
seam, usable stored normals are partitioned into complete-link groups: a candidate joins only when
its angle to every current member is within the configurable limit (30 degrees by default). This
prevents a transitive 0/20/40-degree chain from merging endpoints beyond the requested limit. Groups
with at least two members receive one normalized average; divisions above the limit remain hard.
On-demand analysis reports changed vertices/groups/seams and marks impact in cyan. Explicit apply
preserves positions, UVs, and weights, commits one Undo entry, and reruns the analysis.

**Exact Coincident Positions** is the final read-only source-geometry check for those established
seam groups. It compares the stored XYZ coordinates directly, without tolerance rounding, reports
how many groups have any strict nonzero spread, the affected vertex count, minimum and maximum
nonzero spread, the number of copies and pair comparisons actually evaluated, and the spread of the
pinned group when available. Affected copies are marked red.
It does not weld vertices or otherwise modify geometry; its purpose is to distinguish a real
micro-gap in the imported mesh from weight, deformation, topology, or normal artifacts.
If a visible fissure is already present in the static source mesh before a skeleton, weights, or
animation exists, skeletal deformation is excluded as its cause. The skeletal investigation may be
resumed for another asset only when that asset is intact before rigging and develops a fissure after
weighting or deformation.

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
is transactional, participates in the whole-asset rollback, and opens **Paint Weights**
immediately so influences can be redistributed with its visual authoring and regional tools. Existing
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
seek by time, change the shared `0.05x..4x` playback speed, or explicitly return the mesh to bind
pose. Speed advances the base clip, Absolute layer, and active fade from the same scaled delta.
Choose Auto, LBS, or rigid DQS in the same panel;
changing it rebuilds the preview so the method is selected before mesh loading and shader creation.
Auto selects DQS only if bind and all clips use unit scale; otherwise it selects LBS and shows the
reason. The panel reports requested/resolved methods and explains the limits directly: how many bones this mesh requires and the
maximum accepted by the current device for one mesh draw. Multiple mesh instances are evaluated
separately; the capacity is not a combined scene-wide bone budget. The Execution Path selector
chooses Auto, GPU, or CPU before loading the preview instance. Auto is the default, prefers GPU,
and falls back to CPU only when the resolved LBS or rigid
DQS mesh cannot use GPU and CPU is ready; it never changes the selected LBS/DQS method.
Explicit GPU and CPU selections remain mandatory comparison paths with no fallback. CPU supports
resolved LBS or rigid DQS when the loaded report says `cpu-lbs-ready` or `cpu-dqs-ready`; invalid
non-rigid DQS content reports an unavailable reason rather than changing method or claiming a CPU
fallback.
Bind restoration stops
the active player; it does not assume that time zero of an authored clip is the bind pose.
The slider is a lightweight playback scrubber, not the future Animation-node
timeline: it does not expose tracks or edit keys. The mesh deformation uses the runtime player and
matching active-backend LBS or DQS palette. When a clip layer is active, the optional mask skeleton
follows the primary preview's final evaluated global transforms while retaining per-bone mask colors.
In LBS/DQS and GPU/CPU comparison it intentionally does not duplicate the secondary instance.
Runtime Preview can also load multiple optional secondary **wearable / follower** `.msh` meshes.
The editor loads each follower with the primary preview's resolved LBS or DQS method and the same
GPU or CPU execution path, runs `getSkeletalSharingCompatibility` against the primary runtime mesh,
displays the compatibility reason and relevant mismatch fields, and only then calls
`enableSkeletalPoseSharing(primary)`.
When compatible, each follower keeps its own mesh, material, textures, and skin weights while
rendering from the primary player's already evaluated pose. Followers mirror the primary preview's
editor transform, including comparison offsets, and each has its own visibility and remove action.
A remove-all action clears the whole transient collection. Rebuilding or reloading the primary
preview, resetting the editor mesh, leaving the scene, or destroying the editor safely unlinks and
destroys every follower. This composition is transient editor state and is not persisted; it is
separate from the optional LBS/DQS comparison mesh and does not replace or reuse that comparison
instance.
Resolution details, per-instance capacity guidance, and the evaluated-gizmo scope are
available as hover tooltips on their corresponding Runtime Preview report or control.
Runtime Preview also provides a movable editor-only light window following Mesh Debug's controls:
**Enable Preview Lighting**, ambient color, directional color, an orbit direction gizmo with numeric
XYZ feedback, and Reset. Disabled is an explicit unlit baseline. Enabling or disabling rebuilds the
runtime mesh under the matching load-time shader state; color and direction edits apply immediately.
Reset restores the inspection defaults and disables lighting. Other worktrees force 3D lighting off,
and none of these diagnostic values are serialized.

Open **Create / Edit Animations** to inspect the canonical type-43 structure before editing is
enabled. The node selects a named clip and displays its stable ID, duration, looping policy, tracks,
target bone identity, T/R/S channel mask, and every key's time, local quaternion TRS, easing, and
Cubic-Bezier controls. Selecting a track synchronizes the editor's selected bone index. Clips,
tracks, keys, poses, and timeline operations are editable through the transaction model described
below.

**Import Animation from MSH...**, located before the clip selector, opens a separate adjustment
window. It loads one animated source MSH, lets the user select a source clip and assign a unique
destination name, and imports only canonical type-43 animation data. The target geometry,
materials, skeleton identities, and vertex weights remain unchanged. Common bone-name prefixes are
detected generically from delimiters such as `:`, `|`, `_`, and `-`; the operation does not contain
RenderPeople- or Mixamo-specific prefixes. After prefix removal, every normalized bone name must be
unique and present on both sides, the bone counts must match, and mapped parent relationships must
be identical. Incompatible sources are rejected before target mutation.

For a compatible skeleton, keys are adapted offline from the source bind-local TRS to the target
bind-local TRS. Translation deltas use the corresponding bone-length ratio, root translation uses
the measured skeleton-height ratio, rotation preserves the source bind-relative quaternion delta,
and scale preserves its bind-relative ratio. Key times, channel masks, easing, Bezier controls,
duration, and looping policy are retained. Import creates one destination clip and pastes all keys
through the canonical batch API. The editor's whole-asset snapshot restores the original target if
either stage fails, and a successful import participates in Undo/Redo. After commit, the editor
rebuilds the runtime preview once from the updated in-memory canonical MSH, so the imported clip is
available in Runtime Skeletal Preview without requiring a persistent Save first. This is deliberately a
narrow offline same-topology retarget workflow for combining separately exported animations; it is
not runtime retargeting or arbitrary humanoid semantic mapping.

The same node can create an empty clip and update the selected clip's name, duration, and loop
policy. Clip IDs remain unchanged when properties are edited. A duration reduction that would
exclude an existing key is rejected rather than truncating or moving animation data. Clip removal
requires explicit confirmation because it removes all contained tracks and keys; removing the final
clip also removes canonical type-43 storage. These operations use the shared whole-asset history
boundary; track, key, timeline, and viewport authoring are documented below.

Track-container authoring is now available for the selected clip. The editor lists only bones that
do not already have a track there, accepts any nonempty T/R/S channel combination, and creates the
track with one key at time zero copied from the bone's local bind TRS. This seed prevents an invalid
empty-track intermediate and initially evaluates to bind pose. Existing track channel masks may be
changed transactionally; stored key values remain intact and are revalidated under the enabled
channels. Track removal requires confirmation because all of that track's keys are removed with it.
Key-value editing and the timeline are available as described below.

Keyframe authoring is available inside each expanded track. A new key time must be unique and lie
inside the clip; insertion samples the existing clip and captures that bone's evaluated local TRS,
so merely adding a key preserves the current curve. Expanded keys expose editable time,
translation, quaternion rotation, scale, easing, and Cubic-Bezier controls. Applying normalizes the
quaternion, reorders a moved key by time, and validates the complete type-43 collection. Removal
requires confirmation and the final key cannot be removed because canonical tracks may not be
empty. Every insertion, update, and removal uses whole-asset history. The graphical timeline and
pose-oriented viewport controls described below use the same canonical transactions.

The Animation worktree now also has the shared in-memory pose contract needed by those controls.
Its time scrubber evaluates the current unsaved clip directly from `meshDebug`, installs the packed
LBS/DQS palette on the preview instance without saving or reloading, and rebuilds the visible
skeleton from the same evaluated global transforms. Consequently the mesh and skeleton show the
same pose while editing. Mouse picking, translation/rotation/uniform-scale gizmos, explicit commit,
Auto Key, playback, and the graphical timeline build on this contract; numeric key fields remain
precise diagnostic/fallback controls rather than the intended primary UX.
Move, Rotate, Scale, and Auto Key keep their detailed guidance in hover tooltips so the Animation
panel remains readable at its standard width.
Bone selection is now viewport-driven as well as tree/track-driven. Clicking either an evaluated
joint or its parent-to-child segment performs a nearest-hit ray test and selects the child bone;
dragging empty viewport space continues to orbit the camera. This establishes the selection
semantics that translation and rotation gizmos will consume without yet mutating the pose.

The selected evaluated bone pose can be copied into a separate pose clipboard. **Copy selected bone
pose** captures the local translation, quaternion rotation, and scale currently visible at the
playhead, including an uncommitted temporary gizmo result. **Paste bone pose at playhead** resolves
the same stable bone ID in the current skeleton and commits T+R+S as one canonical key transaction.
It works after switching clips, creates or extends the required bone track through the existing
authoring-key contract, and produces one Undo entry. A missing bone rejects without mutation. This
clipboard is distinct from the timeline multi-key clipboard, so their controls and meanings do not
compete for Ctrl+C/Ctrl+V. The copy and paste actions occupy separate GUI lines for readability,
and each exposes a tooltip describing the one-bone scope, temporary-pose capture, T+R+S key commit,
indirect descendant motion, and Undo boundary.

A second, explicitly labeled clipboard handles the **complete skeleton pose**. Copy captures the
evaluated local T/R/S of every bone at the current playhead, including the current temporary gizmo
override. Paste requires exactly the same stable bone identities, then creates or updates T+R+S keys
for all bones at the destination playhead through one candidate validation, one commit, and one Undo
entry. No partial skeleton pose is retained if any identity or transform is invalid. The UI reports
the copied bone count, source clip, and source time, and tooltips distinguish this operation from the
single-bone and timeline-key clipboards.

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
Enable **Compare GPU / CPU** to reuse the same side-by-side preview and synchronization lifecycle
for execution-path parity: the left instance is GPU and the right instance is CPU using the same
resolved method selected before load. This mode is mutually exclusive with LBS/DQS pose stress,
disables the normal method/execution selectors while active, and reports each side from its loaded
`getSkeletalSkinningReport()` resolved execution path rather than the requested combo state. Disabling it
returns to the ordinary single-preview controls without writing method or execution changes into
the mesh asset.

The editor supports **Save**, **Save As**, and bounded 50-entry **Undo/Redo** across existing atomic
bind, bone, weight, clip, track, key, timeline, and pose-authoring operations. New commits clear Redo;
loading another mesh or quitting removes the editor-owned temporary snapshots. On Windows these
snapshots are anchored under `TEMP`/`TMP` because MinGW's `os.tmpname()` may return a root-relative
name that is not writable; other platforms retain their native temporary directory behavior.

Preview reconstruction follows the authoritative asset boundary. An unmodified load uses the
selected file directly. Once any canonical edit makes the session dirty, a preview request first
saves the current `meshDebug` state to an editor-owned temporary mesh, loads the runtime preview
from that snapshot, and immediately removes the temporary file. Saving is therefore a persistence
choice, not a prerequisite for Paint Weights, Runtime Preview, or animation authoring to observe
unsaved skeleton, weight, and clip changes.

## 3. Interface workflow

Only one worktree is open at a time. Opening another automatically closes the previous one and
updates the viewport. **Show Mesh** is shared. Skeleton visualization is contextual: Bind Pose
Contract displays the bind skeleton automatically, Paint Weights owns its local visibility option,
and Runtime Skeletal Preview optionally draws the primary player's final evaluated pose with mask
colors. Runtime LBS/DQS comparison geometry is shown only in that worktree; its secondary instance
does not receive a duplicate skeleton gizmo.

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

**Visual tail orientation** is an explicit editor-only normalization for rigs whose authored FBX
bone axes do not follow their hierarchy positions. A bone with one child points to that child's
head; leaves and safe multi-child branches continue their incoming direction while retaining their
current visual length. Every bone that owns a connected child is skipped so no joint or bind
transform moves.
The complete operation is transactional and Undoable. It changes only explicit `tailOffset` and
`length`: bind rotations, weights, animation tracks, and the bone axes reconstructed during
MSH-to-FBX export from global bind matrices remain unchanged. Consequently, saving and reopening
the edited MSH preserves these visual tails, but an MSH -> FBX -> MSH round-trip does not: FBX
export reconstructs the armature from the unchanged bind axes, and FBX import then captures those
Blender bone tails as the new explicit endpoints. Running **Visual tail orientation** again after
that round-trip is the expected workflow, not repair of damaged animation data.

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
Animation also retains each explicit authoring tail and its head-to-tail segment as a non-selectable
reference that follows the evaluated bone matrix. Real transform joints and parent-to-child links
remain the only animation-picking targets, but the visible armature no longer loses its Bone Editor
shape merely because the workspace changed.
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
**Duplicate at playhead** copies the complete selected group within its existing tracks, aligning the
earliest selected key to the current playhead and preserving relative timing and all key payloads.
The action is disabled for a zero delta or an out-of-range group; collisions reject atomically and
leave both original keys and selection source data unchanged.
**Copy selected keys** (`Ctrl+C`) stores detached complete payloads together with stable bone IDs,
track channel masks, and original times. **Paste at playhead** (`Ctrl+V`) aligns the earliest copied
key to the playhead and atomically resolves or creates destination tracks. The clipboard therefore
survives selection changes, source edits, and switching clips without retaining fragile indices.
An existing destination track must have the same T/R/S mask; missing tracks are created with the
copied mask. Unknown bones, incompatible masks, an out-of-range destination, or a collision reject
the complete paste without modifying the asset. Keyboard shortcuts do not intercept an active ImGui
control, and the timeline identifies the clipboard's source clip.
**Insert at playhead (ripple)** opens space equal to the selected group's time span across every
track, shifts all keys at or after the playhead by that span, and inserts a copy aligned by its
earliest key. The clip duration grows with the inserted space; endpoint keys receive only the
canonical numerical separation required to remain distinct. Invalid candidates are rejected atomically.
**Insert empty time at playhead** creates a gap of the requested duration without copying or creating
keys. Every key at or after the playhead moves by the same amount across all tracks, and the clip
duration grows equally. The operation produces one rollback entry.
The timeline can also preview a future **time removal** interval beginning at the playhead. A shaded
range and impact summary report its effective bounds, keys that would be deleted, and tracks that
would become empty. This stage is deliberately non-mutating; canonical tracks with no surviving key
are identified as blockers before transactional removal is enabled. After explicit confirmation,
**Remove interval** deletes keys in the semi-open range, shifts later keys left, shrinks the clip,
and creates one rollback entry. Confirmation is invalidated whenever the playhead or duration changes.
The detached canonical clip/track/key report is cached while the asset is unchanged. Skeleton or
animation mutation invalidates it through the shared bind/report refresh boundary. This avoids
rebuilding and garbage-collecting the complete nested Lua report every frame while Animation is idle.
The timeline maintains a clip-specific visible time range. Ctrl plus the mouse wheel zooms around
the time under the cursor, the unmodified wheel remains vertical track scrolling, middle-button
drag pans horizontally, and **Fit clip** restores the complete
duration. The ruler, playhead, markers, picking, dragging, box selection, and removal preview all
share this visible-range transform; off-screen keys are not submitted as edge-clamped markers.
When zoom makes the visible range smaller than the clip, a full-width **Horizontal pan** slider is
shown and moves the same range used by middle-button drag. It disappears automatically at full-clip
framing.
When keys are selected, **Fit selection** frames their minimum/maximum times with ten-percent padding
on each side. A selection confined to one time column receives a small centered range instead of a
zero-width view. Both framing actions preserve the canonical clip and selection.
**Time snap** optionally quantizes timeline seeking and the dragged selection anchor to a positive
configurable interval (default `1/30 s`). Group key movement still uses one shared delta, preserving
relative timing. Insert/duplicate/remove actions inherit snap through the positioned playhead, while
all mutations retain canonical collision and bounds validation.
Preset buttons for 24, 25, 30, 50, and 60 FPS set the interval to the exact reciprocal of the chosen
rate and enable snap immediately. The numeric field remains available for arbitrary intervals.
The ruler chooses major divisions adaptively from the `1/2/5 x 10^n` family using the current
visible duration and pixel width. Labels increase decimal precision as the step shrinks, major grid
lines span the track canvas, and a hard tick cap protects the frame loop from pathological ranges.
Animation pose tools now expose Move, Rotate, and Scale. The compact LBS Scale tool draws one
yellow bone-local diagonal handle and changes X/Y/Z by the same strictly positive factor, evaluating
the temporary in-memory pose continuously. With Auto Key disabled,
the result remains temporary until explicitly committed as channel `S`; with Auto Key enabled, mouse
release commits only scale through the shared snapshot/rollback transaction. DQS incompatibility is
still reported by the existing runtime method contract rather than silently changing skinning mode.
The yellow diagonal Scale handle applies one positive factor to all three local scale components,
preserving their existing proportions. Per-axis scale handles are intentionally unavailable: compact
LBS palettes do not carry inverse-transpose normal matrices, while rigid DQS rejects scale.
Non-uniform X/Y/Z scale remains reserved for a later non-compact normal-palette/shader contract.
The Scale tool is enabled from the preview's resolved method, not merely its requested method. Forced
DQS and Auto resolving to DQS disable Scale; forced LBS and Auto resolving to LBS enable its uniform
handle. Changing to an incompatible method while Scale is selected returns the tool to Move.
Animation also exposes its own **Authoring preview method** selector for Auto, LBS, or DQS and reports
the resolved method. Switching serializes the current unsaved canonical asset to a temporary file,
rebuilds the immutable-method preview from that snapshot, then removes the file; unsaved skeleton,
weights, and animation edits are therefore retained. Scaled clips can explicitly select LBS instead
of first visiting Runtime Skeletal Preview.
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
and mouse manipulation of joints/segments use the same canonical asset and history boundary.

Viewport picking distinguishes the head (initial joint), tail (final joint), and bone segment. A selected joint
highlights only that endpoint; selecting the segment highlights the segment and both endpoints.
Clicking empty viewport space clears the selection and remains available for camera orbit. This
selection feeds the delivered move/rotate, snapping, connection, radius, and structural operations.

## Retired Skin Weight Lab reference

The following sections preserve the behavior of the retired laboratory for historical comparison
and maintenance of shared algorithms. They do not describe an accessible worktree. Paint Weights
owns the supported authoring, mask, diagnostic, repair, Undo, and persistence workflow.

The retired GUI, mouse paths, and exclusive Lua helpers have been physically removed. The active
Paint Weights workflow performs mutations with the atomic batch setter; the scalar setter is
deprecated and retained only for compatibility and its existing tests.
Canonical weight reads, initialization/removal, palette inspection, and batch mutation still have
active Paint Weights, Bone Editor, or Mesh Debug consumers and are not removal candidates.

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
The selection, shell, heatmap-bucket, abrupt-vertex, and seam visuals are per-vertex cross markers.
Their shared helper batches at most 500 sampled vertices as independent, double-sided orthogonal
bars in one shape, remaining legible from different camera angles without line-strip connectors.
Actual abrupt-edge and selection-boundary edge lines remain separate intentional topology visuals.

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

### ImGui text compatibility

The current ImGui font atlas supports the Portuguese letters and accents used by the editor, but
does not guarantee typographic punctuation or symbol glyphs. Every runtime-visible label, tooltip,
status message, formatted range, and dynamically truncated name must therefore use ASCII-safe
symbols: `-` instead of typographic dashes, `...` instead of the ellipsis glyph, `deg` instead of
the degree glyph, `x` instead of the multiplication glyph, and `->` instead of a Unicode arrow.
Unsupported symbols commonly render as `?`. This restriction does not apply to comments or
documentation that ImGui never renders.

The initial workflow was validated with the following assets. Availability is stated explicitly so
a historical validation artifact is not mistaken for a currently versioned fixture:

| Fixture | Availability | Purpose |
|---|---|---|
| 41-bone alien-rat mesh | Historical validation artifact; not currently versioned | Main fixture used for AABB, proximity, heatmap, smoothing, rigid cavity, persistence, and FBX tests. Recreate it before using it as an automated acceptance fixture. |
| `T-BONE-rato-scale-100-one-unweighted-vertex.msh` | Historical validation artifact; not currently versioned | Normalize exceptional case: one vertex must be skipped without receiving an invented influence. Recreate it deterministically before using it as an automated acceptance fixture. |
| `Crate.msh` | Versioned under `src/test-lib/` | Mesh without skeleton/weights and two-subset isolation (`192 + 24 = 216` vertices). |

Accepted normalization results for the one-unweighted-vertex fixture were `179` corrected,
`35,969` already valid, `1` skipped, and `0` failures. Boundary-safe targeted smoothing verified
five external neighbors with zero modifications and zero audit failures.

## 9. Current limitations and future work

The following are current editor limitations rather than regressions caused by retiring Skin Weight Lab:

- runtime preview uses the engine's compiled OpenGL ES, DirectX 9, or Metal backend; there is no runtime
  backend selector;
- Runtime Skeletal Preview's evaluated mask gizmo follows the primary instance only; pose-stress
  comparison does not duplicate it for the secondary LBS/DQS instance;
- Runtime Skeletal Preview supports multiple transient compatible wearable/followers sharing the
  primary evaluated pose. Persistence and shared-player authoring/resource management remain future
  work;
- no protected/exclusion volumes;
- abrupt-transition classification still follows stored triangle indices rather than adding welded
  diagnostic edges; its repair separately synchronizes compatible connected coincident copies;
- no automatic heavy whole-mesh weight generation;
- no custom-tail animation generation;
- multi-clip playback is intentionally limited to one transient per-instance layer over a
  base clip. Lua and Runtime Skeletal Preview expose layer clip, independent time/seek, and weight
  before one final LBS/DQS palette. A linear timed fade can target base or layer; reaching zero
  removes the layer. Global pause freezes everything, while Pause Layer independently freezes the
  layer time and fade. The layer is not serialized; transition curves/queues and priority remain
  pending. Absolute and bind-relative Additive modes are explicit in Lua
  and Runtime Skeletal Preview; both share time, weight, speed, and fade controls;
- per-bone animation-layer masks multiply layer weight per stable bone identity and default to all
  ones when absent. Runtime Skeletal Preview exposes the canonical hierarchy, selected-bone weight,
  descendant propagation, All 0, All 1, Invert, and selected-subtree controls. Multi-bone actions
  commit transactionally through one pose evaluation. An optional evaluated-pose skeleton follows
  the active player's final transforms each frame, including base-only playback. It maps low-to-high
  mask weights from blue through green to red, defaults to all-high without a mask, and highlights
  the selected bone in cyan. The mask
  remains transient per-instance state and is distinct from skin weights and Paint Weights vertex masks;
- history entries carry operation-specific translation keys rather than frozen display strings, so
  menus and Undo/Redo feedback use the editor's current language even when it changed after the edit.

Richer pose-stress overlays and antipodality tooling are outside the current editor capability.
Backend validation coverage and its boundaries are recorded in
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md). Mesh Debug's legacy Bone
node/window has been retired; canonical bind inspection and weight repair belong to this editor.

The editor maintains bounded 50-entry Undo and Redo stacks backed by complete temporary MSH
snapshots. Every existing atomic commit boundary creates one Undo entry and clears Redo; Undo/Redo
restores canonical mesh data, modified state, workspace, selected bone, selected clip, and playhead,
then rebuilds reports, visuals, analysis state, and a memory-backed runtime preview. `Ctrl+Z` undoes,
while `Ctrl+Y` or `Ctrl+Shift+Z` redoes. Loading another mesh and normal editor quit paths remove all
owned history snapshots. Completed mouse drags remain one entry rather than one entry per move event.
Successful Undo/Redo also starts the editor's global timed overlay for four seconds, so its operation
description stays visible independent of left-panel scroll, active worktree, detached timeline, or
viewport focus. Every operation resets the interval, including repeated descriptions and operations
performed after changing the editor language. The ordinary persistent status remains available at the
top of the panel as well.

History descriptions distinguish skeleton initialization and structure edits, bind changes, bone
editor operations, weight initialization/normalization/smoothing/application, clip/track/key edits,
timeline movement/duplication/ripple/time insertion/removal, and viewport-authored TRS keys.

Runtime Skeletal Preview identifies whether its player was built from the saved asset or an
in-memory canonical snapshot. **Refresh runtime preview from memory** serializes current unsaved
skeleton, weights, and clips to a temporary MSH, constructs the real immutable-method runtime mesh
and player from it, then removes the temporary file. Later editor mutations mark that snapshot stale
until refreshed again. Method and comparison changes preserve an in-memory source by rebuilding a
fresh snapshot instead of silently returning to the saved file.
