#!/usr/bin/env python3
"""
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|
"""

from __future__ import annotations

from types import SimpleNamespace
import unittest

import blender_mesh_export as exporter


def make_armature(name: str, bone_names: list[str]) -> SimpleNamespace:
    bones = [SimpleNamespace(name=bone_name) for bone_name in bone_names]
    return SimpleNamespace(name=name, type="ARMATURE", data=SimpleNamespace(bones=bones))


def make_mesh(name: str,
              armature: SimpleNamespace | None,
              group_names: list[str],
              vertex_weights: list[tuple[int, float]]) -> SimpleNamespace:
    modifier = SimpleNamespace(type="ARMATURE", object=armature) if armature is not None else None
    modifiers = [modifier] if modifier is not None else []
    vertex_groups = [SimpleNamespace(name=group_name) for group_name in group_names]
    vertices = [SimpleNamespace(groups=[
        SimpleNamespace(group=group_index, weight=weight) for group_index, weight in vertex_weights
    ])]
    return SimpleNamespace(
        name=name,
        type="MESH",
        modifiers=modifiers,
        vertex_groups=vertex_groups,
        data=SimpleNamespace(vertices=vertices),
    )


def make_scene(objects: list[SimpleNamespace]) -> SimpleNamespace:
    return SimpleNamespace(objects=objects)


class CanonicalSkeletalCapabilityTests(unittest.TestCase):
    def test_reports_available_for_chosen_armature_with_matching_positive_weights(self) -> None:
        armature = make_armature("Armature", ["Root", "Spine"])
        mesh = make_mesh("Body", armature, ["Root"], [(0, 0.75)])

        cap = exporter.detect_canonical_skeletal_capability(make_scene([armature, mesh]))

        self.assertIs(cap["available"], True)
        self.assertEqual(cap["boneCount"], 2)
        self.assertEqual(cap["skinnedMeshCount"], 1)

    def test_does_not_aggregate_weights_from_unrelated_armature(self) -> None:
        first_armature = make_armature("FirstArmature", ["FirstBone"])
        second_armature = make_armature("SecondArmature", ["SecondBone"])
        mesh = make_mesh("SecondMesh", second_armature, ["SecondBone"], [(0, 1.0)])

        cap = exporter.detect_canonical_skeletal_capability(
            make_scene([first_armature, second_armature, mesh])
        )

        self.assertIs(cap["available"], False)
        self.assertEqual(cap["armatureCount"], 2)
        self.assertEqual(cap["boneCount"], 1)
        self.assertEqual(cap["skinnedMeshCount"], 0)

    def test_does_not_claim_second_armature_when_extractor_would_choose_empty_first_armature(self) -> None:
        empty_first_armature = make_armature("EmptyFirstArmature", [])
        second_armature = make_armature("SecondArmature", ["SecondBone"])
        mesh = make_mesh("SecondMesh", second_armature, ["SecondBone"], [(0, 1.0)])

        cap = exporter.detect_canonical_skeletal_capability(
            make_scene([empty_first_armature, second_armature, mesh])
        )

        self.assertIs(cap["available"], False)
        self.assertEqual(cap["armatureCount"], 2)
        self.assertEqual(cap["boneCount"], 0)
        self.assertIn("no bones", cap["reason"])

    def test_requires_positive_weights_matching_chosen_armature_bones(self) -> None:
        armature = make_armature("Armature", ["Root"])
        zero_weight_mesh = make_mesh("ZeroWeightMesh", armature, ["Root"], [(0, 0.0)])
        unrelated_group_mesh = make_mesh("UnrelatedGroupMesh", armature, ["Other"], [(0, 1.0)])

        cap = exporter.detect_canonical_skeletal_capability(
            make_scene([armature, zero_weight_mesh, unrelated_group_mesh])
        )

        self.assertIs(cap["available"], False)
        self.assertEqual(cap["boneCount"], 1)
        self.assertEqual(cap["skinnedMeshCount"], 0)

    def test_reports_mesh_cache_reason_when_no_armature_exists(self) -> None:
        cap = exporter.detect_canonical_skeletal_capability(
            make_scene([]),
            [{"object": "CachedMesh"}],
        )

        self.assertIs(cap["available"], False)
        self.assertEqual(cap["armatureCount"], 0)
        self.assertIn("Mesh Sequence Cache", cap["reason"])


class CanonicalSkeletalExportModeTests(unittest.TestCase):
    def test_default_indexed_large_mesh_mode_keeps_available_skeletal_export(self) -> None:
        cap = {"available": True}

        self.assertIs(exporter.can_export_canonical_skeletal(cap, "fail"), True)
        self.assertIsNone(exporter.get_canonical_skeletal_export_fallback_reason("fail"))

    def test_vb_only_large_mesh_mode_forces_baked_fallback_even_when_capable(self) -> None:
        cap = {"available": True}

        self.assertIs(exporter.can_export_canonical_skeletal(cap, "vb_only"), False)
        reason = exporter.get_canonical_skeletal_export_fallback_reason("vb_only")
        self.assertIsNotNone(reason)
        self.assertIn("type-42 skin weights", reason or "")


class SkeletalActionSourceTests(unittest.TestCase):
    def test_detects_matching_pose_bone_curves_in_layered_actions(self) -> None:
        curves = [SimpleNamespace(data_path='pose.bones["Root"].rotation_quaternion')]
        channel_bag = SimpleNamespace(fcurves=curves)
        strip = SimpleNamespace(channelbags=[channel_bag])
        layer = SimpleNamespace(strips=[strip])
        action = SimpleNamespace(layers=[layer])

        self.assertIs(exporter.action_animates_pose_bones(action, {"Root"}), True)
        self.assertIs(exporter.action_animates_pose_bones(action, {"Other"}), False)

    def test_parses_explicit_action_source_identity(self) -> None:
        args = exporter.parse_args([
            "--input", "character.fbx", "--output", "character.msh",
            "--animation-source", "Walk", "1", "30", "1", "action", "Armature", "WalkAction",
        ])

        clips = exporter.parse_animation_clips(args, SimpleNamespace())

        self.assertEqual(len(clips), 1)
        self.assertEqual(clips[0]["sourceKind"], "action")
        self.assertEqual(clips[0]["sourceObject"], "Armature")
        self.assertEqual(clips[0]["sourceAction"], "WalkAction")


if __name__ == "__main__":
    unittest.main()
