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

"""Reject invalid timing records before including them in benchmark summaries."""
import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('benchmark', Path(__file__).with_name('run-normal-map-benchmark.py'))
benchmark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(benchmark)


class MeasurementTests(unittest.TestCase):
    def setUp(self):
        self.sample = dict(normal=1, lights=2, point_lights=1, case='mapped', vertices=24,
                           warm_submit_us=[1, 2], warm_sync_us=[3, 4], warm_gpu_us=[0.5, 0.7],
                           first_gpu_us=1, rss_before=100, rss_loaded=200, rss_first=300, rss_warm=300,
                           metal_allocated_before=100, metal_allocated_loaded=200,
                           metal_allocated_first=300, metal_allocated_warm=300, source_gpu_bytes=768,
                           load_sync_us=1, material_sync_us=1, geometric_compile_sync_us=1,
                           first_submit_us=1, first_sync_us=3, removal_us=0,
                           derived_before=0, derived_first=1200, derived_warm=1200)

    def validate(self, sample):
        benchmark.validate_sample(sample, 1, 2, 'mapped', 2, 2, 'metal', 1)

    def test_valid_record(self):
        self.validate(self.sample)

    def test_rejects_missing_or_invalid_gpu_timing(self):
        for values in ([], [1], [0, 1], [-1, 1], [float('nan'), 1], [float('inf'), 1]):
            sample = copy.deepcopy(self.sample)
            sample['warm_gpu_us'] = values
            with self.subTest(values=values), self.assertRaises(ValueError):
                self.validate(sample)
        self.sample['first_gpu_us'] = 0
        with self.assertRaises(ValueError):
            self.validate(self.sample)

    def test_rejects_identity_or_allocation_mismatch(self):
        for key, value in (('point_lights', 2), ('normal', 0), ('vertices', 25),
                           ('derived_before', 1), ('derived_warm', 0), ('derived_first', 0)):
            sample = copy.deepcopy(self.sample)
            sample[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.validate(sample)

    def test_gpu_summary_uses_process_block_medians(self):
        first = dict(self.sample, grid=2, repeat=0, log='a.log')
        second = dict(first, repeat=1, warm_gpu_us=[2, 4])
        result = benchmark.summarize([first, second])['1/2/mapped']['metrics']['warm_gpu_us']
        self.assertEqual(result['median'], 1.8)
        self.assertEqual(result['maximum'], 3)

    def test_gles_does_not_require_metal_metrics(self):
        sample = {key: value for key, value in self.sample.items()
                  if not key.startswith('metal_') and key not in ('first_gpu_us', 'warm_gpu_us', 'point_lights')}
        benchmark.validate_sample(sample, 1, 2, 'mapped', 2, 2, 'gles', 0)


if __name__ == '__main__':
    unittest.main()
