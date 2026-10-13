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

"""Run extracted scene candidates; passing means runtime smoke coverage, not visual equivalence."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def lua_string(value):
    # JSON strings for these filesystem paths/identifiers are also Lua strings.
    return json.dumps(str(value), ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", required=True, type=Path)
    parser.add_argument("--documentation", required=True, type=Path)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seconds", type=float, default=2.0)
    parser.add_argument("--plugin", choices=("all", "box2d", "box2dLiquidFun"), default="all")
    args = parser.parse_args()
    if not 0.1 <= args.seconds <= 30:
        parser.error("--seconds must be between 0.1 and 30")
    args.output.mkdir(parents=True, exist_ok=True)
    if sys.platform == 'darwin':
        suffix = '.dylib'
    elif os.name == 'nt':
        suffix = '.dll'
    else:
        suffix = '.so'
    binaries = {}
    for path in [args.engine.resolve()] + sorted(args.engine.resolve().parent.glob('box2d*' + suffix)):
        binaries[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    records = json.loads(args.inventory.read_text())["examples"]
    environment = os.environ.copy()
    module_dir = args.engine.resolve().parent.as_posix()
    environment.update(LD_LIBRARY_PATH=module_dir, DYLD_LIBRARY_PATH=module_dir,
                       LUA_CPATH=module_dir + '/?' + suffix)
    environment['PATH'] = str(args.engine.resolve().parent) + os.pathsep + environment.get('PATH', '')
    results = []
    for record in records:
        result = {key: record[key] for key in ("id", "source", "line", "sha256", "input_callbacks")}
        if args.plugin != "all" and args.plugin not in record["requires"]:
            result["status"] = "outside_profile"
            results.append(result)
            continue
        # Select self-contained construction examples, not isolated calls/tables.
        runnable = record["requires"] and re.search(r"(?:shape|sprite):new", record["code"])
        if not runnable:
            result["status"] = "needs_context"
            results.append(result)
            continue
        stem = record["id"].replace("/", "__")
        scene = (args.output / f"{stem}.lua").resolve()
        log = args.output / f"{stem}.log"
        scene.write_text(f'''-- Generated from {record['source']}:{record['line']}; do not edit.
mbm.onErrorStop(true)
package.cpath = {lua_string(module_dir + '/?' + suffix)}
mbm.addPath({lua_string((args.documentation.resolve() / 'doc').as_posix())})
mbm.addPath({lua_string((args.documentation.resolve() / 'doc/_static').as_posix())})
local failed = false
local function checked(fn, ...)
    if not fn or failed then return end
    local ok, err = pcall(fn, ...)
    if not ok then
        failed = true
        print('EXAMPLE_SMOKE_FAILED', tostring(err))
        mbm.quit()
    end
end
checked(function()
{record['code']}
end)
local originalInit, originalLoop = onInitScene, onLoop
local elapsed = 0
function onInitScene() checked(originalInit) end
function onLoop(delta)
    checked(originalLoop, delta)
    elapsed = elapsed + delta
    if not failed and elapsed >= {args.seconds} then
        print('EXAMPLE_SMOKE_OK', {lua_string(record['id'])})
        mbm.quit()
    end
end
''')
        command = [str(args.engine.resolve()), "--scene", str(scene),
                   "--disable_select_monitor", "--nosplash", "-w", "640", "-h", "480"]
        try:
            with log.open("w") as output:
                process = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT,
                                         timeout=args.seconds + 12, check=False, env=environment)
            text = log.read_text(errors="replace")
            passed = process.returncode == 0 and "EXAMPLE_SMOKE_OK" in text and "EXAMPLE_SMOKE_FAILED" not in text
            result.update(status="smoke_pass" if passed else "smoke_fail", exit_code=process.returncode)
        except subprocess.TimeoutExpired:
            result.update(status="timeout")
        result["log"] = log.name
        result["visual_equivalence"] = "not_checked"
        result["interactive_inputs"] = "not_replayed"
        results.append(result)
        print(record["id"], result["status"], flush=True)
        (args.output / "results.json").write_text(json.dumps({"complete": False, "binaries_sha256": binaries, "examples": results}, indent=2) + "\n")
    (args.output / "results.json").write_text(json.dumps({
        "complete": True, "binaries_sha256": binaries,
        "engine": str(args.engine.resolve()), "display": os.environ.get("DISPLAY"),
        "duration_seconds": args.seconds, "examples": results}, indent=2) + "\n")
    failures = sum(item["status"] in ("smoke_fail", "timeout") for item in results)
    print(f"Completed: {len(results)} catalogued, {failures} runtime failures")
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
