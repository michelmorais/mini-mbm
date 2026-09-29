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

#ifndef NORMAL_MAP_GLES_H
#define NORMAL_MAP_GLES_H
namespace mbm {
namespace normal_map
{
    // Used only by engine-generated/reserved lighting shaders, never injected into user GLSL.
    inline const char *fragmentGles()
    {
        return R"GLSL(
            varying vec4 vTangentView;
            uniform int HasTangentBasis;
            uniform vec3 NormalMapSettings;
            vec3 mbmSafeNormal(vec3 v) {
                float n = dot(v,v);
                return n > 0.00000001 ? v * inversesqrt(n) : vec3(0.0,0.0,1.0);
            }
            vec3 mbmMappedNormal() {
                vec3 n = mbmSafeNormal(vNormalView);
                if (HasTangentBasis == 0 || abs(vTangentView.w) < 0.5) return n;
                vec3 t = vTangentView.xyz - n * dot(n,vTangentView.xyz);
                if (dot(t,t) < 0.00000001) return n;
                t = normalize(t);
                vec3 b = cross(n,t) * sign(vTangentView.w);
                vec3 m = texture2D(TextureNormal,vTexCoord).xyz * 2.0 - 1.0;
                m.xy *= NormalMapSettings.y;
                m.y *= NormalMapSettings.x;
                m.z *= NormalMapSettings.z;
                return mbmSafeNormal(t*m.x + b*m.y + n*m.z);
            }
        )GLSL";
    }
}}
#endif
