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

#include <gles-debug.h>
#include <blend.h>

namespace mbm
{
    const char * RENDER_STATE::getDesc(const BLEND_OPENGLES blendState)  const noexcept
    {
        switch (blendState)
        {
            case BLEND_DISABLE:       return "disable";
            case BLEND_ZERO:          return "zero";
            case BLEND_ONE:           return "one";
            case BLEND_SRCCOLOR:      return "src_color";
            case BLEND_INVSRCCOLOR:   return "inv_src_color";
            case BLEND_SRCALPHA:      return "src_alpha";
            case BLEND_INVSRCALPHA:   return "inv_src_alpha";
            case BLEND_DESTALPHA:     return "dest_alpha";
            case BLEND_INVDESTALPHA:  return "inv_dest_alpha";
            case BLEND_DESTCOLOR:     return "dest_color";
            case BLEND_INVDESTCOLOR:  return "inv_dest_color";
            default:                  return "Invalid dst";
        }
    }
    void RENDER_STATE::set(const BLEND_OPENGLES blendState) const noexcept
    {
        GLBlendFunc(GL_DST_ALPHA, GL_ONE);
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
            case BLEND_ZERO:         GLBlendFunc(GL_DST_ALPHA, GL_ZERO);                break;
            case BLEND_ONE:          GLBlendFunc(GL_DST_ALPHA, GL_ONE);                 break;
            case BLEND_SRCCOLOR:     GLBlendFunc(GL_DST_ALPHA, GL_SRC_COLOR);           break;
            case BLEND_INVSRCCOLOR:  GLBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_COLOR); break;
            case BLEND_SRCALPHA:     GLBlendFunc(GL_DST_ALPHA, GL_SRC_ALPHA);           break;
            case BLEND_INVSRCALPHA:  GLBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
            case BLEND_DESTALPHA:    GLBlendFunc(GL_DST_ALPHA, GL_DST_ALPHA);           break;
            case BLEND_INVDESTALPHA: GLBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA); break;
            case BLEND_DESTCOLOR:    GLBlendFunc(GL_DST_ALPHA, GL_DST_COLOR);           break;
            case BLEND_INVDESTCOLOR: GLBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_DST_COLOR); break;
            default:{}
        }
    }
}
