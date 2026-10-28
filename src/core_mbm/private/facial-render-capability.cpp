/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                             |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                    |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE  |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/

#include "facial-render-capability.h"

#include <mutex>
#include <algorithm>
#include <limits>

namespace mbm::facial
{
    namespace
    {
        std::mutex capabilityMutex;
        GPU_MORPH_CAPABILITY measuredCapability;
    }

    GPU_MORPH_CAPABILITY calculateGpuMorphCapability(const uint32_t maxVertexTextureUnits,
                                                      const uint32_t maxTextureSize,
                                                      const bool hasFloatTexture) noexcept
    {
        GPU_MORPH_CAPABILITY result;
        result.maxVertexTextureUnits = maxVertexTextureUnits;
        result.maxTextureSize = maxTextureSize;
        result.hasFloatTexture = hasFloatTexture;
        result.measured = maxVertexTextureUnits > 0 && maxTextureSize > 0;
        // The resource prerequisites are measured separately from implementation support.
        // Each backend that consumes this shared contract reports its own measured limits.
        // Texture unit 6 is reserved for the morph stream; stages 0-5 remain
        // available for the material and animation textures.
        result.supportsIndexedDeltas = result.measured && hasFloatTexture && maxVertexTextureUnits >= 7u;
        return result;
    }

    void setMeasuredGpuMorphCapability(const uint32_t maxVertexTextureUnits,
                                       const uint32_t maxTextureSize,
                                       const bool hasFloatTexture) noexcept
    {
        const GPU_MORPH_CAPABILITY measured = calculateGpuMorphCapability(
            maxVertexTextureUnits, maxTextureSize, hasFloatTexture);
        std::lock_guard<std::mutex> lock(capabilityMutex);
        measuredCapability = measured;
    }

    GPU_MORPH_CAPABILITY getMeasuredGpuMorphCapability() noexcept
    {
        std::lock_guard<std::mutex> lock(capabilityMutex);
        return measuredCapability;
    }

    bool buildGpuMorphLayout(const ASSET &asset, const GPU_MORPH_CAPABILITY &capability,
                             const bool includeNormals, GPU_MORPH_LAYOUT &out, std::string &error) noexcept
    {
        out = {};
        error.clear();
        if (!capability.measured || !capability.supportsIndexedDeltas)
        { error = "gpu-morph-capability-unavailable"; return false; }
        if (!asset.vertexCount || asset.targets.empty())
        { error = "gpu-morph-empty-asset"; return false; }
        if (asset.vertexCount > capability.maxTextureSize)
        {
            // The layout can tile vertices vertically, but each target block must still fit.
            if (capability.maxTextureSize == 0)
            { error = "gpu-morph-invalid-texture-size"; return false; }
        }
        const uint32_t width = std::min(asset.vertexCount, capability.maxTextureSize);
        if (!width)
        { error = "gpu-morph-invalid-texture-width"; return false; }
        const uint64_t blockRows = (static_cast<uint64_t>(asset.vertexCount) + width - 1u) / width;
        const uint64_t rowsPerTarget = blockRows * (includeNormals ? 2u : 1u);
        const uint64_t height = rowsPerTarget * asset.targets.size();
        if (blockRows > std::numeric_limits<uint32_t>::max() ||
            rowsPerTarget > std::numeric_limits<uint32_t>::max() ||
            height > capability.maxTextureSize)
        { error = "gpu-morph-texture-layout-too-large"; return false; }
        out.textureWidth = width;
        out.textureHeight = static_cast<uint32_t>(height);
        out.rowsPerVertexBlock = static_cast<uint32_t>(blockRows);
        out.rowsPerTarget = static_cast<uint32_t>(rowsPerTarget);
        out.targetCount = static_cast<uint32_t>(asset.targets.size());
        out.vertexCount = asset.vertexCount;
        out.hasNormals = includeNormals;
        out.valid = true;
        return true;
    }
}
