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
#ifndef DIRECTX9_VERTEX_SPECIFIC_H
#define DIRECTX9_VERTEX_SPECIFIC_H

#include <specific-directx9.h>

namespace mbm
{
    class D3D_VERTEX_CONVERTER
    {
    public:
        explicit D3D_VERTEX_CONVERTER(const VEC3 *_pos, const VEC3 *_normal, const VEC2 *_uv, unsigned int _size_array) noexcept;
        D3D_VERTEX_CONVERTER(const D3D_VERTEX_CONVERTER&) = delete;
        D3D_VERTEX_CONVERTER(D3D_VERTEX_CONVERTER&&) = delete;
        D3D_VERTEX_CONVERTER& operator=(const D3D_VERTEX_CONVERTER&) = delete;
        D3D_VERTEX_CONVERTER& operator=(D3D_VERTEX_CONVERTER&&) = delete;
        ~D3D_VERTEX_CONVERTER() = default;

        void copyTod3dVertexBuffer(void *pvertex) const noexcept;
        uint32_t getSizeOfStructureInBytes() const noexcept;
        FVF_PROVIDE_BY_ENGINE getFVF() const noexcept;
        DWORD get3d3FVF() const;

    private:
        FVF_PROVIDE_BY_ENGINE FVF;
        const VEC3 *pos;
        const VEC3 *normal;
        const VEC2 *uv;
        unsigned int size_array;
    };
}

#endif // DIRECTX9_VERTEX_SPECIFIC_H
#endif // USE_DIRECTX9
