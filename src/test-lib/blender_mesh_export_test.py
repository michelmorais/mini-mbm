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
from pathlib import Path


EXPORTER_PATH = Path(__file__).resolve().parents[2] / "editor" / "blender_mesh_export.py"
SPEC = importlib.util.spec_from_file_location("blender_mesh_export", EXPORTER_PATH)
assert SPEC and SPEC.loader
EXPORTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORTER)


class BlenderMeshExportTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
