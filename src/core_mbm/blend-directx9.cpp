/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <blend.h>
#include <specific-directx9.h>
#include <device.h>
#include <shader-fx.h>

namespace mbm
{
    void RENDER_STATE::set(const BLEND_STATE blendState) const noexcept
    {
		mbm::DEVICE* device = mbm::DEVICE::getInstance();
		IDirect3DDevice9* pd3dDevice = device->specificContextDevice->pd3dDevice;
        switch (blendState)
        {
            case BLEND_DISABLE:
            {
                //Transparencia dos pixels definido no color Keying
                pd3dDevice->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
                pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                return;
            }
            default:{}
        }
        switch (blendState)
        {
            case BLEND_DISABLE:     pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);             break;
            case BLEND_ZERO:        pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);           break;
            case BLEND_ONE:         pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);            break;
            case BLEND_SRCCOLOR:    pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);       break;
            case BLEND_INVSRCCOLOR: pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);    break;
            case BLEND_SRCALPHA:    pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCALPHA);       break;
            case BLEND_INVSRCALPHA: pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);    break;
            case BLEND_DESTALPHA:   pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_DESTALPHA);      break;
            case BLEND_INVDESTALPHA:pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVDESTALPHA);   break;
            case BLEND_DESTCOLOR:   pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_DESTCOLOR);      break;
            case BLEND_INVDESTCOLOR:pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVDESTCOLOR);   break;
            default: {}
        }
    }

    void FX::setBlendDefaultOp()
    {
        IDirect3DDevice9* pd3dDevice = mbm::DEVICE::getInstance()->specificContextDevice->pd3dDevice;
        pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    }

    void FX::setBlendOp()
    {
        IDirect3DDevice9* pd3dDevice = mbm::DEVICE::getInstance()->specificContextDevice->pd3dDevice;
        switch (blendOperation)
        {
        case 1: // D3DBLENDOP_ADD              = 1,
        {
            pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        }
        break;
        case 2: // D3DBLENDOP_SUBTRACT         = 2,
        {
            pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_SUBTRACT);
        }
        break;
        case 3: // D3DBLENDOP_REVSUBTRACT      = 3,
        {
            pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
        }
        break;
        case 4: // D3DBLENDOP_MIN              = 4,
        {
            pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_MIN);
        }
        break;
        case 5: // D3DBLENDOP_MAX              = 5,
        {
            pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_MAX);
        }
        break;
        }
    }
}

#endif // USE_DIRECTX9