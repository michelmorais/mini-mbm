#!/usr/bin/env python3
"""
Blender headless exporter for mini-mbm's Mesh Debug tool: takes a JSON dump of a loaded mesh's raw
geometry (and, optionally, its editor-authored/imported bone hierarchy) and produces a real FBX
with a skinned armature, ready for upload to an auto-rigging/animation service (Mixamo or
similar).

This script's input comes from editor/mesh_debug.lua's own geometry -- which is in the ENGINE's
own coordinate convention (Y-up), not Blender's native Z-up, ever since editor/blender_mesh_export.py's
import side started baking its Z-up -> Y-up correction directly into vertex/bone data (rather than
into a separate, since-removed "Default Angle" field that never touched the actual stored
position/normal/bone values). So this script's own output would come out wrongly oriented in
Blender/Mixamo without an inverse rotation applied first -- see --angle-x/y/z below, defaulted by
the caller to the exact inverse of the import side's own default (90 degrees on X, undoing that
side's default -90). Has NO per-vertex bone ownership, so this script uses Blender's built-in
automatic (heat-map) weight painting instead of exact vertex groups.

Invoked headlessly:
    blender -b --factory-startup --python blender_mesh_skeleton_export.py -- \
        --input mesh_dump.json --output character.fbx [--angle-x X] [--angle-y Y] [--angle-z Z] \
        [--cancel-file F] [--debug-steps]
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
import traceback


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--angle-x", type=float, default=0.0)
    parser.add_argument("--angle-y", type=float, default=0.0)
    parser.add_argument("--angle-z", type=float, default=0.0)
    parser.add_argument("--cancel-file", default="")
    parser.add_argument("--debug-steps", action="store_true")
    return parser.parse_args(argv)


def rotate_point_deg(x: float, y: float, z: float, angle_x_deg: float, angle_y_deg: float, angle_z_deg: float) -> tuple[float, float, float]:
    """Rotates a point by degrees around X, then Y, then Z. Kept in lockstep by hand with the
    identical helper of the same name in editor/blender_mesh_export.py (which is in turn kept in
    lockstep with editor/mesh_debug.lua's rotateX/Y/Z + applyRotationToBonesDeg) -- there is no
    shared module between the two standalone headless-Blender scripts.
    """
    if angle_x_deg:
        a = math.radians(angle_x_deg)
        c, s = math.cos(a), math.sin(a)
        y, z = y * c - z * s, y * s + z * c
    if angle_y_deg:
        a = math.radians(angle_y_deg)
        c, s = math.cos(a), math.sin(a)
        x, z = x * c + z * s, -x * s + z * c
    if angle_z_deg:
        a = math.radians(angle_z_deg)
        c, s = math.cos(a), math.sin(a)
        x, y = x * c - y * s, x * s + y * c
    return x, y, z


def debug_print(enabled: bool, message: str) -> None:
    if enabled:
        print(f"[mesh_skeleton_export] {message}", flush=True)


def check_cancel_requested(cancel_file: str) -> None:
    if cancel_file and os.path.exists(cancel_file):
        raise RuntimeError("Canceled by user.")


def load_json(input_path: str) -> dict:
    with open(input_path, "r", encoding="utf-8") as f:
        return json.load(f)


def build_mesh(data: dict, debug: bool, rotation_deg: tuple[float, float, float] | None = None):
    import bpy

    verts_data = data["mesh"]["vertices"]
    subsets = data["mesh"]["subsets"]

    # rotation_deg undoes the import side's own Z-up -> Y-up bake (see module docstring) so this
    # exported FBX comes out correctly oriented for Blender/Mixamo and for being re-imported later.
    if rotation_deg:
        positions = [rotate_point_deg(v["x"], v["y"], v["z"], *rotation_deg) for v in verts_data]
    else:
        positions = [(v["x"], v["y"], v["z"]) for v in verts_data]
    # JSON indices are 1-based (written straight from Lua array indices); from_pydata expects
    # 0-based, and are already global across the whole vertex list (mesh_debug.lua's dumper
    # offsets each subset's indices when writing). Each face's originating subset index is kept
    # alongside it (face_subset) so it can be assigned that subset's own material below -- subset
    # boundaries no longer collapse away here now that each subset can carry its own texture.
    faces = []
    face_subset: list[int] = []
    for subset_idx, subset in enumerate(subsets):
        idx = subset["indices"]
        for i in range(0, len(idx), 3):
            faces.append((idx[i] - 1, idx[i + 1] - 1, idx[i + 2] - 1))
            face_subset.append(subset_idx)

    mesh_data = bpy.data.meshes.new("MeshDebugMesh")
    mesh_data.from_pydata(positions, [], faces)
    mesh_data.update()
    # Without this, Blender defaults to flat shading -- every loop gets its own per-face normal,
    # so re-exporting this mesh later (editor/blender_mesh_export.py's export_frame_subsets, whose
    # vertex-dedup key includes the normal) finds almost no shared vertices even though the
    # topology hasn't changed: a real round-tripped character mesh went from 15882 vertices to
    # 86640 (== face_count * 3, i.e. zero sharing) without this call, blowing past the 65535
    # per-frame index-buffer limit on re-import even though the original import was well under it.
    # mesh_debug's own vertex data has no stored normals to restore exactly (writeMeshDebugJson
    # only dumps position/uv), so this recomputes smooth normals from topology instead -- not
    # byte-identical to whatever the source mesh's normals were, but restores the vertex sharing
    # that smooth shading is expected to have.
    mesh_data.shade_smooth()

    uv_layer = mesh_data.uv_layers.new(name="UVMap")
    for loop in mesh_data.loops:
        v = verts_data[loop.vertex_index]
        uv_layer.data[loop.index].uv = (v.get("u", 0.0), v.get("v", 0.0))

    # One material per subset, each wired to that subset's own texture (writeMeshDebugJson already
    # resolved and existence-checked the path, so a missing file just means a plain untextured
    # material here, not a load error). Without ANY material at all, the exported FBX's geometry
    # carries zero material/texture data -- confirmed via direct user testing that Mixamo's viewer
    # renders that as fully invisible (not a plain/gray fallback like Blender's own viewport would
    # show), so a material is required even when no texture is available.
    for subset_idx, subset in enumerate(subsets):
        mat = bpy.data.materials.new(name=f"MeshDebugMat_{subset_idx}")
        mat.use_nodes = True
        tex_path = subset.get("texture")
        if tex_path:
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            tex_node = mat.node_tree.nodes.new("ShaderNodeTexImage")
            try:
                tex_node.image = bpy.data.images.load(tex_path, check_existing=True)
            except RuntimeError:
                tex_node.image = None
            if bsdf and tex_node.image:
                mat.node_tree.links.new(tex_node.outputs["Color"], bsdf.inputs["Base Color"])
        mesh_data.materials.append(mat)

    for poly, subset_idx in zip(mesh_data.polygons, face_subset):
        poly.material_index = subset_idx

    mesh_obj = bpy.data.objects.new("MeshDebugMesh", mesh_data)
    bpy.context.collection.objects.link(mesh_obj)
    debug_print(debug, f"mesh built: vertices={len(positions)} faces={len(faces)} materials={len(subsets)}")
    return mesh_obj


def build_armature(data: dict, debug: bool, rotation_deg: tuple[float, float, float] | None = None):
    import bpy

    joints = {j["name"]: j for j in data.get("joints", [])}
    if not joints:
        return None

    roots = [name for name, j in joints.items() if not j.get("parent")]
    if not roots:
        return None

    arm_data = bpy.data.armatures.new("MeshDebugArmature")
    arm_obj = bpy.data.objects.new("MeshDebugArmature", arm_data)
    bpy.context.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode='EDIT')

    # Same rotation as build_mesh's positions -- bones and vertices must land in the same space.
    def joint_pos(name):
        j = joints[name]
        if rotation_deg:
            return rotate_point_deg(j["x"], j["y"], j["z"], *rotation_deg)
        return (j["x"], j["y"], j["z"])

    def dist(a, b):
        return sum((a[i] - b[i]) ** 2 for i in range(3)) ** 0.5

    children_by_parent: dict[str, list[str]] = {}
    for name, j in joints.items():
        parent = j.get("parent")
        if parent:
            children_by_parent.setdefault(parent, []).append(name)

    # Every bone represents the joint of the SAME name: head = that joint's own position. A single,
    # unambiguous child (the normal case through most of the skeleton -- spine/neck/limb chains)
    # gets tail = that child's own position, a real anatomical span. This must match the source
    # rig's own head=self/tail=child convention -- verified against a real 112-bone Mixamo FBX
    # (mixamorig:L_Ear/L_Temple/etc. each have their own distinct head_local in the original file).
    # An earlier version of this function set child_bone.head = joint_pos(parent_name) instead of
    # the joint's own position, shifting every bone in the hierarchy by one level -- confirmed as
    # the cause of severe Mixamo retargeting artifacts (arms crossing, deformed shoulders/groin) on
    # a real round-tripped character.
    #
    # A bone with SEVERAL children (Hand's 5 fingers, Head's ~40 facial bones) has no single
    # unambiguous child to point toward -- "first child" (this function's own earlier approach) is
    # order-dependent and was confirmed, via direct user comparison against a real Mixamo FBX loaded
    # side by side in Blender, to visibly misrepresent these joints (LeftHand's tail pointing
    # straight at LeftHandThumb1 instead of a neutral hand direction; Head's ~40 facial bones all
    # skewed toward whichever one happened to be listed first). The source rig's own convention for
    # these, confirmed directly against the same real FBX, is instead to continue the direction the
    # bone arrived from at its own length (e.g. LeftHand's own tail is a plain continuation of the
    # incoming LeftForeArm->LeftHand direction, not aimed at any specific finger) -- same formula
    # already used below for genuine leaves (no children at all). A multi-child ROOT (Hips' 3
    # children) has no incoming direction to continue, so it's the one remaining case that still
    # points toward its first child -- verified separately to already closely match the source rig's
    # own Hips bone (both point toward Spine, proportionally similar length).
    def compute_tail(name, pos, parent_pos):
        children = children_by_parent.get(name, [])
        if len(children) == 1 or (children and parent_pos is None):
            target = joint_pos(children[0])
            if dist(pos, target) >= 1e-6:
                return target
        if parent_pos is not None:
            dx, dy, dz = (pos[i] - parent_pos[i] for i in range(3))
            dlen = (dx * dx + dy * dy + dz * dz) ** 0.5
            if dlen >= 1e-6:
                stub_len = max(0.01, dlen)
                return (pos[0] + dx / dlen * stub_len,
                        pos[1] + dy / dlen * stub_len,
                        pos[2] + dz / dlen * stub_len)
        # Only reachable for a rootless leaf (no parent, no children) or a degenerate coincident
        # parent -- both rare. Z is "up" here, not Y: joint_pos already applied rotation_deg, which
        # for the standard export undoes the Y-up bake back to Blender's own Z-up space.
        return (pos[0], pos[1], pos[2] + 0.01)

    # Envelope-based binding (see bind_mesh_to_armature) sizes each bone's influence region from
    # head_radius/tail_radius, which Blender defaults to a small fixed value with no relation to
    # this character's actual scale or any individual bone's own thickness. Left at that default,
    # thick limbs (arms/legs) get an undersized envelope relative to the mesh's real cross-section
    # -- skin polygons away from the bone's own axis but still within the limb get weak/no weight
    # from the correct bone, which is what produces a "bellows"/pinching fold at each joint once
    # bent, most visible at the elbow/knee. joints[name]["radius"] already carries a real,
    # per-bone, scale-appropriate size (extract_armature_joints in blender_mesh_export.py derives
    # it from each source bone's own head-to-tail length), previously computed only for an editor
    # gizmo's display size and otherwise unused here -- reused directly as the envelope radius.
    #
    # envelope_distance (an ADDITIONAL smooth falloff zone beyond head/tail_radius, also part of
    # Blender's envelope weight calculation) also needs setting -- its own default, 0.25 (25cm), is
    # sized for typical hand-authored rigs and dwarfs this character's own scale: found via direct
    # testing that with radius fixed but envelope_distance left at 0.25, a left-knee vertex still
    # picked up real weight (0.17-0.22) from the RIGHT leg's bones, since 0.25 comfortably reaches
    # all the way across the ~0.19-unit gap between the two legs. Scaled down to a small fraction of
    # the bone's own radius so the falloff stays local to that specific limb.
    def set_envelope_radius(bone, name):
        # joints[name]["radius"] alone (radius_scale=1.0) left some inner-thigh/groin vertices with
        # zero total weight -- outside every nearby bone's envelope+falloff entirely, which leaves
        # them rigidly stuck at the bind pose while their neighbors deform, a tearing artifact
        # rather than a fix. 1.8x closes that gap on the same real character without reintroducing
        # the opposite-leg cross-talk envelope_distance=0.25 caused.
        radius = float(joints[name].get("radius", 0.02)) * 1.8
        bone.head_radius = radius
        bone.tail_radius = radius
        bone.envelope_distance = radius * 0.3

    edit_bones = arm_data.edit_bones
    created = {}

    for root_name in roots:
        root_pos = joint_pos(root_name)
        root_bone = edit_bones.new(root_name)
        root_bone.head = root_pos
        root_bone.tail = compute_tail(root_name, root_pos, None)
        set_envelope_radius(root_bone, root_name)
        created[root_name] = root_bone

    queue = list(roots)
    while queue:
        parent_name = queue.pop(0)
        parent_pos = joint_pos(parent_name)
        for child_name in children_by_parent.get(parent_name, []):
            child_pos = joint_pos(child_name)
            child_bone = edit_bones.new(child_name)
            child_bone.head = child_pos
            child_bone.tail = compute_tail(child_name, child_pos, parent_pos)
            child_bone.parent = created[parent_name]
            set_envelope_radius(child_bone, child_name)
            created[child_name] = child_bone
            queue.append(child_name)

    bpy.ops.object.mode_set(mode='OBJECT')
    debug_print(debug, f"armature built: {len(created)} bones ({len(roots)} root(s))")
    return arm_obj


def bind_mesh_to_armature(mesh_obj, armature_obj, debug: bool) -> None:
    import bpy

    # mesh_debug's bones carry no per-vertex ownership, so some form of automatic weighting is
    # required. ARMATURE_AUTO (heat-map, "Automatic Weights") was tried first and found, via direct
    # testing, to fail COMPLETELY (100% of vertices left with zero weight in every vertex group, not
    # a partial/edge-case failure) despite only printing a mild "failed to find solution for one or
    # more bones" warning -- reproduced on both a 65-bone and a 112-bone real character, so this
    # isn't specific to unusually complex rigs. Heat mapping needs a watertight-ish manifold mesh to
    # diffuse through; build_mesh's single combined mesh_data merges what the source file often has
    # as several separate objects (body, teeth, eyelashes, eyes, a flat "head mask" overlay...),
    # several of which are inherently open/thin surfaces, and the solver appears to bail out for the
    # whole mesh rather than degrading gracefully per-island. ARMATURE_ENVELOPE (distance-based, not
    # heat-diffusion) has no such requirement and was verified to weight 100% of vertices on both
    # test rigs. Its raw per-bone weights aren't normalized (multiple overlapping bone envelopes can
    # each assign a vertex weight 1.0 independently, which would double/triple-count influence at
    # deform time), so vertex_group_limit_total(4) + vertex_group_normalize_all bring it in line with
    # the standard FBX/game-engine convention of <=4 normalized influences per vertex.
    bpy.ops.object.select_all(action='DESELECT')
    mesh_obj.select_set(True)
    armature_obj.select_set(True)
    bpy.context.view_layer.objects.active = armature_obj
    bpy.ops.object.parent_set(type='ARMATURE_ENVELOPE')

    bpy.context.view_layer.objects.active = mesh_obj
    bpy.ops.object.vertex_group_limit_total(limit=4)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    debug_print(debug, "bound mesh to armature via envelope weights (normalized, <=4 influences/vertex)")

    filled = _assign_nearest_bone_to_unweighted(mesh_obj, armature_obj)
    if filled:
        debug_print(debug, f"nearest-bone fallback filled {filled} vertices envelope weighting missed entirely")

    fixed = _resolve_left_right_crosstalk(mesh_obj)
    if fixed:
        debug_print(debug, f"resolved left/right envelope overlap on {fixed} vertices near the body midline")


def _resolve_left_right_crosstalk(mesh_obj) -> int:
    """Envelope weighting still leaves a thin sliver of vertices near the body midline (measured
    via direct testing: ~0.6% of vertices, all within ~4cm of center, concentrated in the inner-
    thigh/groin) with real weight on BOTH a Left-side and a Right-side bone at once, since both
    legs' envelopes reach a little past the midline there. In bind pose the two sides move together
    so this is invisible, but reported directly by the user as a deformation "below the groin" when
    a pose actually spreads the legs apart -- a vertex weighted to both sides gets pulled toward two
    increasingly distant targets, a webbing/tearing artifact between the legs. Keeps only the
    dominant side's weights per vertex and renormalizes, rather than trying to further tune envelope
    radius/distance to eliminate the overlap without reintroducing the zero-weight gaps the
    nearest-bone fallback above already fixed.
    """
    changed = 0
    for v in mesh_obj.data.vertices:
        groups = [g for g in v.groups if g.weight > 1e-6]
        left = [g for g in groups if _is_left_bone(mesh_obj.vertex_groups[g.group].name)]
        right = [g for g in groups if _is_right_bone(mesh_obj.vertex_groups[g.group].name)]
        if not left or not right:
            continue
        left_w = sum(g.weight for g in left)
        right_w = sum(g.weight for g in right)
        drop = left if left_w < right_w else right
        keep = [g for g in groups if g not in drop]
        for g in drop:
            g.weight = 0.0
        total = sum(g.weight for g in keep)
        if total > 1e-6:
            for g in keep:
                g.weight /= total
        changed += 1
    return changed


def _is_left_bone(name: str) -> bool:
    return 'Left' in name or ':L_' in name


def _is_right_bone(name: str) -> bool:
    return 'Right' in name or ':R_' in name


def _assign_nearest_bone_to_unweighted(mesh_obj, armature_obj) -> int:
    """Envelope weighting (see bind_mesh_to_armature) can still leave some vertices with zero
    total weight even with a per-bone radius scaled to this character: a bone's envelope radius is
    derived from its own length, which is a poor proxy for wide-but-short bones (found via direct
    testing that this concentrates almost entirely in the shoulder/chest/neck region, where Spine2/
    shoulder bones are short relative to how wide the torso actually is). Tuning the radius scale
    higher to cover this would just reintroduce cross-talk between other close-together bones
    (fingers, facial detail) that the current scale was tuned to avoid -- a uniform scale can't
    solve both at once. A completely unweighted vertex doesn't deform with the skeleton at all,
    which reads as the mesh tearing away from its neighbors during animation -- worse than a rough
    but present weight -- so any vertex envelope weighting missed entirely gets rigidly assigned
    (weight 1.0) to whichever bone's head-to-tail segment it's geometrically closest to.
    """
    import bpy
    from mathutils.geometry import intersect_point_line

    me = mesh_obj.data
    bone_segs = [(b.name, b.head_local, b.tail_local) for b in armature_obj.data.bones]

    filled = 0
    for v in me.vertices:
        if sum(g.weight for g in v.groups) > 1e-6:
            continue
        best_name, best_dist = None, None
        for name, head, tail in bone_segs:
            closest, factor = intersect_point_line(v.co, head, tail)
            factor = max(0.0, min(1.0, factor))
            closest = head + (tail - head) * factor
            d = (v.co - closest).length
            if best_dist is None or d < best_dist:
                best_dist, best_name = d, name
        if best_name is not None:
            vg = mesh_obj.vertex_groups.get(best_name) or mesh_obj.vertex_groups.new(name=best_name)
            vg.add([v.index], 1.0, 'REPLACE')
            filled += 1
    return filled


def prepare_and_export(mesh_obj, armature_obj, output_path: str, debug: bool) -> None:
    import bpy
    from mathutils import Vector

    # Center X/Y, feet at Z=0. No rescale to a target height here -- mesh_debug's geometry is
    # already in the mesh's own real/intended scale, not raw photo-pixel units.
    world_corners = [mesh_obj.matrix_world @ Vector(c) for c in mesh_obj.bound_box]
    min_x = min(c.x for c in world_corners)
    max_x = max(c.x for c in world_corners)
    min_y = min(c.y for c in world_corners)
    max_y = max(c.y for c in world_corners)
    min_z = min(c.z for c in world_corners)
    delta = Vector(((min_x + max_x) / -2.0, (min_y + max_y) / -2.0, -min_z))

    target = armature_obj if armature_obj else mesh_obj
    target.location += delta

    bpy.ops.object.select_all(action='DESELECT')
    mesh_obj.select_set(True)
    object_types = {'MESH'}
    if armature_obj:
        armature_obj.select_set(True)
        object_types.add('ARMATURE')
    bpy.context.view_layer.objects.active = armature_obj or mesh_obj

    bpy.ops.export_scene.fbx(
        filepath=output_path,
        use_selection=True,
        bake_anim=False,
        add_leaf_bones=False,
        mesh_smooth_type='FACE',
        object_types=object_types,
        # build_mesh's materials reference textures by their original on-disk path -- COPY +
        # embed_textures bakes the actual image bytes into the FBX itself, so the file stays
        # self-contained after being uploaded to a remote service (Mixamo) that has no access to
        # this machine's filesystem.
        path_mode='COPY',
        embed_textures=True,
    )
    debug_print(debug, f"exported: {output_path}")


def main() -> int:
    argv = []
    if "--" in sys.argv:
        argv = sys.argv[sys.argv.index("--") + 1:]
    args = parse_args(argv)

    check_cancel_requested(args.cancel_file)
    debug_print(args.debug_steps, f"input: {args.input}")
    data = load_json(args.input)
    if not data.get("mesh", {}).get("vertices"):
        raise RuntimeError("Input JSON has no mesh vertices -- nothing to export.")

    rotation_deg = (args.angle_x, args.angle_y, args.angle_z)
    if rotation_deg != (0.0, 0.0, 0.0):
        debug_print(args.debug_steps, f"applying export rotation (deg): {rotation_deg}")
    else:
        rotation_deg = None

    check_cancel_requested(args.cancel_file)
    mesh_obj = build_mesh(data, args.debug_steps, rotation_deg)
    check_cancel_requested(args.cancel_file)
    armature_obj = build_armature(data, args.debug_steps, rotation_deg)
    if armature_obj:
        check_cancel_requested(args.cancel_file)
        bind_mesh_to_armature(mesh_obj, armature_obj, args.debug_steps)
    else:
        debug_print(args.debug_steps, "no bones in input -- exporting mesh-only FBX")

    check_cancel_requested(args.cancel_file)
    out_path = os.path.abspath(args.output)
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    prepare_and_export(mesh_obj, armature_obj, out_path, args.debug_steps)

    debug_print(args.debug_steps, "done")
    return 0


if __name__ == "__main__":
    try:
        import bpy  # type: ignore
    except Exception as exc:
        sys.stderr.write(f"Failed to import bpy: {exc}\n")
        sys.exit(2)

    cli_argv = []
    if "--" in sys.argv:
        cli_argv = sys.argv[sys.argv.index("--") + 1:]
    debug_on = "--debug-steps" in cli_argv

    try:
        sys.exit(main())
    except Exception as exc:
        if debug_on:
            print(f"[mesh_skeleton_export] ERROR: {exc}", file=sys.stderr, flush=True)
            traceback.print_exc()
        else:
            sys.stderr.write(f"Exporter failed: {exc}\n")
        sys.exit(1)
