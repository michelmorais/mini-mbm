# /*-----------------------------------------------------------------------------------------------------------------------|
# | MIT License (MIT)                                                                                                      |
# | Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
# |                                                                                                                        |
# | Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
# | documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
# | the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
# | to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
# |                                                                                                                        |
# | The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
# | the Software.                                                                                                          |
# |                                                                                                                        |
# | THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
# | WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
# | COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
# | OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
# |                                                                                                                        |
# |-----------------------------------------------------------------------------------------------------------------------*/

"""Collect serial native benchmark repetitions with provenance and no render loop."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--backends", nargs="+", choices=("rigid", "liquidfun", "candidate"), default=["rigid", "candidate"])
    parser.add_argument("--cpu", type=int, help="Pin collector and serial child processes to one Linux CPU")
    args = parser.parse_args()
    if args.cpu is not None:
        if not hasattr(os, "sched_getaffinity") or args.cpu not in os.sched_getaffinity(0):
            parser.error("--cpu must be in this process's available Linux CPU set")
        os.sched_setaffinity(0, {args.cpu})
    if args.runs < 1:
        parser.error("--runs must be positive")
    root = Path(__file__).resolve().parents[3]
    report = {
        "engine_revision": subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip(),
        "platform": platform.platform(), "processor": platform.processor(),
        "warmup_steps": 600, "sample_steps": 3600, "dt": "1/60",
        "timing_unit": "microseconds", "scope": "native Step only; no Lua or rendering",
        "cpu_affinity": sorted(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else None,
        "warmup_note": "600 simulation steps, not 10 seconds of wall-clock warmup",
        "binaries": {}, "source_sha256": {}, "runs": [], "summary": [],
    }
    if Path("/proc/cpuinfo").exists():
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                report["processor"] = line.split(":", 1)[1].strip()
                break
    report["cmake_cache"] = (args.build / "CMakeCache.txt").read_text()
    for directory in ("third-party/box2d-2.4.1", "src/test-lib/physics-compat"):
        for path in sorted((root / directory).rglob("*")):
            if path.is_file() and path.suffix in (".h", ".cpp", ".py", ".txt"):
                report["source_sha256"][str(path.relative_to(root))] = sha(path)
    if "liquidfun" in args.backends:
        entry = next((line.split("=", 1)[1] for line in report["cmake_cache"].splitlines()
                      if line.startswith("PHYSICS_LEGACY_SOURCE:PATH=")), "")
        if not entry:
            raise ValueError("liquidfun requires a build configured with PHYSICS_LEGACY_SOURCE")
        legacy = Path(entry)
        for path in sorted(legacy.rglob("*")):
            if path.is_file() and path.suffix in (".h", ".cpp", ".txt"):
                report["source_sha256"]["legacy/" + str(path.relative_to(legacy))] = sha(path)
    for name in args.backends:
        path = args.build / (f"physics_{name}" + (".exe" if platform.system() == "Windows" else ""))
        report["binaries"][name] = {"path": str(path.resolve()), "sha256": sha(path)}
    # Alternate backend order to reduce systematic ordering bias. Never parallelize timings.
    for repetition in range(args.runs):
        names = args.backends if repetition % 2 == 0 else list(reversed(args.backends))
        for name in names:
            result = subprocess.run([report["binaries"][name]["path"], "--benchmark"],
                                    text=True, capture_output=True, check=True, timeout=180)
            for line in result.stdout.splitlines():
                row = json.loads(line)
                row["repetition"] = repetition + 1
                report["runs"].append(row)
            print(f"Repetition {repetition + 1}/{args.runs}: {name}", flush=True)
    keys = sorted({(r["backend"], r["scenario"]) for r in report["runs"]})
    for backend, scenario in keys:
        rows = [r for r in report["runs"] if (r["backend"], r["scenario"]) == (backend, scenario)]
        summary = {"backend": backend, "scenario": scenario}
        for field in ("median_us", "p95_us", "p99_us"):
            summary[field] = statistics.median(r[field] for r in rows)
        summary["median_range_us"] = [min(r["median_us"] for r in rows), max(r["median_us"] for r in rows)]
        report["summary"].append(summary)
    report["binaries_unchanged"] = all(sha(Path(value["path"])) == value["sha256"]
                                       for value in report["binaries"].values())
    if not report["binaries_unchanged"]:
        raise RuntimeError("Benchmark binary changed during collection")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["summary"], indent=2))


if __name__ == "__main__":
    main()
