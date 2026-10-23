#!/usr/bin/env python3
# ----------------------------------------------------------------------------------------------------------------------|
# MIT License (MIT)                                                                                                     |
# Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                            |
#                                                                                                                       |
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated          |
# documentation files (the "Software"), to deal in the Software without restriction, including without limitation      |
# the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to   |
# permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
#                                                                                                                       |
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the   |
# Software.                                                                                                             |
#                                                                                                                       |
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE |
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR|
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.      |
#                                                                                                                       |
# ----------------------------------------------------------------------------------------------------------------------|

"""Headless regression for Mesh Debug -> Blender facial curve interchange.

Run with Blender, for example:

    blender -b --factory-startup --python blender_facial_curve_roundtrip_test.py -- \
        --output-dir /tmp/mini-mbm-facial-curve-roundtrip
"""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import sys

import bpy


ROOT = Path(__file__).resolve().parents[2]
EXPORTER_PATH = ROOT / "editor" / "blender_mesh_skeleton_export.py"
SPEC = importlib.util.spec_from_file_location("blender_mesh_skeleton_export", EXPORTER_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Could not load {EXPORTER_PATH}")
EXPORTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORTER)
IMPORTER_PATH = ROOT / "editor" / "blender_mesh_export.py"
IMPORTER_SPEC = importlib.util.spec_from_file_location("blender_mesh_export", IMPORTER_PATH)
if IMPORTER_SPEC is None or IMPORTER_SPEC.loader is None:
    raise RuntimeError(f"Could not load {IMPORTER_PATH}")
IMPORTER = importlib.util.module_from_spec(IMPORTER_SPEC)
IMPORTER_SPEC.loader.exec_module(IMPORTER)


def payload() -> dict:
    vertices = [
        {"x": -1.0, "y": -1.0, "z": 0.0, "u": 0.0, "v": 0.0},
        {"x": 0.0, "y": 1.0, "z": 0.0, "u": 0.5, "v": 1.0},
        {"x": 1.0, "y": -1.0, "z": 0.0, "u": 1.0, "v": 0.0},
    ]
    zero_normals = [[0.0, 0.0, 0.0] for _ in vertices]
    return {
        "canonicalSkeleton": False,
        "mesh": {
            "vertices": vertices,
            "subsets": [{"name": "Face", "texture": "", "indices": [1, 2, 3]}],
        },
        "facial": {
            "meshId": "101",
            "geometrySignature": "202",
            "vertexCount": len(vertices),
            "sampleRate": 24,
            "targets": [
                {"id": "11", "name": "jawOpen", "convention": "ARKit", "defaultWeight": 0.0,
                 "positionDeltas": [[0.0, -0.2, 0.0], [0.0, 0.3, 0.0], [0.0, -0.2, 0.0]],
                 "normalDeltas": zero_normals},
                {"id": "12", "name": "mouthSmile", "convention": "ARKit", "defaultWeight": 0.1,
                 "positionDeltas": [[0.2, 0.0, 0.0], [0.0, 0.0, 0.0], [-0.2, 0.0, 0.0]],
                 "normalDeltas": zero_normals},
            ],
            "clips": [
                {"id": 301, "name": "FaceIdle", "duration": 1.0, "loop": True, "interpolation": 1,
                 "keys": [{"time": 0.0, "weights": [0.0, 0.1]},
                          {"time": 0.5, "weights": [0.4, 0.2]},
                          {"time": 1.0, "weights": [0.0, 0.1]}]},
                {"id": 302, "name": "FaceBlink", "duration": 0.25, "loop": False, "interpolation": 0,
                 "keys": [{"time": 0.0, "weights": [0.0, 0.0]},
                          {"time": 0.125, "weights": [1.0, 0.0]},
                          {"time": 0.25, "weights": [0.0, 0.0]}]},
            ],
        },
    }


def action_curves(action):
    if hasattr(action, "fcurves"):
        return list(action.fcurves)
    if not action.layers or not action.slots:
        return []
    strip = action.layers[0].strips[0]
    channel_bag = strip.channelbag(action.slots[0], ensure=False)
    return list(channel_bag.fcurves) if channel_bag else []


def verify(path: Path) -> None:
    bpy.ops.wm.open_mainfile(filepath=str(path))
    objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    if len(objects) != 1:
        raise AssertionError(f"expected one mesh object, got {len(objects)}")
    shape_keys = objects[0].data.shape_keys
    names = [key.name for key in shape_keys.key_blocks]
    if names != ["Basis", "jawOpen", "mouthSmile"]:
        raise AssertionError(f"shape keys changed: {names}")
    if abs(shape_keys.key_blocks["jawOpen"].data[1].co.y - 1.3) > 1e-6:
        raise AssertionError("jawOpen geometry changed")
    if objects[0].data["mini_mbm_facial_target_ids"] != '["11", "12"]':
        raise AssertionError("facial target metadata changed")

    actions = {action.name: action for action in bpy.data.actions}
    if set(actions) != {"FaceIdle", "FaceBlink"}:
        raise AssertionError(f"facial actions changed: {sorted(actions)}")
    for name, expected_interpolation, expected_points in (
        ("FaceIdle", "LINEAR", [(1.0, 0.0), (13.0, 0.4), (25.0, 0.0)]),
        ("FaceBlink", "CONSTANT", [(1.0, 0.0), (4.0, 1.0), (7.0, 0.0)]),
    ):
        action = actions[name]
        curves = {curve.data_path: curve for curve in action_curves(action)}
        expected_paths = {'key_blocks["jawOpen"].value', 'key_blocks["mouthSmile"].value'}
        if set(curves) != expected_paths:
            raise AssertionError(f"{name} target curves changed: {sorted(curves)}")
        jaw_curve = curves['key_blocks["jawOpen"].value']
        points = [(round(point.co.x, 6), round(point.co.y, 6)) for point in jaw_curve.keyframe_points]
        if points != expected_points:
            raise AssertionError(f"{name} keyframes changed: {points}")
        if any(point.interpolation != expected_interpolation for point in jaw_curve.keyframe_points):
            raise AssertionError(f"{name} interpolation changed")


def verify_fbx(path: Path) -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=False)
    args = type("FacialArgs", (), {"include_facial": True, "debug_steps": False,
                                    "facial_normal_deltas": False})()
    source = IMPORTER.discover_facial_source(bpy.context.scene, args)
    if source is None or len(source.get("clipPayload", [])) != 2:
        raise AssertionError("FBX facial clip metadata was not restored")
    specs = IMPORTER.facial_clip_specs(source, [], bpy.context.scene)
    clips = IMPORTER.sample_facial_clips(bpy.context.scene, source, specs, 24.0, args)
    if len(clips) != 2 or clips[0]["id"] != 301 or clips[0]["keys"][1]["weights"] != [0.4, 0.2]:
        raise AssertionError("FBX facial weights were not restored")
    if clips[1]["id"] != 302 or clips[1]["interpolation"] != "step" or clips[1]["keys"][1]["weights"] != [1.0, 0.0]:
        raise AssertionError("FBX facial step clip was not restored")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / "facial-curve-roundtrip.blend"
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    data = payload()
    mesh_obj = EXPORTER.build_mesh(data, False)
    target_names, target_count = EXPORTER.build_facial_shape_keys(data, mesh_obj, None, False)
    action_count = EXPORTER.build_facial_animation_actions(data, mesh_obj, target_names, False)
    if target_count != 2 or action_count != 2:
        raise AssertionError(f"unexpected build counts: targets={target_count} actions={action_count}")
    bpy.ops.wm.save_as_mainfile(filepath=str(output))
    verify(output)
    fbx_output = output_dir / "facial-curve-roundtrip.fbx"
    mesh_obj = next(obj for obj in bpy.data.objects if obj.type == "MESH")
    EXPORTER.prepare_and_export(mesh_obj, None, str(fbx_output), action_count, False)
    verify_fbx(fbx_output)
    print("BLENDER FACIAL CURVE ROUNDTRIP PASS", json.dumps({"targets": target_count,
          "actions": action_count, "fbx": str(fbx_output), "output": str(output)}), flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"BLENDER FACIAL CURVE ROUNDTRIP FAIL: {exc}", file=sys.stderr, flush=True)
        sys.exit(1)
