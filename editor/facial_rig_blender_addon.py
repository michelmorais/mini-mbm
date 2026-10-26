"""Blender UI entry point for the mini-mbm facial rig bridge workflow.

Run once from Blender's scripting workspace or with ``blender --python``. The script registers a
small View3D sidebar panel. It imports the bridge into the currently open .blend, so the user can
review the armature against the existing mesh before saving manually. It can also create an
optional conservative starter set of Blender Shape Keys from the bridge landmarks. The generated
keys are authoring aids; the user must review the result before exporting a runtime MSH.
"""

from __future__ import annotations

import json
import sys
import traceback
from pathlib import Path

import bpy
from bpy.props import BoolProperty, FloatProperty, StringProperty
from bpy.types import Operator, Panel
from bpy_extras.io_utils import ImportHelper


MODULE_DIR = str(Path(__file__).resolve().parent)
if MODULE_DIR not in sys.path:
    sys.path.insert(0, MODULE_DIR)

from facial_rig_blender_import import (  # noqa: E402
    create_armature,
    parse_bridge,
    rotate_point,
)


def mesh_matches(scene, vertex_count: int) -> list[str]:
    return [obj.name for obj in scene.objects
            if obj.type == "MESH" and len(obj.data.vertices) == vertex_count]


def scene_mesh_vertex_total(scene) -> int:
    return sum(len(obj.data.vertices) for obj in scene.objects if obj.type == "MESH")


STARTER_TARGET_NAMES = (
    "jawOpen", "mouthSmile", "mouthPucker", "eyeBlinkLeft", "eyeBlinkRight", "browUp"
)


def existing_starter_target_names(mesh_obj):
    shape_keys = mesh_obj.data.shape_keys
    if shape_keys is None:
        return []
    names = {key.name for key in shape_keys.key_blocks}
    return [name for name in STARTER_TARGET_NAMES if name in names]


def _normalize(vector, fallback):
    from mathutils import Vector
    result = Vector(vector)
    if result.length <= 1.0e-8:
        return Vector(fallback)
    return result.normalized()


def _clamp(value, lower=0.0, upper=1.0):
    return max(lower, min(upper, float(value)))


def _smoothstep(value):
    value = _clamp(value)
    return value * value * (3.0 - 2.0 * value)


def _ellipse_weight(u, v, center_u, center_v, radius_u, radius_v):
    radius_u = max(abs(radius_u), 1.0e-6)
    radius_v = max(abs(radius_v), 1.0e-6)
    distance = ((u - center_u) / radius_u) ** 2 + ((v - center_v) / radius_v) ** 2
    return _smoothstep(1.0 - distance)


def _landmark_points(data, mesh_obj, angles):
    """Convert bridge landmark positions to Blender world space."""
    from mathutils import Vector
    landmarks = {}
    binding = data.get("binding") or {}
    for entry in binding.get("landmarks") or []:
        name = str(entry.get("name") or "")
        point = entry.get("position") or {}
        if name:
            world = Vector(rotate_point(point, angles))
            landmarks[name] = world
    return landmarks


def _starter_frame(data, mesh_obj, angles):
    """Build a local face frame from landmarks, with mesh-bounds fallbacks."""
    from mathutils import Vector
    points = _landmark_points(data, mesh_obj, angles)
    vertices = [mesh_obj.matrix_world @ vertex.co for vertex in mesh_obj.data.vertices]
    if not vertices:
        raise ValueError("selected facial mesh has no vertices")
    bounds_min = Vector((min(point.x for point in vertices), min(point.y for point in vertices), min(point.z for point in vertices)))
    bounds_max = Vector((max(point.x for point in vertices), max(point.y for point in vertices), max(point.z for point in vertices)))
    bounds_center_world = (bounds_min + bounds_max) * 0.5
    top = points.get("face_top", bounds_max)
    bottom = points.get("face_bottom", bounds_min)
    left = points.get("face_left", bounds_center_world - Vector((1.0, 0.0, 0.0)))
    right = points.get("face_right", bounds_center_world + Vector((1.0, 0.0, 0.0)))
    vertical = _normalize(top - bottom, (0.0, 0.0, 1.0))
    horizontal = _normalize(right - left, (1.0, 0.0, 0.0))
    horizontal = _normalize(horizontal - vertical * horizontal.dot(vertical), (1.0, 0.0, 0.0))
    depth = _normalize(horizontal.cross(vertical), (0.0, 1.0, 0.0))
    nose = points.get("nose_tip", points.get("nose_bridge", bounds_center_world))
    if depth.dot(nose - bounds_center_world) < 0.0:
        depth.negate()
    center = points.get("nose_bridge", points.get("nose_tip", bounds_center_world))
    width = max(abs((right - left).dot(horizontal)), (bounds_max - bounds_min).length * 0.1, 1.0e-4)
    height = max(abs((top - bottom).dot(vertical)), (bounds_max - bounds_min).length * 0.1, 1.0e-4)
    return points, center, horizontal, vertical, depth, width, height


def _create_starter_targets(data, mesh_obj, angles, replace_existing=False):
    """Create conservative Basis-relative starter targets on one Blender mesh."""
    from mathutils import Vector
    shape_keys = mesh_obj.data.shape_keys
    existing_names = {key.name for key in shape_keys.key_blocks} if shape_keys else set()
    generated_names = set(STARTER_TARGET_NAMES)
    if existing_names.intersection(generated_names) and not replace_existing:
        names = sorted(existing_names.intersection(generated_names))
        raise ValueError("starter targets already exist: " + ", ".join(names) + "; enable Replace generated targets to rebuild")
    if shape_keys is None:
        basis = mesh_obj.shape_key_add(name="Basis")
    else:
        basis = shape_keys.key_blocks[0]
        if basis.name != "Basis":
            basis.name = "Basis"
    if not basis:
        raise ValueError("could not create the Basis shape key")
    if replace_existing and mesh_obj.data.shape_keys:
        for name in STARTER_TARGET_NAMES:
            key = mesh_obj.data.shape_keys.key_blocks.get(name)
            if key is not None:
                mesh_obj.shape_key_remove(key)

    points, center, horizontal, vertical, depth, width, height = _starter_frame(data, mesh_obj, angles)
    def coordinate(point):
        delta = point - center
        return delta.dot(horizontal) / width, delta.dot(vertical) / height, delta.dot(depth) / width

    normalized_points = [coordinate(mesh_obj.matrix_world @ vertex.co) for vertex in mesh_obj.data.vertices]
    def landmark_uv(name, fallback):
        point = points.get(name)
        return coordinate(point) if point is not None else fallback

    mouth_left = landmark_uv("mouth_left", (-0.18, -0.22, 0.0))
    mouth_right = landmark_uv("mouth_right", (0.18, -0.22, 0.0))
    mouth_upper = landmark_uv("upper_lip", (0.0, -0.18, 0.0))
    mouth_lower = landmark_uv("lower_lip", (0.0, -0.25, 0.0))
    mouth_u = (mouth_left[0] + mouth_right[0]) * 0.5
    mouth_v = (mouth_upper[1] + mouth_lower[1]) * 0.5
    mouth_width = max(abs(mouth_right[0] - mouth_left[0]) * 0.5, 0.08)
    mouth_height = max(abs(mouth_upper[1] - mouth_lower[1]) * 1.8, 0.06)
    chin = landmark_uv("chin", (0.0, -0.48, 0.0))
    bottom_v = chin[1]
    eye_left = landmark_uv("left_eye_center", (-0.18, 0.10, 0.0))
    eye_right = landmark_uv("right_eye_center", (0.18, 0.10, 0.0))
    brow_left = landmark_uv("brow_left", (-0.18, 0.23, 0.0))
    brow_right = landmark_uv("brow_right", (0.18, 0.23, 0.0))
    inverse_basis = mesh_obj.matrix_world.to_3x3().inverted()

    def delta_for(name, u, v, d):
        if name == "jawOpen":
            region = _smoothstep((mouth_v - v) / max(mouth_v - bottom_v, 1.0e-5))
            region *= _smoothstep(1.0 - abs(u) / 0.72)
            return vertical * (-height * 0.16 * region)
        if name == "mouthSmile":
            region = _ellipse_weight(u, v, mouth_u, mouth_v, mouth_width * 1.35, mouth_height)
            side = _clamp(abs(u - mouth_u) / max(mouth_width, 1.0e-5))
            return horizontal * (width * 0.06 * (1.0 if u >= mouth_u else -1.0) * region) + \
                vertical * (height * 0.08 * side * region)
        if name == "mouthPucker":
            region = _ellipse_weight(u, v, mouth_u, mouth_v, mouth_width * 1.35, mouth_height)
            return depth * (width * 0.10 * region)
        if name == "eyeBlinkLeft" or name == "eyeBlinkRight":
            eye = eye_left if name.endswith("Left") else eye_right
            region = _ellipse_weight(u, v, eye[0], eye[1], 0.13, 0.075)
            return vertical * (height * (eye[1] - v) * 0.28 * region)
        if name == "browUp":
            left_weight = _ellipse_weight(u, v, brow_left[0], brow_left[1], 0.18, 0.08)
            right_weight = _ellipse_weight(u, v, brow_right[0], brow_right[1], 0.18, 0.08)
            return vertical * (height * 0.10 * max(left_weight, right_weight))
        return Vector((0.0, 0.0, 0.0))

    created = []
    for name in STARTER_TARGET_NAMES:
        key = mesh_obj.shape_key_add(name=name)
        key.relative_key = basis
        key.slider_min = 0.0
        key.slider_max = 1.0
        key.value = 0.0
        for index, (u, v, d) in enumerate(normalized_points):
            delta_local = inverse_basis @ delta_for(name, u, v, d)
            key.data[index].co = basis.data[index].co + delta_local
        created.append(name)
    mesh_obj.data["mini_mbm_facial_target_convention"] = "ARKit-compatible starter targets"
    mesh_obj.data["mini_mbm_facial_target_names"] = json.dumps(created)
    return created


def find_facial_mesh(scene, data):
    """Select the source mesh using the bridge role name, then vertex count."""
    vertex_count = int(data.get("vertexCount") or 0)
    binding = data.get("binding") or {}
    main_face = next((row for row in binding.get("roles") or [] if row.get("name") == "main_face"), None)
    object_name = str(main_face.get("object") or "") if main_face else ""
    if object_name:
        candidate = bpy.data.objects.get(object_name)
        if candidate is not None and candidate.type == "MESH" and \
                (vertex_count <= 0 or len(candidate.data.vertices) == vertex_count):
            return candidate
    matches = [obj for obj in scene.objects if obj.type == "MESH" and len(obj.data.vertices) == vertex_count]
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise ValueError(f"no mesh matches the bridge vertex count ({vertex_count})")
    raise ValueError("multiple meshes match the bridge vertex count; assign main_face to one mesh object")


def read_json_property(obj, name: str) -> dict:
    value = obj.get(name, "")
    if not value:
        return {}
    try:
        result = json.loads(value)
        return result if isinstance(result, dict) else {}
    except (TypeError, ValueError):
        return {}


def command_line_bridge() -> str:
    """Return an optional bridge path passed after Blender's ``--`` separator."""
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    for index, argument in enumerate(arguments):
        if argument == "--bridge" and index + 1 < len(arguments):
            return arguments[index + 1]
        if argument.startswith("--bridge="):
            return argument.split("=", 1)[1]
    return ""


def command_line_flag(name: str) -> bool:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return name in arguments


def command_line_output() -> str:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    for index, argument in enumerate(arguments):
        if argument == "--output" and index + 1 < len(arguments):
            return arguments[index + 1]
        if argument.startswith("--output="):
            return argument.split("=", 1)[1]
    return ""


def set_import_status(scene, status: str, error: str = "", path: str = "") -> None:
    scene["mini_mbm_facial_bridge_status"] = status
    scene["mini_mbm_facial_bridge_error"] = error
    if path:
        scene["mini_mbm_facial_bridge_path"] = str(Path(path).expanduser().resolve())


def source_armature_scale(scene):
    """Find the source skeleton scale used by Blender's mesh import workflow."""
    for obj in scene.objects:
        if obj.type != "ARMATURE" or obj.name.startswith("FacialRigPreview"):
            continue
        return obj.scale.copy(), obj.name
    return None, ""


def import_bridge(context, filepath: str, angle_x: float = 90.0,
                  angle_y: float = 0.0, angle_z: float = 0.0,
                  replace_preview: bool = True):
    """Import one bridge and leave a persistent status in the scene custom properties."""
    if not filepath:
        raise ValueError("no facial rig bridge file was selected")
    scene = context.scene
    try:
        source_scale, source_name = source_armature_scale(scene)
        if context.object and context.object.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        data = parse_bridge(filepath)
        armature, count = create_armature(
            data,
            (angle_x, angle_y, angle_z),
            replace_preview,
            False,
        )
        if source_scale is not None:
            armature.scale = source_scale
            armature["mini_mbm_source_armature"] = source_name
            armature["mini_mbm_source_armature_scale"] = tuple(float(value) for value in source_scale)
        # Keep the imported preview visible and make it the only selected object. This
        # avoids the common case where the armature exists but remains hidden in a mesh
        # selection, which looks like a no-op in the viewport.
        for obj in context.selected_objects:
            obj.select_set(False)
        armature.hide_set(False)
        armature.hide_viewport = False
        armature.hide_render = False
        context.view_layer.objects.active = armature
        armature.select_set(True)
        scene["mini_mbm_facial_bridge_schema"] = data["schema"]
        scene["mini_mbm_facial_bridge_vertex_count"] = int(data.get("vertexCount") or 0)
        set_import_status(scene, f"Imported facial rig preview ({count} bones).", "", filepath)
        return armature, count
    except Exception as error:
        message = str(error)
        set_import_status(scene, "Facial rig bridge import failed.", message, filepath)
        print(f"[mini_mbm_facial] ERROR: {message}", file=sys.stderr, flush=True)
        traceback.print_exc()
        raise


class MBM_OT_import_facial_rig_bridge(Operator, ImportHelper):
    bl_idname = "mbm.import_facial_rig_bridge"
    bl_label = "Import Facial Rig Bridge"
    bl_description = "Create a read-only facial armature preview from a mini-mbm bridge file"
    filename_ext = ".lua"
    filter_glob: StringProperty(default="*.lua", options={"HIDDEN"})
    angle_x: FloatProperty(name="Engine X rotation", default=90.0,
                           description="Coordinate conversion from mini-mbm Y-up to Blender Z-up; keep 90")
    angle_y: FloatProperty(name="Engine Y rotation", default=0.0)
    angle_z: FloatProperty(name="Engine Z rotation", default=0.0)
    replace_preview: BoolProperty(name="Replace previous preview", default=True)

    def execute(self, context):
        try:
            _, count = import_bridge(
                context,
                self.filepath,
                self.angle_x,
                self.angle_y,
                self.angle_z,
                self.replace_preview,
            )
            self.report({"INFO"}, f"Imported facial rig preview ({count} bones). Save the .blend manually.")
            return {"FINISHED"}
        except Exception as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}


class MBM_OT_generate_facial_targets(Operator):
    bl_idname = "mbm.generate_facial_targets"
    bl_label = "Generate Starter Facial Targets"
    bl_description = "Create conservative Basis-relative starter Shape Keys from the mini-mbm landmarks"
    replace_existing: BoolProperty(
        name="Replace generated targets",
        description="Replace only the standard starter target names; other Shape Keys are preserved",
        default=False,
    )

    def execute(self, context):
        bridge_path = context.scene.get("mini_mbm_facial_bridge_path", "")
        if not bridge_path:
            self.report({"ERROR"}, "Import a mini-mbm facial rig bridge first")
            return {"CANCELLED"}
        try:
            data = parse_bridge(bridge_path)
            mesh_obj = find_facial_mesh(context.scene, data)
            existing = existing_starter_target_names(mesh_obj)
            if existing and not self.replace_existing:
                message = "Starter facial targets already exist on " + mesh_obj.name + ": " + ", ".join(existing)
                context.scene["mini_mbm_facial_target_status"] = message + ". No changes made."
                self.report({"INFO"}, context.scene["mini_mbm_facial_target_status"])
                return {"FINISHED"}
            names = _create_starter_targets(
                data, mesh_obj, (90.0, 0.0, 0.0), self.replace_existing)
            context.view_layer.objects.active = mesh_obj
            for obj in context.selected_objects:
                obj.select_set(False)
            mesh_obj.select_set(True)
            context.scene["mini_mbm_facial_target_status"] = (
                f"Generated {len(names)} starter Shape Keys on {mesh_obj.name}. Review and save a new .blend.")
            self.report({"INFO"}, context.scene["mini_mbm_facial_target_status"])
            return {"FINISHED"}
        except Exception as error:
            context.scene["mini_mbm_facial_target_status"] = f"Starter target generation failed: {error}"
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}


class MBM_PT_facial_rig_bridge(Panel):
    bl_label = "Mini MBM Facial"
    bl_idname = "MBM_PT_facial_rig_bridge"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Mini MBM"

    def draw(self, context):
        layout = self.layout
        layout.operator(MBM_OT_import_facial_rig_bridge.bl_idname, icon="ARMATURE_DATA")
        status = context.scene.get("mini_mbm_facial_bridge_status", "")
        error = context.scene.get("mini_mbm_facial_bridge_error", "")
        if status:
            status_box = layout.box()
            status_box.label(text=status, icon="ERROR" if error else "CHECKMARK")
            if error:
                status_box.label(text=error[:180], icon="INFO")
            bridge_path = context.scene.get("mini_mbm_facial_bridge_path", "")
            if bridge_path:
                status_box.label(text=Path(bridge_path).name)
        armature = context.scene.objects.get("FacialRigPreview")
        if armature is None or armature.type != "ARMATURE":
            layout.label(text="No facial rig preview imported.", icon="INFO")
            return
        box = layout.box()
        box.label(text=armature.name, icon="ARMATURE_DATA")
        box.label(text=f"Bones: {len(armature.data.bones)}")
        vertex_count = int(armature.get("mini_mbm_vertex_count", 0))
        box.label(text=f"Bridge vertices: {vertex_count}")
        matches = mesh_matches(context.scene, vertex_count)
        if matches:
            box.label(text=f"Mesh vertex count matches: {matches[0]}", icon="CHECKMARK")
        else:
            total = scene_mesh_vertex_total(context.scene)
            box.label(text="No single mesh has the bridge count", icon="INFO")
            box.label(text=f"Scene mesh total: {total}")
            box.label(text="Use viewport alignment for merged assets.")
        box.label(text="Preview only: no weights assigned.", icon="INFO")
        notice = box.box()
        notice.label(text="Rig preview only", icon="INFO")
        notice.label(text="This preview does not assign weights.")
        notice.label(text="Use Expressions for starter Shape Keys.")
        notice.label(text="Use MeshDebug Blender import")
        notice.label(text="with Import facial targets enabled.")
        existing = []
        bridge_path = context.scene.get("mini_mbm_facial_bridge_path", "")
        if bridge_path:
            try:
                bridge_data = parse_bridge(bridge_path)
                target_mesh = find_facial_mesh(context.scene, bridge_data)
                existing = existing_starter_target_names(target_mesh)
            except Exception:
                existing = []
        button_row = layout.row()
        button_row.enabled = not bool(existing)
        button_row.operator(MBM_OT_generate_facial_targets.bl_idname, icon="SHAPEKEY_DATA")
        if existing:
            completed = layout.box()
            completed.label(text="Starter Shape Keys already exist", icon="CHECKMARK")
            completed.label(text=", ".join(existing))
        target_status = context.scene.get("mini_mbm_facial_target_status", "")
        if target_status:
            status_box = layout.box()
            status_box.label(text=target_status[:180], icon="INFO")
        report = read_json_property(armature, "mini_mbm_role_report")
        box.label(text=f"Role entries: {len(report.get('entries') or [])}")
        for warning in report.get("warnings") or []:
            box.label(text=f"Warning: {warning}", icon="ERROR")
        binding = read_json_property(armature, "mini_mbm_binding_report")
        roles = binding.get("roles") or []
        if roles:
            box.separator()
            box.label(text="Role object review:")
            for role in roles:
                name = str(role.get("name") or "role")
                object_name = str(role.get("object") or "")
                target = bpy.data.objects.get(object_name) if object_name else None
                if target is not None and target.type == "MESH":
                    box.label(text=f"{name}: {object_name}", icon="CHECKMARK")
                elif object_name:
                    box.label(text=f"{name}: missing {object_name}", icon="ERROR")
                else:
                    box.label(text=f"{name}: unassigned", icon="INFO")


CLASSES = (MBM_OT_import_facial_rig_bridge, MBM_OT_generate_facial_targets, MBM_PT_facial_rig_bridge)


def auto_import_bridge():
    filepath = command_line_bridge()
    if not filepath:
        return None
    try:
        import_bridge(bpy.context, filepath)
        if command_line_flag("--generate-targets"):
            data = parse_bridge(filepath)
            mesh_obj = find_facial_mesh(bpy.context.scene, data)
            names = _create_starter_targets(data, mesh_obj, (90.0, 0.0, 0.0), False)
            status = f"Generated {len(names)} starter Shape Keys on {mesh_obj.name}."
            bpy.context.scene["mini_mbm_facial_target_status"] = status
            output = command_line_output()
            if output:
                bpy.ops.wm.save_as_mainfile(filepath=str(Path(output).expanduser().resolve()))
                print(f"[mini_mbm_facial] {status} -> {output}", flush=True)
            else:
                print(f"[mini_mbm_facial] {status} Save the .blend manually.", flush=True)
        print(f"[mini_mbm_facial] imported bridge: {filepath}", flush=True)
    except Exception as error:
        # Keep the timer one-shot but leave a useful diagnostic for both bridge import and
        # assisted target generation failures.
        print(f"[mini_mbm_facial] ERROR: {error}", file=sys.stderr, flush=True)
        traceback.print_exc()
    return None


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    if command_line_bridge():
        # Blender may still be creating the initial view layer while --python runs.
        # A one-shot timer gives the normal UI context time to become available.
        bpy.app.timers.register(auto_import_bridge, first_interval=0.25)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
