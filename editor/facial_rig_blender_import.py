#!/usr/bin/env python3
"""Import a mini-mbm facial rig bridge into a Blender project.

The bridge is intentionally a small Lua assignment document because the editor already writes
portable Lua projects. This importer parses only that generated assignment subset; it never
executes the bridge file. The first importer milestone creates a read-only armature preview and
stores the role/mask report as armature metadata. It does not bind or modify mesh vertices.

Example (keep an existing mesh in the opened .blend):
    blender -b megan.blend --python facial_rig_blender_import.py -- \
        --bridge facial-rig-meg-blender-bridge.lua \
        --output megan-facial-rig-preview.blend --debug
"""

from __future__ import annotations

import argparse
import ast
import json
import re
import sys
from pathlib import Path
from typing import Any


SCHEMA = "mini-mbm-facial-rig-bridge-1"
ASSIGNMENT = re.compile(r'^facialRigBridge((?:\["(?:\\.|[^"\\])*"\]|\[\d+\])+?)\s*=\s*(.+)$')
PATH_PART = re.compile(r'\["((?:\\.|[^"\\])*)"\]|\[(\d+)\]')


def parse_literal(text: str) -> Any:
    text = text.strip()
    if text == "{}":
        return {}
    if text == "true":
        return True
    if text == "false":
        return False
    if text == "nil":
        return None
    if text.startswith('"'):
        return ast.literal_eval(text)
    if text.startswith("facialRigBridge"):
        return ("__bridge_ref__", parse_path(text[len("facialRigBridge"):]))
    # tUtil.save uses %a, which is accepted by Python's float.fromhex().
    return float.fromhex(text)


def parse_path(path_text: str) -> list[str | int]:
    parts: list[str | int] = []
    cursor = 0
    for match in PATH_PART.finditer(path_text):
        if match.start() != cursor:
            raise ValueError(f"invalid bridge path: {path_text}")
        parts.append(ast.literal_eval('"' + match.group(1) + '"') if match.group(1) is not None
                     else int(match.group(2)))
        cursor = match.end()
    if cursor != len(path_text) or not parts:
        raise ValueError(f"invalid bridge path: {path_text}")
    return parts


def parse_bridge(path: str | Path) -> dict[str, Any]:
    root: dict[str, Any] = {}
    with open(path, "r", encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            line = line.strip()
            if not line or line.startswith("--") or line == "return facialRigBridge":
                continue
            if line in {"local facialRigBridge = {}", "facialRigBridge = {}"}:
                continue
            match = ASSIGNMENT.match(line)
            if not match:
                raise ValueError(f"unsupported bridge syntax at line {line_number}")
            parts = parse_path(match.group(1))
            value = parse_literal(match.group(2))
            current: dict[Any, Any] = root
            for part in parts[:-1]:
                child = current.get(part)
                if not isinstance(child, dict):
                    child = {}
                    current[part] = child
                current = child
            current[parts[-1]] = value
    if root.get("schema") != SCHEMA:
        raise ValueError(f"unsupported bridge schema: {root.get('schema')!r}")
    return normalize_arrays(resolve_references(root, root))


def resolve_references(value: Any, root: dict[str, Any]) -> Any:
    if isinstance(value, tuple) and len(value) == 2 and value[0] == "__bridge_ref__":
        current: Any = root
        for part in value[1]:
            current = current[part]
        return resolve_references(current, root)
    if isinstance(value, dict):
        return {key: resolve_references(item, root) for key, item in value.items()}
    return value


def normalize_arrays(value: Any) -> Any:
    if isinstance(value, dict):
        normalized = {key: normalize_arrays(item) for key, item in value.items()}
        numeric = [key for key in normalized if isinstance(key, int) and key > 0]
        if numeric and len(numeric) == len(normalized) and sorted(numeric) == list(range(1, len(numeric) + 1)):
            return [normalized[index] for index in range(1, len(numeric) + 1)]
        return normalized
    if isinstance(value, list):
        return [normalize_arrays(item) for item in value]
    return value


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bridge", required=True, help="facial-rig-*-blender-bridge.lua")
    parser.add_argument("--output", required=True, help="output .blend path")
    parser.add_argument("--angle-x", type=float, default=90.0,
                        help="Y-up engine to Blender Z-up rotation, default: 90")
    parser.add_argument("--angle-y", type=float, default=0.0)
    parser.add_argument("--angle-z", type=float, default=0.0)
    parser.add_argument("--replace", action="store_true",
                        help="remove previous FacialRigPreview armatures before import")
    parser.add_argument("--debug", action="store_true")
    return parser.parse_args(argv)


def debug(enabled: bool, message: str) -> None:
    if enabled:
        print(f"[facial_rig_bridge] {message}", flush=True)


def rotate_point(point: dict[str, Any], angles: tuple[float, float, float]) -> tuple[float, float, float]:
    import math
    x, y, z = float(point.get("x", 0.0)), float(point.get("y", 0.0)), float(point.get("z", 0.0))
    ax, ay, az = (math.radians(value) for value in angles)
    if ax:
        c, s = math.cos(ax), math.sin(ax)
        y, z = y * c - z * s, y * s + z * c
    if ay:
        c, s = math.cos(ay), math.sin(ay)
        x, z = x * c + z * s, -x * s + z * c
    if az:
        c, s = math.cos(az), math.sin(az)
        x, y = x * c - y * s, x * s + y * c
    return x, y, z


def remove_previous_previews() -> None:
    import bpy
    for obj in list(bpy.data.objects):
        if obj.name.startswith("FacialRigPreview"):
            bpy.data.objects.remove(obj, do_unlink=True)
    for armature in list(bpy.data.armatures):
        if armature.name.startswith("FacialRigPreview") and armature.users == 0:
            bpy.data.armatures.remove(armature)


def create_armature(data: dict[str, Any], angles: tuple[float, float, float], replace: bool,
                    debug_enabled: bool):
    import bpy
    from mathutils import Vector

    rig = data.get("rig") or {}
    bones = rig.get("bones") or []
    if not bones:
        raise ValueError("bridge contains no generated bones")
    if replace:
        remove_previous_previews()
    arm_data = bpy.data.armatures.new("FacialRigPreview")
    arm_obj = bpy.data.objects.new("FacialRigPreview", arm_data)
    bpy.context.collection.objects.link(arm_obj)
    arm_obj.show_in_front = True
    arm_data.display_type = "STICK"
    arm_obj["mini_mbm_bridge_schema"] = data["schema"]
    arm_obj["mini_mbm_coordinate_system"] = data.get("coordinateSystem", "")
    arm_obj["mini_mbm_mesh_path"] = data.get("meshPath", "")
    arm_obj["mini_mbm_topology_signature"] = data.get("meshTopologySignature", "")
    arm_obj["mini_mbm_vertex_count"] = int(data.get("vertexCount") or 0)
    arm_obj["mini_mbm_role_report"] = json.dumps(data.get("report") or {}, sort_keys=True)
    arm_obj["mini_mbm_binding_report"] = json.dumps(data.get("binding") or {}, sort_keys=True)

    bpy.context.view_layer.objects.active = arm_obj
    arm_obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    created = {}
    try:
        for entry in bones:
            bone_id = str(entry.get("id") or entry.get("name") or "FacialBone")
            edit_bone = arm_data.edit_bones.new(bone_id)
            head = rotate_point(entry.get("head") or {}, angles)
            tail = rotate_point(entry.get("tail") or {}, angles)
            head_vector, tail_vector = Vector(head), Vector(tail)
            if (tail_vector - head_vector).length < 1.0e-5:
                tail_vector = head_vector + Vector((0.0, 0.0, 0.01))
            edit_bone.head, edit_bone.tail = head_vector, tail_vector
            edit_bone["mini_mbm_kind"] = str(entry.get("kind") or "facial")
            edit_bone["mini_mbm_landmarks"] = json.dumps(entry.get("landmarks") or [])
            created[bone_id] = edit_bone
        for entry in bones:
            bone_id = str(entry.get("id") or entry.get("name") or "FacialBone")
            parent_id = entry.get("parent")
            if parent_id and bone_id in created and str(parent_id) in created:
                created[bone_id].parent = created[str(parent_id)]
    finally:
        bpy.ops.object.mode_set(mode="OBJECT")
    debug(debug_enabled, f"created armature {arm_obj.name}: {len(created)} bones")
    return arm_obj, len(created)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    data = parse_bridge(args.bridge)
    angles = (args.angle_x, args.angle_y, args.angle_z)
    armature, count = create_armature(data, angles, args.replace, args.debug)
    import bpy
    bpy.context.view_layer.objects.active = armature
    armature.select_set(True)
    output = str(Path(args.output).expanduser().resolve())
    bpy.ops.wm.save_as_mainfile(filepath=output)
    print(f"Facial rig bridge imported: {count} bones -> {output}", flush=True)
    return 0


if __name__ == "__main__":
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    try:
        raise SystemExit(main(arguments))
    except Exception as error:  # Blender must report a useful headless failure.
        print(f"[facial_rig_bridge] ERROR: {error}", file=sys.stderr, flush=True)
        raise
