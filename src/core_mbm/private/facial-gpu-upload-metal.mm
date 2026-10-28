/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                             |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE  |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/

#if defined(USE_METAL)
#include "facial-gpu-upload.h"
#include "specific-metal-buffer.h"
#include <core_mbm/device.h>
#include <core_mbm/shader.h>
#include <vector>

namespace mbm::facial
{
    bool uploadGpuMorphStream(BUFFER_GL *buffer, const ASSET &asset,
                              const GPU_MORPH_LAYOUT &layout) noexcept
    {
        if (!buffer || !layout.valid || asset.targets.size() != layout.targetCount ||
            asset.vertexCount != layout.vertexCount || !asset.vertexCount ||
            buffer->sizeOfArrayVertex != asset.vertexCount)
            return false;
        BUFFER_SPECIFIC *backend = buffer->getBackendBuffer();
        DEVICE *device = DEVICE::getInstance();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = device ? device->getSpecificContextDevice() : nullptr;
        if (!backend || !context || !context->mtlDevice) return false;
        const uint64_t texels = static_cast<uint64_t>(layout.textureWidth) * layout.textureHeight;
        // Bound temporary CPU memory and the immutable GPU resource independently of texture limits.
        if (!texels || texels > (256u * 1024u * 1024u) / (4u * sizeof(float))) return false;
        std::vector<float> data(static_cast<size_t>(texels) * 4u, 0.0f);
        for (uint32_t t = 0; t < layout.targetCount; ++t)
        {
            const TARGET &target = asset.targets[t];
            if (target.positions.size() != asset.vertexCount ||
                (layout.hasNormals && target.normals.size() != asset.vertexCount)) return false;
            for (uint32_t v = 0; v < asset.vertexCount; ++v)
            {
                const size_t offset = (static_cast<size_t>(t) * layout.rowsPerTarget * layout.textureWidth + v) * 4u;
                const VEC3 &position = target.positions[v];
                data[offset] = position.x; data[offset + 1u] = position.y; data[offset + 2u] = position.z;
                if (layout.hasNormals)
                {
                    const size_t normalOffset = offset + static_cast<size_t>(layout.rowsPerVertexBlock) * layout.textureWidth * 4u;
                    const VEC3 &normal = target.normals[v];
                    data[normalOffset] = normal.x; data[normalOffset + 1u] = normal.y; data[normalOffset + 2u] = normal.z;
                }
            }
        }
        MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:layout.textureWidth
            height:layout.textureHeight mipmapped:NO];
        descriptor.usage = MTLTextureUsageShaderRead;
        descriptor.storageMode = MTLStorageModeShared;
        id<MTLTexture> texture = [context->mtlDevice newTextureWithDescriptor:descriptor];
        if (!texture) return false;
        [texture replaceRegion:MTLRegionMake2D(0, 0, layout.textureWidth, layout.textureHeight)
            mipmapLevel:0 withBytes:data.data() bytesPerRow:layout.textureWidth * 4u * sizeof(float)];
        backend->facialMorphTexture = texture;
        return true;
    }
}
#endif
