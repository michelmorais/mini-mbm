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

"""Run Ratworks acceptance scenes with isolated libraries (RSS sampling on Linux)."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--game', type=Path)
    parser.add_argument('--legacy', action='store_true', help='Compare the old plugin (full game only)')
    parser.add_argument('--seconds', type=float, default=600)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or args.seconds < 8 or args.seconds > 3600:
        parser.error('--seconds must be between 8 and 3600')
    if args.legacy and not args.game:
        parser.error('--legacy requires --game')
    engine = args.engine.resolve()
    scene = Path(__file__).with_name('ratworks-client.lua' if args.game else 'ratworks-reduced.lua').resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    suffix = '.dll' if os.name == 'nt' else '.so'
    env.update(LD_LIBRARY_PATH=str(engine.parent), LUA_CPATH=engine.parent.as_posix() + '/?' + suffix,
               MBM_PHYSICS_MODULE_DIR=engine.parent.as_posix(), MBM_PHYSICS_SECONDS=str(args.seconds),
               MBM_PHYSICS_BACKEND='legacy' if args.legacy else 'unified')
    env['PATH'] = str(engine.parent) + os.pathsep + env.get('PATH', '')
    if args.game:
        env['MBM_RATWORKS_ROOT'] = args.game.resolve().as_posix()
    command = [str(engine), '--scene', str(scene), '--disable_select_monitor', '--nosplash',
               '-w', '800', '-h', '600']
    files = [engine, scene, *sorted(engine.parent.glob('box2d*' + suffix)),
             engine.parent / ('core_mbm.dll' if os.name == 'nt' else 'libcore_mbm.so')]
    hashes = {str(p): digest(p) for p in files}
    game_hashes = ({str(p.relative_to(args.game)): digest(p)
                    for p in sorted(args.game.rglob('*.lua')) if '.git' not in p.parts}
                   if args.game else None)
    start = time.monotonic()
    samples = []
    timed_out = False
    with (args.output / 'run.log').open('w') as log:
        process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            while process.poll() is None:
                elapsed = time.monotonic() - start
                if elapsed > args.seconds + 45:
                    timed_out = True
                    process.kill()
                    break
                try:
                    status = Path(f'/proc/{process.pid}/status').read_text()
                    rss = next(int(line.split()[1]) for line in status.splitlines() if line.startswith('VmRSS:'))
                    samples.append({'wall_s': round(elapsed, 3), 'rss_kib': rss})
                except (OSError, StopIteration):
                    pass
                time.sleep(1)
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
    text = (args.output / 'run.log').read_text(errors='replace')
    marker = 'RATWORKS_OK' if args.game else 'RATWORKS_REDUCED_OK'
    passed = process.returncode == 0 and marker in text and '_FAILED' not in text and not timed_out
    unchanged = all(digest(p) == hashes[str(p)] for p in files)
    if args.game:
        unchanged = unchanged and all(digest(args.game / p) == value for p, value in game_hashes.items())
    passed = passed and unchanged
    result = dict(passed=passed, exit_code=process.returncode, timed_out=timed_out,
                  inputs_unchanged=unchanged, backend='legacy' if args.legacy else 'unified',
                  command=command, requested_seconds=args.seconds,
                  wall_seconds=round(time.monotonic()-start, 3), host=platform.platform(),
                  hashes=hashes, rss_samples=samples, rss_supported=sys.platform.startswith('linux'),
                  markers=[line for line in text.splitlines() if 'RATWORKS_' in line])
    if args.game:
        result['game_revision'] = subprocess.check_output(
            ['git', '-c', 'safe.directory=' + args.game.resolve().as_posix(),
             '-C', str(args.game), 'rev-parse', 'HEAD'], text=True).strip()
        result['game_source_hashes'] = game_hashes
    (args.output / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ('passed', 'exit_code', 'wall_seconds', 'markers')}))
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
