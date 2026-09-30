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

"""Compare prebuilt Linux/GLES or macOS/Metal ON/OFF snapshots; synthetic timings are not FPS."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sys

CASES = ('plain', 'retained', 'zero', 'mapped', 'mixed', 'removed')
spec = importlib.util.spec_from_file_location('normal_test_runner', Path(__file__).with_name('run-normal-map-tests.py'))
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)


def distribution(values):
    ordered = sorted(values)
    return dict(median=statistics.median(ordered), p95=ordered[math.ceil(len(ordered)*0.95)-1],
                minimum=ordered[0], maximum=ordered[-1])


def summarize(samples):
    groups = {}
    for normal in (0, 1):
        for grid in sorted({s['grid'] for s in samples}):
            for case in CASES:
                rows = [s for s in samples if (s['normal'], s['grid'], s['case']) == (normal, grid, case)]
                if not rows:
                    continue
                numeric = {key: distribution([row[key] for row in rows]) for key in rows[0]
                           if key not in ('normal', 'lights', 'grid', 'repeat', 'vertices', 'case', 'point_lights', 'warm_submit_us', 'warm_sync_us', 'warm_gpu_us', 'log')}
                for kind in (('submit', 'sync', 'gpu') if 'warm_gpu_us' in rows[0] else ('submit', 'sync')):
                    numeric['warm_'+kind+'_us'] = distribution([statistics.median(row['warm_'+kind+'_us']) for row in rows])
                groups[f'{normal}/{grid}/{case}'] = dict(samples=len(rows), vertices=rows[0]['vertices'], metrics=numeric)
    return groups


def validate_sample(sample, normal, lights, case, grid, blocks, backend, point_lights):
    if (sample['normal'], sample['lights'], sample['case'], sample['vertices']) != (normal, lights, case, grid*grid*6):
        raise ValueError('measurement identity mismatch')
    kinds = ('submit', 'sync', 'gpu') if backend == 'metal' else ('submit', 'sync')
    if backend == 'metal' and sample['point_lights'] != point_lights:
        raise ValueError('point-light workload mismatch')
    for kind in kinds:
        values = sample['warm_'+kind+'_us']
        if len(values) != blocks or any(not isinstance(v, (int, float)) or not math.isfinite(v) or v <= 0 for v in values):
            raise ValueError('invalid/incomplete warm '+kind+' blocks')
    positive = ['rss_before', 'rss_loaded', 'rss_first', 'rss_warm', 'source_gpu_bytes']
    if backend == 'metal':
        positive += ['first_gpu_us', 'metal_allocated_before', 'metal_allocated_loaded', 'metal_allocated_first', 'metal_allocated_warm']
    for key in positive:
        if not math.isfinite(sample[key]) or sample[key] <= 0:
            raise ValueError('missing/invalid measurement: '+key)
    for key in ('load_sync_us', 'material_sync_us', 'geometric_compile_sync_us', 'first_submit_us', 'first_sync_us',
                'removal_us', 'derived_before', 'derived_first', 'derived_warm'):
        if not math.isfinite(sample[key]) or sample[key] < 0:
            raise ValueError('missing/invalid measurement: '+key)
    effective = normal == 1 and case in ('mapped', 'mixed', 'removed')
    if sample['derived_before'] != 0 or (sample['derived_first'] > 0) != effective or sample['derived_warm'] != sample['derived_first']:
        raise ValueError('derived allocation contract mismatch')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--enabled', required=True, type=Path, help='Enabled testLib snapshot; matching libraries beside it')
    parser.add_argument('--disabled', required=True, type=Path, help='Disabled testLib snapshot; matching libraries beside it')
    parser.add_argument('--backend', choices=('gles', 'metal'), default='gles')
    parser.add_argument('--point-lights', type=int, default=0, help='Metal only: fixed active point-light workload')
    parser.add_argument('--validate-metal', action='store_true', help='Separate correctness run with Metal validation; not performance evidence')
    parser.add_argument('--lights', type=int, choices=(1, 2, 3, 4), default=2)
    parser.add_argument('--grids', type=int, nargs='+', default=[32, 128])
    parser.add_argument('--repeats', type=int, default=5)
    parser.add_argument('--draws', type=int, default=32)
    parser.add_argument('--blocks', type=int, default=8)
    parser.add_argument('--seed', type=int, default=2026)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--timeout', type=float, default=90)
    parser.add_argument('--allow-mesa-disk-cache', action='store_true')
    args = parser.parse_args()
    if (args.backend == 'gles' and sys.platform != 'linux') or (args.backend == 'metal' and sys.platform != 'darwin'):
        parser.error('benchmark requires Linux/GLES or macOS/Metal')
    if not 0 <= args.point_lights <= args.lights or (args.backend != 'metal' and (args.point_lights or args.validate_metal)):
        parser.error('point lights / Metal validation require Metal and point lights <= build cap')
    if (args.repeats < 3 or args.repeats > 100 or not 1 <= args.draws <= 4096 or
            not 2 <= args.blocks <= 100 or not math.isfinite(args.timeout) or args.timeout <= 0):
        parser.error('invalid repetition, draw, block or timeout bounds')
    if any(g < 2 or g > 256 or g % 2 for g in args.grids) or len(set(args.grids)) != len(args.grids):
        parser.error('grids must be distinct even sizes between 2 and 256')
    binaries = {1: args.enabled.resolve(), 0: args.disabled.resolve()}
    library = 'libcore_mbm.dylib' if args.backend == 'metal' else 'libcore_mbm.so'
    for binary in binaries.values():
        if not binary.is_file() or not (binary.parent/library).is_file():
            parser.error('each snapshot needs testLib and matching '+library)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    records = []
    report = dict(platform=platform.platform(), backend=args.backend, point_lights=args.point_lights,
                  metal_validation=args.validate_metal, lights=args.lights, grids=args.grids,
                  repeats=args.repeats, draws_per_block=args.draws, blocks=args.blocks, seed=args.seed,
                  mesa_disk_cache_disabled=not args.allow_mesa_disk_cache if args.backend == 'gles' else None,
                  metal_cache_policy='OS/driver shader cache uncontrolled; fresh engine process' if args.backend == 'metal' else None,
                  timing=('CPU wall microseconds; sync includes waitUntilCompleted; GPU command-buffer span includes pass overhead'
                          if args.backend == 'metal' else 'CPU wall microseconds; sync includes glFinish, not a GPU timer'),
                  memory='Process RSS includes driver/allocator; GPU bytes are buffer payload only',
                  binaries={}, binary_sizes={}, fixtures={}, samples=records, complete=False)
    def save():
        report['summary'] = summarize(records)
        (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')

    def run(normal, option, label, extra, sentinel):
        env = os.environ.copy()
        # No fault injection/profiling preload in timed processes.
        env.pop('LD_PRELOAD', None)
        env.pop('DYLD_INSERT_LIBRARIES', None)
        env.pop('MBM_NORMAL_MAP_METAL_CAPTURE', None)
        env['LD_LIBRARY_PATH'] = str(binaries[normal].parent)
        env['MESA_SHADER_CACHE_DISABLE'] = 'false' if args.allow_mesa_disk_cache else 'true'
        if args.backend == 'metal':
            env['DYLD_LIBRARY_PATH'] = str(binaries[normal].parent)
            env['MTL_DEBUG_LAYER'] = '1' if args.validate_metal else '0'
            env['MTL_SHADER_VALIDATION'] = '0'
            env['MTL_CAPTURE_ENABLED'] = '0'
        env.update(extra)
        timed_out = False
        try:
            child = subprocess.run([str(binaries[normal]), option], env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=args.timeout)
            code, data = child.returncode, child.stdout
        except subprocess.TimeoutExpired as exc:
            code, data, timed_out = -1, exc.output or b'', True
        text = data.decode('utf-8', errors='replace')
        (out/(label+'.log')).write_text(text, encoding='utf-8')
        reason = checks.verdict(code, text, sentinel, timed_out)
        if reason:
            raise RuntimeError(label+': '+reason)
        return text

    try:
        for normal, binary in binaries.items():
            run(normal, '--normal-map-build-info', 'build-'+str(normal), {},
                f'NORMAL MAP BUILD backend={args.backend} normal={normal} lights={args.lights}')
            report['binaries'][str(normal)] = {name: hashlib.sha256((binary.parent/name).read_bytes()).hexdigest()
                                               for name in (binary.name, library)}
            report['binary_sizes'][str(normal)] = {name: (binary.parent/name).stat().st_size for name in (binary.name, library)}
        for grid in args.grids:
            folder = out/f'fixtures-{grid}'
            run(1, '--normal-map-benchmark-fixtures', 'fixtures-'+str(grid),
                dict(MBM_NORMAL_BENCH_DIR=str(folder), MBM_NORMAL_BENCH_GRID=str(grid)), 'NORMAL MAP BENCH FIXTURES PASS')
            report['fixtures'][str(grid)] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.glob('*.msh')}
        jobs = [(n,g,c,i) for n in (0,1) for g in args.grids for c in CASES for i in range(args.repeats)]
        random.Random(args.seed).shuffle(jobs)
        for index, (normal,grid,case,repeat) in enumerate(jobs, 1):
            label = f'{normal}-{grid}-{case}-{repeat}'
            text = run(normal, '--normal-map-benchmark', label,
                       dict(MBM_NORMAL_BENCH_DIR=str(out/f'fixtures-{grid}'), MBM_NORMAL_BENCH_CASE=case,
                            MBM_NORMAL_BENCH_DRAWS=str(args.draws), MBM_NORMAL_BENCH_BLOCKS=str(args.blocks),
                            MBM_NORMAL_BENCH_POINT_LIGHTS=str(args.point_lights)),
                       'NORMAL MAP BENCH PASS')
            lines = [line.split(' ',1)[1] for line in text.splitlines() if line.startswith('NORMAL_MAP_BENCH_JSON ')]
            if len(lines) != 1:
                raise RuntimeError(label+': missing/duplicate measurement record')
            sample = json.loads(lines[0])
            validate_sample(sample, normal, args.lights, case, grid, args.blocks, args.backend, args.point_lights)
            if args.backend == 'metal':
                if 'NORMAL MAP BENCH CONFIG release=1' not in text:
                    raise RuntimeError(label+': expected Release build')
                active = 'Metal API Validation Enabled' in text
                if active != args.validate_metal:
                    raise RuntimeError(label+': Metal validation state differs from requested run')
            sample.update(grid=grid, repeat=repeat, log=label+'.log',
                          rss_load_delta=sample['rss_loaded']-sample['rss_before'],
                          rss_first_delta=sample['rss_first']-sample['rss_loaded'])
            records.append(sample)
            save()
            print(f'{index}/{len(jobs)} {label} PASS', flush=True)
        report['complete'] = True
    except (RuntimeError, OSError, ValueError, KeyError) as exc:
        report['failure'] = str(exc)
        print('NORMAL MAP BENCHMARK FAIL: '+str(exc), file=sys.stderr)
        save()
        return 1
    save()
    print('NORMAL MAP BENCHMARK PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
