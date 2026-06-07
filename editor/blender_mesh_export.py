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
import re
import shutil
import sys
import traceback
from typing import Any


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--scan-only", action="store_true")
    parser.add_argument("--stream-output", action="store_true")
    parser.add_argument("--bake-animation", action="store_true")
    parser.add_argument("--use-scene-frame-range", action="store_true")
    parser.add_argument("--frame-start", type=int, default=1)
    parser.add_argument("--frame-end", type=int, default=1)
    parser.add_argument("--sample-step", type=int, default=1)
    parser.add_argument("--animation-name", default="Bake")
    parser.add_argument("--cancel-file", default="")
    parser.add_argument("--debug-steps", action="store_true")
    return parser.parse_args(argv)


def debug_print(enabled: bool, message: str) -> None:
    if enabled:
        print(f"[blender_export] {message}", flush=True)


def check_cancel_requested(cancel_file: str) -> None:
    if cancel_file and os.path.exists(cancel_file):
        raise RuntimeError("Canceled by user.")


def apply_numpy_compat_shim(debug_enabled: bool) -> None:
    """
    Blender 3.4 glTF importer still references np.bool in some builds.
    Newer NumPy removed this alias, so we restore it to keep imports working.
    """
    try:
        import numpy as np  # type: ignore
    except Exception:
        return

    if "bool" not in np.__dict__:
        # Keep behavior compatible with old addon code paths.
        np.bool = bool  # type: ignore[attr-defined]
        debug_print(debug_enabled, "applied numpy compatibility shim: np.bool -> bool")


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


def resolve_image_sequence_path(image: Any, image_user: Any, scene_frame: int) -> str:
    raw_path = getattr(image, "filepath", "") or ""
    if raw_path == "":
        return ""

    frame_start = int(getattr(image_user, "frame_start", 1) or 1)
    frame_duration = int(getattr(image_user, "frame_duration", 0) or 0)
    frame_offset = int(getattr(image_user, "frame_offset", 0) or 0)
    seq_frame = int(scene_frame) - frame_start + 1 + frame_offset

    if frame_duration > 0:
        if getattr(image_user, "use_cyclic", False):
            seq_frame = ((seq_frame - 1) % frame_duration) + 1
        else:
            seq_frame = max(1, min(seq_frame, frame_duration))
    else:
        seq_frame = max(1, seq_frame)

    match = re.match(r"^(.*?)(\d+)(\.[^/\\.]*)$", raw_path)
    if not match:
        return bpy.path.abspath(raw_path)

    prefix, digits, suffix = match.groups()
    seq_path = f"{prefix}{seq_frame:0{len(digits)}d}{suffix}"
    return bpy.path.abspath(seq_path)


def get_first_texture_path(material: Any, scene_frame: int) -> str:
    if material is None or not getattr(material, "use_nodes", False):
        return ""
    ntree = material.node_tree
    if ntree is None:
        return ""
    for node in ntree.nodes:
        if node.type == "TEX_IMAGE" and getattr(node, "image", None) is not None:
            try:
                image = node.image
                if getattr(image, "source", "") == "SEQUENCE":
                    return resolve_image_sequence_path(image, getattr(node, "image_user", None), scene_frame)
                return bpy.path.abspath(node.image.filepath)
            except Exception:
                return str(node.image.filepath or "")
    return ""


def prepare_mesh_normals(mesh: Any) -> None:
    """
    Keep compatibility across Blender versions:
      - Older versions expose calc_normals_split()
      - Newer versions may only expose calc_normals()
    """
    if hasattr(mesh, "calc_normals_split"):
        mesh.calc_normals_split()
    elif hasattr(mesh, "calc_normals"):
        mesh.calc_normals()


def get_loop_normal(loop: Any, vert: Any) -> Any:
    # Prefer per-loop normals when available; fallback to vertex normal.
    loop_normal = getattr(loop, "normal", None)
    if loop_normal is not None:
        return loop_normal
    return vert.normal


def get_scene_fps(scene: Any) -> float:
    fps_base = float(scene.render.fps_base) if scene.render.fps_base else 1.0
    fps = float(scene.render.fps) / fps_base if fps_base > 0 else 24.0
    return fps if fps > 0 else 24.0


def get_material_texture_sequence_info(material: Any) -> list[dict[str, Any]]:
    if material is None or not getattr(material, "use_nodes", False) or material.node_tree is None:
        return []

    out: list[dict[str, Any]] = []
    for node in material.node_tree.nodes:
        if node.type != "TEX_IMAGE" or getattr(node, "image", None) is None:
            continue
        image = node.image
        if getattr(image, "source", "") != "SEQUENCE":
            continue
        image_user = getattr(node, "image_user", None)
        out.append(
            {
                "node": str(getattr(node, "name", "")),
                "path": bpy.path.abspath(getattr(image, "filepath", "") or ""),
                "frameStart": int(getattr(image_user, "frame_start", 1) or 1),
                "frameDuration": int(getattr(image_user, "frame_duration", 0) or 0),
                "frameOffset": int(getattr(image_user, "frame_offset", 0) or 0),
                "autoRefresh": bool(getattr(image_user, "use_auto_refresh", False)),
            }
        )
    return out


def action_frame_range(action: Any) -> tuple[int, int]:
    frame_range = getattr(action, "frame_range", (1, 1))
    return int(frame_range[0]), int(frame_range[1])


def append_unique_source(sources: list[dict[str, Any]], source: dict[str, Any]) -> None:
    key = (
        source.get("kind"),
        source.get("name"),
        source.get("frameStart"),
        source.get("frameEnd"),
        source.get("object"),
    )
    for existing in sources:
        existing_key = (
            existing.get("kind"),
            existing.get("name"),
            existing.get("frameStart"),
            existing.get("frameEnd"),
            existing.get("object"),
        )
        if existing_key == key:
            return
    sources.append(source)


def get_mesh_cache_issues(scene: Any) -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    for obj in scene.objects:
        if obj.type != "MESH":
            continue
        for mod in getattr(obj, "modifiers", []):
            if mod.type != "MESH_SEQUENCE_CACHE":
                continue
            cache_file = getattr(mod, "cache_file", None)
            read_data = set(getattr(mod, "read_data", set()) or set())
            path_count = len(getattr(cache_file, "object_paths", [])) if cache_file else 0
            layer_count = len(getattr(cache_file, "layers", [])) if cache_file else 0
            if cache_file is None:
                issues.append(
                    {
                        "object": str(obj.name),
                        "modifier": str(mod.name),
                        "message": "Mesh Sequence Cache has no cache file.",
                    }
                )
            elif "VERT" in read_data and path_count == 0 and layer_count == 0:
                issues.append(
                    {
                        "object": str(obj.name),
                        "modifier": str(mod.name),
                        "cacheFile": bpy.path.abspath(getattr(cache_file, "filepath", "") or ""),
                        "message": "Mesh Sequence Cache has no loaded Alembic object paths/layers. This Blender build may not support Alembic cache evaluation.",
                    }
                )
    return issues


def build_scan_data(args: argparse.Namespace) -> dict[str, Any]:
    source_path = os.path.abspath(args.input)
    debug_print(args.debug_steps, f"scan source: {source_path}")
    import_source(source_path)
    scene = bpy.context.scene
    fps = get_scene_fps(scene)

    mesh_cache_objects: list[dict[str, Any]] = []
    texture_sequences: list[dict[str, Any]] = []
    animated_objects: list[dict[str, Any]] = []
    nla_sources: list[dict[str, Any]] = []

    for obj in scene.objects:
        object_has_animation = bool(getattr(obj, "animation_data", None) and obj.animation_data.action)
        if object_has_animation:
            action = obj.animation_data.action
            start, end = action_frame_range(action)
            animated_objects.append(
                {
                    "object": str(obj.name),
                    "type": str(obj.type),
                    "action": str(action.name),
                    "frameStart": start,
                    "frameEnd": end,
                }
            )

        animation_data = getattr(obj, "animation_data", None)
        if animation_data is not None:
            for track in getattr(animation_data, "nla_tracks", []):
                for strip in getattr(track, "strips", []):
                    nla_sources.append(
                        {
                            "object": str(obj.name),
                            "track": str(getattr(track, "name", "")),
                            "name": str(getattr(strip, "name", "")),
                            "frameStart": int(getattr(strip, "frame_start", 1)),
                            "frameEnd": int(getattr(strip, "frame_end", 1)),
                        }
                    )

        if obj.type == "MESH":
            for mod in getattr(obj, "modifiers", []):
                if mod.type == "MESH_SEQUENCE_CACHE":
                    cache_file = getattr(mod, "cache_file", None)
                    mesh_cache_objects.append(
                        {
                            "object": str(obj.name),
                            "modifier": str(mod.name),
                            "cacheFile": bpy.path.abspath(getattr(cache_file, "filepath", "") or ""),
                            "objectPath": str(getattr(mod, "object_path", "")),
                        }
                    )
            for mat in obj.data.materials:
                for seq in get_material_texture_sequence_info(mat):
                    seq["object"] = str(obj.name)
                    seq["material"] = str(getattr(mat, "name", ""))
                    texture_sequences.append(seq)

            shape_keys = getattr(obj.data, "shape_keys", None)
            shape_anim = getattr(shape_keys, "animation_data", None) if shape_keys else None
            if shape_anim and shape_anim.action:
                start, end = action_frame_range(shape_anim.action)
                animated_objects.append(
                    {
                        "object": str(obj.name),
                        "type": "SHAPE_KEYS",
                        "action": str(shape_anim.action.name),
                        "frameStart": start,
                        "frameEnd": end,
                    }
                )

    mesh_cache_issues = get_mesh_cache_issues(scene)
    has_mesh_cache_issue = len(mesh_cache_issues) > 0
    sources: list[dict[str, Any]] = []
    has_geometry_animation = bool((mesh_cache_objects and not has_mesh_cache_issue) or animated_objects or nla_sources)
    has_texture_animation = bool(texture_sequences)

    if int(scene.frame_end) > int(scene.frame_start):
        reasons: list[str] = []
        if mesh_cache_objects:
            reasons.append("Mesh Sequence Cache")
        if has_mesh_cache_issue:
            reasons.append("mesh cache not evaluable")
        if texture_sequences:
            reasons.append("image sequence")
        if animated_objects:
            reasons.append("object/action animation")
        if nla_sources:
            reasons.append("NLA strips")
        confidence = "high" if reasons and not has_mesh_cache_issue else "low"
        append_unique_source(
            sources,
            {
                "kind": "scene_range",
                "name": str(mesh_cache_objects[0]["object"]) if len(mesh_cache_objects) == 1 else "Scene range",
                "frameStart": int(scene.frame_start),
                "frameEnd": int(scene.frame_end),
                "fps": fps,
                "hasGeometryAnimation": has_geometry_animation,
                "hasTextureAnimation": has_texture_animation,
                "confidence": confidence,
                "reason": " + ".join(reasons) if reasons else "Scene timeline range is longer than one frame",
            },
        )

    for item in animated_objects:
        append_unique_source(
            sources,
            {
                "kind": "action",
                "name": item["action"],
                "frameStart": item["frameStart"],
                "frameEnd": item["frameEnd"],
                "fps": fps,
                "object": item["object"],
                "confidence": "medium",
                "reason": f"{item['type']} action",
            },
        )

    for item in nla_sources:
        append_unique_source(
            sources,
            {
                "kind": "nla",
                "name": item["name"] or item["track"] or "NLA strip",
                "frameStart": item["frameStart"],
                "frameEnd": item["frameEnd"],
                "fps": fps,
                "object": item["object"],
                "confidence": "medium",
                "reason": "NLA strip",
            },
        )

    sources.sort(key=lambda s: (int(s.get("frameStart", 1)), str(s.get("kind", "")), str(s.get("name", ""))))

    return {
        "version": 1,
        "source": source_path,
        "scene": {
            "frameStart": int(scene.frame_start),
            "frameEnd": int(scene.frame_end),
            "currentFrame": int(scene.frame_current),
            "fps": fps,
        },
        "sources": sources,
        "meshCaches": mesh_cache_objects,
        "textureSequences": texture_sequences,
        "animatedObjects": animated_objects,
        "nlaStrips": nla_sources,
        "meshCacheIssues": mesh_cache_issues,
    }


def export_frame_subsets(scene: Any) -> list[dict[str, Any]]:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    subsets_out: list[dict[str, Any]] = []
    scene_frame = int(scene.frame_current)

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
            prepare_mesh_normals(mesh)
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
                        "texture": get_first_texture_path(mat, scene_frame),
                        "vertices": [],
                        "indices": [],
                        "_vmap": {},
                    }

                bucket = buckets[mat_idx]
                vmap = bucket["_vmap"]

                for loop_index in tri.loops:
                    loop = mesh.loops[loop_index]
                    vert = mesh.vertices[loop.vertex_index]
                    loop_no = get_loop_normal(loop, vert)

                    world_pos = eval_obj.matrix_world @ vert.co
                    world_no = (eval_obj.matrix_world.to_3x3() @ loop_no).normalized()

                    if uv_data is not None:
                        uv = uv_data[loop_index].uv
                        u = float(uv.x)
                        v = float(uv.y)
                    else:
                        u = 0.0
                        v = 0.0

                    # Use a topology-safe key. Avoid float rounding welds because they can
                    # collapse distinct vertices and produce broken triangles.
                    key = (
                        int(loop.vertex_index),
                        float(u),
                        float(v),
                        float(world_no.x),
                        float(world_no.y),
                        float(world_no.z),
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
    debug_print(args.debug_steps, f"import source: {source_path}")
    import_source(source_path)
    scene = bpy.context.scene
    mesh_cache_issues = get_mesh_cache_issues(scene)
    if args.bake_animation and mesh_cache_issues:
        details = "; ".join(issue.get("message", "") for issue in mesh_cache_issues)
        raise RuntimeError(f"Cannot bake mesh-cache animation: {details}")

    if args.bake_animation and args.use_scene_frame_range:
        frame_start = int(scene.frame_start)
        frame_end = int(scene.frame_end)
    else:
        frame_start = int(args.frame_start)
        frame_end = int(args.frame_end)

    frame_start = max(1, frame_start)
    frame_end = max(1, frame_end)
    step = max(1, int(args.sample_step))
    if frame_end < frame_start:
        frame_start, frame_end = frame_end, frame_start

    source_file = source_path

    frames_out: list[dict[str, Any]] = []
    if args.bake_animation:
        debug_print(args.debug_steps, f"bake animation range: {frame_start}..{frame_end} step={step}")
        for frame in range(frame_start, frame_end + 1, step):
            check_cancel_requested(args.cancel_file)
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            debug_print(args.debug_steps, f"export frame: {frame}")
            frames_out.append({"frame": frame, "subsets": export_frame_subsets(scene)})
            check_cancel_requested(args.cancel_file)
    else:
        check_cancel_requested(args.cancel_file)
        frame = frame_start
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        debug_print(args.debug_steps, f"export current frame: {frame}")
        frames_out.append({"frame": frame, "subsets": export_frame_subsets(scene)})
        check_cancel_requested(args.cancel_file)
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
                "name": args.animation_name or "Bake",
                "initialFrame": 1,
                "finalFrame": len(frames_out),
                "timeBetweenFrame": float(step) / fps,
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


def write_lua_atomic(path: str, data: dict[str, Any]) -> None:
    tmp_out = f"{path}.tmp.{os.getpid()}"
    with open(tmp_out, "w", encoding="utf-8") as fp:
        fp.write("return ")
        fp.write(as_lua(data, 0))
        fp.write("\n")
        fp.flush()
        os.fsync(fp.fileno())
    os.replace(tmp_out, path)


def build_stream_output(args: argparse.Namespace, out_dir: str) -> dict[str, Any]:
    source_path = os.path.abspath(args.input)
    debug_print(args.debug_steps, f"import source: {source_path}")
    import_source(source_path)
    scene = bpy.context.scene
    mesh_cache_issues = get_mesh_cache_issues(scene)
    if args.bake_animation and mesh_cache_issues:
        details = "; ".join(issue.get("message", "") for issue in mesh_cache_issues)
        raise RuntimeError(f"Cannot bake mesh-cache animation: {details}")

    if args.bake_animation and args.use_scene_frame_range:
        frame_start = int(scene.frame_start)
        frame_end = int(scene.frame_end)
    else:
        frame_start = int(args.frame_start)
        frame_end = int(args.frame_end)

    frame_start = max(1, frame_start)
    frame_end = max(1, frame_end)
    step = max(1, int(args.sample_step))
    if frame_end < frame_start:
        frame_start, frame_end = frame_end, frame_start

    frames_dir = os.path.join(out_dir, "frames")
    os.makedirs(frames_dir, exist_ok=True)

    frames_manifest: list[dict[str, Any]] = []
    if args.bake_animation:
        debug_print(args.debug_steps, f"bake animation range: {frame_start}..{frame_end} step={step}")
        out_index = 0
        for frame in range(frame_start, frame_end + 1, step):
            check_cancel_requested(args.cancel_file)
            out_index += 1
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            debug_print(args.debug_steps, f"export frame: {frame}")
            rel_path = f"frames/frame_{out_index:06d}.lua"
            frame_data = {"frame": frame, "subsets": export_frame_subsets(scene)}
            write_lua_atomic(os.path.join(out_dir, rel_path), frame_data)
            frames_manifest.append({"sourceFrame": frame, "path": rel_path})
            check_cancel_requested(args.cancel_file)
    else:
        check_cancel_requested(args.cancel_file)
        frame = frame_start
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        debug_print(args.debug_steps, f"export current frame: {frame}")
        rel_path = "frames/frame_000001.lua"
        frame_data = {"frame": frame, "subsets": export_frame_subsets(scene)}
        write_lua_atomic(os.path.join(out_dir, rel_path), frame_data)
        frames_manifest.append({"sourceFrame": frame, "path": rel_path})
        frame_start = frame
        frame_end = frame
        check_cancel_requested(args.cancel_file)

    fps = get_scene_fps(scene)
    animations: list[dict[str, Any]] = []
    if args.bake_animation and len(frames_manifest) > 1:
        animations.append(
            {
                "name": args.animation_name or "Bake",
                "initialFrame": 1,
                "finalFrame": len(frames_manifest),
                "timeBetweenFrame": float(step) / fps,
                "typeAnimation": 1,
            }
        )

    return {
        "version": 3,
        "streamOutput": True,
        "source": source_path,
        "bakeAnimation": bool(args.bake_animation),
        "frameStart": frame_start,
        "frameEnd": frame_end,
        "sampleStep": step,
        "frames": frames_manifest,
        "animations": animations,
    }


def main() -> int:
    argv = []
    if "--" in sys.argv:
        argv = sys.argv[sys.argv.index("--") + 1 :]
    args = parse_args(argv)
    apply_numpy_compat_shim(args.debug_steps)

    out_path = os.path.abspath(args.output)
    debug_print(args.debug_steps, f"output: {out_path}")

    if args.stream_output and not args.scan_only:
        if os.path.isdir(out_path):
            shutil.rmtree(out_path)
        elif os.path.exists(out_path):
            os.remove(out_path)
        os.makedirs(out_path, exist_ok=True)
        data = build_stream_output(args, out_path)
        debug_print(args.debug_steps, f"frames exported: {len(data.get('frames', []))}")
        check_cancel_requested(args.cancel_file)
        debug_print(args.debug_steps, "writing output")
        write_lua_atomic(os.path.join(out_path, "manifest.lua"), data)
        debug_print(args.debug_steps, "done")
        return 0

    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    if args.scan_only:
        check_cancel_requested(args.cancel_file)
        data = build_scan_data(args)
        debug_print(args.debug_steps, f"scan sources: {len(data.get('sources', []))}")
    else:
        data = build_data(args)
        debug_print(args.debug_steps, f"frames exported: {len(data.get('frames', []))}")
    check_cancel_requested(args.cancel_file)
    debug_print(args.debug_steps, "writing output")
    write_lua_atomic(out_path, data)
    debug_print(args.debug_steps, "done")

    return 0


def debug_requested_from_argv(argv: list[str]) -> bool:
    return "--debug-steps" in argv


if __name__ == "__main__":
    try:
        import bpy  # type: ignore
    except Exception as exc:
        sys.stderr.write(f"Failed to import bpy: {exc}\n")
        sys.exit(2)

    cli_argv = []
    if "--" in sys.argv:
        cli_argv = sys.argv[sys.argv.index("--") + 1 :]
    debug_on = debug_requested_from_argv(cli_argv)

    try:
        sys.exit(main())
    except Exception as exc:
        if debug_on:
            print(f"[blender_export] ERROR: {exc}", file=sys.stderr, flush=True)
            traceback.print_exc()
        else:
            sys.stderr.write(f"Exporter failed: {exc}\n")
        sys.exit(1)
