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

"""Run the real engine's Box2D Lua contracts with isolated platform libraries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--profile', choices=('on', 'off'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    engine = args.engine.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if sys.platform == 'darwin':
        suffix = '.dylib'
    elif os.name == 'nt':
        suffix = '.dll'
    else:
        suffix = '.so'
    artifacts = [engine, engine.parent / ('box2d' + suffix),
                 engine.parent / ('box2dLiquidFun' + suffix)]
    for path in artifacts:
        if not path.is_file():
            parser.error(f'Missing profile artifact: {path}')
    hashes = {str(p): digest(p) for p in artifacts}
    environment = os.environ.copy()
    environment.update(MBM_PHYSICS_MODULE_DIR=engine.parent.as_posix(),
                       MBM_PHYSICS_MODULE_SUFFIX=suffix,
                       MBM_PHYSICS_PROFILE=args.profile,
                       LUA_CPATH=engine.parent.as_posix() + '/?' + suffix,
                       LD_LIBRARY_PATH=str(engine.parent), DYLD_LIBRARY_PATH=str(engine.parent))
    environment['PATH'] = str(engine.parent) + os.pathsep + environment.get('PATH', '')
    here = Path(__file__).resolve().parent
    cases = [('modern', here / 'lua-unified.lua', 'PHYSICS_LUA_OK')]
    if args.profile == 'on':
        cases.append(('legacy', here / 'lua-unified.lua', 'PHYSICS_LUA_OK'))
        for name in ('rise', 'filter', 'convex', 'depth'):
            cases.append((name, here.parent / ('fluid_' + name + '_smoke.lua'),
                          'FLUID_' + name.upper() + '_SMOKE_OK'))
    results = []
    for name, scene, marker in cases:
        env = environment.copy()
        env['MBM_PHYSICS_ORDER'] = name
        if name == 'depth':
            env['MBM_FLUID_CAPTURE'] = (output / 'depth.png').as_posix()
        command = [str(engine), '--scene', scene.as_posix(), '--disable_select_monitor',
                   '--nosplash', '-w', '400', '-h', '300']
        log = output / (name + '.log')
        timed_out = False
        with log.open('w') as stream:
            try:
                process = subprocess.run(command, env=env, stdout=stream,
                                         stderr=subprocess.STDOUT, timeout=45, check=False)
                code = process.returncode
            except subprocess.TimeoutExpired:
                timed_out, code = True, None
        text = log.read_text(errors='replace')
        passed = code == 0 and marker in text and '_FAILED' not in text
        results.append(dict(case=name, passed=passed, exit_code=code, timed_out=timed_out,
                            marker_seen=marker in text, scene_sha256=digest(scene),
                            log_sha256=digest(log), command=command))
        print(name, 'PASS' if passed else 'FAIL', flush=True)
    unchanged = all(digest(p) == hashes[str(p)] for p in artifacts)
    passed = unchanged and all(case['passed'] for case in results)
    report = dict(passed=passed, profile=args.profile, binaries_unchanged=unchanged,
                  binaries_sha256=hashes, cases=results)
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
