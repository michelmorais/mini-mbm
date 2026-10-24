import importlib.util
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parents[2] / "editor" / "facial_rig_blender_import.py"
SPEC = importlib.util.spec_from_file_location("facial_rig_blender_import", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class FacialRigBridgeImportTest(unittest.TestCase):
    def test_parse_generated_lua_assignments_and_references(self):
        source = """-- mini-mbm facial rig Blender bridge
local facialRigBridge = {}
facialRigBridge = {}
facialRigBridge[\"schema\"] = \"mini-mbm-facial-rig-bridge-1\"
facialRigBridge[\"coordinateSystem\"] = \"mini-mbm-3d-y-up\"
facialRigBridge[\"vertexCount\"] = 0x1.8p+1
facialRigBridge[\"rig\"] = {}
facialRigBridge[\"rig\"][\"origin\"] = {}
facialRigBridge[\"rig\"][\"origin\"][\"position\"] = {}
facialRigBridge[\"rig\"][\"origin\"][\"position\"][\"x\"] = 0x0p+0
facialRigBridge[\"rig\"][\"origin\"][\"position\"][\"y\"] = 0x1p+1
facialRigBridge[\"rig\"][\"origin\"][\"position\"][\"z\"] = -0x1p+0
facialRigBridge[\"rig\"][\"bones\"] = {}
facialRigBridge[\"rig\"][\"bones\"][1] = {}
facialRigBridge[\"rig\"][\"bones\"][1][\"id\"] = \"face_root\"
facialRigBridge[\"rig\"][\"bones\"][1][\"head\"] = facialRigBridge[\"rig\"][\"origin\"][\"position\"]
return facialRigBridge
"""
        with tempfile.NamedTemporaryFile("w", suffix=".lua", encoding="utf-8") as output:
            output.write(source)
            output.flush()
            bridge = MODULE.parse_bridge(output.name)
        self.assertEqual(bridge["vertexCount"], 3.0)
        self.assertEqual(bridge["rig"]["bones"][0]["head"], bridge["rig"]["origin"]["position"])
        self.assertEqual(bridge["rig"]["origin"]["position"]["z"], -1.0)

    def test_y_up_rotation_to_blender(self):
        self.assertEqual(MODULE.rotate_point({"x": 2, "y": 3, "z": 4}, (90.0, 0.0, 0.0))[0], 2.0)
        self.assertAlmostEqual(MODULE.rotate_point({"x": 2, "y": 3, "z": 4}, (90.0, 0.0, 0.0))[1], -4.0)
        self.assertAlmostEqual(MODULE.rotate_point({"x": 2, "y": 3, "z": 4}, (90.0, 0.0, 0.0))[2], 3.0)


if __name__ == "__main__":
    unittest.main()
