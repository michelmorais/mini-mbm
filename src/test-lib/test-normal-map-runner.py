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

"""Guard against false passes in the cross-platform regression runner."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('normal_runner', Path(__file__).with_name('run-normal-map-tests.py'))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class VerdictTests(unittest.TestCase):
    def test_success_requires_marker_and_exit_zero(self):
        self.assertIsNone(runner.verdict(0, 'SUITE PASS', 'SUITE PASS'))
        self.assertIsNotNone(runner.verdict(0, 'ordinary log', 'SUITE PASS'))
        self.assertIsNotNone(runner.verdict(1, 'SUITE PASS', 'SUITE PASS'))

    def test_failure_overrides_success(self):
        for failure in ('SUITE FAIL', 'stack traceback:', 'D3D11 WARNING: binding',
                        'DirectX 11 debug message severity=2',
                        '-[MTLDebugRenderCommandEncoder validateDraw:]:42: failed assertion',
                        'Metal API Validation error: buffer', 'runtime error: unaligned access'):
            with self.subTest(failure=failure):
                self.assertIsNotNone(runner.verdict(0, 'SUITE PASS\n'+failure, 'SUITE PASS'))

    def test_timeout_overrides_partial_success(self):
        self.assertEqual(runner.verdict(0, 'SUITE PASS', 'SUITE PASS', True), 'timeout')

    def test_expected_asset_errors_are_not_driver_failures(self):
        self.assertIsNone(runner.verdict(0, 'failed to load malformed fixture\nSUITE PASS', 'SUITE PASS'))

    def test_sm2_requires_specific_capacity_failure(self):
        self.assertIsNone(runner.sm2_capacity_verdict('error X5608: slots\nerror X5609: total slots'))
        self.assertIsNone(runner.sm2_capacity_verdict('error X4505: registers'))
        self.assertIsNotNone(runner.sm2_capacity_verdict('NORMAL MAP SM2 PASS'))
        self.assertIsNotNone(runner.sm2_capacity_verdict('error X3000: syntax'))
        self.assertIsNotNone(runner.sm2_capacity_verdict('error X5608: slots\nerror X3000: syntax'))


if __name__ == '__main__':
    unittest.main()
