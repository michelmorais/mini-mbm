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
import io
import math
import os
import re
import shutil
import struct
import sys
import tempfile
import traceback
import zlib
from typing import Any


MAX_MBM_SUBSET_VERTICES = 65535

# v11 mesh format constants (docs/mesh-v11-format.md, mirrors src/core_mbm/mesh-v11-io.cpp).
TYPE_MESH_3D = 0
SECTION_MATERIAL_TRANSFORM = 1
SECTION_ANIMATION = 2
SECTION_FRAME_STATIC = 10
SECTION_FRAME_SKINNED = 11
SECTION_DETAIL_PHYSICS = 20
SECTION_EXTRA_PATHS = 30
SECTION_VERTEX_SKIN_WEIGHTS = 40
SECTION_SKELETAL_SKELETON = 41
SECTION_SKELETAL_WEIGHTS = 42
SECTION_SKELETAL_ANIMATION = 43

TEXTURE_ROLE_NORMAL = 2
TEXTURE_ROLE_SPECULAR = 3
TEXTURE_ROLE_EMISSIVE = 4
TEXTURE_ROLE_MASK = 5


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--scan-only", action="store_true")
    parser.add_argument("--stream-output", action="store_true")
    parser.add_argument("--direct-msh-output", action="store_true")
    parser.add_argument("--bake-animation", action="store_true")
    parser.add_argument("--use-scene-frame-range", action="store_true")
    parser.add_argument("--frame-start", type=int, default=1)
    parser.add_argument("--frame-end", type=int, default=1)
    parser.add_argument("--sample-step", type=int, default=1)
    parser.add_argument("--animation-name", default="Bake")
    parser.add_argument("--animation-clip", action="append", nargs=4, metavar=("NAME", "START", "END", "STEP"))
    parser.add_argument("--post-process", action="store_true")
    parser.add_argument("--invert-u", action="store_true")
    parser.add_argument("--invert-v", action="store_true")
    parser.add_argument("--angle-x", type=float, default=0.0)
    parser.add_argument("--angle-y", type=float, default=0.0)
    parser.add_argument("--angle-z", type=float, default=0.0)
    parser.add_argument("--large-mesh-mode", choices=("fail", "vb_only"), default="fail")
    parser.add_argument("--include-bones", action="store_true")
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


_EXTRACTED_IMAGE_PATHS: dict[int, str] = {}


def _image_extension(image: Any) -> str:
    format_extensions = {
        "BMP": ".bmp",
        "JPEG": ".jpg",
        "JPEG2000": ".jp2",
        "PNG": ".png",
        "TARGA": ".tga",
        "TARGA_RAW": ".tga",
        "TIFF": ".tif",
        "WEBP": ".webp",
    }
    return format_extensions.get(str(getattr(image, "file_format", "")).upper(), ".png")


def _extract_embedded_image(image: Any, output_dir: str) -> str:
    """Mixamo (and other) FBX downloads commonly embed textures directly in the binary rather than
    shipping them as loose files -- Blender's importer unpacks these into memory fine
    (image.packed_file is set, real pixel data available), but image.filepath still holds whatever
    path the ORIGINAL AUTHOR's machine recorded at export time (confirmed via direct testing on a
    real Mixamo download: "/home/app/mixamo-mini/tmp/skins_<uuid>.fbm/Ch36_1001_Diffuse.png", a path
    that obviously doesn't exist on this machine). Returning that recorded path unchanged, as this
    function used to, meant the .msh's own texture reference -- and the SECTION_EXTRA_PATHS search
    directory built from it -- pointed nowhere, so the texture could never resolve even though the
    actual image bytes were sitting right there in Blender's memory the whole time. Saves the image
    to a real file next to the output .msh instead, and returns that path; cached by image identity
    so a texture shared by several subsets (the common case) is only written once.
    """
    cache_key = image.as_pointer()
    cached = _EXTRACTED_IMAGE_PATHS.get(cache_key)
    if cached and os.path.isfile(cached):
        return cached
    base_name = os.path.basename(image.filepath) if image.filepath else ""
    if not base_name or "." not in base_name:
        safe_name = re.sub(r"[^A-Za-z0-9_.-]", "_", image.name) or "texture"
        base_name = safe_name + _image_extension(image)
    out_path = os.path.join(output_dir, base_name)
    orig_filepath_raw = image.filepath_raw
    orig_format = image.file_format
    try:
        image.filepath_raw = out_path
        image.save()
    finally:
        image.filepath_raw = orig_filepath_raw
        image.file_format = orig_format
    _EXTRACTED_IMAGE_PATHS[cache_key] = out_path
    return out_path


def _texture_node_priority(node: Any) -> int:
    """Prefer the color texture which the engine's single subset texture slot can represent.

    A glTF material commonly contains base-color, metallic/roughness, normal and emissive image
    nodes. Blender does not guarantee their node-tree iteration order, so taking the first image
    can select a data map (or an emissive map) and leave the imported mesh looking untextured.
    """
    priority = 0
    for output in getattr(node, "outputs", []):
        for link in getattr(output, "links", []):
            target_node = getattr(link, "to_node", None)
            target_type = str(getattr(target_node, "type", ""))
            socket_name = str(getattr(getattr(link, "to_socket", None), "name", "")).lower()
            if target_type == "BSDF_PRINCIPLED" and socket_name in ("base color", "base_color"):
                priority = max(priority, 100)
            elif target_type == "BSDF_DIFFUSE" and socket_name == "color":
                priority = max(priority, 90)
            elif "base color" in socket_name or "diffuse" in socket_name:
                priority = max(priority, 80)
            elif socket_name in ("normal", "metallic", "roughness", "emission", "emission color"):
                priority = max(priority, 10)
            else:
                priority = max(priority, 40)
    return priority


def _texture_node_role(node: Any) -> int | None:
    """Map a Blender/glTF image node to the closest v11 extra material texture role."""
    pending = list(getattr(node, "outputs", []))
    visited_nodes: set[int] = set()
    while pending:
        output = pending.pop(0)
        for link in getattr(output, "links", []):
            target = getattr(link, "to_node", None)
            target_type = str(getattr(target, "type", ""))
            socket_name = str(getattr(getattr(link, "to_socket", None), "name", "")).lower()
            if target_type == "NORMAL_MAP" or socket_name == "normal":
                return TEXTURE_ROLE_NORMAL
            if "emission" in socket_name:
                return TEXTURE_ROLE_EMISSIVE
            if "specular" in socket_name:
                return TEXTURE_ROLE_SPECULAR
            if target_type in ("SEPARATE_COLOR", "SEPRGB") or socket_name in ("metallic", "roughness"):
                return TEXTURE_ROLE_MASK
            if target is not None:
                target_key = int(target.as_pointer()) if hasattr(target, "as_pointer") else id(target)
                if target_key not in visited_nodes:
                    visited_nodes.add(target_key)
                    pending.extend(getattr(target, "outputs", []))
    return None


def _resolve_texture_path(image: Any, image_user: Any, scene_frame: int,
                          output_dir: str | None) -> str:
    if getattr(image, "source", "") == "SEQUENCE":
        return resolve_image_sequence_path(image, image_user, scene_frame)
    recorded_path = bpy.path.abspath(image.filepath)
    needs_extraction = image.packed_file is not None or not os.path.isfile(recorded_path)
    if needs_extraction and output_dir:
        return _extract_embedded_image(image, output_dir)
    return recorded_path


def get_material_texture_paths(material: Any, scene_frame: int,
                               output_dir: str | None = None) -> tuple[str, list[dict[str, Any]]]:
    if material is None or not getattr(material, "use_nodes", False) or material.node_tree is None:
        return "", []
    image_nodes = [
        node for node in material.node_tree.nodes
        if node.type == "TEX_IMAGE" and getattr(node, "image", None) is not None
    ]
    image_nodes.sort(key=_texture_node_priority, reverse=True)
    primary_node = next((node for node in image_nodes if _texture_node_priority(node) >= 80), None)
    if primary_node is None and image_nodes:
        primary_node = image_nodes[0]
    primary = ""
    extras: list[dict[str, Any]] = []
    used_roles: set[int] = set()
    mask_texture_hint = str(material.get("mbm_mask_texture", "")) if hasattr(material, "get") else ""
    for node in image_nodes:
        try:
            path = _resolve_texture_path(node.image, getattr(node, "image_user", None), scene_frame, output_dir)
        except Exception:
            path = str(node.image.filepath or "")
        if not path:
            continue
        if node is primary_node:
            primary = path
            continue
        image_name = str(getattr(node.image, "name", ""))
        role = TEXTURE_ROLE_MASK if mask_texture_hint and image_name == mask_texture_hint else _texture_node_role(node)
        if role is not None and role not in used_roles:
            extras.append({"role": role, "texture": path})
            used_roles.add(role)
    return primary, extras


def get_first_texture_path(material: Any, scene_frame: int, output_dir: str | None = None) -> str:
    primary, _ = get_material_texture_paths(material, scene_frame, output_dir)
    return primary


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


def rotate_point_deg(x: float, y: float, z: float, angle_x_deg: float, angle_y_deg: float, angle_z_deg: float) -> tuple[float, float, float]:
    """Rotates a point (or, equally, a direction vector -- this is a pure rotation, no
    translation) by degrees around X, then Y, then Z. Formulas and order match
    editor/mesh_debug.lua's rotateX/Y/Z + applyRotationToBonesDeg exactly, which in turn match
    MESH_MBM_DEBUG::rotateFrame's own per-axis math (src/core_mbm/mesh-manager.cpp:3128) -- kept
    in lockstep by hand since there is no shared implementation between the Python and Lua/C++
    sides. Used for the Blender-import post-process rotation (the usual Z-up -> Y-up correction,
    "Rot X/Y/Z" in the import dialog, default -90 on X): baked directly into vertex/normal/bone
    data at export time instead of being written into the file's now-deprecated
    SECTION_MATERIAL_TRANSFORM angle field, which the engine no longer applies at load.
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


def frame_to_euler_xyz(ydir: tuple[float, float, float], zdir: tuple[float, float, float]) -> tuple[float, float, float]:
    """Inverse of rotate_point_deg applied to (0,1,0)/(0,0,1): given a bone's local Y (axis
    direction, head->tail) and Z (roll axis) basis vectors already expressed in the space rotX/Y/Z
    are stored in, extracts the equivalent Euler XYZ degrees. Closed-form from M = Rx*Ry*Rz (this
    engine's row-vector convention -- matrix rows are the images of the X/Y/Z basis vectors, X
    derived here as cross(Y,Z) since only Y/Z are ever needed). Must stay in lockstep, by hand,
    with editor/mesh_debug.lua's identical boneFrameToEuler -- there is no shared implementation
    between Python and Lua.
    """
    yx, yy, yz = ydir
    zx, zy, zz = zdir
    xx = yy * zz - yz * zy
    xy = yz * zx - yx * zz
    xz = yx * zy - yy * zx
    clamped = max(-1.0, min(1.0, -xz))
    rot_y = math.asin(clamped)
    if abs(xz) > 0.999999:
        # gimbal lock: X and Z rotation become indistinguishable, collapse to rot_x=0
        rot_x = 0.0
        rot_z = math.atan2(-yx, yy)
    else:
        rot_x = math.atan2(yz, zz)
        rot_z = math.atan2(xy, xx)
    return math.degrees(rot_x), math.degrees(rot_y), math.degrees(rot_z)


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


def split_subset_for_uint16_indices(subset: dict[str, Any]) -> list[dict[str, Any]]:
    vertices = subset.get("vertices") or []
    indices = subset.get("indices") or []
    if len(vertices) <= MAX_MBM_SUBSET_VERTICES:
        return [subset]

    chunks: list[dict[str, Any]] = []
    current_vertices: list[dict[str, Any]] = []
    current_indices: list[int] = []
    current_map: dict[int, int] = {}

    def flush_chunk() -> None:
        nonlocal current_vertices, current_indices, current_map
        if not current_vertices or not current_indices:
            return
        chunks.append(
            {
                "name": str(subset.get("name") or "Subset"),
                "texture": str(subset.get("texture") or ""),
                "vertices": current_vertices,
                "indices": current_indices,
            }
        )
        current_vertices = []
        current_indices = []
        current_map = {}

    for tri_start in range(0, len(indices), 3):
        tri = indices[tri_start:tri_start + 3]
        if len(tri) != 3:
            raise RuntimeError(f"Subset {subset.get('name', '')} has a non-triangle index tail.")

        missing = [idx for idx in tri if idx not in current_map]
        if current_indices and len(current_vertices) + len(missing) > MAX_MBM_SUBSET_VERTICES:
            flush_chunk()

        for idx in tri:
            idx_int = int(idx)
            if idx_int < 1 or idx_int > len(vertices):
                raise RuntimeError(f"Subset {subset.get('name', '')} index {idx_int} is out of range.")
            mapped = current_map.get(idx_int)
            if mapped is None:
                mapped = len(current_vertices) + 1
                current_map[idx_int] = mapped
                current_vertices.append(dict(vertices[idx_int - 1]))
            current_indices.append(mapped)

    flush_chunk()

    if len(chunks) <= 1:
        return chunks
    for i, chunk in enumerate(chunks, start=1):
        chunk["name"] = f"{chunk['name']}#{i}"
    return chunks


def validate_frame_vertex_limit(subsets: list[dict[str, Any]]) -> None:
    total_vertices = sum(len(subset.get("vertices") or []) for subset in subsets)
    if total_vertices > MAX_MBM_SUBSET_VERTICES:
        raise RuntimeError(
            f"Cannot export this mesh as one MSH frame: {total_vertices} vertices exceed the "
            f"{MAX_MBM_SUBSET_VERTICES} vertex limit. mini-mbm MSH v8 uses a 16-bit index buffer per frame; "
            "reduce/split the model in Blender or export it as multiple meshes."
        )


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
    mesh_stats: dict[str, Any] = {"available": False}

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

    try:
        scene.frame_set(int(scene.frame_current))
        bpy.context.view_layer.update()
        stat_subsets = export_frame_subsets(scene)
        stat_texture_paths: set[str] = set()
        total_vertices = 0
        total_indices = 0
        for subset in stat_subsets:
            total_vertices += len(subset.get("vertices") or [])
            total_indices += len(subset.get("indices") or [])
            add_texture_search_path(stat_texture_paths, str(subset.get("texture") or ""))
        mesh_stats = {
            "available": True,
            "frame": int(scene.frame_current),
            "subsets": len(stat_subsets),
            "vertices": total_vertices,
            "indices": total_indices,
            "textureSearchPaths": sorted(stat_texture_paths),
        }
    except Exception as exc:
        mesh_stats = {
            "available": False,
            "error": str(exc),
        }

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
        "meshStats": mesh_stats,
    }


def export_frame_subsets(scene: Any, rotation_deg: tuple[float, float, float] | None = None,
                          output_dir: str | None = None, capture_weights: bool = False,
                          canonical_coordinates: bool = False) -> list[dict[str, Any]]:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    subsets_out: list[dict[str, Any]] = []
    scene_frame = int(scene.frame_current)

    mesh_objects = [o for o in scene.objects if o.type == "MESH" and o.visible_get()]
    mesh_objects.sort(key=lambda o: o.name)

    for obj in mesh_objects:
        # Real per-vertex bone weights (SECTION_VERTEX_SKIN_WEIGHTS, docs/mesh-v11-format.md Sec.
        # 6f), captured here -- before evaluated_get()/to_mesh() below -- so the depsgraph
        # evaluation picks up the now-capped/normalized weights. Capped at 4 influences + summed to
        # ~1.0 via the exact same Blender ops (vertex_group_limit_total/vertex_group_normalize_all)
        # editor/blender_mesh_skeleton_export.py's bind_mesh_to_armature already applies to its own
        # ARMATURE_ENVELOPE fallback weights -- keeps the on-disk representation identical either
        # way, real or invented. Only meaningful for an object that already has real vertex groups
        # (a rigged Mixamo/Blender source) -- an object with none is left alone entirely (no groups
        # to select 4 out of).
        if capture_weights and obj.vertex_groups:
            bpy.ops.object.select_all(action="DESELECT")
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.vertex_group_limit_total(limit=4)
            bpy.ops.object.vertex_group_normalize_all(lock_active=False)

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
                    primary_texture, extra_textures = get_material_texture_paths(mat, scene_frame, output_dir)
                    buckets[mat_idx] = {
                        "name": f"{obj.name}:{mat_name}",
                        "texture": primary_texture,
                        "extraTextures": extra_textures,
                        "vertices": [],
                        "indices": [],
                        "_vmap": {},
                    }

                bucket = buckets[mat_idx]
                vmap = bucket["_vmap"]

                # The canonical FBX boundary reflects X after the usual Z-up -> Y-up rotation.
                # Reflection reverses handedness, so reverse every triangle to preserve its facing.
                triangle_loops = tuple(reversed(tri.loops)) if canonical_coordinates else tri.loops
                for loop_index in triangle_loops:
                    loop = mesh.loops[loop_index]
                    vert = mesh.vertices[loop.vertex_index]
                    loop_no = get_loop_normal(loop, vert)

                    world_pos = eval_obj.matrix_world @ vert.co
                    world_no = (eval_obj.matrix_world.to_3x3() @ loop_no).normalized()
                    pos_x, pos_y, pos_z = float(world_pos.x), float(world_pos.y), float(world_pos.z)
                    no_x, no_y, no_z = float(world_no.x), float(world_no.y), float(world_no.z)
                    if rotation_deg:
                        pos_x, pos_y, pos_z = rotate_point_deg(pos_x, pos_y, pos_z, *rotation_deg)
                        no_x, no_y, no_z = rotate_point_deg(no_x, no_y, no_z, *rotation_deg)
                    if canonical_coordinates:
                        pos_x = -pos_x
                        no_x = -no_x

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
                        no_x,
                        no_y,
                        no_z,
                    )

                    idx = vmap.get(key)
                    if idx is None:
                        idx = len(bucket["vertices"]) + 1
                        vmap[key] = idx
                        vertex_dict: dict[str, Any] = {
                            "x": pos_x,
                            "y": pos_y,
                            "z": pos_z,
                            "u": u,
                            "v": v,
                            "nx": no_x,
                            "ny": no_y,
                            "nz": no_z,
                        }
                        if capture_weights and obj.vertex_groups:
                            # Already capped to <=4 influences summing to ~1.0 by the
                            # vertex_group_limit_total/vertex_group_normalize_all ops applied
                            # above, before evaluated_get() -- sort defensively anyway rather than
                            # trusting group iteration order.
                            top_groups = sorted(vert.groups, key=lambda g: g.weight, reverse=True)[:4]
                            vertex_dict["boneNames"] = [obj.vertex_groups[g.group].name for g in top_groups]
                            vertex_dict["weights"] = [float(g.weight) for g in top_groups]
                        bucket["vertices"].append(vertex_dict)

                    bucket["indices"].append(idx)

            for mat_idx in sorted(buckets.keys()):
                bucket = buckets[mat_idx]
                if bucket["vertices"] and bucket["indices"]:
                    bucket.pop("_vmap", None)
                    subsets_out.extend(split_subset_for_uint16_indices(bucket))
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


def normalize_frame_range(frame_start: int, frame_end: int) -> tuple[int, int]:
    frame_start = max(1, int(frame_start))
    frame_end = max(1, int(frame_end))
    if frame_end < frame_start:
        frame_start, frame_end = frame_end, frame_start
    return frame_start, frame_end


def parse_animation_clips(args: argparse.Namespace, scene: Any) -> list[dict[str, Any]]:
    clips: list[dict[str, Any]] = []
    if args.animation_clip:
        for raw in args.animation_clip:
            name = str(raw[0] or "Bake")
            frame_start, frame_end = normalize_frame_range(int(raw[1]), int(raw[2]))
            step = max(1, int(raw[3]))
            clips.append(
                {
                    "name": name,
                    "frameStart": frame_start,
                    "frameEnd": frame_end,
                    "sampleStep": step,
                }
            )
        return clips

    if args.bake_animation:
        if args.use_scene_frame_range:
            frame_start = int(scene.frame_start)
            frame_end = int(scene.frame_end)
        else:
            frame_start = int(args.frame_start)
            frame_end = int(args.frame_end)
        frame_start, frame_end = normalize_frame_range(frame_start, frame_end)
        clips.append(
            {
                "name": args.animation_name or "Bake",
                "frameStart": frame_start,
                "frameEnd": frame_end,
                "sampleStep": max(1, int(args.sample_step)),
            }
        )
    return clips


def clip_frame_numbers(clip: dict[str, Any]) -> list[int]:
    frame_start, frame_end = normalize_frame_range(int(clip.get("frameStart", 1)), int(clip.get("frameEnd", 1)))
    step = max(1, int(clip.get("sampleStep", 1)))
    return list(range(frame_start, frame_end + 1, step))


def append_animation_header_for_clip(animations: list[dict[str, Any]],
                                     clip: dict[str, Any],
                                     target_start: int,
                                     target_end: int,
                                     fps: float) -> None:
    step = max(1, int(clip.get("sampleStep", 1)))
    animations.append(
        {
            "name": str(clip.get("name") or "Bake"),
            "initialFrame": target_start,
            "finalFrame": target_end,
            "timeBetweenFrame": float(step) / fps,
            "typeAnimation": 1,
        }
    )


def build_data(args: argparse.Namespace) -> dict[str, Any]:
    source_path = os.path.abspath(args.input)
    debug_print(args.debug_steps, f"import source: {source_path}")
    import_source(source_path)
    scene = bpy.context.scene
    mesh_cache_issues = get_mesh_cache_issues(scene)
    if args.bake_animation and mesh_cache_issues:
        details = "; ".join(issue.get("message", "") for issue in mesh_cache_issues)
        raise RuntimeError(f"Cannot bake mesh-cache animation: {details}")

    source_file = source_path
    output_dir = os.path.dirname(os.path.abspath(args.output))
    clips = parse_animation_clips(args, scene)

    frames_out: list[dict[str, Any]] = []
    animations: list[dict[str, Any]] = []
    fps = get_scene_fps(scene)
    if clips:
        for clip in clips:
            target_start = len(frames_out) + 1
            debug_print(
                args.debug_steps,
                f"bake animation clip: {clip.get('name', 'Bake')} {clip['frameStart']}..{clip['frameEnd']} step={clip['sampleStep']}",
            )
            for frame in clip_frame_numbers(clip):
                check_cancel_requested(args.cancel_file)
                scene.frame_set(frame)
                bpy.context.view_layer.update()
                debug_print(args.debug_steps, f"export frame: {frame}")
                frames_out.append({"frame": frame, "subsets": export_frame_subsets(scene, output_dir=output_dir)})
                check_cancel_requested(args.cancel_file)
            append_animation_header_for_clip(animations, clip, target_start, len(frames_out), fps)
    else:
        check_cancel_requested(args.cancel_file)
        frame = max(1, int(args.frame_start))
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        debug_print(args.debug_steps, f"export current frame: {frame}")
        frames_out.append({"frame": frame, "subsets": export_frame_subsets(scene, output_dir=output_dir)})
        check_cancel_requested(args.cancel_file)

    return {
        "version": 2,
        "source": source_file,
        "bakeAnimation": bool(clips),
        "frameStart": clips[0]["frameStart"] if clips else frames_out[0]["frame"],
        "frameEnd": clips[-1]["frameEnd"] if clips else frames_out[0]["frame"],
        "sampleStep": clips[0]["sampleStep"] if clips else 1,
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

    frames_dir = os.path.join(out_dir, "frames")
    os.makedirs(frames_dir, exist_ok=True)
    clips = parse_animation_clips(args, scene)

    frames_manifest: list[dict[str, Any]] = []
    animations: list[dict[str, Any]] = []
    fps = get_scene_fps(scene)
    if clips:
        out_index = 0
        for clip in clips:
            target_start = len(frames_manifest) + 1
            debug_print(
                args.debug_steps,
                f"bake animation clip: {clip.get('name', 'Bake')} {clip['frameStart']}..{clip['frameEnd']} step={clip['sampleStep']}",
            )
            for frame in clip_frame_numbers(clip):
                check_cancel_requested(args.cancel_file)
                out_index += 1
                scene.frame_set(frame)
                bpy.context.view_layer.update()
                debug_print(args.debug_steps, f"export frame: {frame}")
                rel_path = f"frames/frame_{out_index:06d}.lua"
                frame_data = {"frame": frame, "subsets": export_frame_subsets(scene, output_dir=out_dir)}
                write_lua_atomic(os.path.join(out_dir, rel_path), frame_data)
                frames_manifest.append({"sourceFrame": frame, "path": rel_path})
                check_cancel_requested(args.cancel_file)
            append_animation_header_for_clip(animations, clip, target_start, len(frames_manifest), fps)
    else:
        check_cancel_requested(args.cancel_file)
        frame = max(1, int(args.frame_start))
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        debug_print(args.debug_steps, f"export current frame: {frame}")
        rel_path = "frames/frame_000001.lua"
        frame_data = {"frame": frame, "subsets": export_frame_subsets(scene, output_dir=out_dir)}
        write_lua_atomic(os.path.join(out_dir, rel_path), frame_data)
        frames_manifest.append({"sourceFrame": frame, "path": rel_path})
        check_cancel_requested(args.cancel_file)

    return {
        "version": 3,
        "streamOutput": True,
        "source": source_path,
        "bakeAnimation": bool(clips),
        "frameStart": clips[0]["frameStart"] if clips else frames_manifest[0]["sourceFrame"],
        "frameEnd": clips[-1]["frameEnd"] if clips else frames_manifest[0]["sourceFrame"],
        "sampleStep": clips[0]["sampleStep"] if clips else 1,
        "frames": frames_manifest,
        "animations": animations,
    }


def write_u8(fp: Any, value: int) -> None:
    fp.write(struct.pack("<B", int(value)))


def write_u16(fp: Any, value: int) -> None:
    fp.write(struct.pack("<H", int(value)))


def write_i32(fp: Any, value: int) -> None:
    fp.write(struct.pack("<i", int(value)))


def write_u32(fp: Any, value: int) -> None:
    fp.write(struct.pack("<I", int(value)))


def write_u64(fp: Any, value: int) -> None:
    fp.write(struct.pack("<Q", int(value)))


def write_f32(fp: Any, value: float) -> None:
    fp.write(struct.pack("<f", float(value)))


def write_color(fp: Any, color: tuple[float, float, float, float]) -> None:
    for value in color:
        write_f32(fp, value)


def write_material_default(fp: Any) -> None:
    # Blender's exported material data has no equivalent of the engine's specular highlight, and
    # a white Specular/Power=1 default made every imported mesh show an unwanted shiny highlight.
    # Imports start flat (no specular) until the user deliberately adds it in the mesh debug tool.
    write_color(fp, (1.0, 1.0, 1.0, 1.0))  # Diffuse
    write_color(fp, (1.0, 1.0, 1.0, 1.0))  # Ambient
    write_color(fp, (0.0, 0.0, 0.0, 0.0))  # Specular
    write_color(fp, (0.0, 0.0, 0.0, 0.0))  # Emissive
    write_f32(fp, 0.0)  # Power


def write_vec3(fp: Any, value: tuple[float, float, float]) -> None:
    write_f32(fp, value[0])
    write_f32(fp, value[1])
    write_f32(fp, value[2])


def write_vec2(fp: Any, value: tuple[float, float]) -> None:
    write_f32(fp, value[0])
    write_f32(fp, value[1])


def write_string_v11(fp: Any, value: str) -> None:
    data = str(value or "").encode("utf-8")
    if len(data) > 0xFFFF:
        raise RuntimeError(f"string too long for a v11 length-prefixed field: {len(data)} bytes")
    write_u16(fp, len(data))
    fp.write(data)


def write_file_header_v11(fp: Any, type_mesh: int, back_buffer_width: int, back_buffer_height: int,
                           section_count: int) -> None:
    fp.write(b"MBM1")
    write_u16(fp, 11)
    fp.write(bytes((type_mesh, 0)))
    write_i32(fp, back_buffer_width)
    write_i32(fp, back_buffer_height)
    write_u32(fp, section_count)


def write_section_header_v11(fp: Any, section_type: int, section_version: int, compression: int,
                              uncompressed_length: int, compressed_length: int, crc32_value: int) -> None:
    write_u16(fp, section_type)
    write_u16(fp, section_version)
    fp.write(bytes((compression, 0, 0, 0)))
    write_u32(fp, uncompressed_length)
    write_u32(fp, compressed_length)
    write_u32(fp, crc32_value)


def write_section_v11(fp: Any, section_type: int, section_version: int, payload: bytes, compress: bool) -> None:
    if compress and payload:
        compressed = zlib.compress(payload, 9)
        compression = 1
    else:
        compressed = payload
        compression = 0
    crc32_value = zlib.crc32(payload) & 0xFFFFFFFF
    write_section_header_v11(fp, section_type, section_version, compression, len(payload), len(compressed),
                              crc32_value)
    fp.write(compressed)


def build_material_transform_payload_v11(angles: tuple[float, float, float]) -> bytes:
    buf = io.BytesIO()
    write_material_default(buf)
    write_f32(buf, angles[0])
    write_f32(buf, angles[1])
    write_f32(buf, angles[2])
    write_f32(buf, 0.0)  # posX
    write_f32(buf, 0.0)  # posY
    write_f32(buf, 0.0)  # posZ
    write_u32(buf, 4)       # mode_draw = MODE_DRAW_TRIANGLES
    write_u32(buf, 0x0405)  # mode_cull_face = CULL_BACK
    write_u32(buf, 0x0900)  # mode_front_face_direction = CW
    return buf.getvalue()


def build_extra_paths_payload_v11(paths: list[str]) -> bytes:
    buf = io.BytesIO()
    write_u32(buf, len(paths))
    for path in paths:
        write_string_v11(buf, path)
    return buf.getvalue()


def extract_armature_joints(scene: Any, rotation_deg: tuple[float, float, float] | None = None) -> list[dict[str, Any]]:
    """Reads the first ARMATURE object's rest-pose bones into a flat, parent-before-child list of
    {name, parent, x, y, z, radius} dicts -- the same shape SECTION_FRAME_SKINNED expects (docs/
    mesh-v11-format.md Sec. 6e). Diagnostic/editor round-trip data only, mirroring
    MESH_MBM_DEBUG::addBone's own contract; never consulted by rendering.

    No Y-up/Z-up axis swap here -- this reads directly from an already-Blender-native scene the
    exact same way export_frame_subsets() already reads mesh vertices above (eval_obj.matrix_world
    @ vert.co, no axis conversion) -- both bones and vertices need to end up in the same space so
    gizmos drawn over the imported geometry line up. rotation_deg, when given, must be the exact
    same post-process rotation passed to export_frame_subsets() for the same reason -- bones and
    vertices have to end up in the same space after rotation too.
    """
    armature_obj = next((o for o in scene.objects if o.type == "ARMATURE"), None)
    if armature_obj is None:
        return []

    bones = list(armature_obj.data.bones)
    if not bones:
        return []

    by_name = {b.name: b for b in bones}
    children_of: dict[str, list[str]] = {}
    roots: list[str] = []
    for b in bones:
        parent_name = b.parent.name if b.parent else None
        if parent_name is None:
            roots.append(b.name)
        else:
            children_of.setdefault(parent_name, []).append(b.name)

    # BFS from every root bone so a parent always precedes its children in the emitted list --
    # required by parse_skeleton_section_v11's on-load ordering check (mesh-manager.cpp:593-616).
    ordered: list[str] = []
    queue = list(roots)
    while queue:
        name = queue.pop(0)
        ordered.append(name)
        queue.extend(children_of.get(name, []))

    joints: list[dict[str, Any]] = []
    for name in ordered:
        bone = by_name[name]
        world_head = armature_obj.matrix_world @ bone.head_local
        world_tail = armature_obj.matrix_world @ bone.tail_local
        parent_name = bone.parent.name if bone.parent else None
        # No natural Blender source for an authoring-time marker radius -- derive from bone
        # length (a visible gizmo size, not a measurement that means anything to rendering).
        # MUST use the world-space head/tail
        # distance, not bone.length -- bone.length is measured in the armature's own unscaled rest
        # space and ignores armature_obj's own object-level scale entirely (bug found via a real
        # user-downloaded rig: armature_obj.scale == 0.01 there, so bone.length read ~100x too
        # large relative to the already-correctly-world-scaled x/y/z position above, producing
        # marker spheres bigger than the whole imported character).
        radius = max(0.001, (world_tail - world_head).length * 0.15)
        head_x, head_y, head_z = float(world_head.x), float(world_head.y), float(world_head.z)

        # Real orientation, captured here for the first time (SECTION_FRAME_SKINNED sectionVersion
        # 2): world_head/world_tail already give the exact bone axis (no ambiguity, unlike
        # editor/blender_mesh_skeleton_export.py's build_armature, which has to guess this from
        # nothing but a scatter of positions when this data is absent). matrix_local is armature-
        # space (not parent-relative), matching this function's own position convention above.
        length = (world_tail - world_head).length
        if length > 1e-6:
            ydir_world = ((world_tail - world_head) / length)
            zdir_raw = (armature_obj.matrix_world @ bone.matrix_local).to_3x3() @ Vector((0.0, 0.0, 1.0))
            # Gram-Schmidt: orthonormalize the roll axis against the bone axis before extraction,
            # guaranteeing a clean orthonormal frame even if matrix_local's own Z isn't exactly
            # perpendicular to head->tail (it always should be, this is defensive).
            zdir_raw = zdir_raw - ydir_world * zdir_raw.dot(ydir_world)
            zdir_len = zdir_raw.length
            zdir_world = zdir_raw / zdir_len if zdir_len > 1e-6 else Vector((1.0, 0.0, 0.0))
            ydir = tuple(ydir_world)
            zdir = tuple(zdir_world)
            if rotation_deg:
                ydir = rotate_point_deg(*ydir, *rotation_deg)
                zdir = rotate_point_deg(*zdir, *rotation_deg)
            rot_x, rot_y, rot_z = frame_to_euler_xyz(ydir, zdir)
        else:
            # Degenerate zero-length bone (head==tail) -- no orientation to extract. length stays
            # 0, which is exactly the sentinel build_armature uses to fall back to its own
            # position-topology heuristic for this bone.
            rot_x, rot_y, rot_z = 0.0, 0.0, 0.0

        if rotation_deg:
            head_x, head_y, head_z = rotate_point_deg(head_x, head_y, head_z, *rotation_deg)
        joints.append({
            "name": name,
            "parent": parent_name,
            "x": head_x,
            "y": head_y,
            "z": head_z,
            "radius": radius,
            "rotX": rot_x,
            "rotY": rot_y,
            "rotZ": rot_z,
            # Blender rest/edit bones carry no meaningful per-bone scale (real scale only exists on
            # pose bones, out of scope for a rest-pose skeleton dump) -- stored as identity purely
            # for round-trip completeness with the engine's own scaleX/Y/Z fields.
            "scaleX": 1.0,
            "scaleY": 1.0,
            "scaleZ": 1.0,
            "length": length,
        })
    return joints


def build_skeleton_payload_v11(joints: list[dict[str, Any]]) -> bytes:
    """Payload for SECTION_FRAME_SKINNED sectionVersion 2: SKELETON_HEADER_V11{jointCount:u16}
    followed by jointCount SKELETON_BONE_V11 records (name, parentName length-prefixed strings +
    x,y,z,radius,rotX,rotY,rotZ,scaleX,scaleY,scaleZ,length f32 -- 11 floats total), matching
    writeSkeletonHeaderV11/writeSkeletonBoneV11's exact byte layout
    (src/core_mbm/mesh-v11-io.cpp:634-657) -- there is no shared serializer between this script and
    the C++ side, so the layout is replicated by hand and must stay in lockstep with it. This
    script always writes the full v2 layout (the SECTION_FRAME_SKINNED section header this payload
    is wrapped in must be stamped sectionVersion=2 by the caller, see build_direct_msh_output).
    """
    buf = io.BytesIO()
    write_u16(buf, len(joints))
    for j in joints:
        write_string_v11(buf, str(j["name"]))
        write_string_v11(buf, str(j["parent"] or ""))
        write_f32(buf, j["x"])
        write_f32(buf, j["y"])
        write_f32(buf, j["z"])
        write_f32(buf, j["radius"])
        write_f32(buf, j.get("rotX", 0.0))
        write_f32(buf, j.get("rotY", 0.0))
        write_f32(buf, j.get("rotZ", 0.0))
        write_f32(buf, j.get("scaleX", 1.0))
        write_f32(buf, j.get("scaleY", 1.0))
        write_f32(buf, j.get("scaleZ", 1.0))
        write_f32(buf, j.get("length", 0.0))
    return buf.getvalue()


def build_vertex_skin_weights_payload_v11(subsets: list[dict[str, Any]]) -> bytes | None:
    """Payload for SECTION_VERTEX_SKIN_WEIGHTS sectionVersion 1: VERTEX_SKIN_WEIGHTS_HEADER_V11
    {paletteCount:u32, vertexCount:u32} followed by paletteCount length-prefixed bone-name strings,
    then vertexCount VERTEX_BONE_WEIGHT_V11 records (4x u8 paletteIndex, 0xFF = unused slot, then
    4x f32 weight), matching writeVertexSkinWeightsHeaderV11/writeVertexBoneWeightV11's exact byte
    layout (src/core_mbm/mesh-v11-io.cpp) -- replicated by hand, must stay in lockstep with it,
    mirroring build_skeleton_payload_v11's own precedent.

    Vertex order here MUST exactly match write_direct_frame_chunk_indexed's own position/normal/uv
    ordering (subset order, then each subset's own `vertices` list order) -- `subsets` must be the
    very same list frame 1's SECTION_FRAME_STATIC was written from, unmodified in count/order since
    (u/v-invert mutation in place is fine; that doesn't change vertex count or order). Only correct
    for the default indexed write path -- callers must not use this when large_mesh_mode=="vb_only"
    (that mode duplicates vertices per triangle-corner instead of deduplicating them, a different
    vertex order this function does not account for).

    Returns None (write nothing) if no vertex anywhere in `subsets` actually carries real weight
    data (capture_weights was off, or none of the source objects had vertex groups).
    """
    palette: list[str] = []
    palette_index: dict[str, int] = {}
    any_weighted = False
    entries: list[tuple[list[int], list[float]]] = []

    for subset in subsets:
        for vertex in (subset.get("vertices") or []):
            names = vertex.get("boneNames") or []
            weights = vertex.get("weights") or []
            slot_indices: list[int] = []
            slot_weights: list[float] = []
            for name, weight in list(zip(names, weights))[:4]:
                if not name:
                    continue
                if name not in palette_index:
                    if len(palette) >= 0xFF:
                        continue  # palette full (255 unique bones already referenced) -- drop overflow
                    palette_index[name] = len(palette)
                    palette.append(name)
                slot_indices.append(palette_index[name])
                slot_weights.append(float(weight))
                any_weighted = True
            entries.append((slot_indices, slot_weights))

    if not any_weighted:
        return None

    buf = io.BytesIO()
    write_u32(buf, len(palette))
    write_u32(buf, len(entries))
    for name in palette:
        write_string_v11(buf, name)
    for slot_indices, slot_weights in entries:
        for slot in range(4):
            write_u8(buf, slot_indices[slot] if slot < len(slot_indices) else 0xFF)
        for slot in range(4):
            write_f32(buf, slot_weights[slot] if slot < len(slot_weights) else 0.0)
    return buf.getvalue()


def stable_id(domain: str, value: str) -> int:
    result = 14695981039346656037
    for byte in (domain + value).encode("utf-8"):
        result ^= byte
        result = (result * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return result or 1


def extract_canonical_skeleton(scene: Any,
                               rotation_deg: tuple[float, float, float] | None = None) -> dict[str, Any] | None:
    """Extract parent-relative bind-local TRS directly from Blender's armature.

    This is the FBX import boundary for SECTION_SKELETAL_SKELETON, not a conversion from the
    exploratory SECTION_FRAME_SKINNED representation. Blender uses column matrices; storing the
    resulting quaternion components unchanged produces the equivalent transposed row rotation used
    by mini-mbm's buildTrsMatrix.
    """
    from mathutils import Matrix

    armature_obj = next((obj for obj in scene.objects if obj.type == "ARMATURE"), None)
    if armature_obj is None or not armature_obj.data.bones:
        return None

    bones = list(armature_obj.data.bones)
    by_name = {bone.name: bone for bone in bones}
    children: dict[str, list[str]] = {}
    roots: list[str] = []
    for bone in bones:
        if bone.parent:
            children.setdefault(bone.parent.name, []).append(bone.name)
        else:
            roots.append(bone.name)
    roots.sort()
    for names in children.values():
        names.sort()

    ordered: list[str] = []
    paths: dict[str, str] = {}
    queue = list(roots)
    while queue:
        name = queue.pop(0)
        bone = by_name[name]
        paths[name] = f"{paths[bone.parent.name]}/{name}" if bone.parent else name
        ordered.append(name)
        queue.extend(children.get(name, []))

    conversion = Matrix.Identity(4)
    if rotation_deg:
        ax, ay, az = (math.radians(float(value)) for value in rotation_deg)
        conversion = Matrix.Rotation(az, 4, "Z") @ Matrix.Rotation(ay, 4, "Y") @ Matrix.Rotation(ax, 4, "X")
    reflection = Matrix.Identity(4)
    reflection[0][0] = -1.0
    coordinate_change = reflection @ conversion
    inverse_coordinate_change = coordinate_change.inverted()

    global_bind: dict[str, Any] = {}
    records: list[dict[str, Any]] = []
    ids = {name: stable_id("mini-mbm.skeleton.bone/", paths[name]) for name in ordered}
    if len(set(ids.values())) != len(ids):
        raise RuntimeError("canonical skeleton bone ID collision")

    for name in ordered:
        bone = by_name[name]
        # Coordinate changes transform matrices by conjugation. The X reflection is part of the
        # accepted FBX boundary (-x,z,-y); unlike a one-sided multiplication, conjugation preserves
        # a proper rotation and therefore does not manufacture a negative root scale.
        world_bind = coordinate_change @ armature_obj.matrix_world @ bone.matrix_local @ inverse_coordinate_change
        global_bind[name] = world_bind
        local_bind = global_bind[bone.parent.name].inverted_safe() @ world_bind if bone.parent else world_bind
        translation, rotation, scale = local_bind.decompose()
        if min(abs(float(scale.x)), abs(float(scale.y)), abs(float(scale.z))) <= 1.0e-8:
            raise RuntimeError(f"canonical bone '{name}' has singular bind scale")
        if float(scale.x) < 0.0 or float(scale.y) < 0.0 or float(scale.z) < 0.0:
            raise RuntimeError(f"canonical bone '{name}' has unsupported negative bind scale")
        rotation.normalize()
        world_head = coordinate_change @ (armature_obj.matrix_world @ bone.head_local)
        world_tail = coordinate_change @ (armature_obj.matrix_world @ bone.tail_local)
        length = float((world_tail - world_head).length)
        records.append({
            "boneId": ids[name],
            "parentBoneId": ids[bone.parent.name] if bone.parent else 0,
            "name": name,
            "translation": (float(translation.x), float(translation.y), float(translation.z)),
            "rotation": (float(rotation.x), float(rotation.y), float(rotation.z), float(rotation.w)),
            "scale": (float(scale.x), float(scale.y), float(scale.z)),
            "radius": max(0.001, length * 0.15),
            "length": length,
        })

    identity = "|".join(paths[name] for name in ordered)
    return {
        "skeletonId": stable_id("mini-mbm.skeleton/", identity),
        "bones": records,
        "boneIdByName": ids,
        "orderedNames": ordered,
        "coordinateChange": coordinate_change,
        "inverseCoordinateChange": inverse_coordinate_change,
        "armatureObject": armature_obj,
    }


def build_canonical_skeleton_payload_v11(skeleton: dict[str, Any]) -> bytes:
    buf = io.BytesIO()
    bones = skeleton["bones"]
    write_u64(buf, int(skeleton["skeletonId"]))
    write_u32(buf, len(bones))
    for bone in bones:
        write_u64(buf, int(bone["boneId"]))
        write_u64(buf, int(bone["parentBoneId"]))
        write_string_v11(buf, str(bone["name"]))
        write_vec3(buf, bone["translation"])
        for value in bone["rotation"]:
            write_f32(buf, value)
        write_vec3(buf, bone["scale"])
        write_f32(buf, bone["radius"])
        write_f32(buf, bone["length"])
    return buf.getvalue()


def build_canonical_weights_payload_v11(subsets: list[dict[str, Any]],
                                         skeleton: dict[str, Any]) -> bytes | None:
    bone_ids = skeleton["boneIdByName"]
    palette: list[int] = []
    palette_index: dict[int, int] = {}
    entries: list[tuple[list[int], list[float]]] = []
    for subset in subsets:
        for vertex in (subset.get("vertices") or []):
            influences: list[tuple[int, float]] = []
            for name, raw_weight in list(zip(vertex.get("boneNames") or [], vertex.get("weights") or []))[:4]:
                weight = float(raw_weight)
                if weight <= 0.0:
                    continue
                if name not in bone_ids:
                    raise RuntimeError(f"vertex weight targets unknown canonical bone '{name}'")
                bone_id = int(bone_ids[name])
                if bone_id not in palette_index:
                    if len(palette) >= 0xFFFF:
                        raise RuntimeError("canonical weight palette exceeds 65535 bones")
                    palette_index[bone_id] = len(palette)
                    palette.append(bone_id)
                influences.append((palette_index[bone_id], weight))
            total = sum(weight for _, weight in influences)
            if total <= 1.0e-8:
                raise RuntimeError("canonical skinning requires at least one effective influence per vertex")
            entries.append(([index for index, _ in influences], [weight / total for _, weight in influences]))

    if not entries:
        return None
    buf = io.BytesIO()
    write_u64(buf, int(skeleton["skeletonId"]))
    write_u32(buf, 0)  # frameIndex: bind geometry is frame 0
    write_u32(buf, len(entries))
    write_u32(buf, len(palette))
    for bone_id in palette:
        write_u64(buf, bone_id)
    for indices, weights in entries:
        for slot in range(4):
            write_u16(buf, indices[slot] if slot < len(indices) else 0xFFFF)
        for slot in range(4):
            write_f32(buf, weights[slot] if slot < len(weights) else 0.0)
    return buf.getvalue()


def extract_canonical_animations(scene: Any, clips: list[dict[str, Any]], fps: float,
                                 skeleton: dict[str, Any]) -> list[dict[str, Any]]:
    armature_obj = skeleton["armatureObject"]
    coordinate_change = skeleton["coordinateChange"]
    inverse_coordinate_change = skeleton["inverseCoordinateChange"]
    ordered_names = skeleton["orderedNames"]
    bone_ids = skeleton["boneIdByName"]
    pose_by_name = {bone.name: bone for bone in armature_obj.pose.bones}
    result: list[dict[str, Any]] = []
    for clip in clips:
        frames = clip_frame_numbers(clip)
        if not frames:
            continue
        tracks = {name: [] for name in ordered_names}
        for frame in frames:
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            global_pose: dict[str, Any] = {}
            for name in ordered_names:
                pose_bone = pose_by_name.get(name)
                if pose_bone is None:
                    raise RuntimeError(f"animation pose is missing canonical bone '{name}'")
                world_pose = (coordinate_change @ armature_obj.matrix_world @ pose_bone.matrix @
                              inverse_coordinate_change)
                global_pose[name] = world_pose
                parent_name = pose_bone.parent.name if pose_bone.parent else None
                local_pose = global_pose[parent_name].inverted_safe() @ world_pose if parent_name else world_pose
                translation, rotation, scale = local_pose.decompose()
                rotation.normalize()
                tracks[name].append({
                    "time": float(frame - frames[0]) / fps,
                    "translation": (float(translation.x), float(translation.y), float(translation.z)),
                    "rotation": (float(rotation.x), float(rotation.y), float(rotation.z), float(rotation.w)),
                    "scale": (float(scale.x), float(scale.y), float(scale.z)),
                })
        name = str(clip.get("name") or "Bake")
        duration = max(float(int(clip.get("frameEnd", frames[-1])) -
                             int(clip.get("frameStart", frames[0]))) / fps, 1.0 / fps)
        result.append({
            "clipId": stable_id("mini-mbm.skeleton.clip/", f"{skeleton['skeletonId']}/{name}"),
            "name": name,
            "duration": duration,
            "loop": True,
            "tracks": [{"boneId": bone_ids[bone_name], "keys": tracks[bone_name]}
                       for bone_name in ordered_names],
        })
    return result


def build_canonical_animations_payload_v11(skeleton_id: int, clips: list[dict[str, Any]]) -> bytes:
    buf = io.BytesIO()
    reserved = b"\x00\x00\x00"
    write_u64(buf, skeleton_id)
    write_u32(buf, len(clips))
    for clip in clips:
        write_u64(buf, clip["clipId"])
        write_string_v11(buf, clip["name"])
        write_f32(buf, clip["duration"])
        write_u8(buf, 1 if clip["loop"] else 0)
        buf.write(reserved)
        write_u32(buf, len(clip["tracks"]))
        for track in clip["tracks"]:
            write_u64(buf, track["boneId"])
            write_u8(buf, 7)  # translation | rotation | scale
            buf.write(reserved)
            write_u32(buf, len(track["keys"]))
            for key in track["keys"]:
                write_f32(buf, key["time"])
                write_vec3(buf, key["translation"])
                for value in key["rotation"]:
                    write_f32(buf, value)
                write_vec3(buf, key["scale"])
                write_u8(buf, 0)  # linear
                buf.write(reserved)
                write_f32(buf, 0.0)
                write_f32(buf, 0.0)
                write_f32(buf, 1.0)
                write_f32(buf, 1.0)
    return buf.getvalue()


def build_animation_payload_v11(anim: dict[str, Any]) -> bytes:
    buf = io.BytesIO()
    write_string_v11(buf, str(anim.get("name", "default")))
    write_i32(buf, int(anim.get("initialFrame", 1)) - 1)
    write_i32(buf, int(anim.get("finalFrame", 1)) - 1)
    write_f32(buf, float(anim.get("timeBetweenFrame", 0.0)))
    write_i32(buf, int(anim.get("typeAnimation", 1)))
    write_u16(buf, 0)     # blendState
    buf.write(b"\x00")    # hasFx - this exporter never authors shader FX
    return buf.getvalue()


def write_frame_header_v11(fp: Any, total_subset: int, vertex_count: int, index_width: int, has_normal: bool,
                            has_uv: bool, uv_source: int, index_count: int) -> None:
    write_u32(fp, total_subset)
    write_u32(fp, vertex_count)
    fp.write(bytes((index_width, 1 if has_normal else 0, 1 if has_uv else 0, uv_source)))
    write_u32(fp, index_count)


def write_texture_ref_v11(fp: Any, path: str) -> None:
    fp.write(b"\x00")  # storage = PATH_REFERENCE
    write_string_v11(fp, path)


def write_subset_desc_v11(fp: Any, texture: str, extra_textures: list[dict[str, Any]], vertex_count: int,
                          vertex_start: int, index_start: int, index_count: int) -> None:
    write_texture_ref_v11(fp, texture)
    write_i32(fp, vertex_count)
    write_i32(fp, vertex_start)
    write_i32(fp, index_start)
    write_i32(fp, index_count)
    fp.write(bytes((1, 0, 0, 0)))  # alphaColor: hasAlpha forced on, matching saveV11's convention
    write_u16(fp, len(extra_textures))
    for extra in extra_textures:
        fp.write(bytes((int(extra["role"]),)))
        write_texture_ref_v11(fp, str(extra["texture"]))


def build_detail_physics_payload_v11(bounds: dict[str, tuple[float, float, float]]) -> bytes:
    buf = io.BytesIO()
    write_detail_cube_v8(buf, bounds)
    return buf.getvalue()


def write_detail_cube_v8(fp: Any, bounds: dict[str, tuple[float, float, float]]) -> None:
    write_i32(fp, ord("P"))
    write_i32(fp, 1)
    write_i32(fp, 1)
    write_i32(fp, 1)
    write_vec3(fp, bounds["half"])
    write_vec3(fp, bounds["center"])


def texture_name_for_msh(path: str) -> str:
    name = os.path.basename(path or "")
    return name if name else "default"


def add_texture_search_path(texture_paths: set[str], texture_path: str) -> None:
    if not texture_path:
        return
    full_path = os.path.abspath(texture_path)
    tex_dir = os.path.dirname(full_path)
    if tex_dir:
        texture_paths.add(tex_dir)


def apply_direct_vertex_options(vertices: list[dict[str, Any]], args: argparse.Namespace) -> None:
    if not args.post_process:
        return
    if not args.invert_u and not args.invert_v:
        return
    for vertex in vertices:
        if args.invert_u and isinstance(vertex.get("u"), (int, float)):
            vertex["u"] = 1.0 - float(vertex["u"])
        if args.invert_v and isinstance(vertex.get("v"), (int, float)):
            vertex["v"] = 1.0 - float(vertex["v"])


def update_bounds(bounds: dict[str, list[float]], vertices: list[dict[str, Any]]) -> None:
    for vertex in vertices:
        x = float(vertex.get("x", 0.0))
        y = float(vertex.get("y", 0.0))
        z = float(vertex.get("z", 0.0))
        bounds["min"][0] = min(bounds["min"][0], x)
        bounds["min"][1] = min(bounds["min"][1], y)
        bounds["min"][2] = min(bounds["min"][2], z)
        bounds["max"][0] = max(bounds["max"][0], x)
        bounds["max"][1] = max(bounds["max"][1], y)
        bounds["max"][2] = max(bounds["max"][2], z)


def finalize_bounds(bounds: dict[str, list[float]]) -> dict[str, tuple[float, float, float]]:
    if bounds["min"][0] == float("inf"):
        return {"half": (0.0, 0.0, 0.0), "center": (0.0, 0.0, 0.0)}
    half = (
        (bounds["max"][0] - bounds["min"][0]) * 0.5,
        (bounds["max"][1] - bounds["min"][1]) * 0.5,
        (bounds["max"][2] - bounds["min"][2]) * 0.5,
    )
    center = (
        bounds["min"][0] + half[0],
        bounds["min"][1] + half[1],
        bounds["min"][2] + half[2],
    )
    return {"half": half, "center": center}


def append_vertex_stream(vertex: dict[str, Any],
                         positions: list[tuple[float, float, float]],
                         normals: list[tuple[float, float, float]],
                         uvs: list[tuple[float, float]]) -> None:
    positions.append((float(vertex.get("x", 0.0)), float(vertex.get("y", 0.0)), float(vertex.get("z", 0.0))))
    normals.append((float(vertex.get("nx", 0.0)), float(vertex.get("ny", 0.0)), float(vertex.get("nz", 0.0))))
    uvs.append((float(vertex.get("u", 0.0)), float(vertex.get("v", 0.0))))


def write_direct_frame_chunk_indexed(path: str, subsets: list[dict[str, Any]], args: argparse.Namespace) -> None:
    vertex_start = 0
    index_start = 0
    subset_headers: list[dict[str, Any]] = []
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    uvs: list[tuple[float, float]] = []
    index_buffer: list[int] = []

    for subset_index, subset in enumerate(subsets, start=1):
        vertices = subset.get("vertices") or []
        indices = subset.get("indices") or []
        if vertex_start + len(vertices) > MAX_MBM_SUBSET_VERTICES:
            raise RuntimeError(
                f"Cannot export this mesh as one MSH frame: total frame vertices exceed "
                f"{MAX_MBM_SUBSET_VERTICES}. mini-mbm MSH v11 frames use a 16-bit index buffer."
            )
        if len(vertices) > MAX_MBM_SUBSET_VERTICES:
            raise RuntimeError(f"Subset {subset_index} exceeds {MAX_MBM_SUBSET_VERTICES} vertices after splitting.")
        if not vertices or not indices:
            raise RuntimeError(f"Subset {subset_index} has no vertices or indices.")
        apply_direct_vertex_options(vertices, args)
        for index in indices:
            idx = int(index)
            if idx < 1 or idx > len(vertices):
                raise RuntimeError(f"Subset {subset_index} index {idx} is out of range.")
            index_buffer.append(vertex_start + idx - 1)
        for vertex in vertices:
            append_vertex_stream(vertex, positions, normals, uvs)
        subset_headers.append(
            {
                "texture": texture_name_for_msh(str(subset.get("texture") or "")),
                "extraTextures": [
                    {"role": int(extra["role"]), "texture": texture_name_for_msh(str(extra.get("texture") or ""))}
                    for extra in (subset.get("extraTextures") or [])
                ],
                "vertexCount": len(vertices),
                "vertexStart": vertex_start,
                "indexStart": index_start,
                "indexCount": len(indices),
            }
        )
        vertex_start += len(vertices)
        index_start += len(indices)

    with open(path, "wb") as fp:
        write_frame_header_v11(fp, len(subset_headers), len(positions), 16, True, True, 0, len(index_buffer))
        for pos in positions:
            write_vec3(fp, pos)
        for normal in normals:
            write_vec3(fp, normal)
        for uv in uvs:
            write_vec2(fp, uv)
        for index in index_buffer:
            write_u16(fp, index)
        for header in subset_headers:
            write_subset_desc_v11(fp, header["texture"], header["extraTextures"], header["vertexCount"], header["vertexStart"],
                                   header["indexStart"], header["indexCount"])


def write_direct_frame_chunk_vb_only(path: str, subsets: list[dict[str, Any]], args: argparse.Namespace) -> None:
    vertex_start = 0
    subset_headers: list[dict[str, Any]] = []
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    uvs: list[tuple[float, float]] = []

    for subset_index, subset in enumerate(subsets, start=1):
        vertices = subset.get("vertices") or []
        indices = subset.get("indices") or []
        if not vertices or not indices:
            raise RuntimeError(f"Subset {subset_index} has no vertices or indices.")
        if (len(indices) % 3) != 0:
            raise RuntimeError(f"Subset {subset_index} indices must be divisible by 3 for VB-only export.")
        apply_direct_vertex_options(vertices, args)
        for index in indices:
            idx = int(index)
            if idx < 1 or idx > len(vertices):
                raise RuntimeError(f"Subset {subset_index} index {idx} is out of range.")
            append_vertex_stream(vertices[idx - 1], positions, normals, uvs)
        subset_headers.append(
            {
                "texture": texture_name_for_msh(str(subset.get("texture") or "")),
                "extraTextures": [
                    {"role": int(extra["role"]), "texture": texture_name_for_msh(str(extra.get("texture") or ""))}
                    for extra in (subset.get("extraTextures") or [])
                ],
                "vertexCount": len(indices),
                "vertexStart": vertex_start,
                "indexStart": 0,
                "indexCount": 0,
            }
        )
        vertex_start += len(indices)

    with open(path, "wb") as fp:
        write_frame_header_v11(fp, len(subset_headers), len(positions), 16, True, True, 0, 0)
        for pos in positions:
            write_vec3(fp, pos)
        for normal in normals:
            write_vec3(fp, normal)
        for uv in uvs:
            write_vec2(fp, uv)
        for header in subset_headers:
            write_subset_desc_v11(fp, header["texture"], header["extraTextures"], header["vertexCount"], header["vertexStart"],
                                   header["indexStart"], header["indexCount"])


def write_direct_frame_chunk(path: str, subsets: list[dict[str, Any]], args: argparse.Namespace) -> None:
    if args.large_mesh_mode == "vb_only":
        write_direct_frame_chunk_vb_only(path, subsets, args)
        return
    write_direct_frame_chunk_indexed(path, subsets, args)


def build_direct_msh_output(args: argparse.Namespace, out_path: str) -> int:
    source_path = os.path.abspath(args.input)
    debug_print(args.debug_steps, f"import source: {source_path}")
    import_source(source_path)
    scene = bpy.context.scene
    mesh_cache_issues = get_mesh_cache_issues(scene)
    if args.bake_animation and mesh_cache_issues:
        details = "; ".join(issue.get("message", "") for issue in mesh_cache_issues)
        raise RuntimeError(f"Cannot bake mesh-cache animation: {details}")

    clips = parse_animation_clips(args, scene)
    temp_root = tempfile.mkdtemp(prefix="mbm_direct_msh_")
    frame_paths: list[str] = []
    animations: list[dict[str, Any]] = []
    texture_paths: set[str] = set()
    bounds = {"min": [float("inf"), float("inf"), float("inf")], "max": [-float("inf"), -float("inf"), -float("inf")]}
    try:
        fps = get_scene_fps(scene)
        # Baked directly into vertex/normal/bone data below (rotate_point_deg) instead of being
        # written into SECTION_MATERIAL_TRANSFORM's angle field, which the engine no longer applies
        # at load time (see rotate_point_deg's own docstring).
        import_rotation_deg = (float(args.angle_x), float(args.angle_y), float(args.angle_z)) if args.post_process else None
        canonical_skeleton = extract_canonical_skeleton(scene, import_rotation_deg) if args.include_bones else None
        canonical_clips = (extract_canonical_animations(scene, clips, fps, canonical_skeleton)
                           if canonical_skeleton and clips else [])
        # Embedded/packed textures (common in Mixamo downloads) get unpacked next to the output
        # .msh -- see get_first_texture_path/_extract_embedded_image -- rather than trusting the
        # FBX's own recorded source path, which points at wherever the original author's machine
        # had the file and is otherwise meaningless on this one.
        output_dir = os.path.dirname(os.path.abspath(out_path))
        # Real per-vertex weights only make sense alongside a real skeleton (--include-bones), and
        # only for the default indexed write path -- see build_vertex_skin_weights_payload_v11's
        # own docstring for why vb_only mode is excluded.
        capture_weights = bool(args.include_bones) and args.large_mesh_mode != "vb_only"
        # SECTION_VERTEX_SKIN_WEIGHTS is a bind-pose property (docs/mesh-v11-format.md Sec. 6f) --
        # tied to frame 1's topology only, so only the very first exported frame's subsets are kept
        # around for build_vertex_skin_weights_payload_v11 below.
        first_frame_subsets: list[dict[str, Any]] | None = None

        def export_frame_to_chunk(frame: int) -> None:
            nonlocal first_frame_subsets
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            debug_print(args.debug_steps, f"export frame: {frame}")
            subsets = export_frame_subsets(scene, import_rotation_deg, output_dir, capture_weights,
                                           canonical_coordinates=bool(args.include_bones))
            if args.large_mesh_mode == "vb_only":
                debug_print(args.debug_steps, "large mesh mode: vertex buffer only")
            else:
                validate_frame_vertex_limit(subsets)
            for subset in subsets:
                add_texture_search_path(texture_paths, str(subset.get("texture") or ""))
                for extra in (subset.get("extraTextures") or []):
                    add_texture_search_path(texture_paths, str(extra.get("texture") or ""))
                update_bounds(bounds, subset.get("vertices") or [])
            frame_path = os.path.join(temp_root, f"frame_{out_index:06d}.bin")
            write_direct_frame_chunk(frame_path, subsets, args)
            frame_paths.append(frame_path)
            if first_frame_subsets is None:
                first_frame_subsets = subsets
            check_cancel_requested(args.cancel_file)

        out_index = 0
        if canonical_skeleton:
            # Canonical weights and inverse bind target the undeformed bind geometry, never an
            # arbitrary sampled pose. Animation lives in type-43 local tracks instead of duplicate
            # SECTION_FRAME_STATIC geometry.
            armature_obj = canonical_skeleton["armatureObject"]
            previous_pose_position = armature_obj.data.pose_position
            armature_obj.data.pose_position = "REST"
            try:
                out_index = 1
                export_frame_to_chunk(max(1, int(args.frame_start)))
            finally:
                armature_obj.data.pose_position = previous_pose_position
            animations = [{
                "name": "default", "initialFrame": 1, "finalFrame": 1,
                "timeBetweenFrame": 0.0, "typeAnimation": 1,
            }]
        elif clips:
            for clip in clips:
                target_start = len(frame_paths) + 1
                debug_print(
                    args.debug_steps,
                    f"bake animation clip: {clip.get('name', 'Bake')} {clip['frameStart']}..{clip['frameEnd']} step={clip['sampleStep']}",
                )
                for frame in clip_frame_numbers(clip):
                    check_cancel_requested(args.cancel_file)
                    out_index += 1
                    export_frame_to_chunk(frame)
                append_animation_header_for_clip(animations, clip, target_start, len(frame_paths), fps)
        else:
            out_index = 1
            frame = max(1, int(args.frame_start))
            check_cancel_requested(args.cancel_file)
            debug_print(args.debug_steps, f"export current frame: {frame}")
            export_frame_to_chunk(frame)
            animations = [
                {
                    "name": "default",
                    "initialFrame": 1,
                    "finalFrame": 1,
                    "timeBetweenFrame": 0.0,
                    "typeAnimation": 1,
                }
            ]

        # Always zero -- the rotation is baked into vertex/normal/bone data above now, not stored
        # in this field (see import_rotation_deg's own comment).
        angles = (0.0, 0.0, 0.0)
        texture_path_list = sorted(texture_paths)

        if canonical_skeleton:
            debug_print(args.debug_steps, f"extracted canonical armature: {len(canonical_skeleton['bones'])} bone(s)")

        canonical_weights_payload = None
        if canonical_skeleton and first_frame_subsets:
            canonical_weights_payload = build_canonical_weights_payload_v11(first_frame_subsets, canonical_skeleton)
        if canonical_weights_payload is not None:
            debug_print(args.debug_steps, "captured canonical per-vertex skin weights")
        canonical_animations_payload = (build_canonical_animations_payload_v11(
            canonical_skeleton["skeletonId"], canonical_clips) if canonical_clips else None)
        if canonical_animations_payload is not None:
            debug_print(args.debug_steps, f"captured canonical animation: {len(canonical_clips)} clip(s)")

        section_count = 1  # SECTION_MATERIAL_TRANSFORM
        if texture_path_list:
            section_count += 1  # SECTION_EXTRA_PATHS
        section_count += 1  # SECTION_DETAIL_PHYSICS
        section_count += len(animations)
        section_count += len(frame_paths)
        if canonical_skeleton:
            section_count += 1  # SECTION_SKELETAL_SKELETON
        if canonical_weights_payload is not None:
            section_count += 1  # SECTION_SKELETAL_WEIGHTS
        if canonical_animations_payload is not None:
            section_count += 1  # SECTION_SKELETAL_ANIMATION

        check_cancel_requested(args.cancel_file)
        debug_print(args.debug_steps, "writing output")
        tmp_out = f"{out_path}.tmp.{os.getpid()}"
        with open(tmp_out, "wb") as fp:
            write_file_header_v11(fp, TYPE_MESH_3D, 0, 0, section_count)

            write_section_v11(fp, SECTION_MATERIAL_TRANSFORM, 1, build_material_transform_payload_v11(angles), False)

            if texture_path_list:
                write_section_v11(fp, SECTION_EXTRA_PATHS, 1, build_extra_paths_payload_v11(texture_path_list), False)

            write_section_v11(fp, SECTION_DETAIL_PHYSICS, 1, build_detail_physics_payload_v11(finalize_bounds(bounds)),
                               False)

            for anim in animations:
                write_section_v11(fp, SECTION_ANIMATION, 1, build_animation_payload_v11(anim), False)

            for frame_path in frame_paths:
                with open(frame_path, "rb") as frame_fp:
                    frame_payload = frame_fp.read()
                write_section_v11(fp, SECTION_FRAME_STATIC, 1, frame_payload, True)

            if canonical_skeleton:
                write_section_v11(fp, SECTION_SKELETAL_SKELETON, 1,
                                  build_canonical_skeleton_payload_v11(canonical_skeleton), False)

            if canonical_weights_payload is not None:
                write_section_v11(fp, SECTION_SKELETAL_WEIGHTS, 1, canonical_weights_payload, False)

            if canonical_animations_payload is not None:
                write_section_v11(fp, SECTION_SKELETAL_ANIMATION, 1, canonical_animations_payload, False)

            fp.flush()
            os.fsync(fp.fileno())
        check_cancel_requested(args.cancel_file)
        os.replace(tmp_out, out_path)
        return len(frame_paths)
    finally:
        shutil.rmtree(temp_root, ignore_errors=True)


def main() -> int:
    argv = []
    if "--" in sys.argv:
        argv = sys.argv[sys.argv.index("--") + 1 :]
    args = parse_args(argv)
    apply_numpy_compat_shim(args.debug_steps)

    out_path = os.path.abspath(args.output)
    debug_print(args.debug_steps, f"output: {out_path}")

    if args.direct_msh_output and not args.scan_only:
        out_dir = os.path.dirname(out_path)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)
        frames_exported = build_direct_msh_output(args, out_path)
        debug_print(args.debug_steps, f"frames exported: {frames_exported}")
        debug_print(args.debug_steps, "done")
        return 0

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
        from mathutils import Vector  # type: ignore
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
