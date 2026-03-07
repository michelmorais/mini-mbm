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


#if defined(USE_OPENGL_ES)

#include <blend.h>
#include <specific-opengl_es.h>
#include <shader-fx.h>
#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
// needed GL_MIN / GL_MAX definitions
#include <../third-party/gles/GLES3/gl3.h>
#endif

namespace mbm
{
    void RENDER_STATE::set(const BLEND_STATE blendState) const noexcept
    {
        // Match DX9 behavior: SrcBlend is always GL_SRC_ALPHA (equivalent to D3DBLEND_SRCALPHA)
        // Previously this incorrectly used GL_DST_ALPHA as source factor, which depends on
        // framebuffer alpha and is undefined when the window has no alpha channel.
        GLBlendFunc(GL_SRC_ALPHA, GL_ONE);
        switch (blendState)
        {
            case BLEND_DISABLE:
            {
                //  pixels's transparency  defined for color Keying
                // ---------------------------------------------------------
                GLBlendFunc(GL_SRC_ALPHA, 0x0303);
                return;
            }
            default:{}
        }
        switch (blendState)
        {
            case BLEND_ZERO:         GLBlendFunc(GL_SRC_ALPHA, GL_ZERO);                break;
            case BLEND_ONE:          GLBlendFunc(GL_SRC_ALPHA, GL_ONE);                 break;
            case BLEND_SRCCOLOR:     GLBlendFunc(GL_SRC_ALPHA, GL_SRC_COLOR);           break;
            case BLEND_INVSRCCOLOR:  GLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR); break;
            case BLEND_SRCALPHA:     GLBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);           break;
            case BLEND_INVSRCALPHA:  GLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
            case BLEND_DESTALPHA:    GLBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);           break;
            case BLEND_INVDESTALPHA: GLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA); break;
            case BLEND_DESTCOLOR:    GLBlendFunc(GL_SRC_ALPHA, GL_DST_COLOR);           break;
            case BLEND_INVDESTCOLOR: GLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_DST_COLOR); break;
            default:{}
        }
    }

    void FX::setBlendDefaultOp()
    {
        GLBlendEquation(GL_FUNC_ADD);
    }

    void FX::setBlendOp()
    {
        switch (blendOperation)
        {
        case 1: // D3DBLENDOP_ADD              = 1,
        {
            GLBlendEquation(GL_FUNC_ADD);
        }
        break;
        case 2: // D3DBLENDOP_SUBTRACT         = 2,
        {
            GLBlendEquation(GL_FUNC_SUBTRACT);
        }
        break;
        case 3: // D3DBLENDOP_REVSUBTRACT      = 3,
        {
            GLBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        }
        break;
        case 4: // D3DBLENDOP_MIN              = 4,
        {
#if defined(ANDROID) || defined(__linux__) || defined(__APPLE__)
            GLBlendEquation(0x8007);
#else
            GLBlendEquation(GL_MIN);
#endif
        }
        break;
        case 5: // D3DBLENDOP_MAX              = 5,
        {
#if defined(ANDROID) || defined(__linux__) || defined(__APPLE__)
            GLBlendEquation(0x8008);
#else
            GLBlendEquation(GL_MAX);
#endif
        }
        break;
        }
    }
}

#endif // USE_OPENGL_ES