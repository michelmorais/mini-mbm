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

#include <blend.h>

#if defined(USE_DUMMY_BACK_END_ENGINE) || defined(MBM_DIRECTX11_FOUNDATION_STUBS)

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions
#include <shader-fx.h>
#if defined(USE_DIRECTX11)
#include "specific-directx11-context.h"
#include <device.h>
#endif

namespace mbm
{
    void RENDER_STATE::set(const BLEND_STATE blendState) const noexcept
    {
#if defined(USE_DIRECTX11)
        if (blendState < BLEND_DISABLE || blendState > BLEND_INVDESTCOLOR)
            return;
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        context->currentBlendState = static_cast<int>(blendState);
        context->applyBlendState();
#else
        switch (blendState)
        {
            case BLEND_DISABLE:
            {
                REMINDER_TODO
                return;
            }
            default:{}
        }
        switch (blendState)
        {
            case BLEND_ZERO:         ;break;
            case BLEND_ONE:          ;break;
            case BLEND_SRCCOLOR:     ;break;
            case BLEND_INVSRCCOLOR:  ;break;
            case BLEND_SRCALPHA:     ;break;
            case BLEND_INVSRCALPHA:  ;break;
            case BLEND_DESTALPHA:    ;break;
            case BLEND_INVDESTALPHA: ;break;
            case BLEND_DESTCOLOR:    ;break;
            case BLEND_INVDESTCOLOR: ;break;
            default:{}
        }
#endif
    }

    void FX::setBlendDefaultOp()
    {
#if defined(USE_DIRECTX11)
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        context->currentBlendOperation = 1;
        context->applyBlendState();
#else
        REMINDER_TODO
#endif
    }

    void FX::setBlendOp()
    {
#if defined(USE_DIRECTX11)
        if (blendOperation < 1 || blendOperation > 5)
            return;
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        context->currentBlendOperation = blendOperation;
        context->applyBlendState();
#else
        REMINDER_TODO
#endif
    }
}

#endif // USE_DUMMY_BACK_END_ENGINE
