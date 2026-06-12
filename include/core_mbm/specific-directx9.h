/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
#ifndef DIRECTX9_SPECIFIC_H
#define DIRECTX9_SPECIFIC_H
#if defined (USE_DIRECTX9)

//#include <dsetup.h>
//#include <comdef.h>

// #pragma comment is MSVC-only; MinGW links via CMake targets (d3d9, d3dcompiler).
// d3dx9.h is part of the legacy DirectX SDK and is not available in MinGW or
// the modern Windows SDK (VS 2022+). The engine uses the bundled shim instead.
#ifndef __MINGW32__
#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dcompiler.lib")
#endif // !__MINGW32__

// These headers are available in both MSVC and MinGW builds
#include <platform/win32-platform.h>
#include <core-manager.h>
#include <d3d9.h>

// d3dx9.h is part of the legacy DirectX SDK — not available in MinGW or
// the modern Windows SDK (VS 2022+). Always use the bundled shim.
#include <core_mbm/d3dx9-mingw.h>

// 
//#pragma comment (lib,"comsuppwd.lib")
//#pragma comment (lib, "dsetup.lib") //Directx version setup

#include <primitives.h>
#include <shader.h>

namespace mbm
{
    class D3D_VERTEX_CONVERTER // convert automaticlly to directx VERTEX (see above)
    {
    public:
        explicit D3D_VERTEX_CONVERTER(const VEC3* _pos, const VEC3* _normal, const VEC2* _uv, unsigned int _size_array) noexcept;
        D3D_VERTEX_CONVERTER(const D3D_VERTEX_CONVERTER&) = delete;
        D3D_VERTEX_CONVERTER(D3D_VERTEX_CONVERTER&&) = delete;
        D3D_VERTEX_CONVERTER& operator=(const D3D_VERTEX_CONVERTER&) = delete;
        D3D_VERTEX_CONVERTER& operator=(D3D_VERTEX_CONVERTER&&) = delete;
        ~D3D_VERTEX_CONVERTER() = default;

        void copyTod3dVertexBuffer(void* pvertex) const noexcept;
        uint32_t getSizeOfStructureInBytes() const noexcept;
        FVF_PROVIDE_BY_ENGINE getFVF() const noexcept;
        DWORD get3d3FVF() const;
    private:
        FVF_PROVIDE_BY_ENGINE FVF;
        const VEC3* pos;
        const VEC3* normal;
        const VEC2* uv;
        unsigned int size_array;
    };

    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
        WINDOW window;
        DWORD idIcon;
        WIN_EVENT_BY_PASS* win32_EventByPass;
        WIN_JOYSTICK_BY_PASS* win32_joystickByPass;

        IDirect3D9* pD3D;
        IDirect3DDevice9* pd3dDevice;
        SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE& operator=(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        ~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        IDirect3DVertexDeclaration9* getFVF(const FVF_PROVIDE_BY_ENGINE FVF);
        void release() noexcept;
        void initializeWi32Callbacks(CORE_MANAGER* core_manager_ptr);
        DWORD DWORD_D3DSAMP_MINFILTER[2];
        DWORD DWORD_D3DSAMP_MAGFILTER[2];
        DWORD DWORD_D3DSAMP_MIPFILTER[2];
    private:
        IDirect3DVertexDeclaration9* vertex_declaration_pos;
        IDirect3DVertexDeclaration9* vertex_declaration_pos_norm;
        IDirect3DVertexDeclaration9* vertex_declaration_pos_uv;
        IDirect3DVertexDeclaration9* vertex_declaration_pos_norm_uv;
    };

    bool checkAndLogHresultResultDx(HRESULT hr, const char* filename, const int line);
    #define CHECK_AND_LOG_HRESULT_DX(hr) checkAndLogHresultResultDx((hr), __FILE__, __LINE__)

    void copy_pixels_per_row_Pitch(D3DSURFACE_DESC& descSurfaceDest,
        const uint32_t width,
        const uint32_t height,
        D3DLOCKED_RECT& lockDestRect,
        const uint8_t* dataImage) noexcept;
}

#endif
#endif
#endif
