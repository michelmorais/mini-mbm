#!/usr/bin/env python3
"""
Blender helper addon: adjust body proportions of a rigged (T-pose) character
before uploading it to mixamo.com, then prepare + export a clean FBX.

This does NOT sculpt geometry. It scales existing bones (Pose Mode scale,
the same technique used by body-slider character creators) and lets the
skin weights that already exist on the mesh follow along. Typical uses:
stretch/shrink the head, make the character taller/shorter, make one arm
thicker than the other, etc.

How to use inside Blender:
    1. Open the Scripting tab, "Open" this file, click "Run Script"
       (or install it as a regular addon via Preferences > Add-ons > Install).
    2. A new "Mixamo Prep" tab appears in the 3D Viewport sidebar (press N).
    3. Select your rigged character, click "Escanear Armature".
    4. Move the sliders for whichever body part you want to resize.
    5. Click "Gravar Ajuste na Geometria" to bake the adjustment into the
       mesh geometry (this resets the sliders back to 1.0 - that's expected,
       the change is now permanent in the mesh).
    6. Click "Preparar e Exportar para Mixamo (FBX)" to strip cameras/lights/
       extra objects, join mesh parts, center the character at the origin,
       and export a single FBX ready to upload to mixamo.com.
"""

from __future__ import annotations

import re

import bpy
from bpy_extras.io_utils import ExportHelper
from mathutils import Vector


bl_info = {
    "name": "Mixamo Body Adjust",
    "description": "Resize body parts of a rigged character and export a clean FBX for mixamo.com",
    "author": "mini-mbm",
    "version": (1, 0, 0),
    "blender": (3, 0, 0),
    "category": "Object",
}


# (key, friendly label in pt-BR, candidate raw bone names, in priority order)
FRIENDLY_SLOTS = [
    ("hips", "Quadril / Altura geral", ["Hips"]),
    ("spine", "Torax", ["Spine1", "Spine2", "Spine"]),
    ("neck", "Pescoco", ["Neck"]),
    ("head", "Cabeca", ["Head"]),
    ("left_arm", "Braco Esquerdo (superior)", ["LeftArm"]),
    ("left_forearm", "Antebraco Esquerdo", ["LeftForeArm"]),
    ("left_hand", "Mao Esquerda", ["LeftHand"]),
    ("right_arm", "Braco Direito (superior)", ["RightArm"]),
    ("right_forearm", "Antebraco Direito", ["RightForeArm"]),
    ("right_hand", "Mao Direita", ["RightHand"]),
    ("left_upleg", "Coxa Esquerda", ["LeftUpLeg"]),
    ("left_leg", "Perna Esquerda (canela)", ["LeftLeg"]),
    ("left_foot", "Pe Esquerdo", ["LeftFoot"]),
    ("right_upleg", "Coxa Direita", ["RightUpLeg"]),
    ("right_leg", "Perna Direita (canela)", ["RightLeg"]),
    ("right_foot", "Pe Direito", ["RightFoot"]),
]

# Stripped as a best-effort prefix when matching candidate names against the
# armature's actual bone names (covers mixamorig:, DEF-, ORG-, rig_, ...).
_PREFIX_RE = re.compile(r'^[A-Za-z0-9]+[:_|-]')


def _normalize_bone_name(raw_name: str) -> str:
    return _PREFIX_RE.sub('', raw_name).strip()


def _find_armature(context) -> bpy.types.Object | None:
    active = context.view_layer.objects.active
    if active and active.type == 'ARMATURE':
        return active
    for obj in context.selected_objects:
        if obj.type == 'ARMATURE':
            return obj
    for obj in context.scene.objects:
        if obj.type == 'ARMATURE':
            return obj
    return None


def _auto_match_bone(armature_obj: bpy.types.Object, candidates: list[str]) -> str:
    bone_names = list(armature_obj.pose.bones.keys())
    normalized = {_normalize_bone_name(name).lower(): name for name in bone_names}
    for candidate in candidates:
        exact = normalized.get(candidate.lower())
        if exact:
            return exact
    return ''


class MIXAMO_BoneSlot(bpy.types.PropertyGroup):
    key: bpy.props.StringProperty()
    label: bpy.props.StringProperty()
    bone_name: bpy.props.StringProperty()
    auto_detected: bpy.props.BoolProperty(default=False)


class MIXAMO_OT_scan_armature(bpy.types.Operator):
    bl_idname = "mixamo.scan_armature"
    bl_label = "Escanear Armature"
    bl_description = "Procura o armature selecionado (ou o primeiro da cena) e mapeia os ossos conhecidos"

    def execute(self, context):
        armature_obj = _find_armature(context)
        if armature_obj is None:
            self.report({'ERROR'}, "Nenhum armature encontrado na cena.")
            return {'CANCELLED'}

        context.scene.mixamo_armature_name = armature_obj.name
        context.scene.mixamo_bone_slots.clear()

        matched = 0
        for key, label, candidates in FRIENDLY_SLOTS:
            slot = context.scene.mixamo_bone_slots.add()
            slot.key = key
            slot.label = label
            bone_name = _auto_match_bone(armature_obj, candidates)
            slot.bone_name = bone_name
            slot.auto_detected = bool(bone_name)
            if bone_name:
                matched += 1

        self.report({'INFO'}, f"Armature '{armature_obj.name}': {matched}/{len(FRIENDLY_SLOTS)} ossos detectados automaticamente.")
        return {'FINISHED'}


class MIXAMO_OT_bake_adjustment(bpy.types.Operator):
    bl_idname = "mixamo.bake_adjustment"
    bl_label = "Gravar Ajuste na Geometria"
    bl_description = "Grava os ajustes de tamanho atuais na geometria da mesh (os sliders voltam para 1.0)"

    def execute(self, context):
        armature_obj = bpy.data.objects.get(context.scene.mixamo_armature_name)
        if armature_obj is None:
            self.report({'ERROR'}, "Escaneie um armature primeiro.")
            return {'CANCELLED'}

        # NOTE: bpy.ops.pose.armature_apply() only bakes rotation/location into
        # a new rest pose - edit bones have no generic X/Z scale field, so pose
        # bone *scale* (which is what the sliders above use) is silently lost.
        # To actually bake a resize we apply the Armature modifier itself (which
        # evaluates the full deformation numerically) and immediately re-add a
        # fresh one so the mesh stays skinned and re-posable by Mixamo.
        skinned_meshes = [
            obj for obj in bpy.data.objects
            if obj.type == 'MESH' and any(
                mod.type == 'ARMATURE' and mod.object == armature_obj
                for mod in obj.modifiers
            )
        ]
        if not skinned_meshes:
            self.report({'ERROR'}, "Nenhuma mesh usando esse armature foi encontrada.")
            return {'CANCELLED'}

        prev_active = context.view_layer.objects.active

        for mesh_obj in skinned_meshes:
            arm_mod_names = [
                mod.name for mod in mesh_obj.modifiers
                if mod.type == 'ARMATURE' and mod.object == armature_obj
            ]
            context.view_layer.objects.active = mesh_obj
            for mod_name in arm_mod_names:
                bpy.ops.object.modifier_apply(modifier=mod_name)
                new_mod = mesh_obj.modifiers.new(name=mod_name, type='ARMATURE')
                new_mod.object = armature_obj

        for pose_bone in armature_obj.pose.bones:
            pose_bone.scale = (1.0, 1.0, 1.0)

        if prev_active:
            context.view_layer.objects.active = prev_active

        self.report({'INFO'}, f"Ajuste gravado em {len(skinned_meshes)} mesh(es). Os sliders foram resetados para 1.0.")
        return {'FINISHED'}


class MIXAMO_OT_prepare_and_export(bpy.types.Operator, ExportHelper):
    bl_idname = "mixamo.prepare_and_export"
    bl_label = "Preparar e Exportar para Mixamo (FBX)"
    bl_description = "Remove objetos extras, junta as partes da mesh, centraliza na origem e exporta um FBX"

    filename_ext = ".fbx"
    filter_glob: bpy.props.StringProperty(default="*.fbx", options={'HIDDEN'})

    def execute(self, context):
        armature_obj = bpy.data.objects.get(context.scene.mixamo_armature_name)
        if armature_obj is None:
            armature_obj = _find_armature(context)
        if armature_obj is None:
            self.report({'ERROR'}, "Nenhum armature encontrado. Escaneie um armature primeiro.")
            return {'CANCELLED'}

        # Remove scene content Mixamo's auto-rigger rejects (cameras, lights, empties).
        for obj in list(bpy.data.objects):
            if obj.type in {'CAMERA', 'LIGHT', 'EMPTY'}:
                bpy.data.objects.remove(obj, do_unlink=True)

        mesh_objs = [obj for obj in bpy.data.objects if obj.type == 'MESH']
        if not mesh_objs:
            self.report({'ERROR'}, "Nenhuma mesh encontrada na cena.")
            return {'CANCELLED'}

        joined_mesh = mesh_objs[0]
        if len(mesh_objs) > 1:
            bpy.ops.object.select_all(action='DESELECT')
            for obj in mesh_objs:
                obj.select_set(True)
            context.view_layer.objects.active = joined_mesh
            bpy.ops.object.join()

        # Center the character: X/Y centered on the mesh bounding box, feet at Z=0.
        world_corners = [joined_mesh.matrix_world @ Vector(corner) for corner in joined_mesh.bound_box]
        min_x = min(c.x for c in world_corners)
        max_x = max(c.x for c in world_corners)
        min_y = min(c.y for c in world_corners)
        max_y = max(c.y for c in world_corners)
        min_z = min(c.z for c in world_corners)
        delta = Vector(((min_x + max_x) / -2.0, (min_y + max_y) / -2.0, -min_z))

        roots = {armature_obj}
        if joined_mesh.parent != armature_obj:
            roots.add(joined_mesh)
        for root in roots:
            root.location += delta

        bpy.ops.object.select_all(action='DESELECT')
        armature_obj.select_set(True)
        joined_mesh.select_set(True)
        context.view_layer.objects.active = armature_obj

        bpy.ops.export_scene.fbx(
            filepath=self.filepath,
            use_selection=True,
            bake_anim=False,
            add_leaf_bones=False,
            mesh_smooth_type='FACE',
            object_types={'ARMATURE', 'MESH'},
        )

        self.report({'INFO'}, f"Exportado para {self.filepath}")
        return {'FINISHED'}


class MIXAMO_PT_adjust_panel(bpy.types.Panel):
    bl_idname = "MIXAMO_PT_adjust_panel"
    bl_label = "Mixamo Prep"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Mixamo Prep"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        layout.operator("mixamo.scan_armature", icon='ARMATURE_DATA')

        armature_obj = bpy.data.objects.get(scene.mixamo_armature_name)
        if armature_obj is None:
            layout.label(text="Escaneie um armature para comecar.")
            return

        layout.label(text=f"Armature: {armature_obj.name}")
        layout.separator()

        for slot in scene.mixamo_bone_slots:
            box = layout.box()
            row = box.row()
            row.label(text=slot.label + ("  (auto)" if slot.auto_detected else "  (manual)"))
            pose_bone = armature_obj.pose.bones.get(slot.bone_name) if slot.bone_name else None
            if pose_bone:
                box.prop(pose_bone, "scale", text="")
            else:
                box.label(text="Nao detectado - escolha o osso abaixo:", icon='ERROR')
            box.prop_search(slot, "bone_name", armature_obj.pose, "bones", text="Osso")

        layout.separator()
        layout.prop(scene, "mixamo_show_all_bones")
        if scene.mixamo_show_all_bones:
            box = layout.box()
            for pose_bone in armature_obj.pose.bones:
                row = box.row()
                row.label(text=pose_bone.name)
                row.prop(pose_bone, "scale", text="")

        layout.separator()
        layout.operator("mixamo.bake_adjustment", icon='CHECKMARK')
        layout.operator("mixamo.prepare_and_export", icon='EXPORT')


_CLASSES = (
    MIXAMO_BoneSlot,
    MIXAMO_OT_scan_armature,
    MIXAMO_OT_bake_adjustment,
    MIXAMO_OT_prepare_and_export,
    MIXAMO_PT_adjust_panel,
)


def register():
    for cls in _CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Scene.mixamo_armature_name = bpy.props.StringProperty()
    bpy.types.Scene.mixamo_bone_slots = bpy.props.CollectionProperty(type=MIXAMO_BoneSlot)
    bpy.types.Scene.mixamo_show_all_bones = bpy.props.BoolProperty(
        name="Mostrar todos os ossos", default=False)


def unregister():
    del bpy.types.Scene.mixamo_show_all_bones
    del bpy.types.Scene.mixamo_bone_slots
    del bpy.types.Scene.mixamo_armature_name
    for cls in reversed(_CLASSES):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
