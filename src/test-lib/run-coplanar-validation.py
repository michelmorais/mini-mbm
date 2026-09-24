# /*-----------------------------------------------------------------------------------------------------------------------|
# | MIT License (MIT)                                                                                                      |
# | Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                             |
# |                                                                                                                        |
# | Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
# | documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
# | the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
# | to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
# | The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
# | THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
# | WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
# | COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
# | OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
# |-----------------------------------------------------------------------------------------------------------------------*/
"""Repeatable coplanar acceptance checks. Run from any directory after building mini-mbm.

All generated files/logs stay in --output (default: a fresh temporary directory).
The Lua scenes also use their documented /tmp fixtures. Exit status alone is not
sufficient for engine scenes: each explicit success sentinel is mandatory.
"""
import argparse
import json
import pathlib
import re
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCENES = {
    "mesh_coplanar_smoke": "COPLANAR SMOKE OK",
    "mesh_coplanar_holes_smoke": "COPLANAR HOLES SMOKE OK",
    "mesh_coplanar_boundary_smoke": "COPLANAR BOUNDARY SMOKE OK",
    "mesh_coplanar_normals_smoke": "COPLANAR AFFINE NORMALS OK",
    "mesh_coplanar_obstacles_smoke": "COPLANAR OBSTACLE SEPARATION OK",
    "mesh_coplanar_spatial_smoke": "COPLANAR 3D SEPARATION OK",
    "mesh_coplanar_imported_smoke": "COPLANAR IMPORTED ASSETS OK",
    "mesh_coplanar_boundary_budget_smoke": "COPLANAR BOUNDARY COORDINATION BUDGET FALLBACK OK",
    "mesh_coplanar_cancel_smoke": "COPLANAR CANCEL / RETRY / PROGRESS / LATE CANCEL / EDITOR NOOP UNDO / REPORT OK",
    "image_mesh_coplanar_smoke": "IMAGE MESH COPLANAR MODES / PREVIEW / SAVE / EXPORT / LEGACY / CURVED / IDLE OK",
    "mesh_simplify_smoke": "SIMPLIFY SMOKETEST OK",
    "image_mesh_curved_simplify_smoke": "CURVED SIMPLIFY CANCEL / NO PARTIAL RESULT OK",
    "mesh_coplanar_visual_smoke": "COPLANAR VISUAL CHECKER / DIRECTIONAL + POINT LIGHT / OBLIQUE OK",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--engine", type=pathlib.Path, default=ROOT / "bin/debug/linux_x86/mini-mbm")
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--structural-only", action="store_true")
    args = parser.parse_args()
    output = (args.output or pathlib.Path(tempfile.mkdtemp(prefix="coplanar-validation-"))).resolve()
    output.mkdir(parents=True, exist_ok=True)
    results = []

    def run(name, command, sentinel=None):
        start = time.monotonic()
        with (output / (name + ".log")).open("w") as log:
            completed = subprocess.run([str(x) for x in command], cwd=ROOT, stdout=log,
                                       stderr=subprocess.STDOUT, timeout=90, check=False)
        text = (output / (name + ".log")).read_text(errors="replace")
        ok = completed.returncode == 0 and (sentinel is None or sentinel in text)
        ok = ok and re.search(r"\bFAIL\b|stack traceback:|Assertion .*failed", text) is None
        results.append(dict(name=name, ok=ok, seconds=round(time.monotonic()-start, 3)))
        (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
        print(f"{name}: {'OK' if ok else 'FAIL'}", flush=True)
        if not ok:
            raise RuntimeError(f"Validation failed; inspect {output / (name + '.log')}")

    # Source-transformed temporary copies avoid -mlong-double-64 ABI incompatibility
    # with host libstdc++/libm. This is a precision test, NOT a Windows/macOS emulator.
    for precision in ("native", "binary64"):
        tree = output / precision
        for filename in ("mesh-planar-index", "mesh-planar-separation"):
            for relative in (f"src/core_mbm/private/{filename}.h", f"src/test-lib/unit/{filename}-test.cpp"):
                dest = tree / relative
                dest.parent.mkdir(parents=True, exist_ok=True)
                source = (ROOT / relative).read_text()
                if precision == "binary64":
                    source = re.sub(r"(?<=[0-9.])[lL]\b", "", source.replace("long double", "double"))
                dest.write_text(source)
            name = f"{filename}-{precision}"
            binary = output / name
            run(name + "-build", [args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-pedantic",
                                  tree / f"src/test-lib/unit/{filename}-test.cpp", "-o", binary])
            sentinel = "PLANAR INDEX OK" if filename.endswith("index") else "PLANAR 3D SEPARATION OK"
            run(name, [binary], sentinel)
    binary = output / "separation-fast-math"
    run("fast-math-build", [args.compiler, "-std=c++17", "-O2", "-ffast-math",
                            ROOT / "src/test-lib/unit/mesh-planar-separation-test.cpp", "-o", binary])
    run("fast-math", [binary], "PLANAR 3D SEPARATION FAST-MATH FALLBACK OK")
    if not args.structural_only:
        for scene, sentinel in SCENES.items():
            run(scene, [args.engine.resolve(), "--scene", ROOT / f"src/test-lib/{scene}.lua",
                        "--disable_select_monitor", "--nosplash", "-w", "800", "-h", "600"], sentinel)
    print(f"COPLANAR VALIDATION OK: {output}")


if __name__ == "__main__":
    main()
