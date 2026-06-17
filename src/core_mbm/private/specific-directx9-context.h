/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
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
#ifndef DIRECTX9_SPECIFIC_CONTEXT_H
#define DIRECTX9_SPECIFIC_CONTEXT_H
#if defined(USE_DIRECTX9)

#include <specific-directx9.h>

// #pragma comment is MSVC-only; MinGW links via CMake targets (d3d9, d3dcompiler).
// D3DX header/library selection is private to backend files via specific-directx9-d3dx.h.
#ifndef __MINGW32__
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif // !__MINGW32__

#include <platform/win32-platform.h>
#include <d3d9.h>

namespace mbm
{
    class CORE_MANAGER;
    enum class FVF_PROVIDE_BY_ENGINE;

    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
        WINDOW window;
        DWORD idIcon;
        WIN_EVENT_BY_PASS *win32_EventByPass;
        WIN_JOYSTICK_BY_PASS *win32_joystickByPass;

        IDirect3D9 *pD3D;
        IDirect3DDevice9 *pd3dDevice;
        SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE& operator=(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        ~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        IDirect3DVertexDeclaration9 *getFVF(const FVF_PROVIDE_BY_ENGINE FVF);
        void release() noexcept;
        void initializeWi32Callbacks(CORE_MANAGER *core_manager_ptr);
        DWORD DWORD_D3DSAMP_MINFILTER[2];
        DWORD DWORD_D3DSAMP_MAGFILTER[2];
        DWORD DWORD_D3DSAMP_MIPFILTER[2];

    private:
        IDirect3DVertexDeclaration9 *vertex_declaration_pos;
        IDirect3DVertexDeclaration9 *vertex_declaration_pos_norm;
        IDirect3DVertexDeclaration9 *vertex_declaration_pos_uv;
        IDirect3DVertexDeclaration9 *vertex_declaration_pos_norm_uv;
    };
}

#endif // USE_DIRECTX9
#endif // DIRECTX9_SPECIFIC_CONTEXT_H
#endif // Windows
