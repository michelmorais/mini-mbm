#!/usr/bin/env python3
# ----------------------------------------------------------------------------------------------------------------------|
# MIT License (MIT)                                                                                                     |
# Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                            |
#                                                                                                                       |
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated          |
# documentation files (the "Software"), to deal in the Software without restriction, including without limitation      |
# the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    |
# permit persons to whom the Software is furnished to do so, subject to the following conditions:                       |
#                                                                                                                       |
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the  |
# Software.                                                                                                             |
#                                                                                                                       |
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE  |
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      |
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.      |
# ----------------------------------------------------------------------------------------------------------------------|

"""Phase-0 spike, run with Blender --background --python-exit-code 1 --python this_file -- --output-dir DIR.

Writes a generated .blend fixture, FBX/GLB comparison artifacts and an experimental JSON
helper payload. This is deliberately not a Mesh V11 writer or a supported runtime format.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("blender_mesh_export", ROOT / "editor/blender_mesh_export.py")
EXPORTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORTER)
EXPORTER.bpy = bpy
EXPORTER.Vector = Vector
ROTATION = (-90.0, 0.0, 0.0)
FRAMES = (1, 7, 13, 19, 25)
TARGETS = ("jawOpen", "mouthSmileLeft", "eyeBlinkRight")
TOLERANCE = 2e-5


def checked_vector(values):
    values = [float(v) for v in values]
    if not all(math.isfinite(v) for v in values):
        raise ValueError("Non-finite facial geometry or weight")
    return values


def converted_position(obj, point):
    x, y, z = EXPORTER.rotate_point_deg(*(obj.matrix_world @ point), *ROTATION)
    return checked_vector((-x, y, z))


def converted_normal(obj, normal):
    normal = (obj.matrix_world.to_3x3().inverted().transposed() @ normal).normalized()
    x, y, z = EXPORTER.rotate_point_deg(*normal, *ROTATION)
    return checked_vector((-x, y, z))


def validate_source(obj, include_normal_deltas):
    keys = obj.data.shape_keys
    if not keys or len(keys.key_blocks) < 2:
        raise ValueError(f"{obj.name}: missing facial targets")
    if not keys.use_relative:
        raise ValueError(f"{obj.name}: absolute shape keys are unsupported")
    if keys.animation_data and (keys.animation_data.drivers or
                               any(not t.mute for t in keys.animation_data.nla_tracks)):
        raise ValueError(f"{obj.name}: bake drivers/NLA to a shape-key Action first")
    if obj.show_only_shape_key:
        raise ValueError(f"{obj.name}: disable Show Only Shape Key before extraction")
    if include_normal_deltas and (obj.data.has_custom_normals or any(not p.use_smooth for p in obj.data.polygons)):
        raise ValueError(f"{obj.name}: spike supports smooth geometry without custom normals")
    # Same vertex count does not prove same topology or order. Accept no deforming modifier
    # except armature (disabled temporarily to get bind geometry).
    for modifier in obj.modifiers:
        if modifier.type != 'ARMATURE':
            raise ValueError(f"{obj.name}: unsupported modifier {modifier.type}")
    scale = obj.matrix_world.to_scale()
    if min(scale) <= 0 or max(scale) - min(scale) > 1e-6 or obj.matrix_world.determinant() <= 0:
        raise ValueError(f"{obj.name}: spike requires positive uniform object scale")
    axes = obj.matrix_world.to_3x3().normalized()
    if any(abs(axes.col[a].dot(axes.col[b])) > 1e-6 for a, b in ((0, 1), (0, 2), (1, 2))):
        raise ValueError(f"{obj.name}: sheared object transform is unsupported")
    basis = keys.reference_key
    for key in keys.key_blocks[1:]:
        if key.relative_key != basis or key.vertex_group or key.mute:
            raise ValueError(f"{obj.name}/{key.name}: requires unmasked, unmuted Basis-relative keys")
        if len(key.data) != len(obj.data.vertices):
            raise ValueError(f"{obj.name}/{key.name}: topology mismatch")
        for point in key.data:
            checked_vector(point.co)


def extract_probe(scene, frames=FRAMES, include_normal_deltas=True):
    """Freeze base subset order, sample active action, then evaluate one target at a time.

    Restores scene state even on failure. IDs and dense arrays are experimental JSON only.
    Native Bezier/constant curves are baked at the caller's sample times into linear weights.
    """
    objects = sorted((o for o in scene.objects if o.type == 'MESH' and o.visible_get()), key=lambda o: o.name)
    if (not objects or not frames or any(type(f) is not int for f in frames) or
            any(b <= a for a, b in zip(frames, frames[1:]))):
        raise ValueError("Expected visible meshes and strictly increasing sample frames")
    for obj in objects:
        validate_source(obj, include_normal_deltas)
    old_frame, old_subframe = scene.frame_current, scene.frame_subframe
    states = []
    for obj in objects:
        keys = obj.data.shape_keys
        ad = keys.animation_data
        states.append((obj, ad.action if ad else None, ad.action_slot if ad else None,
                       [(k, k.value) for k in keys.key_blocks],
                       [(m, m.show_viewport) for m in obj.modifiers]))
    try:
        tracks = {o.name: [] for o in objects}
        fps = EXPORTER.get_scene_fps(scene)
        if not math.isfinite(fps) or fps <= 0:
            raise ValueError("Invalid sample rate")
        for frame in frames:
            scene.frame_set(frame)
            for obj in objects:
                values = checked_vector(k.value for k in obj.data.shape_keys.key_blocks[1:])
                if any(v < 0 or v > 1 for v in values):
                    raise ValueError(f"{obj.name}: weights outside initial [0, 1] profile")
                tracks[obj.name].append({"time": (frame - frames[0]) / fps, "weights": values})
        scene.frame_set(frames[0])
        for obj, _, _, values, modifiers in states:
            if obj.data.shape_keys.animation_data:
                obj.data.shape_keys.animation_data.action = None
            for key, _ in values:
                key.value = 0
            for modifier, _ in modifiers:
                modifier.show_viewport = False
        bpy.context.view_layer.update()
        subsets = EXPORTER.export_frame_subsets(scene, ROTATION, canonical_coordinates=True,
                                                capture_source_vertices=True)
        vertices = [v for subset in subsets for v in subset['vertices']]
        result = {"protocol": "mini-mbm.facial-spike", "version": 1, "blender": bpy.app.version_string,
                  "fps": fps, "subsets": subsets, "meshes": []}
        for obj, action, _, _, _ in states:
            keys = obj.data.shape_keys
            output_indices = [i for i, v in enumerate(vertices) if v['sourceObject'] == obj.name]
            if not output_indices:
                raise ValueError(f"{obj.name}: no exported corners")
            mesh_id = EXPORTER.stable_id("mini-mbm.facial.mesh/", obj.name)
            entry = {"name": obj.name, "id": str(mesh_id), "sourceVertexCount": len(obj.data.vertices),
                     "outputIndices": output_indices, "targets": [],
                     "clip": {"name": action.name if action else "Scene", "interpolation": "LINEAR",
                              "duration": (frames[-1] - frames[0]) / fps, "keys": tracks[obj.name]}}
            for key in keys.key_blocks[1:]:
                key.value = 1
                bpy.context.view_layer.update()
                graph = bpy.context.evaluated_depsgraph_get()
                evaluated = obj.evaluated_get(graph)
                mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=graph)
                try:
                    if len(mesh.vertices) != len(obj.data.vertices) or len(mesh.loops) != len(obj.data.loops):
                        raise ValueError(f"{obj.name}: evaluated topology mismatch")
                    EXPORTER.prepare_mesh_normals(mesh)
                    positions, normals = [], []
                    for index in output_indices:
                        base = vertices[index]
                        source_index, loop_index = base['sourceVertex'], base['sourceLoop']
                        if mesh.loops[loop_index].vertex_index != source_index:
                            raise ValueError(f"{obj.name}: evaluated corner mapping changed")
                        position = converted_position(obj, mesh.vertices[source_index].co)
                        # Smooth-only spike: same normal accessor as the existing helper.
                        normal = converted_normal(obj, EXPORTER.get_loop_normal(mesh.loops[loop_index], mesh.vertices[source_index]))
                        positions.append(checked_vector(position[i] - base[axis] for i, axis in enumerate(('x', 'y', 'z'))))
                        normals.append(checked_vector(normal[i] - base[axis] for i, axis in enumerate(('nx', 'ny', 'nz'))))
                    target = {"name": key.name,
                        "id": str(EXPORTER.stable_id("mini-mbm.facial.target/", f"{mesh_id}/{key.name}")),
                        "defaultWeight": 0.0, "positionDeltas": positions}
                    if include_normal_deltas:
                        target['normalDeltas'] = normals
                    entry['targets'].append(target)
                finally:
                    evaluated.to_mesh_clear()
                    key.value = 0
            result['meshes'].append(entry)
        return result
    finally:
        for obj, action, slot, values, modifiers in states:
            if obj.data.shape_keys.animation_data:
                obj.data.shape_keys.animation_data.action = action
                if slot:
                    obj.data.shape_keys.animation_data.action_slot = slot
            for key, value in values:
                key.value = value
            for modifier, visible in modifiers:
                modifier.show_viewport = visible
        scene.frame_set(old_frame, subframe=old_subframe)
        bpy.context.view_layer.update()


def make_fixture(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    bpy.context.preferences.filepaths.save_version = 0
    bpy.context.preferences.filepaths.file_preview_type = 'NONE'
    scene.render.fps = 24
    scene.frame_start, scene.frame_end = 1, 25
    mesh = bpy.data.meshes.new("FaceGrid")
    # A bent 3x3 patch; adjacent material subsets and a UV seam duplicate shared vertices.
    mesh.from_pydata([(x, 0.12 * x * x + 0.07 * z * z, z) for z in (-1, 0, 1) for x in (-1, 0, 1)], [],
                     [(0, 1, 4), (0, 4, 3), (1, 2, 5), (1, 5, 4),
                      (3, 4, 7), (3, 7, 6), (4, 5, 8), (4, 8, 7)])
    obj = bpy.data.objects.new("Face", mesh)
    scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    for name in ("SkinLeft", "SkinRight"):
        mesh.materials.append(bpy.data.materials.new(name))
    uv = mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        polygon.use_smooth = True
        polygon.material_index = (polygon.index // 2) % 2
        for loop_index in polygon.loop_indices:
            co = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            uv.data[loop_index].uv = ((co.x + 1) / 2 + (0.1 if polygon.index >= 4 else 0), (co.z + 1) / 2)
    obj.shape_key_add(name="Basis")
    for name in TARGETS:
        key = obj.shape_key_add(name=name)
        for point in key.data:
            x, _, z = point.co
            if name == 'jawOpen' and z < 0:
                point.co.y -= 0.35
                point.co.z -= 0.25
            elif name == 'mouthSmileLeft' and x < 0 and z <= 0:
                point.co.x -= 0.2
                point.co.z += 0.15
            elif name == 'eyeBlinkRight' and x > 0 and z > 0:
                point.co.z -= 0.3
    for target_index, key in enumerate(mesh.shape_keys.key_blocks[1:]):
        for frame, value in zip(FRAMES, ((0, .4, 1, .6, 0), (0, .8, .2, 1, 0), (0, 0, 1, 0, 0))[target_index]):
            key.value = value
            key.keyframe_insert('value', frame=frame)
    mesh.shape_keys.animation_data.action.name = 'FacePerformance'
    for curve in EXPORTER.iter_action_fcurves(mesh.shape_keys.animation_data.action):
        for key in curve.keyframe_points:
            key.interpolation = 'LINEAR'
    armature = bpy.data.armatures.new('HeadRig')
    rig = bpy.data.objects.new('HeadRig', armature)
    scene.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    obj.select_set(False)
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bone = armature.edit_bones.new('Head')
    bone.head, bone.tail = (0, 0, -1), (0, 0, 1)
    bpy.ops.object.mode_set(mode='OBJECT')
    group = obj.vertex_groups.new(name='Head')
    group.add(list(range(9)), 1.0, 'REPLACE')
    modifier = obj.modifiers.new('Skin', 'ARMATURE')
    modifier.object = rig
    # Nonidentity common transform exercises world-space deltas and coordinate conversion.
    for item in (obj, rig):
        item.location = (.3, -.2, .4)
        item.rotation_euler = (.1, -.2, .15)
        item.scale = (1.2, 1.2, 1.2)
    bpy.context.view_layer.update()
    world = obj.matrix_world.copy()
    obj.parent = rig
    obj.matrix_world = world
    rig.pose.bones['Head'].rotation_mode = 'XYZ'
    for frame, angle in ((1, 0), (13, .3), (25, 0)):
        rig.pose.bones['Head'].rotation_euler.z = angle
        rig.pose.bones['Head'].keyframe_insert('rotation_euler', frame=frame)
    scene.frame_set(7)
    bpy.ops.wm.save_as_mainfile(filepath=str(path))


def assert_close(a, b, label):
    error = max((abs(x - y) for x, y in zip(a, b)), default=0)
    if len(a) != len(b) or error > TOLERANCE:
        raise AssertionError(f"{label}: error={error}, lengths={len(a)}/{len(b)}")
    return error


def corner_records(payload):
    vertices = [v for s in payload['subsets'] for v in s['vertices']]
    records = []
    for mesh in payload['meshes']:
        for local, index in enumerate(mesh['outputIndices']):
            vertex = vertices[index]
            records.append(([vertex[a] for a in ('x', 'y', 'z', 'u', 'v')],
                            {t['name']: t['positionDeltas'][local] for t in mesh['targets']}))
    return records


def compare(reference, candidate):
    # Formats may split/reorder vertices. Compare every exported corner by bind position+UV,
    # then verify its own deltas; never reuse pre-interchange Blender vertex indices.
    expected, actual = corner_records(reference), corner_records(candidate)
    if len(actual) != len(expected):
        raise AssertionError(f"corner count changed: {len(expected)} -> {len(actual)}")
    maximum = 0
    remaining = list(actual)
    for position, targets in expected:
        index = min(range(len(remaining)), key=lambda i: sum((a-b)**2 for a, b in zip(position, remaining[i][0])))
        other_pos, other_targets = remaining.pop(index)
        maximum = max(maximum, assert_close(position, other_pos, 'base corner'))
        if targets.keys() != other_targets.keys():
            raise AssertionError('target names changed')
        for name in targets:
            maximum = max(maximum, assert_close(targets[name], other_targets[name], f'{name} delta'))
    ref_mesh = reference['meshes'][0]
    names = [t['name'] for t in ref_mesh['targets']]
    for mesh in candidate['meshes']:
        if len(mesh['clip']['keys']) != len(ref_mesh['clip']['keys']):
            raise AssertionError('sample count changed')
        order = [names.index(t['name']) for t in mesh['targets']]
        for a, b in zip(ref_mesh['clip']['keys'], mesh['clip']['keys']):
            maximum = max(maximum, assert_close([a['time']] + [a['weights'][i] for i in order],
                                                 [b['time']] + b['weights'], 'animated weights'))
    return maximum


def activate_imported_clip(scene):
    """Resolve the fixture's sole shape Action and normalize importer time origins.

    glTF can stash separate skin/shape Actions and activate only the skin one. FBX's default
    importer offsets animation by one frame. Neither implies that the weights were lost.
    """
    starts = []
    for obj in scene.objects:
        if obj.type != 'MESH' or not obj.data.shape_keys:
            continue
        keys = obj.data.shape_keys
        ad = keys.animation_data
        if ad and not ad.action:
            strips = [s for track in ad.nla_tracks for s in track.strips]
            if len(strips) != 1:
                raise ValueError(f'{obj.name}: expected one imported facial Action')
            ad.action = strips[0].action
            ad.action_slot = strips[0].action_slot
            for track in ad.nla_tracks:
                track.mute = True
        if not ad or not ad.action:
            raise ValueError(f'{obj.name}: no imported facial Action')
        frames = [p.co.x for curve in EXPORTER.iter_action_fcurves(ad.action)
                  if curve.data_path.startswith('key_blocks[') for p in curve.keyframe_points]
        if not frames:
            raise ValueError(f'{obj.name}: no imported weight curves')
        assert_close([min(frames), max(frames)], [round(min(frames)), round(min(frames)) + 24], 'imported frame range')
        starts.append(round(min(frames)))
    if not starts or len(set(starts)) != 1:
        raise ValueError('Imported facial clips disagree on time origin')
    return tuple(range(starts[0], starts[0] + 25))


def verify_helper(payload):
    scene = bpy.context.scene
    obj = scene.objects['Face']
    assert len(payload['subsets']) == 2
    vertices = [v for s in payload['subsets'] for v in s['vertices']]
    assert len(vertices) > len(obj.data.vertices)
    targets = payload['meshes'][0]['targets']
    # Check against raw Basis-relative coordinates, independent of evaluated extraction.
    for target in targets:
        key = obj.data.shape_keys.key_blocks[target['name']]
        for vertex, delta in zip(vertices, target['positionDeltas']):
            source = vertex['sourceVertex']
            expected = converted_position(obj, key.data[source].co)
            assert_close([vertex[a] + delta[i] for i, a in enumerate(('x', 'y', 'z'))], expected, 'raw target')
    # Verify all simultaneous target samples in a posed, skinned mesh against Blender evaluation.
    for frame in FRAMES:
        scene.frame_set(frame)
        pose = scene.objects['HeadRig'].pose.bones['Head']
        skin = obj.matrix_world @ pose.matrix @ pose.bone.matrix_local.inverted() @ obj.matrix_world.inverted()
        graph = bpy.context.evaluated_depsgraph_get()
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        try:
            for index, vertex in enumerate(vertices):
                source = vertex['sourceVertex']
                point = Vector(tuple(vertex[a] for a in ('x', 'y', 'z')))
                for target in targets:
                    weight = obj.data.shape_keys.key_blocks[target['name']].value
                    point += Vector(target['positionDeltas'][index]) * weight
                # Inverse of the explicit Blender-world -> engine (-x, z, -y) boundary.
                expected = skin @ Vector((-point.x, -point.z, point.y))
                actual = obj.matrix_world @ mesh.vertices[source].co
                assert_close(list(expected), list(actual), 'morph-before-skin')
        finally:
            evaluated.to_mesh_clear()
    scene.frame_set(7)
    assert any(abs(v) > 1e-3 for t in targets for d in t['normalDeltas'] for v in d)
    ordinary = EXPORTER.export_frame_subsets(scene)
    assert all(not any(k.startswith('source') for k in v) for s in ordinary for v in s['vertices'])
    # Prove exact source remap survives the same subset chunker used by the writer.
    old_limit = EXPORTER.MAX_MBM_SUBSET_VERTICES
    try:
        EXPORTER.MAX_MBM_SUBSET_VERTICES = 3
        for subset in payload['subsets']:
            chunks = EXPORTER.split_subset_for_uint16_indices(subset)
            before = [subset['vertices'][i-1] for i in subset['indices']]
            after = [c['vertices'][i-1] for c in chunks for i in c['indices']]
            assert before == after
    finally:
        EXPORTER.MAX_MBM_SUBSET_VERTICES = old_limit
    # Guard rails and state restoration on an error after temporary mutations.
    modifier = obj.modifiers.new('Unsupported', 'SUBSURF')
    try:
        try:
            extract_probe(scene)
        except ValueError as error:
            assert 'unsupported modifier' in str(error)
        else:
            raise AssertionError('topology modifier accepted')
    finally:
        obj.modifiers.remove(modifier)
    def rejected(fragment):
        try:
            extract_probe(scene)
        except ValueError as error:
            assert fragment in str(error), str(error)
        else:
            raise AssertionError(f'unsupported input accepted: {fragment}')
    keys = obj.data.shape_keys
    keys.use_relative = False
    try:
        rejected('absolute shape keys')
    finally:
        keys.use_relative = True
    keys.key_blocks[2].relative_key = keys.key_blocks[1]
    try:
        rejected('Basis-relative')
    finally:
        keys.key_blocks[2].relative_key = keys.reference_key
    keys.key_blocks[1].vertex_group = 'Head'
    try:
        rejected('Basis-relative')
    finally:
        keys.key_blocks[1].vertex_group = ''
    keys.key_blocks[1].driver_add('value')
    try:
        rejected('bake drivers/NLA')
    finally:
        keys.key_blocks[1].driver_remove('value')
    original = EXPORTER.export_frame_subsets
    def fail(*args, **kwargs):
        raise RuntimeError('injected extraction failure')
    EXPORTER.export_frame_subsets = fail
    action = obj.data.shape_keys.animation_data.action
    try:
        try:
            extract_probe(scene)
        except RuntimeError as error:
            assert str(error) == 'injected extraction failure'
        else:
            raise AssertionError('injected failure did not fire')
        assert obj.data.shape_keys.animation_data.action == action
        assert obj.modifiers[0].show_viewport
        assert scene.frame_current == 7
    finally:
        EXPORTER.export_frame_subsets = original


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-dir', required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    output = Path(args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    fixture = output / 'facial-fixture.blend'
    make_fixture(fixture)
    reference = extract_probe(bpy.context.scene, tuple(range(1, 26)))
    verify_helper(reference)
    assert reference == extract_probe(bpy.context.scene, tuple(range(1, 26))), 'helper mutated scene or identity'
    payload_path = output / 'facial-helper-probe.json'
    payload_path.write_text(json.dumps(reference, indent=2, allow_nan=False) + '\n')
    assert json.loads(payload_path.read_text()) == reference
    report = {'blender': bpy.app.version_string, 'helper': {'status': 'PASS', 'sourceVertices': 9,
              'outputVertices': sum(len(s['vertices']) for s in reference['subsets']), 'targets': 3,
              'samples': 25, 'morphBeforeSkin': 'PASS', 'chunkRemap': 'PASS', 'restoration': 'PASS'},
              'routes': {}, 'faceitAddonTested': False}
    report['helper']['maxNormalDeltaComponent'] = max(
        abs(value) for mesh in reference['meshes'] for target in mesh['targets']
        for delta in target['normalDeltas'] for value in delta)
    for route in ('fbx', 'glb'):
        bpy.ops.wm.open_mainfile(filepath=str(fixture))
        path = output / f'facial-fixture.{route}'
        try:
            if route == 'fbx':
                bpy.ops.export_scene.fbx(filepath=str(path), use_mesh_modifiers=False,
                    add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
                    bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0)
            else:
                bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB',
                    export_morph=True, export_morph_normal=True, export_animations=True,
                    export_animation_mode='SCENE', export_force_sampling=True)
            EXPORTER.import_source(str(path))
            frames = activate_imported_clip(bpy.context.scene)
            imported = extract_probe(bpy.context.scene, frames, include_normal_deltas=False)
            (output / f'facial-{route}-probe.json').write_text(json.dumps(imported, indent=2, allow_nan=False) + '\n')
            error = compare(reference, imported)
            skinned = all(any(m.type == 'ARMATURE' and m.object for m in o.modifiers) and
                          all(v.groups for v in o.data.vertices)
                          for o in bpy.context.scene.objects if o.type == 'MESH' and o.visible_get())
            assert skinned, 'skin binding lost: ' + repr([
                (o.name, [(m.type, getattr(m, 'object', None).name if getattr(m, 'object', None) else None)
                          for m in o.modifiers], sum(bool(v.groups) for v in o.data.vertices), len(o.data.vertices),
                 o.parent_type) for o in bpy.context.scene.objects if o.type == 'MESH' and o.visible_get()])
            report['routes'][route] = {'status': 'PASS', 'maxPositionOrWeightError': error,
                                      'meshes': len(imported['meshes']), 'skinBinding': True,
                                      'sourceFirstFrame': frames[0],
                                      'normalDeltasCompared': False}
        except Exception as error:
            report['routes'][route] = {'status': 'FAIL', 'reason': str(error)}
    (output / 'facial-interchange-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('FACIAL_INTERCHANGE_REPORT ' + json.dumps(report), flush=True)
    assert all(route['status'] == 'PASS' for route in report['routes'].values()), 'Interchange regression; see report'


if __name__ == '__main__':
    main()
