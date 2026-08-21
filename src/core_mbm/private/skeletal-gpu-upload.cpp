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

#if defined(USE_DIRECTX11)

#include "skeletal-gpu-upload.h"
#include "specific-dummy-buffer.h"
#include "specific-directx11-context.h"
#include <core_mbm/device.h>
#include <core_mbm/shader.h>

namespace mbm::skeletal
{
    bool uploadSkinVertexStream(BUFFER_GL *buffer, const GPU_SKINNING_INPUT &input) noexcept
    {
        if (!buffer || !input.ready() || input.vertices.empty() ||
            input.vertices.size() != buffer->sizeOfArrayVertex)
            return false;
        BUFFER_SPECIFIC *backend = buffer->getBackendBuffer();
        DEVICE *device = DEVICE::getInstance();
        if (!backend || !device || !device->getSpecificContextDevice())
            return false;
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = static_cast<UINT>(input.vertices.size() * sizeof(GPU_LBS_VERTEX));
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA initialData = {};
        initialData.pSysMem = input.vertices.data();
        return SUCCEEDED(device->getSpecificContextDevice()->device->CreateBuffer(
            &description, &initialData, &backend->skinVertexBuffer));
    }
}

#elif !defined(USE_OPENGL_ES) && !defined(USE_DIRECTX9) && !defined(USE_METAL)

#include "skeletal-gpu-upload.h"

namespace mbm::skeletal
{
    bool uploadSkinVertexStream(BUFFER_GL *, const GPU_SKINNING_INPUT &) noexcept
    {
        return false;
    }
}

#endif
