#!/usr/bin/env python3
"""Emit deterministic FBX bind/pose evidence through Blender's imported scene API.

Usage:
    blender -b --factory-startup --python skeletal-fbx-bind-audit.py -- input.fbx output.json

Blender does not expose the original FBX cluster Transform/TransformLink matrices after import.
This audit therefore labels its evidence precisely: imported rest matrices, sampled pose matrices,
object transforms, topology orientation, and vertex-group statistics. Cluster matrices require a
future raw-FBX reader or an importer instrumentation hook and are never inferred from rest bones.
"""

from __future__ import annotations

import hashlib
import json
import os
import sys

import bpy
from io_scene_fbx import parse_fbx
from io_scene_fbx.import_fbx import array_to_matrix4


def matrix_values(matrix) -> list[float]:
    return [round(float(matrix[row][column]), 9) for row in range(4) for column in range(4)]


def vector_values(vector) -> list[float]:
    return [round(float(value), 9) for value in vector]


def determinant_sign(matrix) -> int:
    determinant = float(matrix.to_3x3().determinant())
    return 1 if determinant > 0.0 else (-1 if determinant < 0.0 else 0)


def parse_arguments() -> tuple[str, str]:
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(args) != 2:
        raise RuntimeError("expected input.fbx and output.json after --")
    return os.path.abspath(args[0]), os.path.abspath(args[1])


def select_mesh(scene):
    meshes = [obj for obj in scene.objects if obj.type == "MESH"]
    if not meshes:
        return None
    return max(meshes, key=lambda obj: len(obj.data.vertices))


def selected_bone_names(armature) -> list[str]:
    preferred = [
        "mixamorig:Hips", "mixamorig:Spine2", "mixamorig:Head",
        "mixamorig:LeftShoulder", "mixamorig:RightShoulder",
        "mixamorig:LeftHand", "mixamorig:RightHand",
        "mixamorig:LeftUpLeg", "mixamorig:RightUpLeg",
        "mixamorig:LeftFoot", "mixamorig:RightFoot",
    ]
    available = {bone.name for bone in armature.data.bones}
    selected = [name for name in preferred if name in available]
    if selected:
        return selected
    return [bone.name for bone in list(armature.data.bones)[:12]]


def raw_cluster_evidence(input_path: str, selected_names: list[str]) -> dict:
    root, fbx_version = parse_fbx.parse(input_path)
    objects = next(elem for elem in root.elems if elem.id == b"Objects")
    connections = next(elem for elem in root.elems if elem.id == b"Connections")
    by_uuid = {elem.props[0]: elem for elem in objects.elems if elem.props}
    connection_pairs = [(elem.props[1], elem.props[2]) for elem in connections.elems
                        if elem.id == b"C" and len(elem.props) >= 3 and elem.props[0] == b"OO"]

    def clean_name(raw: bytes) -> str:
        return raw.split(b"\x00\x01", 1)[0].decode("utf-8", errors="replace")

    clusters = []
    total_clusters = 0
    for cluster in objects.elems:
        if cluster.id != b"Deformer" or len(cluster.props) < 3 or cluster.props[2] != b"Cluster":
            continue
        total_clusters += 1
        cluster_uuid = cluster.props[0]
        bone_elem = None
        for child_uuid, parent_uuid in connection_pairs:
            if parent_uuid != cluster_uuid:
                continue
            candidate = by_uuid.get(child_uuid)
            if candidate is not None and candidate.id == b"Model" and candidate.props[2] == b"LimbNode":
                bone_elem = candidate
                break
        if bone_elem is None:
            continue
        bone_name = clean_name(bone_elem.props[1])
        if bone_name not in selected_names:
            continue
        fields = {elem.id: elem for elem in cluster.elems}
        transform = fields.get(b"Transform")
        transform_link = fields.get(b"TransformLink")
        clusters.append({
            "bone": bone_name,
            "clusterUuid": int(cluster_uuid),
            "transformMeshBindRawColumnMatrix": matrix_values(array_to_matrix4(transform.props[0]))
                if transform is not None else None,
            "transformLinkBoneBindRawColumnMatrix": matrix_values(array_to_matrix4(transform_link.props[0]))
                if transform_link is not None else None,
        })
    return {
        "available": True,
        "source": "raw FBX elements parsed with Blender's bundled io_scene_fbx parser",
        "fbxVersion": int(fbx_version),
        "clusterCount": total_clusters,
        "selectedClusters": sorted(clusters, key=lambda item: item["bone"]),
    }


def main() -> None:
    input_path, output_path = parse_arguments()
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=input_path)
    scene = bpy.context.scene
    armature = next((obj for obj in scene.objects if obj.type == "ARMATURE"), None)
    if armature is None:
        raise RuntimeError("FBX contains no imported armature")
    mesh = select_mesh(scene)
    names = selected_bone_names(armature)

    armature.data.pose_position = "REST"
    scene.frame_set(int(scene.frame_start))
    bpy.context.view_layer.update()
    rest_bones = []
    maximum_rest_pose_error = 0.0
    for bone in armature.data.bones:
        rest_global = armature.matrix_world @ bone.matrix_local
        pose_global = armature.matrix_world @ armature.pose.bones[bone.name].matrix
        error = max(abs(float(rest_global[row][column] - pose_global[row][column]))
                    for row in range(4) for column in range(4))
        maximum_rest_pose_error = max(maximum_rest_pose_error, error)
        if bone.name in names:
            local = bone.parent.matrix_local.inverted() @ bone.matrix_local if bone.parent else bone.matrix_local
            rest_bones.append({
                "name": bone.name,
                "parent": bone.parent.name if bone.parent else None,
                "restArmatureColumnMatrix": matrix_values(bone.matrix_local),
                "restGlobalColumnMatrix": matrix_values(rest_global),
                "restLocalColumnMatrix": matrix_values(local),
                "headArmatureSpace": vector_values(bone.head_local),
                "tailArmatureSpace": vector_values(bone.tail_local),
            })

    armature.data.pose_position = "POSE"
    actions = []
    for action in sorted(bpy.data.actions, key=lambda item: item.name):
        if armature.animation_data is None:
            armature.animation_data_create()
        armature.animation_data.action = action
        start = int(round(action.frame_range[0]))
        end = int(round(action.frame_range[1]))
        frames = sorted(set((start, (start + end) // 2, end)))
        samples = []
        for frame in frames:
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            samples.append({
                "frame": frame,
                "bones": {
                    name: matrix_values(armature.matrix_world @ armature.pose.bones[name].matrix)
                    for name in names
                },
            })
        maximum_sample_delta = 0.0
        if samples:
            reference = samples[0]["bones"]
            for sample in samples[1:]:
                for name in names:
                    maximum_sample_delta = max(maximum_sample_delta,
                                               max(abs(left - right) for left, right in
                                                   zip(reference[name], sample["bones"][name])))
        actions.append({"name": action.name, "frameStart": start, "frameEnd": end,
                        "maximumSelectedBoneSampleDelta": round(maximum_sample_delta, 9),
                        "samples": samples})

    mesh_evidence = None
    if mesh is not None:
        mesh.data.calc_loop_triangles()
        vertices = mesh.data.vertices
        selected_vertices = sorted(set((0, len(vertices) // 2, max(0, len(vertices) - 1)))) if vertices else []
        first_triangle = list(mesh.data.loop_triangles[0].vertices) if mesh.data.loop_triangles else []
        first_triangle_positions = [vector_values(mesh.matrix_world @ vertices[index].co) for index in first_triangle]
        geometric_normal = None
        imported_normal = None
        normal_alignment = None
        if mesh.data.loop_triangles:
            triangle = mesh.data.loop_triangles[0]
            points = [mesh.matrix_world @ vertices[index].co for index in triangle.vertices]
            geometric = (points[1] - points[0]).cross(points[2] - points[0]).normalized()
            normal_matrix = mesh.matrix_world.to_3x3().inverted().transposed()
            imported = (normal_matrix @ mesh.data.loops[triangle.loops[0]].normal).normalized()
            geometric_normal = vector_values(geometric)
            imported_normal = vector_values(imported)
            normal_alignment = round(float(geometric.dot(imported)), 9)
        mesh_evidence = {
            "name": mesh.name,
            "vertexCount": len(vertices),
            "triangleCount": len(mesh.data.loop_triangles),
            "objectMatrix": matrix_values(mesh.matrix_world),
            "objectDeterminantSign": determinant_sign(mesh.matrix_world),
            "selectedVerticesWorld": {
                str(index): vector_values(mesh.matrix_world @ vertices[index].co) for index in selected_vertices
            },
            "firstTriangleIndices": first_triangle,
            "firstTriangleWorld": first_triangle_positions,
            "firstTriangleGeometricNormalWorld": geometric_normal,
            "firstLoopNormalWorld": imported_normal,
            "firstTriangleNormalAlignment": normal_alignment,
            "vertexGroupCount": len(mesh.vertex_groups),
        }

    with open(input_path, "rb") as source:
        source_hash = hashlib.sha256(source.read()).hexdigest()
    cluster_evidence = raw_cluster_evidence(input_path, names)
    cluster_errors = []
    for cluster in cluster_evidence["selectedClusters"]:
        imported = armature.data.bones.get(cluster["bone"])
        raw_link = cluster["transformLinkBoneBindRawColumnMatrix"]
        if imported is None or raw_link is None:
            continue
        imported_values = matrix_values(imported.matrix_local)
        cluster_errors.append(max(abs(left - right) for left, right in zip(raw_link, imported_values)))
    cluster_evidence["maximumTransformLinkVsImportedRestArmatureError"] = \
        round(max(cluster_errors), 9) if cluster_errors else None

    report = {
        "schema": 1,
        "sourceFile": os.path.basename(input_path),
        "sourceSha256": source_hash,
        "blenderVersion": bpy.app.version_string,
        "coordinateConvention": {
            "blenderImportedScene": "right-handed, column-vector matrices",
            "miniMbm": "left-handed, row-vector matrices",
            "rowVectorConversionRule": "transpose imported matrices after the accepted reflection",
        },
        "clusterBindEvidence": cluster_evidence,
        "armature": {
            "name": armature.name,
            "boneCount": len(armature.data.bones),
            "objectMatrix": matrix_values(armature.matrix_world),
            "objectDeterminantSign": determinant_sign(armature.matrix_world),
            "maximumRestVsRestPoseMatrixError": round(maximum_rest_pose_error, 9),
            "selectedBones": rest_bones,
        },
        "mesh": mesh_evidence,
        "actions": actions,
    }
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as output:
        json.dump(report, output, indent=2, sort_keys=True)
        output.write("\n")
    print(f"skeletal FBX audit written: {output_path}")


if __name__ == "__main__":
    main()
