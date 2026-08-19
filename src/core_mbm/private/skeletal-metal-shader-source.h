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

#ifndef SKELETAL_METAL_SHADER_SOURCE_H
#define SKELETAL_METAL_SHADER_SOURCE_H

#include "skeletal-gpu-lbs.h"

#include <string>

namespace mbm::skeletal
{
    inline void appendMetalSkeletalFunctions(std::string &source,
                                             const SKELETAL_SHADER_METHOD method)
    {
        source += "float4 qmul(float4 a,float4 b){return float4(a.w*b.xyz+b.w*a.xyz+cross(a.xyz,b.xyz),a.w*b.w-dot(a.xyz,b.xyz));}\n"
                  "float3 qrotate(float3 v,float4 q){return v+2.0f*cross(q.xyz,cross(q.xyz,v)+q.w*v);}\n";
        if (method == SKELETAL_SHADER_METHOD::DQS_RIGID)
            source += "void accumulateDq(float bone,float weight,float4 reference,device const float4* palette,thread float4& realQ,thread float4& dualQ){uint first=uint(bone)*2;float4 r=palette[first];float signQ=dot(r,reference)<0.0f?-1.0f:1.0f;realQ+=r*(weight*signQ);dualQ+=palette[first+1]*(weight*signQ);}\n";
        else
            source += "float3 skinPoint(float4 value,float bone,device const float4* palette){uint first=uint(bone)*3;return float3(dot(value,palette[first]),dot(value,palette[first+1]),dot(value,palette[first+2]));}\n"
                      "float3 skinVector(float3 value,float bone,device const float4* palette){return skinPoint(float4(value,0.0f),bone,palette);}\n";
    }

    inline void appendMetalSkeletalDeformation(std::string &source,
                                               const SKELETAL_SHADER_METHOD method,
                                               const bool includeNormal)
    {
        if (method == SKELETAL_SHADER_METHOD::DQS_RIGID)
        {
            source += "float4 dqReal=float4(0.0f),dqDual=float4(0.0f);float4 dqReference=bonePalette[uint(in.boneIndices.x)*2];\n"
                      "accumulateDq(in.boneIndices.x,in.boneWeights.x,dqReference,bonePalette,dqReal,dqDual);accumulateDq(in.boneIndices.y,in.boneWeights.y,dqReference,bonePalette,dqReal,dqDual);accumulateDq(in.boneIndices.z,in.boneWeights.z,dqReference,bonePalette,dqReal,dqDual);accumulateDq(in.boneIndices.w,in.boneWeights.w,dqReference,bonePalette,dqReal,dqDual);\n"
                      "float dqLength=length(dqReal);dqReal/=dqLength;dqDual/=dqLength;dqDual-=dqReal*dot(dqReal,dqDual);float4 dqConjugate=float4(-dqReal.xyz,dqReal.w);float3 dqTranslation=2.0f*qmul(dqDual,dqConjugate).xyz;float4 skinnedPosition=float4(qrotate(in.pos,dqReal)+dqTranslation,1.0f);\n";
            if (includeNormal)
                source += "float3 skinnedNormal=normalize(qrotate(in.nor,dqReal));\n";
            return;
        }
        source += "float4 bindPosition=float4(in.pos,1.0f);float4 skinnedPosition=float4(skinPoint(bindPosition,in.boneIndices.x,bonePalette)*in.boneWeights.x+skinPoint(bindPosition,in.boneIndices.y,bonePalette)*in.boneWeights.y+skinPoint(bindPosition,in.boneIndices.z,bonePalette)*in.boneWeights.z+skinPoint(bindPosition,in.boneIndices.w,bonePalette)*in.boneWeights.w,1.0f);\n";
        if (includeNormal)
            source += "float3 skinnedNormal=normalize(skinVector(in.nor,in.boneIndices.x,bonePalette)*in.boneWeights.x+skinVector(in.nor,in.boneIndices.y,bonePalette)*in.boneWeights.y+skinVector(in.nor,in.boneIndices.z,bonePalette)*in.boneWeights.z+skinVector(in.nor,in.boneIndices.w,bonePalette)*in.boneWeights.w);\n";
    }
}

#endif
