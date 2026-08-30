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

from __future__ import annotations

import importlib.util
import unittest
from argparse import Namespace
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


EXPORTER_PATH = Path(__file__).resolve().parents[2] / "editor" / "blender_mesh_export.py"
SPEC = importlib.util.spec_from_file_location("blender_mesh_export", EXPORTER_PATH)
assert SPEC and SPEC.loader
EXPORTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORTER)


class BlenderMeshExportTests(unittest.TestCase):
    class _Modifiers(list):
        def new(self, name: str, type: str):
            modifier = SimpleNamespace()
            modifier.name = name
            modifier.type = type
            modifier.use_collapse_triangulate = False
            self.append(modifier)
            return modifier

    class _Object:
        def __init__(self, name: str = "Mesh", object_type: str = "MESH") -> None:
            self.name = name
            self.type = object_type
            self.modifiers = BlenderMeshExportTests._Modifiers()
            self.animation_data = None
            self.data = SimpleNamespace(shape_keys=None)

        def visible_get(self) -> bool:
            return True

    @staticmethod
    def _args(ratio: float | None = 0.5, **overrides):
        values = {
            "decimate_ratio": ratio,
            "bake_animation": False,
            "animation_clip": None,
            "animation_source": None,
            "debug_steps": False,
        }
        values.update(overrides)
        return Namespace(**values)

    def test_large_subset_chunks_preserve_semantic_texture_roles(self) -> None:
        vertex_count = EXPORTER.MAX_MBM_SUBSET_VERTICES + 3
        vertices = [{"x": float(index)} for index in range(vertex_count)]
        subset = {
            "name": "material",
            "texture": "diffuse.png",
            "extraTextures": [
                {"role": EXPORTER.TEXTURE_ROLE_NORMAL, "texture": "normal.png"},
                {"role": EXPORTER.TEXTURE_ROLE_EMISSIVE, "texture": "emissive.png"},
            ],
            "vertices": vertices,
            "indices": list(range(1, vertex_count + 1)),
        }

        chunks = EXPORTER.split_subset_for_uint16_indices(subset)

        self.assertGreater(len(chunks), 1)
        for chunk in chunks:
            self.assertEqual(chunk["texture"], "diffuse.png")
            self.assertEqual(chunk["extraTextures"], subset["extraTextures"])
            self.assertIsNot(chunk["extraTextures"], subset["extraTextures"])

    def test_material_ignores_image_node_without_pixel_data_or_file(self) -> None:
        image = SimpleNamespace(has_data=False, filepath="", packed_file=None)
        image.save = mock.Mock(side_effect=AssertionError("empty image must not be saved"))
        base_color = SimpleNamespace(
            type="TEX_IMAGE",
            image=image,
            outputs=[SimpleNamespace(links=[SimpleNamespace(
                to_node=SimpleNamespace(type="BSDF_PRINCIPLED"),
                to_socket=SimpleNamespace(name="Base Color"),
            )])],
        )
        material = SimpleNamespace(
            name="Material",
            use_nodes=True,
            node_tree=SimpleNamespace(nodes=[base_color]),
        )

        primary, extras = EXPORTER.get_material_texture_paths(
            material, 1, "/tmp", normalize_textures=True,
        )

        self.assertEqual(primary, "")
        self.assertEqual(extras, [])
        image.save.assert_not_called()

    def test_static_decimation_adds_collapse_modifier(self) -> None:
        mesh = self._Object()
        scene = type("Scene", (), {"objects": [mesh]})()

        modified = EXPORTER.prepare_static_decimation(scene, self._args(0.25))

        self.assertEqual(modified, 1)
        self.assertEqual(len(mesh.modifiers), 1)
        self.assertEqual(mesh.modifiers[0].decimate_type, "COLLAPSE")
        self.assertEqual(mesh.modifiers[0].ratio, 0.25)
        self.assertTrue(mesh.modifiers[0].use_collapse_triangulate)

    def test_static_decimation_rejects_armature(self) -> None:
        scene = type("Scene", (), {"objects": [self._Object("Rig", "ARMATURE")]})()
        with self.assertRaisesRegex(RuntimeError, "contains an armature"):
            EXPORTER.prepare_static_decimation(scene, self._args())

    def test_static_decimation_rejects_shape_keys(self) -> None:
        mesh = self._Object()
        mesh.data.shape_keys = object()
        scene = type("Scene", (), {"objects": [mesh]})()
        with self.assertRaisesRegex(RuntimeError, "has shape keys"):
            EXPORTER.prepare_static_decimation(scene, self._args())

    def test_decimation_ratio_validation(self) -> None:
        for invalid in (0.0, -0.1, 1.01, float("inf"), float("nan")):
            with self.subTest(ratio=invalid), self.assertRaisesRegex(RuntimeError, "Decimate ratio"):
                EXPORTER.get_decimation_ratio(self._args(invalid))


if __name__ == "__main__":
    unittest.main()
