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
#if defined(USE_DIRECTX9)

#include "skeletal-gpu-upload.h"
#include "specific-directx9-buffer.h"
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
        if (!backend || !device || !device->getSpecificContextDevice()) return false;
        IDirect3DDevice9 *d3d = device->getSpecificContextDevice()->pd3dDevice;
        const UINT bytes = static_cast<UINT>(input.vertices.size() * sizeof(GPU_LBS_VERTEX));
        if (FAILED(d3d->CreateVertexBuffer(bytes, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED,
                                           &backend->pSkinVertexBuffer, nullptr)))
            return false;
        void *destination = nullptr;
        if (FAILED(backend->pSkinVertexBuffer->Lock(0, bytes, &destination, 0))) return false;
        memcpy(destination, input.vertices.data(), bytes);
        backend->pSkinVertexBuffer->Unlock();
        return true;
    }
}
#endif
