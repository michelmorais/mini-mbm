#!/usr/bin/env python3
"""
Blender headless exporter for mini-mbm's Mesh Debug tool: takes a JSON dump of a loaded mesh's raw
geometry (and, optionally, its editor-authored/imported bone hierarchy) and produces a real FBX
with a skinned armature and one Blender Action per sampled canonical skeletal clip, ready for
round-trip editing or upload to an auto-rigging/animation service (Mixamo or similar).

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
import warnings
import traceback


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--angle-x", type=float, default=0.0)
    parser.add_argument("--angle-y", type=float, default=0.0)
    parser.add_argument("--angle-z", type=float, default=0.0)
    # Undoes editor/blender_mesh_export.py's own --invert-u/v (applied at import time via
    # invertMeshUV before this mesh's UVs were ever saved) -- inverting is self-cancelling, so
    # applying the same flip again here restores the original UVs for Blender/Mixamo.
    parser.add_argument("--invert-u", action="store_true")
    parser.add_argument("--invert-v", action="store_true")
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


def canonical_matrix_to_blender(matrix_values: list[float],
                                rotation_deg: tuple[float, float, float] | None,
                                canonical: bool):
    """Converts an engine row-vector global matrix back to Blender's column-vector space."""
    from mathutils import Matrix

    if len(matrix_values) != 16:
        raise RuntimeError("Canonical animation matrix must contain exactly 16 values.")
    canonical_matrix = Matrix([[float(matrix_values[column * 4 + row]) for column in range(4)]
                               for row in range(4)])
    restore = Matrix.Identity(4)
    if canonical:
        reflection = Matrix.Identity(4)
        reflection[0][0] = -1.0
        restore = reflection
    if rotation_deg:
        ax, ay, az = (math.radians(float(value)) for value in rotation_deg)
        rotation = (Matrix.Rotation(az, 4, "Z") @ Matrix.Rotation(ay, 4, "Y") @
                    Matrix.Rotation(ax, 4, "X"))
        restore = rotation @ restore
    return restore @ canonical_matrix @ restore.inverted()


def retarget_sampled_global_to_reconstructed_bind(sampled_global, source_bind,
                                                   reconstructed_bind):
    """Transfers the source bone's animated skinning delta onto Blender's reconstructed rest.

    Blender edit bones cannot retain scale in their rest matrices. Keeping an absolute sampled
    matrix would therefore turn a source bind scale such as 0.01 into animation-only shrinkage.
    """
    return sampled_global @ source_bind.inverted_safe() @ reconstructed_bind


def build_mesh(data: dict, debug: bool, rotation_deg: tuple[float, float, float] | None = None,
                invert_u: bool = False, invert_v: bool = False):
    import bpy

    verts_data = data["mesh"]["vertices"]
    subsets = data["mesh"]["subsets"]

    # rotation_deg undoes the import side's own Z-up -> Y-up bake (see module docstring) so this
    # exported FBX comes out correctly oriented for Blender/Mixamo and for being re-imported later.
    canonical = data.get("canonicalSkeleton") is True
    def restore_position(v):
        position = (-v["x"], v["y"], v["z"]) if canonical else (v["x"], v["y"], v["z"])
        return rotate_point_deg(*position, *rotation_deg) if rotation_deg else position
    positions = [restore_position(v) for v in verts_data]
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
            face = (idx[i] - 1, idx[i + 1] - 1, idx[i + 2] - 1)
            faces.append((face[2], face[1], face[0]) if canonical else face)
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
        u = v.get("u", 0.0)
        vv = v.get("v", 0.0)
        # Undoes editor/blender_mesh_export.py's own --invert-u/v, applied at import time before
        # this mesh's UVs were ever saved (see this script's --invert-u/v argparse comment).
        if invert_u:
            u = 1.0 - u
        if invert_v:
            vv = 1.0 - vv
        uv_layer.data[loop.index].uv = (u, vv)

    def load_texture_node(nodes, path: str | None, non_color: bool = False):
        if not path:
            return None
        tex_node = nodes.new("ShaderNodeTexImage")
        try:
            tex_node.image = bpy.data.images.load(path, check_existing=True)
            if non_color:
                tex_node.image.colorspace_settings.name = "Non-Color"
        except (RuntimeError, TypeError):
            nodes.remove(tex_node)
            return None
        return tex_node

    def find_input(bsdf, *names):
        for name in names:
            socket = bsdf.inputs.get(name)
            if socket is not None:
                return socket
        return None

    # One material per subset, wired to that subset's primary + v11 material-role textures
    # (writeMeshDebugJson already resolved and existence-checked every path). A missing file is a
    # plain untextured material here, not a load error.
    # Without ANY material at all, the exported FBX's geometry
    # carries zero material/texture data -- confirmed via direct user testing that Mixamo's viewer
    # renders that as fully invisible (not a plain/gray fallback like Blender's own viewport would
    # show), so a material is required even when no texture is available.
    for subset_idx, subset in enumerate(subsets):
        mat = bpy.data.materials.new(name=f"MeshDebugMat_{subset_idx}")
        # Blender 5.x still requires this switch for node-backed materials, but warns that the
        # property is scheduled for removal in 6.0. Keep compatibility without presenting that
        # non-fatal API deprecation as an export failure in Mesh Debug's user-facing log.
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", DeprecationWarning)
            mat.use_nodes = True
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links
        bsdf = nodes.get("Principled BSDF")
        tex_path = subset.get("texture")
        tex_node = load_texture_node(nodes, tex_path)
        if bsdf and tex_node:
            links.new(tex_node.outputs["Color"], bsdf.inputs["Base Color"])

        material_textures = subset.get("materialTextures") or {}
        normal_node = load_texture_node(nodes, material_textures.get("normal"), True)
        normal_input = find_input(bsdf, "Normal") if bsdf else None
        if normal_node and normal_input:
            normal_map = nodes.new("ShaderNodeNormalMap")
            links.new(normal_node.outputs["Color"], normal_map.inputs["Color"])
            links.new(normal_map.outputs["Normal"], normal_input)

        specular_node = load_texture_node(nodes, material_textures.get("specular"))
        specular_input = find_input(bsdf, "Specular IOR Level", "Specular") if bsdf else None
        if specular_node and specular_input:
            links.new(specular_node.outputs["Color"], specular_input)

        emissive_node = load_texture_node(nodes, material_textures.get("emissive"))
        emissive_input = find_input(bsdf, "Emission Color", "Emission") if bsdf else None
        if emissive_node and emissive_input:
            links.new(emissive_node.outputs["Color"], emissive_input)

        mask_node = load_texture_node(nodes, material_textures.get("mask"), True)
        roughness_input = find_input(bsdf, "Roughness") if bsdf else None
        metallic_input = find_input(bsdf, "Metallic") if bsdf else None
        if mask_node and (roughness_input or metallic_input):
            separate = nodes.new("ShaderNodeSeparateColor")
            links.new(mask_node.outputs["Color"], separate.inputs["Color"])
            if roughness_input:
                links.new(separate.outputs["Green"], roughness_input)
            if metallic_input:
                links.new(separate.outputs["Blue"], metallic_input)
            # FBX has no metallic/roughness texture channel. When the material has no real
            # specular map, also route the packed mask through FBX's specular channel so Blender
            # embeds it instead of dropping the otherwise-PBR-only image. The custom property lets
            # mini-mbm's Blender importer restore it to MASK rather than misclassifying it as a
            # genuine specular texture on a later FBX -> MSH round trip.
            if specular_node is None and specular_input:
                links.new(mask_node.outputs["Color"], specular_input)
                mat["mbm_mask_texture"] = mask_node.image.name
        mesh_data.materials.append(mat)

    for poly, subset_idx in zip(mesh_data.polygons, face_subset):
        poly.material_index = subset_idx

    mesh_obj = bpy.data.objects.new("MeshDebugMesh", mesh_data)
    bpy.context.collection.objects.link(mesh_obj)
    debug_print(debug, f"mesh built: vertices={len(positions)} faces={len(faces)} materials={len(subsets)}")
    return mesh_obj


def build_armature(data: dict, debug: bool, rotation_deg: tuple[float, float, float] | None = None):
    import bpy
    from mathutils import Vector

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
        position = (-j["x"], j["y"], j["z"]) if data.get("canonicalSkeleton") is True else (j["x"], j["y"], j["z"])
        if rotation_deg:
            return rotate_point_deg(*position, *rotation_deg)
        return position

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

    # Canonical export supplies each bone's row-major global bind matrix. Its Y and Z basis rows
    # give Blender the bone axis and roll directly, including the complete parent composition;
    # this avoids converting canonical local quaternions back through an Euler hierarchy. Assets
    # without a usable matrix/length keep the topology-based fallback for mesh-only robustness.
    EPS = 1e-6

    def has_orientation(name):
        matrix = joints[name].get("globalBindMatrix") or []
        return len(matrix) == 16 and float(joints[name].get("length", 0.0)) > EPS

    def reconstruct_tail_and_roll(name, pos):
        j = joints[name]
        length = float(j["length"])
        # The engine matrix is row-vector/row-major; transpose it into Blender's column-vector
        # convention. Canonical import changed coordinates by conjugation C*M*C^-1, so reverse it
        # by the complete inverse conjugation R*B*R^-1. Transforming Y/Z alone is incorrect because
        # the right-hand factor changes the matrix's local basis too (visible as 90-degree errors on
        # real hips, feet, arms, and face bones).
        blender_matrix = canonical_matrix_to_blender(
            j["globalBindMatrix"], rotation_deg, data.get("canonicalSkeleton") is True)
        y_stored = blender_matrix.to_3x3().col[1]
        z_stored = blender_matrix.to_3x3().col[2]
        y_stored.normalize()
        z_stored.normalize()
        tail = tuple(pos[index] + y_stored[index] * length for index in range(3))
        return tail, tuple(z_stored)

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
        if has_orientation(root_name):
            tail, z_dir = reconstruct_tail_and_roll(root_name, root_pos)
            root_bone.tail = tail
            root_bone.align_roll(Vector(z_dir))
        else:
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
            if has_orientation(child_name):
                tail, z_dir = reconstruct_tail_and_roll(child_name, child_pos)
                child_bone.tail = tail
                child_bone.align_roll(Vector(z_dir))
            else:
                child_bone.tail = compute_tail(child_name, child_pos, parent_pos)
            child_bone.parent = created[parent_name]
            set_envelope_radius(child_bone, child_name)
            created[child_name] = child_bone
            queue.append(child_name)

    bpy.ops.object.mode_set(mode='OBJECT')
    debug_print(debug, f"armature built: {len(created)} bones ({len(roots)} root(s))")
    return arm_obj


def apply_stored_vertex_weights_override(mesh_obj, verts_data: list[dict], debug: bool) -> None:
    """Writes canonical type-42 per-vertex weights, round-tripped by writeMeshDebugJson's
    "boneNames"/"weights" fields, directly into mesh_obj's vertex groups.

    Runs as a FINAL OVERRIDE PASS, after bind_mesh_to_armature has already run its
    ARMATURE_ENVELOPE geometric approximation (+ cleanup passes) for the WHOLE mesh -- previously
    this function replaced that entire pass mesh-wide, which meant any vertex lacking stored data
    (i.e. the rest of the character, whenever only a prop bone like a rigid-bound sword had real
    weights) was left with NO vertex group membership at all, silently zeroing its deformation.
    Only vertices that actually carry stored "boneNames" are touched here: their existing
    (envelope-derived) group memberships are cleared first, then re-set from the stored data --
    so explicit/real per-vertex data always wins for exactly the vertices it targets, without
    needing to skip envelope binding (or its cleanup passes) for anything else in the mesh.
    """
    mesh_data = mesh_obj.data
    vgroups: dict[str, Any] = {}
    weighted_count = 0
    for vertex_index, v in enumerate(verts_data):
        names = v.get("boneNames") or []
        weights = v.get("weights") or []
        if not names:
            continue
        weighted_count += 1
        for g in list(mesh_data.vertices[vertex_index].groups):
            mesh_obj.vertex_groups[g.group].remove([vertex_index])
        for name, weight in zip(names, weights):
            if not name:
                continue
            vg = vgroups.get(name)
            if vg is None:
                vg = mesh_obj.vertex_groups.get(name) or mesh_obj.vertex_groups.new(name=name)
                vgroups[name] = vg
            vg.add([vertex_index], float(weight), 'REPLACE')
    debug_print(debug, f"applied stored vertex weight overrides: {weighted_count}/{len(verts_data)} vertices, "
                        f"{len(vgroups)} bone(s) referenced")


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

    non_deforming = _compute_non_deforming_bone_names(armature_obj)
    if non_deforming:
        stripped = _strip_bone_weight(mesh_obj, non_deforming)
        debug_print(debug, f"stripped weight from {sorted(non_deforming)} (root-motion bone(s)), "
                            f"affected {stripped} vertices")

    filled = _assign_nearest_bone_to_unweighted(mesh_obj, armature_obj, exclude_names=non_deforming)
    if filled:
        debug_print(debug, f"nearest-bone fallback filled {filled} vertices envelope weighting missed entirely")

    fixed = _resolve_left_right_crosstalk(mesh_obj)
    if fixed:
        debug_print(debug, f"resolved left/right envelope overlap on {fixed} vertices near the body midline")


def build_animation_actions(data: dict, armature_obj, rotation_deg: tuple[float, float, float] | None,
                            debug: bool) -> int:
    """Creates one densely sampled Blender Action for every canonical mini-mbm clip.

    Mesh Debug evaluates the source clips before writing JSON, so the matrices received here
    already contain the engine's authoritative easing, quaternion interpolation, hierarchy, and
    scale result. Keying every sampled pose avoids approximating those semantics with Blender
    F-curve interpolation during the round trip.
    """
    import bpy

    animation_data = data.get("animations") or {}
    clips = animation_data.get("clips") or []
    if not armature_obj or not clips:
        return 0
    joints = data.get("joints") or []
    sample_rate = int(animation_data.get("sampleRate") or 30)
    if sample_rate <= 0:
        raise RuntimeError("Canonical animation sample rate must be positive.")

    scene = bpy.context.scene
    scene.render.fps = sample_rate
    scene.render.fps_base = 1.0
    scene.frame_start = 1
    maximum_frame = 1
    armature_obj.animation_data_create()
    pose_bones = []
    source_bind_by_name = {}
    for joint in joints:
        pose_bone = armature_obj.pose.bones.get(joint["name"])
        if pose_bone is None:
            raise RuntimeError(f"Canonical animation references missing bone '{joint['name']}'.")
        pose_bone.rotation_mode = 'QUATERNION'
        pose_bones.append(pose_bone)
        source_bind_by_name[pose_bone.name] = canonical_matrix_to_blender(
            joint["globalBindMatrix"], rotation_deg, data.get("canonicalSkeleton") is True)

    for clip_index, clip in enumerate(clips):
        samples = clip.get("samples") or []
        if not samples:
            continue
        action_name = str(clip.get("name") or f"Clip_{clip_index + 1}")
        action = bpy.data.actions.new(name=action_name)
        action.use_fake_user = True
        action["mini_mbm_loop"] = bool(clip.get("loop", True))
        armature_obj.animation_data.action = action
        previous_rotation = {}
        curve_values = {}

        def ensure_curve(data_path, array_index, group_name):
            # Blender 5 layered Actions replaced Action.fcurves with this compatibility API.
            # Older supported releases still expose the classic collection directly.
            if hasattr(action, "fcurve_ensure_for_datablock"):
                return action.fcurve_ensure_for_datablock(
                    armature_obj, data_path, index=array_index, group_name=group_name)
            return action.fcurves.new(data_path=data_path, index=array_index, action_group=group_name)

        bone_curves = {}
        for pose_bone in pose_bones:
            channels = []
            for property_name, component_count in (("location", 3),
                                                    ("rotation_quaternion", 4),
                                                    ("scale", 3)):
                data_path = pose_bone.path_from_id(property_name)
                for component in range(component_count):
                    curve = ensure_curve(data_path, component, pose_bone.name)
                    curve.keyframe_points.add(len(samples))
                    curve_values[curve] = [0.0] * (len(samples) * 2)
                    channels.append(curve)
            bone_curves[pose_bone.name] = channels

        for sample_index, sample in enumerate(samples):
            frame = sample_index + 1
            maximum_frame = max(maximum_frame, frame)
            matrices = sample.get("globalMatrices") or []
            if len(matrices) != len(pose_bones):
                raise RuntimeError(
                    f"Canonical clip '{action_name}' sample {sample_index} has {len(matrices)} "
                    f"bone matrices; expected {len(pose_bones)}.")
            # Convert every armature-space/global target to matrix_basis explicitly. Assigning
            # PoseBone.matrix parent-before-child is not sufficient: Blender defers dependency-
            # graph evaluation, so a child's setter can still observe its parent's PREVIOUS pose.
            # On common FBX rigs whose armature carries scale 0.01, that reapplies 0.01 once per
            # hierarchy level (0.01, 0.0001, 0.000001...) until deep bones become singular/NaN.
            #
            # For Blender's default full parent inheritance:
            #   pose = parentPose * inverse(parentRest) * rest * basis
            # Solving this equation directly makes every bone independent of deferred evaluation
            # and avoids view-layer updates while constructing the Action.
            target_by_name = {}
            for pose_bone, matrix_values in zip(pose_bones, matrices):
                sampled_global = canonical_matrix_to_blender(
                    matrix_values, rotation_deg, data.get("canonicalSkeleton") is True)
                source_bind = source_bind_by_name[pose_bone.name]
                reconstructed_bind = pose_bone.bone.matrix_local
                # Edit bones encode orientation and length but cannot retain scale in their rest
                # matrices. A canonical FBX root commonly has bind scale 0.01; keying the sampled
                # absolute global matrix against Blender's scale-1 rest pose therefore shrinks the
                # whole character to 1% only while animated. Transfer the source skinning delta
                # onto the reconstructed rest matrix instead:
                #   target = sampledGlobal * inverse(sourceBind) * reconstructedBind
                # At the canonical bind pose this reduces exactly to reconstructedBind.
                target_by_name[pose_bone.name] = retarget_sampled_global_to_reconstructed_bind(
                    sampled_global, source_bind, reconstructed_bind)
            for pose_bone in pose_bones:
                target = target_by_name[pose_bone.name]
                rest = pose_bone.bone.matrix_local
                if pose_bone.parent:
                    parent_rest = pose_bone.parent.bone.matrix_local
                    parent_target = target_by_name[pose_bone.parent.name]
                    basis = (rest.inverted_safe() @ parent_rest @
                             parent_target.inverted_safe() @ target)
                else:
                    basis = rest.inverted_safe() @ target
                location, rotation, scale = basis.decompose()
                prior = previous_rotation.get(pose_bone.name)
                if prior is not None and prior.dot(rotation) < 0.0:
                    rotation.negate()
                previous_rotation[pose_bone.name] = rotation.copy()
                # Fill F-Curve storage in bulk after all samples. Calling keyframe_insert for every
                # channel repeatedly sorts and tags the growing Action, making a 1,342-frame clip
                # take minutes. The solved local values are already final and need no dependency-
                # graph evaluation here.
                values = tuple(location) + tuple(rotation) + tuple(scale)
                for curve, value in zip(bone_curves[pose_bone.name], values):
                    coordinates = curve_values[curve]
                    coordinates[sample_index * 2] = frame
                    coordinates[sample_index * 2 + 1] = value

        for curve, coordinates in curve_values.items():
            curve.keyframe_points.foreach_set("co", coordinates)
            curve.update()

        debug_print(debug, f"animation action built: {action_name} ({len(samples)} samples at "
                           f"{sample_rate} FPS)")

    armature_obj.animation_data.action = None
    scene.frame_end = maximum_frame
    scene.frame_set(1)
    return sum(1 for clip in clips if clip.get("samples"))


def _compute_non_deforming_bone_names(armature_obj) -> set:
    """Identifies "root-motion"/"pass-through" bones that should never receive real vertex weight:
    a top-level root (no parent) whose ONLY purpose is connecting the world origin to the first
    real body joint, recognized by the classic convention of having exactly one child. Confirmed via
    direct testing on a real character (Lorekeeper) that this bone spans floor level up to hip
    height and, left eligible for normal envelope/nearest-bone weighting like any other bone, picks
    up real weight on 276 vertices (some at full weight 1.0) -- purely because it geometrically
    overlaps the hip/pelvis region, not because it's meant to deform anything there. A vertex
    weighted to this bone doesn't move the way its neighbors do during animation (root-motion bones
    commonly translate independently of the rest of the skeleton), which is a plausible direct cause
    of the "melting"/detached-vertex deformation the user reported depending on the animation. Only
    checked at the TOP level (roots), not recursively down the hierarchy -- a single-child CHAIN
    elsewhere (e.g. upperleg->lowerleg->foot->toes) is a normal sequence of real deforming joints,
    not a root-motion artifact; the "exactly one child" signature only means "non-deforming
    pass-through" when it's also the very top of the hierarchy with nothing above it. A root with
    multiple children (e.g. a Mixamo body-only rig where Hips IS the root, branching directly into
    Spine/LeftUpLeg/RightUpLeg) is a real joint and is correctly left alone by this same check.
    """
    names = set()
    for b in armature_obj.data.bones:
        if b.parent is None and len(b.children) == 1:
            names.add(b.name)
    return names


def _strip_bone_weight(mesh_obj, names) -> int:
    """Zeroes out any weight assigned to the given bone names (by vertex-group name) and
    renormalizes each affected vertex's remaining weights back to sum to 1.0. A vertex left with
    zero total weight afterward (it was ONLY weighted to an excluded bone) is intentionally left
    that way here -- the caller's subsequent _assign_nearest_bone_to_unweighted pass (with these
    same names excluded) picks it up and assigns it to the nearest real bone instead.
    """
    group_indices = {g.index for g in mesh_obj.vertex_groups if g.name in names}
    if not group_indices:
        return 0
    changed = 0
    for v in mesh_obj.data.vertices:
        stripped = [g for g in v.groups if g.group in group_indices and g.weight > 1e-6]
        if not stripped:
            continue
        changed += 1
        keep = [g for g in v.groups if g.group not in group_indices]
        for g in stripped:
            g.weight = 0.0
        total = sum(g.weight for g in keep)
        if total > 1e-6:
            for g in keep:
                g.weight /= total
    return changed


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


def _assign_nearest_bone_to_unweighted(mesh_obj, armature_obj, exclude_names=frozenset()) -> int:
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

    exclude_names (root-motion/pass-through bones, see _compute_non_deforming_bone_names) are left
    out of the candidate list entirely -- a vertex stripped of root-motion weight by
    _strip_bone_weight must never land right back on that same bone here as its "nearest" fallback.
    """
    import bpy
    from mathutils.geometry import intersect_point_line

    me = mesh_obj.data
    bone_segs = [(b.name, b.head_local, b.tail_local) for b in armature_obj.data.bones
                 if b.name not in exclude_names]

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


def prepare_and_export(mesh_obj, armature_obj, output_path: str, animation_count: int,
                       debug: bool) -> None:
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
        bake_anim=animation_count > 0,
        bake_anim_use_all_actions=animation_count > 0,
        bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True,
        bake_anim_step=1.0,
        bake_anim_simplify_factor=0.0,
        add_leaf_bones=False,
        mesh_smooth_type='FACE',
        object_types=object_types,
        # build_mesh's materials reference textures by their original on-disk path -- COPY +
        # embed_textures bakes the actual image bytes into the FBX itself, so the file stays
        # self-contained after being uploaded to a remote service (Mixamo) that has no access to
        # this machine's filesystem.
        path_mode='COPY',
        embed_textures=True,
        use_custom_props=True,
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
    mesh_obj = build_mesh(data, args.debug_steps, rotation_deg, args.invert_u, args.invert_v)
    check_cancel_requested(args.cancel_file)
    armature_obj = build_armature(data, args.debug_steps, rotation_deg)
    if armature_obj:
        check_cancel_requested(args.cancel_file)
        verts_data = data["mesh"]["vertices"]
        # ARMATURE_ENVELOPE binding (+ its cleanup passes) always runs first, for every vertex --
        # this is what gives the rest of the mesh (anything WITHOUT stored per-vertex data) normal
        # deformation. Canonical type-42 data, when present on a vertex, then OVERRIDES just that
        # vertex's envelope-derived groups; see
        # apply_stored_vertex_weights_override's own docstring for why this two-pass order replaced
        # the previous mesh-wide either/or (which zeroed the rest of the mesh whenever only a prop
        # bone had real weights).
        bind_mesh_to_armature(mesh_obj, armature_obj, args.debug_steps)
        if any(v.get("boneNames") for v in verts_data):
            debug_print(args.debug_steps, "overriding with real/rigid-bound stored per-vertex weights")
            apply_stored_vertex_weights_override(mesh_obj, verts_data, args.debug_steps)
    else:
        debug_print(args.debug_steps, "no bones in input -- exporting mesh-only FBX")

    check_cancel_requested(args.cancel_file)
    animation_count = build_animation_actions(data, armature_obj, rotation_deg, args.debug_steps)
    check_cancel_requested(args.cancel_file)
    out_path = os.path.abspath(args.output)
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    prepare_and_export(mesh_obj, armature_obj, out_path, animation_count, args.debug_steps)

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
