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

#include <shader-var-cfg.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace mbm
{

    bool isReservedShaderUniformName(const char *name) noexcept
    {
        if (name == nullptr)
            return false;
        static const char *reservedNames[] = {
            "LightEnabled",
            "LightCount",
            "AmbientColor",
            "LightDirectionView",
            "LightColor",
            "MaterialDiffuse",
            "MaterialAmbient",
            "MaterialSpecular",
            "MaterialEmissive",
            "MaterialPower",
            nullptr
        };
        for (const char **it = reservedNames; *it; ++it)
        {
            if (strcmp(name, *it) == 0)
                return true;
        }
        return false;
    }

    int roundClampShaderIntValue(float value, float minValue, float maxValue) noexcept
    {
        if (minValue > maxValue)
            std::swap(minValue, maxValue);
        const int rounded = static_cast<int>(std::lround(value));
        const int minInt  = static_cast<int>(std::lround(minValue));
        const int maxInt  = static_cast<int>(std::lround(maxValue));
        return std::max(minInt, std::min(maxInt, rounded));
    }

    void VAR_SHADER::set(const float newMin[4], const float newMax[4],const float timeAnim)
    {
        for (int i = 0; i < this->sizeVar; ++i)
        {
            this->min[i]         = newMin[i];
            this->max[i]         = newMax[i];
            const float minFloat = newMin[i];
            const float maxFloat = newMax[i];
            if (minFloat <= maxFloat)
            {
                const float interval = maxFloat - minFloat;
                if (interval != 0.0f && timeAnim != 0.0f)
                    this->step[i] = (interval / timeAnim);
                else
                    this->step[i] = 0.0f;
                this->granThen[i] = true;
            }
            else
            {
                const float interval = (minFloat - maxFloat);
                if (interval != 0.0f && timeAnim != 0.0f)
                    this->step[i] = (interval / timeAnim) * -1.0f;
                else
                    this->step[i] = 0.0f;
                this->granThen[i] = false;
            }
        }
    }

    int VAR_SHADER::getCurrentInt(const int index) const noexcept
    {
        if (index < 0 || index >= this->sizeVar)
            return 0;
        return roundClampShaderIntValue(this->current[index], this->min[index], this->max[index]);
    }
}
