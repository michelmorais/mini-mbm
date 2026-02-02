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

#if defined (USE_DUMMY_BACK_END_ENGINE)
#ifndef DUMMY_SPECIFIC_H
#define DUMMY_SPECIFIC_H

#include <primitives.h>

namespace mbm
{
    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
        SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE& operator=(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        ~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        void release() noexcept;
        
    private:
        void * yourBackendSpecificData = nullptr;
    };

    struct BUFFER_SPECIFIC
    {
        BUFFER_SPECIFIC() noexcept;
        ~BUFFER_SPECIFIC();
        void release();
    };

    struct RENDER2TARGET_DUMMY
    {
        void * pRenderSurface = nullptr;
        void release() noexcept;
        RENDER2TARGET_DUMMY() noexcept = default;
        ~RENDER2TARGET_DUMMY();
        // Prevent copying (COM objects should not be copied)
        RENDER2TARGET_DUMMY(const RENDER2TARGET_DUMMY&) = delete;
        RENDER2TARGET_DUMMY& operator=(const RENDER2TARGET_DUMMY&) = delete;
    };
    
}

#endif
#endif