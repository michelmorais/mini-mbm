/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT);                                                                                                     |
| Copyright (C); 2020      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software");, to deal in the Software without restriction, including without limitation       |
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


#include "layer.h"
#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>

namespace mbm
{
    bool  LAYER::createFx()
    {
        auto pShaderCfg = mbm::DEVICE::getInstance()->cfg.getShader("tint.ps");
        if(fx.loadNewShader(pShaderCfg, nullptr, TYPE_ANIMATION_GROWING, 0.1f, TYPE_ANIMATION_GROWING, 0) == true)
        {
            constexpr float color_back[4] = {0,0,0,0};
            fx.setVarPShader("color",    color_back);
            fx.setMaxVarPShader("color", color_back);
            fx.setMinVarPShader("color", color_back);
            return true;
        }
        else
        {
            ERROR_LOG("%s","Failed to load shader tint.ps");
            return false;
        }
    }
}

