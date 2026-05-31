#!/usr/bin/env python3
"""
Blender exporter for mini-mbm Mesh Debug import pipeline.

Writes a Lua intermediate file with baked mesh frames:
  - frames[i].subsets[j].vertices: [{x,y,z,u,v,nx,ny,nz}, ...]
  - frames[i].subsets[j].indices:  [1-based triangle indices]
  - frames[i].subsets[j].texture:  optional image filepath
  - animations: optional baked clip metadata
"""

from __future__ import annotations

import argparse
import os
import sys
from typing import Any


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--bake-animation", action="store_true")
    parser.add_argument("--frame-start", type=int, default=1)
    parser.add_argument("--frame-end", type=int, default=1)
    parser.add_argument("--sample-step", type=int, default=1)
    return parser.parse_args(argv)


def lua_quote(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def as_lua(value: Any, indent: int = 0) -> str:
    pad = " " * indent
    nxt = " " * (indent + 2)

    if value is None:
        return "nil"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return f"{value:.9g}"
    if isinstance(value, str):
        return f'"{lua_quote(value)}"'
    if isinstance(value, list):
        if not value:
            return "{}"
        lines = ["{"]
        for item in value:
            lines.append(f"{nxt}{as_lua(item, indent + 2)},")
        lines.append(f"{pad}}}")
        return "\n".join(lines)
    if isinstance(value, dict):
        if not value:
            return "{}"
        lines = ["{"]
        for key, item in value.items():
            if key.isidentifier():
                key_lua = key
            else:
                key_lua = f'["{lua_quote(key)}"]'
            lines.append(f"{nxt}{key_lua} = {as_lua(item, indent + 2)},")
        lines.append(f"{pad}}}")
        return "\n".join(lines)
    return "nil"


def get_first_texture_path(material: Any) -> str:
    if material is None or not getattr(material, "use_nodes", False):
        return ""
    ntree = material.node_tree
    if ntree is None:
        return ""
    for node in ntree.nodes:
        if node.type == "TEX_IMAGE" and getattr(node, "image", None) is not None:
            try:
                return bpy.path.abspath(node.image.filepath)
            except Exception:
                return str(node.image.filepath or "")
    return ""


def round6(value: float) -> float:
    return round(float(value), 6)


def export_frame_subsets(scene: Any) -> list[dict[str, Any]]:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    subsets_out: list[dict[str, Any]] = []

    mesh_objects = [o for o in scene.objects if o.type == "MESH" and o.visible_get()]
    mesh_objects.sort(key=lambda o: o.name)

    for obj in mesh_objects:
        eval_obj = obj.evaluated_get(depsgraph)
        mesh = eval_obj.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
        if mesh is None:
            continue
        try:
            if len(mesh.vertices) == 0:
                continue
            mesh.calc_normals_split()
            mesh.calc_loop_triangles()

            uv_data = None
            if mesh.uv_layers and mesh.uv_layers.active:
                uv_data = mesh.uv_layers.active.data

            buckets: dict[int, dict[str, Any]] = {}

            for tri in mesh.loop_triangles:
                mat_idx = int(tri.material_index)
                if mat_idx not in buckets:
                    mat = mesh.materials[mat_idx] if mat_idx < len(mesh.materials) else None
                    mat_name = mat.name if mat else f"Material_{mat_idx}"
                    buckets[mat_idx] = {
                        "name": f"{obj.name}:{mat_name}",
                        "texture": get_first_texture_path(mat),
                        "vertices": [],
                        "indices": [],
                        "_vmap": {},
                    }

                bucket = buckets[mat_idx]
                vmap = bucket["_vmap"]

                for loop_index in tri.loops:
                    loop = mesh.loops[loop_index]
                    vert = mesh.vertices[loop.vertex_index]

                    world_pos = eval_obj.matrix_world @ vert.co
                    world_no = (eval_obj.matrix_world.to_3x3() @ loop.normal).normalized()

                    if uv_data is not None:
                        uv = uv_data[loop_index].uv
                        u = float(uv.x)
                        v = float(uv.y)
                    else:
                        u = 0.0
                        v = 0.0

                    key = (
                        round6(world_pos.x),
                        round6(world_pos.y),
                        round6(world_pos.z),
                        round6(u),
                        round6(v),
                        round6(world_no.x),
                        round6(world_no.y),
                        round6(world_no.z),
                    )

                    idx = vmap.get(key)
                    if idx is None:
                        idx = len(bucket["vertices"]) + 1
                        vmap[key] = idx
                        bucket["vertices"].append(
                            {
                                "x": float(world_pos.x),
                                "y": float(world_pos.y),
                                "z": float(world_pos.z),
                                "u": u,
                                "v": v,
                                "nx": float(world_no.x),
                                "ny": float(world_no.y),
                                "nz": float(world_no.z),
                            }
                        )

                    bucket["indices"].append(idx)

            for mat_idx in sorted(buckets.keys()):
                bucket = buckets[mat_idx]
                if bucket["vertices"] and bucket["indices"]:
                    bucket.pop("_vmap", None)
                    subsets_out.append(bucket)
        finally:
            eval_obj.to_mesh_clear()

    return subsets_out


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def import_source(input_path: str) -> None:
    ext = os.path.splitext(input_path)[1].lower()
    if ext == ".blend":
        # When launched as "blender -b file.blend" this is already loaded.
        return

    clear_scene()

    if ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=input_path)
        return
    if ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=input_path)
        return
    if ext == ".obj":
        if hasattr(bpy.ops.wm, "obj_import"):
            bpy.ops.wm.obj_import(filepath=input_path)
        else:
            bpy.ops.import_scene.obj(filepath=input_path)
        return

    raise RuntimeError(f"Unsupported source extension: {ext}")


def build_data(args: argparse.Namespace) -> dict[str, Any]:
    source_path = os.path.abspath(args.input)
    import_source(source_path)
    scene = bpy.context.scene

    frame_start = max(1, int(args.frame_start))
    frame_end = max(1, int(args.frame_end))
    step = max(1, int(args.sample_step))
    if frame_end < frame_start:
        frame_start, frame_end = frame_end, frame_start

    source_file = source_path

    frames_out: list[dict[str, Any]] = []
    if args.bake_animation:
        for frame in range(frame_start, frame_end + 1, step):
            scene.frame_set(frame)
            frames_out.append({"frame": frame, "subsets": export_frame_subsets(scene)})
    else:
        frame = int(scene.frame_current)
        frames_out.append({"frame": frame, "subsets": export_frame_subsets(scene)})
        frame_start = frame
        frame_end = frame

    fps_base = float(scene.render.fps_base) if scene.render.fps_base else 1.0
    fps = float(scene.render.fps) / fps_base if fps_base > 0 else 24.0
    if fps <= 0:
        fps = 24.0

    animations: list[dict[str, Any]] = []
    if args.bake_animation and len(frames_out) > 1:
        animations.append(
            {
                "name": "Bake",
                "initialFrame": 1,
                "finalFrame": len(frames_out),
                "timeBetweenFrame": 1.0 / fps,
                "typeAnimation": 1,
            }
        )

    return {
        "version": 2,
        "source": source_file,
        "bakeAnimation": bool(args.bake_animation),
        "frameStart": frame_start,
        "frameEnd": frame_end,
        "sampleStep": step,
        "frames": frames_out,
        "animations": animations,
    }


def main() -> int:
    argv = []
    if "--" in sys.argv:
        argv = sys.argv[sys.argv.index("--") + 1 :]
    args = parse_args(argv)

    out_path = os.path.abspath(args.output)
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    data = build_data(args)
    with open(out_path, "w", encoding="utf-8") as fp:
        fp.write("return ")
        fp.write(as_lua(data, 0))
        fp.write("\n")

    return 0


if __name__ == "__main__":
    try:
        import bpy  # type: ignore
    except Exception as exc:
        sys.stderr.write(f"Failed to import bpy: {exc}\n")
        sys.exit(2)

    sys.exit(main())
