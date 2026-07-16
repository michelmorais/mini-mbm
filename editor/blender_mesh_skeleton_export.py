#!/usr/bin/env python3
"""
Blender headless exporter for mini-mbm's Mesh Debug tool: takes a JSON dump of a loaded mesh's raw
geometry (and, optionally, its editor-authored/imported bone hierarchy) and produces a real FBX
with a skinned armature, ready for upload to an auto-rigging/animation service (Mixamo or
similar).

This script's input comes from editor/mesh_debug.lua's own already-Blender-native geometry -- no
Y-up/Z-up axis swap needed here, same reasoning as editor/blender_mesh_export.py's import-side
vertex reading (both directions of this round trip use Blender's own coordinate convention
unchanged) -- and has NO per-vertex bone ownership, so this script uses Blender's built-in
automatic (heat-map) weight painting instead of exact vertex groups.

Invoked headlessly:
    blender -b --factory-startup --python blender_mesh_skeleton_export.py -- \
        --input mesh_dump.json --output character.fbx [--cancel-file F] [--debug-steps]
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import traceback


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--cancel-file", default="")
    parser.add_argument("--debug-steps", action="store_true")
    return parser.parse_args(argv)


def debug_print(enabled: bool, message: str) -> None:
    if enabled:
        print(f"[mesh_skeleton_export] {message}", flush=True)


def check_cancel_requested(cancel_file: str) -> None:
    if cancel_file and os.path.exists(cancel_file):
        raise RuntimeError("Canceled by user.")


def load_json(input_path: str) -> dict:
    with open(input_path, "r", encoding="utf-8") as f:
        return json.load(f)


def build_mesh(data: dict, debug: bool):
    import bpy

    verts_data = data["mesh"]["vertices"]
    subsets = data["mesh"]["subsets"]

    # No axis swap -- see module docstring.
    positions = [(v["x"], v["y"], v["z"]) for v in verts_data]
    # JSON indices are 1-based (written straight from Lua array indices); from_pydata expects
    # 0-based, and are already global across the whole vertex list (mesh_debug.lua's dumper
    # offsets each subset's indices when writing, so subset boundaries don't matter here -- there's
    # no per-subset material/vertex-group split to preserve).
    faces = []
    for subset in subsets:
        idx = subset["indices"]
        for i in range(0, len(idx), 3):
            faces.append((idx[i] - 1, idx[i + 1] - 1, idx[i + 2] - 1))

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

    mesh_obj = bpy.data.objects.new("MeshDebugMesh", mesh_data)
    bpy.context.collection.objects.link(mesh_obj)
    debug_print(debug, f"mesh built: vertices={len(positions)} faces={len(faces)}")
    return mesh_obj


def build_armature(data: dict, debug: bool):
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

    def joint_pos(name):
        j = joints[name]
        return (j["x"], j["y"], j["z"])

    def dist(a, b):
        return sum((a[i] - b[i]) ** 2 for i in range(3)) ** 0.5

    children_by_parent: dict[str, list[str]] = {}
    for name, j in joints.items():
        parent = j.get("parent")
        if parent:
            children_by_parent.setdefault(parent, []).append(name)

    edit_bones = arm_data.edit_bones
    created = {}

    # Root joints are a point with no natural direction, so each gets a short stub bone (toward its
    # first child, or straight up if it has none) -- generalized to handle multiple roots, since
    # parse_skeleton_section_v11/JOINT_V11 don't constrain a mesh_debug skeleton to a single
    # always-one-root humanoid hierarchy.
    for root_name in roots:
        root_pos = joint_pos(root_name)
        root_children = children_by_parent.get(root_name, [])
        if root_children:
            stub_len = max(0.01, dist(root_pos, joint_pos(root_children[0])))
            target = joint_pos(root_children[0])
            dx, dy, dz = (target[i] - root_pos[i] for i in range(3))
            dlen = max(1e-6, (dx * dx + dy * dy + dz * dz) ** 0.5)
            stub_tail = (root_pos[0] + dx / dlen * stub_len,
                         root_pos[1] + dy / dlen * stub_len,
                         root_pos[2] + dz / dlen * stub_len)
        else:
            stub_tail = (root_pos[0], root_pos[1] + 1.0, root_pos[2])

        root_bone = edit_bones.new(root_name)
        root_bone.head = root_pos
        root_bone.tail = stub_tail
        created[root_name] = root_bone

    queue = list(roots)
    while queue:
        parent_name = queue.pop(0)
        for child_name in children_by_parent.get(parent_name, []):
            child_bone = edit_bones.new(child_name)
            child_bone.head = joint_pos(parent_name)
            child_bone.tail = joint_pos(child_name)
            child_bone.parent = created[parent_name]
            created[child_name] = child_bone
            queue.append(child_name)

    bpy.ops.object.mode_set(mode='OBJECT')
    debug_print(debug, f"armature built: {len(created)} bones ({len(roots)} root(s))")
    return arm_obj


def bind_mesh_to_armature(mesh_obj, armature_obj, debug: bool) -> None:
    import bpy

    # mesh_debug's bones carry no per-vertex ownership -- automatic (heat-map) weight painting is
    # the only option here. First use of ARMATURE_AUTO in this codebase; the
    # verification step for this milestone includes a manual sanity check (pose-mode bone rotate)
    # rather than trusting this call blindly.
    bpy.ops.object.select_all(action='DESELECT')
    mesh_obj.select_set(True)
    armature_obj.select_set(True)
    bpy.context.view_layer.objects.active = armature_obj
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    debug_print(debug, "bound mesh to armature via automatic (heat-map) weights")


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

    check_cancel_requested(args.cancel_file)
    mesh_obj = build_mesh(data, args.debug_steps)
    check_cancel_requested(args.cancel_file)
    armature_obj = build_armature(data, args.debug_steps)
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
