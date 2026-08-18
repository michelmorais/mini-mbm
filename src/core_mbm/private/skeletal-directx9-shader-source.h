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
#ifndef SKELETAL_DIRECTX9_SHADER_SOURCE_H
#define SKELETAL_DIRECTX9_SHADER_SOURCE_H

#include <core_mbm/shader.h>
#include <string>

namespace mbm::skeletal
{
    inline void appendDirectX9SkeletalFunctions(std::string &source, uint32_t paletteSize,
                                                 SKELETAL_SHADER_METHOD method)
    {
        source += "float4 bonePalette[" + std::to_string(paletteSize *
            (method == SKELETAL_SHADER_METHOD::DQS_RIGID ? 2u : 3u)) + "];";
        if (method == SKELETAL_SHADER_METHOD::DQS_RIGID)
            source += "float4 qmul(float4 a,float4 b){return float4(a.w*b.xyz+b.w*a.xyz+cross(a.xyz,b.xyz),a.w*b.w-dot(a.xyz,b.xyz));}"
                "float3 qrotate(float3 v,float4 q){return v+2.0*cross(q.xyz,cross(q.xyz,v)+q.w*v);}"
                "void accumulateDq(float bone,float weight,float4 reference,inout float4 realQ,inout float4 dualQ){"
                "int first=(int)bone*2;float4 r=bonePalette[first];float signQ=dot(r,reference)<0?-1:1;"
                "realQ+=r*(weight*signQ);dualQ+=bonePalette[first+1]*(weight*signQ);}";
        else
            source += "float3 skinPoint(float4 value,float bone){int first=(int)bone*3;return float3(dot(value,bonePalette[first]),dot(value,bonePalette[first+1]),dot(value,bonePalette[first+2]));}"
                "float3 skinVector(float3 value,float bone){return skinPoint(float4(value,0),bone);}";
    }

    inline void appendDirectX9SkeletalDeformation(std::string &source, SKELETAL_SHADER_METHOD method,
                                                   bool includeNormal)
    {
        if (method == SKELETAL_SHADER_METHOD::DQS_RIGID)
        {
            source += "float4 dqReal=0,dqDual=0;float4 dqReference=bonePalette[(int)input.boneIndices.x*2];"
                "accumulateDq(input.boneIndices.x,input.boneWeights.x,dqReference,dqReal,dqDual);"
                "accumulateDq(input.boneIndices.y,input.boneWeights.y,dqReference,dqReal,dqDual);"
                "accumulateDq(input.boneIndices.z,input.boneWeights.z,dqReference,dqReal,dqDual);"
                "accumulateDq(input.boneIndices.w,input.boneWeights.w,dqReference,dqReal,dqDual);"
                "float dqLength=length(dqReal);dqReal/=dqLength;dqDual/=dqLength;dqDual-=dqReal*dot(dqReal,dqDual);"
                "float4 dqConjugate=float4(-dqReal.xyz,dqReal.w);float3 dqTranslation=2*qmul(dqDual,dqConjugate).xyz;"
                "float4 skinnedPosition=float4(qrotate(input.position.xyz,dqReal)+dqTranslation,1);";
            if (includeNormal) source += "float3 skinnedNormal=normalize(qrotate(input.normal,dqReal));";
        }
        else
        {
            source += "float4 skinnedPosition=float4(skinPoint(input.position,input.boneIndices.x)*input.boneWeights.x+"
                "skinPoint(input.position,input.boneIndices.y)*input.boneWeights.y+skinPoint(input.position,input.boneIndices.z)*input.boneWeights.z+"
                "skinPoint(input.position,input.boneIndices.w)*input.boneWeights.w,1);";
            if (includeNormal) source += "float3 skinnedNormal=normalize(skinVector(input.normal,input.boneIndices.x)*input.boneWeights.x+"
                "skinVector(input.normal,input.boneIndices.y)*input.boneWeights.y+skinVector(input.normal,input.boneIndices.z)*input.boneWeights.z+"
                "skinVector(input.normal,input.boneIndices.w)*input.boneWeights.w);";
        }
    }
}
#endif
