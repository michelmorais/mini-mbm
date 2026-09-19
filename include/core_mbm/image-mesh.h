/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef IMAGE_MESH_H
#define IMAGE_MESH_H

#include <core_mbm/core-exports.h>
#include <cstdint>

namespace mbm
{
    class MESH_MBM_DEBUG;

    // Value-only request/result types. Image and mesh buffers remain implementation-owned.
    struct IMAGE_MESH_OPTIONS
    {
        uint32_t x = 0, y = 0, cropWidth = 0, cropHeight = 0;
        uint32_t columns = 32, rows = 32;
        uint32_t maxVertices = 65535, maxTriangles = 131070;
        float width = 100.0f, height = 100.0f, depth = 20.0f, relief = 8.0f;
        float borderWidth = 0.1f;
        bool invert = false;
        bool lockBorder = true;
    };

    struct IMAGE_MESH_REPORT
    {
        uint32_t vertices = 0, triangles = 0;
        float minHeight = 0.0f, maxHeight = 0.0f;
    };

    // CPU-only rectangular proof of concept. Destination must have no frames.
    // Crop uses zero-based top-left pixels; zero crop dimensions mean remaining image.
    // Front faces -Z; relief extends outward; back is +depth/2. Texture references
    // the resolved source image (not copied). On failure discard the destination.
    API_IMPL bool generateImageMesh(const char *imagePath, const IMAGE_MESH_OPTIONS &options,
                                    MESH_MBM_DEBUG &destination, IMAGE_MESH_REPORT &report,
                                    char *errorOut, int errorOutLen);
}

#endif
