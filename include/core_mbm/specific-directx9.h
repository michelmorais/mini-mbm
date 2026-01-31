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

#include <platform/win32-platform.h>
#include <core-manager.h>

//#include <dsetup.h>
//#include <comdef.h>

#ifndef __MINGW32__
#pragma comment (lib, "d3d9.lib")
#if defined _DEBUG
    #define D3D_DEBUG_INFO
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/enabling-direct3d-debug-information
    // The debug runtime is part of the DirectX SDK.
    //Enable the regedit HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Direct3D
    // set 
    //HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Direct3D\D3D9Debugging
    //HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Direct3D\\D3D9Debugging\\EnableCreationStack
    // to 1 enables call stack tracking for object creation, which helps detect resource leaks.
    // Enable in
    // C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\dxcpl.exe
    //
    // Building your application with debug enabled will give you access to this additional variable :
    // LPCWSTR CreationCallStack;
    #pragma comment (lib, "d3dx9d.lib") // Debug
#else
    #pragma comment (lib, "d3dx9.lib")
#endif

#include <d3d9.h>
#include <d3dx9.h>

// 
//#pragma comment (lib,"comsuppwd.lib")
//#pragma comment (lib, "dsetup.lib") //Directx version setup

#endif

#include <primitives.h>

namespace mbm
{
    enum class FVF_PROVIDE_BY_ENGINE // we only provide those type of FVF for this engine
    {
        FVF_POS,
        FVF_POS_UV,
        FVF_POS_NOR,
        FVF_POS_NOR_UV,
    };

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
    private:
        IDirect3DVertexDeclaration9* vertex_declaration_pos;
        IDirect3DVertexDeclaration9* vertex_declaration_pos_norm;
        IDirect3DVertexDeclaration9* vertex_declaration_pos_uv;
        IDirect3DVertexDeclaration9* vertex_declaration_pos_norm_uv;
    };

    bool checkAndLogHresultResultDx(HRESULT hr, const char* filename, const int line);
    #define CHECK_AND_LOG_HRESULT_DX(hr) checkAndLogHresultResultDx((hr), __FILE__, __LINE__)

    struct BUFFER_SPECIFIC
    {
        BUFFER_SPECIFIC() noexcept;
        ~BUFFER_SPECIFIC();
        FVF_PROVIDE_BY_ENGINE FVF;
        uint32_t sizeStructVertexInBytes;
        IDirect3DVertexBuffer9* pVertexBuffer;
        IDirect3DIndexBuffer9* pIndexBuffer;
        void release();
    };

    struct D3D_PS_VS
    {
        IDirect3DPixelShader9* pd3dPixelShader;//Pixel Shader
        IDirect3DVertexShader9* pd3dVertexShader;//Vertex Shader
        ID3DXConstantTable* constantTablePS;
        ID3DXConstantTable* constantTableVS;

        
        D3DXHANDLE mvpMatrixHandle;
        D3DXHANDLE mvMatrixHandle;
        D3DXHANDLE samplerHandle0;
        D3DXHANDLE samplerHandle1;

        D3D_PS_VS() noexcept;
        ~D3D_PS_VS();
        void release() noexcept;
        // Prevent copying (COM objects should not be copied)
        D3D_PS_VS(const D3D_PS_VS&) = delete;
        D3D_PS_VS& operator=(const D3D_PS_VS&) = delete;
    };

    struct RENDER2TARGET_DIRECTX9
    {
        IDirect3DSurface9* pRenderSurface = nullptr;
        void release() noexcept;
        RENDER2TARGET_DIRECTX9() noexcept = default;
        ~RENDER2TARGET_DIRECTX9();
        // Prevent copying (COM objects should not be copied)
        RENDER2TARGET_DIRECTX9(const RENDER2TARGET_DIRECTX9&) = delete;
        RENDER2TARGET_DIRECTX9& operator=(const RENDER2TARGET_DIRECTX9&) = delete;
    };

    void copy_pixels_per_row_Pitch(D3DSURFACE_DESC& descSurfaceDest,
        const uint32_t width,
        const uint32_t height,
        D3DLOCKED_RECT& lockDestRect,
        const uint8_t* dataImage) noexcept;
}

#endif
#endif
#endif