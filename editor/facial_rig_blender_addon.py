"""Blender UI entry point for the mini-mbm facial rig bridge preview.

Run once from Blender's scripting workspace or with ``blender --python``. The script registers a
small View3D sidebar panel. It imports the bridge into the currently open .blend, so the user can
review the armature against the existing mesh before saving manually. No vertex groups or mesh
coordinates are changed.
"""

from __future__ import annotations

import json
import sys
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
)


def mesh_matches(scene, vertex_count: int) -> list[str]:
    return [obj.name for obj in scene.objects
            if obj.type == "MESH" and len(obj.data.vertices) == vertex_count]


def scene_mesh_vertex_total(scene) -> int:
    return sum(len(obj.data.vertices) for obj in scene.objects if obj.type == "MESH")


def read_json_property(obj, name: str) -> dict:
    value = obj.get(name, "")
    if not value:
        return {}
    try:
        result = json.loads(value)
        return result if isinstance(result, dict) else {}
    except (TypeError, ValueError):
        return {}


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
            if context.object and context.object.mode != "OBJECT":
                bpy.ops.object.mode_set(mode="OBJECT")
            data = parse_bridge(self.filepath)
            armature, count = create_armature(
                data,
                (self.angle_x, self.angle_y, self.angle_z),
                self.replace_preview,
                False,
            )
            context.view_layer.objects.active = armature
            armature.select_set(True)
            context.scene["mini_mbm_facial_bridge_path"] = str(Path(self.filepath).resolve())
            context.scene["mini_mbm_facial_bridge_schema"] = data["schema"]
            context.scene["mini_mbm_facial_bridge_vertex_count"] = int(data.get("vertexCount") or 0)
            self.report({"INFO"}, f"Imported facial rig preview ({count} bones). Save the .blend manually.")
            return {"FINISHED"}
        except Exception as error:
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


CLASSES = (MBM_OT_import_facial_rig_bridge, MBM_PT_facial_rig_bridge)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
