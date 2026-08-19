/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef SKELETAL_PARITY_TESTS_H
#define SKELETAL_PARITY_TESTS_H

#include <skeletal-gpu-lbs.h>

#include <cstdint>
#include <string>
#include <vector>

namespace mbm::skeletal::test
{
    struct SKELETAL_PARITY_CASE
    {
        std::string fixtureName;
        SKELETAL_SHADER_METHOD method = SKELETAL_SHADER_METHOD::LBS;
        uint32_t paletteSize = 0;
        std::vector<float> palette;
        std::vector<VEC3> positions;
        std::vector<VEC3> normals;
        std::vector<GPU_LBS_VERTEX> influences;
        std::vector<VEC3> expectedPositions;
        std::vector<VEC3> expectedNormals;
        std::vector<uint32_t> sourceVertexIndices;
    };

    struct SKELETAL_PARITY_ENCODING
    {
        VEC3 positionCenter;
        float positionExtent = 0.0f;
        float positionTolerance = 0.0f;
        float normalTolerance = 0.0f;
    };

    using SKELETAL_PARITY_CAPTURE_RGBA8 = bool (*)(
        const SKELETAL_PARITY_CASE &testCase,
        const SKELETAL_PARITY_ENCODING &encoding,
        std::vector<uint8_t> &positionPixels,
        std::vector<uint8_t> &normalPixels,
        std::string &error);

    bool buildSkeletalParityCases(std::vector<SKELETAL_PARITY_CASE> &cases,
                                  std::string &error);
    bool runSkeletalParitySuite(const char *backendName,
                                SKELETAL_PARITY_CAPTURE_RGBA8 capture,
                                std::string &error);
}

#endif
