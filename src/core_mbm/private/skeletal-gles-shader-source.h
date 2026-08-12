/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef SKELETAL_GLES_SHADER_SOURCE_H
#define SKELETAL_GLES_SHADER_SOURCE_H

#include <core_mbm/shader.h>

#include <string>

namespace mbm::skeletal
{
    inline void appendGlesSkeletalFunctions(std::string &source, const uint32_t paletteSize,
                                             const SKELETAL_SHADER_METHOD method)
    {
        source += " uniform vec4 bonePalette[";
        source += std::to_string(paletteSize *
            (method == SKELETAL_SHADER_METHOD::DQS_RIGID ? 2u : 3u));
        if (method == SKELETAL_SHADER_METHOD::DQS_RIGID)
        {
            source += "];"
                "vec4 qmul(vec4 a, vec4 b) { return vec4(a.w*b.xyz+b.w*a.xyz+cross(a.xyz,b.xyz),"
                "a.w*b.w-dot(a.xyz,b.xyz)); }"
                "vec3 qrotate(vec3 v, vec4 q) { return v+2.0*cross(q.xyz,cross(q.xyz,v)+q.w*v); }"
                "void accumulateDq(float bone,float weight,vec4 reference,inout vec4 realQ,inout vec4 dualQ) {"
                "int first=int(bone)*2; vec4 r=bonePalette[first]; float signQ=dot(r,reference)<0.0?-1.0:1.0;"
                "realQ+=r*(weight*signQ); dualQ+=bonePalette[first+1]*(weight*signQ); }";
        }
        else
        {
            source += "];"
                "vec3 skinPoint(vec4 value, float bone) { int first = int(bone) * 3;"
                " return vec3(dot(value, bonePalette[first]), dot(value, bonePalette[first + 1]),"
                " dot(value, bonePalette[first + 2])); }"
                "vec3 skinVector(vec3 value, float bone) { return skinPoint(vec4(value, 0.0), bone); }";
        }
    }

    inline void appendGlesSkeletalDeformation(std::string &source,
                                               const SKELETAL_SHADER_METHOD method,
                                               const bool includeNormal)
    {
        if (method == SKELETAL_SHADER_METHOD::DQS_RIGID)
        {
            source += " vec4 dqReal=vec4(0.0); vec4 dqDual=vec4(0.0);"
                "vec4 dqReference=bonePalette[int(aBoneIndices.x)*2];"
                "accumulateDq(aBoneIndices.x,aBoneWeights.x,dqReference,dqReal,dqDual);"
                "accumulateDq(aBoneIndices.y,aBoneWeights.y,dqReference,dqReal,dqDual);"
                "accumulateDq(aBoneIndices.z,aBoneWeights.z,dqReference,dqReal,dqDual);"
                "accumulateDq(aBoneIndices.w,aBoneWeights.w,dqReference,dqReal,dqDual);"
                "float dqLength=length(dqReal); dqReal/=dqLength; dqDual/=dqLength;"
                "dqDual-=dqReal*dot(dqReal,dqDual); vec4 dqConjugate=vec4(-dqReal.xyz,dqReal.w);"
                "vec3 dqTranslation=2.0*qmul(dqDual,dqConjugate).xyz;"
                "vec4 skinnedPosition=vec4(qrotate(aPosition.xyz,dqReal)+dqTranslation,1.0);";
            if (includeNormal)
                source += " vec3 skinnedNormal=normalize(qrotate(aNormal,dqReal));";
        }
        else
        {
            source += " vec4 skinnedPosition = vec4("
                "skinPoint(aPosition, aBoneIndices.x) * aBoneWeights.x +"
                "skinPoint(aPosition, aBoneIndices.y) * aBoneWeights.y +"
                "skinPoint(aPosition, aBoneIndices.z) * aBoneWeights.z +"
                "skinPoint(aPosition, aBoneIndices.w) * aBoneWeights.w, 1.0);";
            if (includeNormal)
            {
                source += " vec3 skinnedNormal = normalize("
                    "skinVector(aNormal, aBoneIndices.x) * aBoneWeights.x +"
                    "skinVector(aNormal, aBoneIndices.y) * aBoneWeights.y +"
                    "skinVector(aNormal, aBoneIndices.z) * aBoneWeights.z +"
                    "skinVector(aNormal, aBoneIndices.w) * aBoneWeights.w);";
            }
        }
    }
}

#endif
