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

#if defined (USE_DIRECTX9)

#include <directx9-specific.h>
#include <util-interface.h>

namespace mbm
{
    SPECIFIC_AUX_CONTEXT_DEVICE::SPECIFIC_AUX_CONTEXT_DEVICE() noexcept
    {
        pD3D = nullptr;
        pd3dDevice = nullptr;
    };

    SPECIFIC_AUX_CONTEXT_DEVICE::~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept
    {
        if (pd3dDevice)
        {
            pd3dDevice->Release();
            pd3dDevice = nullptr;
        }
        if (pD3D)
        {
            pD3D->Release();
            pD3D = nullptr;
        }
    };

    bool checkAndLogHresultResultDx(HRESULT hr, const char* filename, const int line)
    {
        if (FAILED(hr))
        {
            if(D3DERR_DEVICELOST == hr)
            {
                ERROR_AT(line, filename, "failed to begin the scene D3DERR_DEVICELOST");
            }
            else if (D3DERR_DRIVERINTERNALERROR == hr)
            {
                ERROR_AT(line, filename, "failed to begin the scene D3DERR_DRIVERINTERNALERROR");
            }
			else if (D3DERR_INVALIDCALL == hr)
            {
                ERROR_AT(line, filename, "failed to begin the scene D3DERR_INVALIDCALL");
            }
            else if (D3DERR_OUTOFVIDEOMEMORY == hr)
            {
                ERROR_AT(line, filename, "failed to begin the scene D3DERR_OUTOFVIDEOMEMORY");
            }
            else if (D3DERR_NOTAVAILABLE == hr)
            {
                ERROR_AT(line, filename, "failed to begin the scene D3DERR_NOTAVAILABLE");
            }
            else if (D3DERR_INVALIDDEVICE == hr)
            {
                ERROR_AT(line, filename, "failed to begin the scene D3DERR_INVALIDDEVICE");
            }
            else if (D3DERR_UNSUPPORTEDCOLORARG == hr)
            {
                ERROR_AT(line, filename, "failed to begin the scene D3DERR_UNSUPPORTEDCOLORARG");
            }
            else if (D3DERR_UNSUPPORTEDALPHAARG == hr)
            {
                ERROR_AT(line, filename, "failed to begin the scene D3DERR_UNSUPPORTEDALPHAARG");
            }
            return false;
        }
        return true;
    }
}

#endif