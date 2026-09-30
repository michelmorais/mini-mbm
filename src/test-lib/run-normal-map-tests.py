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

"""Run one prebuilt normal-mapping matrix entry on Linux, Windows or macOS."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

# Expected malformed-asset diagnostics are not failures by themselves. Test
# sentinels decide those cases. Driver validation and runtime exceptions are fatal.
DIAGNOSTIC = re.compile(
    r"\bFAIL\b|stack traceback|AddressSanitizer|runtime error:|"
    r"D3D11 (?:ERROR|WARNING|CORRUPTION)|DirectX 11 debug message severity=|"
    r"resource-lifecycle validation found|Metal API Validation.*(?:error|warning)|"
    r"failed assertion|Execution of the command buffer was aborted",
    re.IGNORECASE,
)


def verdict(returncode, output, sentinel, timed_out=False):
    if timed_out:
        return 'timeout'
    if returncode != 0:
        return 'exit code {}'.format(returncode)
    if DIAGNOSTIC.search(output):
        return 'failure sentinel or runtime/driver diagnostic'
    if sentinel not in output:
        return 'missing success sentinel: ' + sentinel
    return None


def sm2_capacity_verdict(output):
    codes = set(re.findall(r'error (X\d+):', output))
    if not codes.intersection({'X5608', 'X4505'}):
        return 'missing SM2 instruction/register capacity diagnostic'
    if codes.difference({'X5608', 'X5609', 'X4505'}):
        return 'unexpected shader compiler error: ' + ', '.join(sorted(codes))
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-lib', required=True, type=Path)
    parser.add_argument('--engine', required=True, type=Path)
    parser.add_argument('--backend', required=True, choices=['gles', 'dx9', 'dx11', 'metal'])
    parser.add_argument('--normal', required=True, type=int, choices=[0, 1])
    parser.add_argument('--lights', required=True, type=int, choices=[1, 2, 3, 4])
    parser.add_argument('--output', required=True, type=Path, help='New artifact directory for this matrix entry')
    parser.add_argument('--library-dir', action='append', default=[], type=Path)
    parser.add_argument('--timeout', type=float, default=90)
    parser.add_argument('--gles-fault-library', type=Path,
                        help='Opt-in Linux GLES LD_PRELOAD library for failure/retry tests')
    parser.add_argument('--require-native-validation', action='store_true',
                        help='Require DX11 Debug info queue/lifecycle, or enable Metal API validation')
    parser.add_argument('--skeletal-parity', action='store_true',
                        help='Also verify native DX9/DX11 synthetic/Lorekeeper LBS/DQS parity')
    parser.add_argument('--dx11-failure', action='store_true',
                        help='Also inject DX11 mapped COM creation failures and verify retry')
    args = parser.parse_args()
    if args.dx11_failure and args.backend != 'dx11':
        parser.error('--dx11-failure requires dx11')
    if args.skeletal_parity and args.backend not in ('dx9', 'dx11'):
        parser.error('--skeletal-parity currently supports dx9 and dx11')
    if args.gles_fault_library:
        if sys.platform != 'linux' or args.backend != 'gles':
            parser.error('--gles-fault-library requires Linux/GLES')
        if not args.gles_fault_library.is_file():
            parser.error('fault library not found: ' + str(args.gles_fault_library))
        if any(c.isspace() or c == ':' for c in str(args.gles_fault_library.resolve())):
            parser.error('LD_PRELOAD library path cannot contain whitespace or colons')
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    if args.require_native_validation and args.backend not in ('dx11', 'metal'):
        parser.error('native validation switch supports dx11 and metal')
    for binary in (args.test_lib, args.engine):
        if not binary.is_file():
            parser.error('binary not found: ' + str(binary))
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)  # Never mix stale images/logs with this run.
    fixtures = output / 'fixtures'
    fixtures.mkdir()
    source = Path(__file__).resolve().parent
    env = os.environ.copy()
    libraries = [str(p.resolve()) for p in args.library_dir]
    libraries += [str(args.test_lib.resolve().parent), str(args.engine.resolve().parent)]
    for key in ('PATH', 'LD_LIBRARY_PATH', 'DYLD_LIBRARY_PATH'):
        env[key] = os.pathsep.join(libraries + [env.get(key, '')])
    env.update(MBM_NORMAL_MAP_FIXTURE_DIR=str(fixtures),
               MBM_EXPECT_NORMAL_MAPPING_3D=str(args.normal), MBM_EXPECT_MAX_LIGHTS=str(args.lights),
               MBM_EXPECT_BACKEND=args.backend)
    if args.backend == 'metal':
        env['MTL_DEBUG_LAYER'] = '1'
    if args.backend == 'dx11':
        env['MBM_DIRECTX11_VALIDATE'] = '1'
    results = []

    def run(name, command, sentinel, extra=None, required=(), validator=None):
        child_env = env.copy()
        child_env.update(extra or {})
        start = time.monotonic()
        timed_out = False
        # Direct executable launch, no shell; subprocess.run kills and reaps the
        # child on timeout on all three hosts. Engine launchers must not fork.
        try:
            proc = subprocess.run(command, cwd=str(source.parents[1]), env=child_env,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  timeout=args.timeout)
            code, data = proc.returncode, proc.stdout
        except subprocess.TimeoutExpired as exc:
            code, data, timed_out = -1, exc.output or b'', True
        except OSError as exc:
            code, data = -1, str(exc).encode()
        text = data.decode('utf-8', errors='replace')
        (output / (name + '.log')).write_text(text, encoding='utf-8')
        reason = verdict(code, text, sentinel, timed_out)
        for marker in required:
            if marker not in text:
                reason = reason or 'missing validation evidence: ' + marker
        if validator:
            reason = reason or validator(text)
        results.append(dict(name=name, command=command, exit_code=code,
                            seconds=round(time.monotonic()-start, 3), failure=reason))
        print('{}: {}{}'.format(name, 'FAIL' if reason else 'PASS', ': '+reason if reason else ''), flush=True)
        return reason is None

    test_lib, engine = str(args.test_lib.resolve()), str(args.engine.resolve())
    build = 'NORMAL MAP BUILD backend={} normal={} lights={}'.format(args.backend, args.normal, args.lights)
    ready = run('build-info', [test_lib, '--normal-map-build-info'], build)
    if ready:
        run('preparation', [test_lib, '--normal-map-preparation-tests'], '[normal-map-preparation] PASS')
        ready = run('persistence', [test_lib, '--normal-map-persistence-tests'], '[normal-map-persistence] PASS')
    if ready:
        def scene(name, script, sentinel, extra=None):
            return run(name, [engine, '--scene', str(source / script), '--disable_select_monitor',
                              '--nosplash', '-w', '640', '-h', '480'], sentinel, extra)
        ready = scene('build-config', 'render-build-config-test.lua',
                      'RENDER BUILD CONFIG PASS normal={} lights={}'.format(args.normal, args.lights))
        if ready:
            markers = ()
            if args.backend == 'dx11' and args.require_native_validation:
                markers = ('DirectX 11 debug-layer validation passed',
                           'DirectX 11 resource-lifecycle validation passed')
            run('resources', [test_lib, '--normal-map-lazy-resource-test'],
                'NORMAL MAP LAZY RESOURCES PASS', required=markers)
            if args.skeletal_parity:
                backend_name = 'DirectX 9' if args.backend == 'dx9' else 'DirectX 11'
                cases = tuple('skeletal GPU parity: backend={} fixture={} method={}'.format(
                    backend_name, fixture, method)
                    for fixture in ('synthetic', 'Lorekeeper') for method in ('lbs', 'dqs'))
                run('skeletal-parity', [test_lib, '--directx{}-skeletal-parity-test'.format(
                    '9' if args.backend == 'dx9' else '11')],
                    'skeletal GPU parity suite: backend={} cases=4 PASS'.format(backend_name),
                    required=cases + markers)
            if args.backend == 'dx9':
                run('sm2', [test_lib, '--normal-map-sm2-test'], 'NORMAL MAP SM2 PASS',
                    required=('NORMAL MAP SM2 LIGHTING LIMIT: default lighting unavailable',
                              'NORMAL MAP SM2 BYTECODE PASS vs_2_0 ps_2_0'),
                    validator=sm2_capacity_verdict)
            if args.gles_fault_library:
                run('recovery', [test_lib, '--normal-map-failure-test'], 'NORMAL MAP RECOVERY PASS',
                    {'LD_PRELOAD': str(args.gles_fault_library.resolve())})
            if args.dx11_failure:
                cases = ('vertex-shader', 'pixel-shader', 'input-layout', 'linear-sampler',
                         'nearest-sampler', 'matrix-buffer', 'light-buffer', 'normal-settings',
                         'zero-tangent', 'derived-vertices', 'derived-indices',
                         'partition-second-vertices', 'partition-second-indices',
                         'subset-second-vertices', 'subset-second-indices',
                         'map-matrix', 'map-light', 'map-normal', 'map-second-light', 'map-second-normal')
                map_cases = cases[-5:]
                run('recovery', [test_lib, '--normal-map-failure-test'],
                    'NORMAL MAP RECOVERY PASS cases=20 normal={}'.format(args.normal),
                    required=markers + tuple('NORMAL MAP RECOVERY CASE {} PASS normal={}'.format(
                        case, args.normal) for case in cases) + tuple(
                        'NORMAL MAP RECOVERY PIXELS {} PASS normal={}'.format(case, args.normal) for case in cases) + tuple(
                        'NORMAL MAP RECOVERY MAP {} PASS normal={} injected={}'.format(
                            case, args.normal, 0 if args.normal == 0 and case in ('map-normal', 'map-second-normal') else 2)
                        for case in map_cases))
            if sys.platform == 'linux' and args.backend == 'gles':
                run('context', [test_lib, '--normal-map-context-test'], 'NORMAL MAP CONTEXT PASS')
            scene('runtime', 'normal-map-runtime-test.lua', 'NORMAL MAP RUNTIME PASS')
            scene('readback', 'normal-map-readback-test.lua', 'NORMAL MAP READBACK PASS')
            for mode in ('ib', 'vb'):
                images = output / mode
                images.mkdir()
                scene('visual-'+mode, 'normal-map-render-test.lua', 'NORMAL MAP VISUAL PASS',
                      dict(MBM_NORMAL_MAP_TEST_VB='1' if mode == 'vb' else '0',
                           MBM_NORMAL_MAP_RENDER_DIR=str(images)))
    report = dict(backend=args.backend, normal=args.normal, lights=args.lights,
                  fault_library=str(args.gles_fault_library.resolve()) if args.gles_fault_library else None,
                  native_validation_requested=args.require_native_validation, results=results)
    report['skeletal_parity_requested'] = args.skeletal_parity
    report['dx11_failure_requested'] = args.dx11_failure
    (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return 1 if any(item['failure'] for item in results) else 0


if __name__ == '__main__':
    sys.exit(main())
