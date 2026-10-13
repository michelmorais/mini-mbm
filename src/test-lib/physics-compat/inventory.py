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

"""Inventory published Lua examples and local changes without executing examples."""

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess
import textwrap


def revision(root):
    return subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def examples(path):
    lines = path.read_text().splitlines()
    anchor = "top"
    ordinals = {}
    result = []
    index = 0
    while index < len(lines):
        anchor_match = re.match(r"^\.\. _([^:]+):\s*$", lines[index])
        if anchor_match:
            anchor = anchor_match[1]
        directive = re.match(r"^(\s*)\.\. code-block:: lua\s*$", lines[index])
        if not directive:
            index += 1
            continue
        start = index
        indentation = len(directive[1])
        index += 1
        while index < len(lines) and (not lines[index].strip() or lines[index].lstrip().startswith(":")):
            index += 1
        code = []
        while index < len(lines):
            line = lines[index]
            if line.strip() and len(line) - len(line.lstrip()) <= indentation:
                break
            code.append(line)
            index += 1
        content = textwrap.dedent("\n".join(code)).rstrip() + "\n"
        ordinals[anchor] = ordinals.get(anchor, 0) + 1
        identifier = f"{path.stem}/{anchor}/{ordinals[anchor]:02d}"
        result.append({
            "id": identifier,
            "source": f"doc/{path.name}",
            "line": start + 1,
            "sha256": digest(content.encode()),
            "kind": "scene_candidate" if "function onInitScene" in content or 'require "box2d' in content else "fragment",
            "requires": sorted(set(re.findall(r"require\s*\(?\s*['\"]([^'\"]+)['\"]", content))),
            "assets_mentioned": sorted(set(re.findall(r"['\"]([^'\"\n]+\.(?:png|jpg|spt|msh|ptl|ttf|fnt))['\"]", content))),
            "input_callbacks": sorted(set(re.findall(r"function\s+(on(?:Touch|Key)\w+)\s*\(", content))),
            "status": "pending_baseline",
            "code": content,
        })
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--documentation", type=Path, required=True)
    parser.add_argument("--upstream-box2d", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    local = root / "third-party/box2d-2.4.1"
    upstream = args.upstream_box2d
    if not (upstream / "include/box2d/b2_body.h").is_file():
        parser.error("--upstream-box2d must point to an extracted Box2D v2.4.1 tree")
    records = []
    diffs = []
    for folder in ("include", "src"):
        paths = sorted(set(p.relative_to(local) for p in (local / folder).rglob("*") if p.suffix in (".h", ".cpp")) |
                       set(p.relative_to(upstream) for p in (upstream / folder).rglob("*") if p.suffix in (".h", ".cpp")))
        for relative in paths:
            a, b = upstream / relative, local / relative
            before = a.read_bytes() if a.is_file() else None
            after = b.read_bytes() if b.is_file() else None
            state = "identical" if before == after else "modified"
            if before is None:
                state = "local_only"
            elif after is None:
                state = "upstream_only"
            records.append({"path": str(relative), "state": state,
                            "upstream_sha256": digest(before) if before is not None else None,
                            "local_sha256": digest(after) if after is not None else None})
            if state != "identical":
                diffs.extend(difflib.unified_diff(
                    (before or b"").decode().splitlines(keepends=True),
                    (after or b"").decode().splitlines(keepends=True),
                    fromfile=f"upstream/{relative}", tofile=f"local/{relative}"))
    blocks = []
    for name in ("box2d.rst", "box2LiquidFun.rst"):
        blocks.extend(examples(args.documentation / "doc" / name))
    assert len({block["id"] for block in blocks}) == len(blocks)
    report = {"engine_revision": revision(root), "documentation_revision": revision(args.documentation),
              "upstream_tag": "v2.4.1", "files": records, "examples": blocks}
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "inventory.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    (args.output / "box2d-local.patch").write_text("".join(diffs))
    counts = {name: sum(block["source"] == f"doc/{name}" for block in blocks)
              for name in ("box2d.rst", "box2LiquidFun.rst")}
    print(json.dumps({"example_blocks": counts, "changed_files": sum(r["state"] != "identical" for r in records)}, indent=2))


if __name__ == "__main__":
    main()
