/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
#if defined(USE_DIRECTX9)
#ifndef DIRECTX9_SHADER_SPECIFIC_H
#define DIRECTX9_SHADER_SPECIFIC_H

#include <specific-directx9.h>

namespace mbm
{
    struct D3D_PS_VS
    {
        IDirect3DPixelShader9 *pd3dPixelShader;
        IDirect3DVertexShader9 *pd3dVertexShader;
        ID3DXConstantTable *constantTablePS;
        ID3DXConstantTable *constantTableVS;

        D3DXHANDLE mvpMatrixHandle;
        D3DXHANDLE mvMatrixHandle;
        D3DXHANDLE samplerHandle0;
        D3DXHANDLE samplerHandle1;

        D3D_PS_VS() noexcept;
        ~D3D_PS_VS();
        void release() noexcept;
        D3D_PS_VS(const D3D_PS_VS&) = delete;
        D3D_PS_VS& operator=(const D3D_PS_VS&) = delete;
    };
}

#endif // DIRECTX9_SHADER_SPECIFIC_H
#endif // USE_DIRECTX9
