#!/usr/bin/env python3
"""
Blender headless builder for mini-mbm's Mannequin Editor (Marco 3).

Reads the JSON handoff file produced by editor/mannequin_editor.lua (joint
hierarchy + triangle-soup mesh + per-vertex UV + owning-joint), builds a real
Blender armature and skinned mesh from scratch (nothing here reads/imports an
existing .blend scene), centers/cleans the result and exports an FBX ready to
upload to an auto-rigging service (Mixamo or similar).

Invoked headlessly, same convention as blender_mesh_export.py:
    blender -b --factory-startup --python blender_mannequin_build.py -- \
        --input mannequin.json --output character.fbx [--cancel-file F] [--debug-steps]
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import traceback

# Exported character height, in Blender's default meter units -- the JSON's own
# coordinates are in raw source-image pixels (hundreds to low thousands), which
# would otherwise import as an absurdly large "character", confusing Mixamo's
# auto-rigger and any FBX importer that assumes human-scale input.
TARGET_HEIGHT_M = 1.7


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--cancel-file", default="")
    parser.add_argument("--debug-steps", action="store_true")
    return parser.parse_args(argv)


def debug_print(enabled: bool, message: str) -> None:
    if enabled:
        print(f"[mannequin_build] {message}", flush=True)


def check_cancel_requested(cancel_file: str) -> None:
    if cancel_file and os.path.exists(cancel_file):
        raise RuntimeError("Canceled by user.")


def load_json(input_path: str) -> dict:
    with open(input_path, "r", encoding="utf-8") as f:
        return json.load(f)


def build_armature(data: dict, debug: bool):
    import bpy

    joints = {j["name"]: j for j in data["joints"]}
    root_name = next(name for name, j in joints.items() if j.get("parent") is None)

    arm_data = bpy.data.armatures.new("MannequinArmature")
    arm_obj = bpy.data.objects.new("MannequinArmature", arm_data)
    bpy.context.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode='EDIT')

    def joint_pos(name):
        # The JSON is Y-up (mannequin_editor.lua's own convention -- see
        # docs/mannequin-editor-plan.md) but Blender's native space is Z-up.
        # Blender doesn't know or care what our axes "mean" -- it just treats
        # whatever (x,y,z) we hand it as (right, depth, up). Swapping y/z here
        # is what actually reorients the character upright inside Blender;
        # the FBX exporter's own axis settings only govern the OUTPUT file's
        # convention, they don't fix how geometry sits in Blender's own scene.
        j = joints[name]
        return (j["x"], j["z"], j["y"])

    def dist(a, b):
        return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5

    children_by_parent: dict[str, list[str]] = {}
    for name, j in joints.items():
        parent = j.get("parent")
        if parent:
            children_by_parent.setdefault(parent, []).append(name)

    edit_bones = arm_data.edit_bones
    created = {}

    # The root joint is a point, not a segment -- give it a short stub bone (most
    # rigging pipelines expect a root/hips bone to exist) oriented toward its
    # first child if one exists, so it at least points somewhere sensible.
    root_pos = joint_pos(root_name)
    root_children = children_by_parent.get(root_name, [])
    if root_children:
        stub_len = max(1.0, dist(root_pos, joint_pos(root_children[0])))
        stub_dir_target = joint_pos(root_children[0])
        dx, dy, dz = (stub_dir_target[i] - root_pos[i] for i in range(3))
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

    # BFS from the root so every bone's parent already exists in `created` when
    # it's the child's turn. Each non-root bone spans from its PARENT joint's
    # position (head) to its OWN joint's position (tail) -- i.e. bone "lElbow"
    # is the segment shoulder->elbow, matching the owner convention the JSON's
    # vertices already use. use_connect is deliberately left False throughout:
    # it's a Blender edit-mode UX convenience only (auto-syncing a child's head
    # to its parent's tail), irrelevant to a script that sets exact positions
    # once and exports immediately -- using it would additionally require the
    # root's stub tail to exactly match every direct child's head, which it
    # doesn't (the stub only points toward ONE child).
    queue = [root_name]
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
    debug_print(debug, f"armature built: {len(created)} bones (root={root_name})")
    return arm_obj, root_name


def build_mesh(data: dict, armature_obj, debug: bool):
    import bpy

    verts_data = data["mesh"]["vertices"]
    subsets = data["mesh"]["subsets"]

    # Same Y-up (JSON) -> Z-up (Blender) swap as build_armature's joint_pos.
    positions = [(v["x"], v["z"], v["y"]) for v in verts_data]
    # JSON indices are 1-based (written straight from Lua array indices) --
    # from_pydata expects 0-based.
    faces = []
    for subset in subsets:
        idx = subset["indices"]
        for i in range(0, len(idx), 3):
            faces.append((idx[i] - 1, idx[i + 1] - 1, idx[i + 2] - 1))

    mesh_data = bpy.data.meshes.new("MannequinMesh")
    mesh_data.from_pydata(positions, [], faces)
    mesh_data.update()

    uv_layer = mesh_data.uv_layers.new(name="UVMap")
    for loop in mesh_data.loops:
        v = verts_data[loop.vertex_index]
        uv_layer.data[loop.index].uv = (v["u"], v["v"])

    mesh_obj = bpy.data.objects.new("MannequinMesh", mesh_data)
    bpy.context.collection.objects.link(mesh_obj)

    # One vertex group per owning joint, weight 1.0 -- exact, since the JSON
    # already records precisely which joint generated each vertex, no need for
    # Blender's automatic (heuristic, distance-based) weight painting.
    groups = {}
    for i, v in enumerate(verts_data):
        owner = v["owner"]
        if owner not in groups:
            groups[owner] = mesh_obj.vertex_groups.new(name=owner)
        groups[owner].add([i], 1.0, 'REPLACE')

    mesh_obj.parent = armature_obj
    mod = mesh_obj.modifiers.new(name="Armature", type='ARMATURE')
    mod.object = armature_obj

    debug_print(debug, f"mesh built: vertices={len(positions)} faces={len(faces)} vertex_groups={len(groups)}")
    return mesh_obj


def normalize_scale(armature_obj, mesh_obj, data: dict, debug: bool):
    import bpy

    ys = [j["y"] for j in data["joints"]]
    height = max(ys) - min(ys)
    if height <= 1e-6:
        return
    scale = TARGET_HEIGHT_M / height

    bpy.ops.object.select_all(action='DESELECT')
    armature_obj.select_set(True)
    armature_obj.scale = (scale, scale, scale)
    bpy.context.view_layer.objects.active = armature_obj
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    debug_print(debug, f"scaled by {scale:.5f} (raw height {height:.1f} -> {TARGET_HEIGHT_M}m)")


def prepare_and_export(armature_obj, mesh_obj, output_path: str, debug: bool):
    import bpy
    from mathutils import Vector

    # Same cleanup + same Z-up convention as blender_body_adjust.py's
    # MIXAMO_OT_prepare_and_export (build_armature/build_mesh already swapped
    # the JSON's Y-up into Blender's native Z-up, so "up" here is genuinely Z):
    # center X/Y, feet at Z=0.
    world_corners = [mesh_obj.matrix_world @ Vector(c) for c in mesh_obj.bound_box]
    min_x = min(c.x for c in world_corners)
    max_x = max(c.x for c in world_corners)
    min_y = min(c.y for c in world_corners)
    max_y = max(c.y for c in world_corners)
    min_z = min(c.z for c in world_corners)
    delta = Vector(((min_x + max_x) / -2.0, (min_y + max_y) / -2.0, -min_z))
    armature_obj.location += delta

    bpy.ops.object.select_all(action='DESELECT')
    armature_obj.select_set(True)
    mesh_obj.select_set(True)
    bpy.context.view_layer.objects.active = armature_obj

    # No explicit axis_forward/axis_up -- rely on the FBX exporter's own
    # defaults, same as blender_body_adjust.py's already-proven export call.
    bpy.ops.export_scene.fbx(
        filepath=output_path,
        use_selection=True,
        bake_anim=False,
        add_leaf_bones=False,
        mesh_smooth_type='FACE',
        object_types={'ARMATURE', 'MESH'},
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
    if not data.get("joints"):
        raise RuntimeError("Input JSON has no joints -- mark at least one point before exporting.")
    if not data.get("mesh", {}).get("vertices"):
        raise RuntimeError("Input JSON has no mesh vertices -- nothing to build.")

    check_cancel_requested(args.cancel_file)
    armature_obj, root_name = build_armature(data, args.debug_steps)
    check_cancel_requested(args.cancel_file)
    mesh_obj = build_mesh(data, armature_obj, args.debug_steps)
    check_cancel_requested(args.cancel_file)
    normalize_scale(armature_obj, mesh_obj, data, args.debug_steps)

    check_cancel_requested(args.cancel_file)
    out_path = os.path.abspath(args.output)
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    prepare_and_export(armature_obj, mesh_obj, out_path, args.debug_steps)

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
            print(f"[mannequin_build] ERROR: {exc}", file=sys.stderr, flush=True)
            traceback.print_exc()
        else:
            sys.stderr.write(f"Exporter failed: {exc}\n")
        sys.exit(1)
