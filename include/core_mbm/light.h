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

#ifndef LIGHT_MBM_H
#define LIGHT_MBM_H

#include "core-exports.h"
#include "primitives.h"

namespace mbm
{
    enum LIGHT_TARGET : int
    {
        LIGHT_TARGET_3D = 0,
        LIGHT_TARGET_2DW = 1,
    };

    struct LIGHT_STATE
    {
        bool enabled = false;
        COLOR ambientColor = COLOR(0.2f, 0.2f, 0.2f, 1.0f);
        COLOR directionalColor = COLOR(1.0f, 1.0f, 1.0f, 1.0f);
        VEC3 directionalDirection = VEC3(0.0f, -0.70710677f, -0.70710677f);
        COLOR pointColor = COLOR(1.0f, 1.0f, 1.0f, 1.0f);
        VEC3 pointPosition = VEC3(0.0f, 0.0f, 128.0f);
        float pointRadius = 512.0f;
        bool ambientConfigured = false;
        bool directionalColorConfigured = false;
        bool directionalDirectionConfigured = false;
        bool pointColorConfigured = false;
        bool pointPositionConfigured = false;
        bool pointRadiusConfigured = false;
    };

    API_IMPL bool isValidLightTarget(const LIGHT_TARGET target) noexcept;
    API_IMPL const char *getLightTargetName(const LIGHT_TARGET target) noexcept;
    API_IMPL bool lightTargetFromString(const char *targetName, LIGHT_TARGET &targetOut) noexcept;

    API_IMPL bool setLightEnabled(const LIGHT_TARGET target, const bool enabled) noexcept;
    API_IMPL bool setAmbientLight(const LIGHT_TARGET target, const COLOR &ambientColor) noexcept;
    API_IMPL bool setDirectionalLight(const LIGHT_TARGET target, const VEC3 &directionalDirection,
                                      const COLOR &directionalColor) noexcept;
    API_IMPL bool setDirectionalLightDirection(const LIGHT_TARGET target, const VEC3 &directionalDirection) noexcept;
    API_IMPL bool setDirectionalLightColor(const LIGHT_TARGET target, const COLOR &directionalColor) noexcept;
    API_IMPL bool setPointLight(const LIGHT_TARGET target, const VEC3 &pointPosition, const float pointRadius,
                                const COLOR &pointColor) noexcept;
    API_IMPL bool setPointLightPosition(const LIGHT_TARGET target, const VEC3 &pointPosition) noexcept;
    API_IMPL bool setPointLightRadius(const LIGHT_TARGET target, const float pointRadius) noexcept;
    API_IMPL bool setPointLightColor(const LIGHT_TARGET target, const COLOR &pointColor) noexcept;
    API_IMPL bool resetLight(const LIGHT_TARGET target) noexcept;
    API_IMPL void resetAllLights() noexcept;
    API_IMPL bool getLightState(const LIGHT_TARGET target, LIGHT_STATE &outState) noexcept;
    API_IMPL const LIGHT_STATE &getLightState(const LIGHT_TARGET target) noexcept;
}

#endif
