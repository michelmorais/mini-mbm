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
    inline void appendDirectX11FacialMorphFunctions(std::string &source, const uint32_t targetCount,
                                                    const uint32_t textureWidth, const uint32_t textureHeight,
                                                    const uint32_t rowsPerTarget, const bool includeNormals)
    {
        if (!targetCount || !textureWidth || !textureHeight || !rowsPerTarget)
            return;
        source += "Texture2D facialMorphTexture:register(t6); cbuffer FacialMorph:register(b3){float facialMorphWeights[";
        source += std::to_string(targetCount);
        source += "];}; float4 sampleFacialDelta(float vertexId,float target,float normalOffset){uint vertex=(uint)(vertexId+0.5);"
            "uint block=vertex/";
        source += std::to_string(textureWidth);
        source += ";uint column=vertex%";
        source += std::to_string(textureWidth);
        source += ";uint row=(uint)target*";
        source += std::to_string(rowsPerTarget);
        source += "+block+(uint)normalOffset;return facialMorphTexture.Load(int3(column,row,0));}"
            "void applyFacialMorph(inout float4 facialPosition";
        if (includeNormals)
            source += ",inout float3 facialNormal";
        source += ",float facialVertexId){[unroll]for(int facialTarget=0;facialTarget<";
        source += std::to_string(targetCount);
        source += ";++facialTarget){float facialWeight=facialMorphWeights[facialTarget];if(abs(facialWeight)>0.000001){"
            "facialPosition+=sampleFacialDelta(facialVertexId,(float)facialTarget,0.0)*facialWeight;";
        if (includeNormals)
        {
            source += "facialNormal+=sampleFacialDelta(facialVertexId,(float)facialTarget,";
            source += std::to_string(rowsPerTarget / 2u);
            source += ".0).xyz*facialWeight;";
        }
        source += "}}";
        if (includeNormals)
            source += "facialNormal=normalize(facialNormal);";
        source += "}";
    }

    inline void appendDirectXFacialMorphFunctions(std::string &source, const uint32_t targetCount,
                                                   const uint32_t textureWidth, const uint32_t textureHeight,
                                                   const uint32_t rowsPerTarget, const bool includeNormals)
    {
        if (!targetCount || !textureWidth || !textureHeight || !rowsPerTarget)
            return;
        source += "sampler2D facialMorphTexture : register(s6); float facialMorphWeights[";
        source += std::to_string(targetCount);
        source += "]; float4 sampleFacialDelta(float vertexId,float target,float normalOffset) {"
            "float vertex=floor(vertexId+0.5); float block=floor(vertex/";
        source += std::to_string(textureWidth);
        source += ".0); float column=vertex-block*";
        source += std::to_string(textureWidth);
        source += ".0; float row=target*";
        source += std::to_string(rowsPerTarget);
        source += ".0+block+normalOffset; return tex2Dlod(facialMorphTexture,float4((column+0.5)/";
        source += std::to_string(textureWidth);
        source += ".0,(row+0.5)/";
        source += std::to_string(textureHeight);
        source += ".0,0,0)); } void applyFacialMorph(inout float4 facialPosition";
        if (includeNormals)
            source += ",inout float3 facialNormal";
        source += ",float facialVertexId) { for (int facialTarget=0; facialTarget<";
        source += std::to_string(targetCount);
        source += "; ++facialTarget) { float facialWeight=facialMorphWeights[facialTarget];"
            "if (abs(facialWeight)>0.000001) { facialPosition += sampleFacialDelta(facialVertexId,float(facialTarget),0.0)*facialWeight;";
        if (includeNormals)
        {
            source += " facialNormal += sampleFacialDelta(facialVertexId,float(facialTarget),";
            source += std::to_string(rowsPerTarget / 2u);
            source += ".0).xyz*facialWeight;";
        }
        source += " } }";
        if (includeNormals)
            source += " facialNormal=normalize(facialNormal);";
        source += " }";
    }

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
                                                   bool includeNormal, const char *positionName = "input.position",
                                                   const char *normalName = "input.normal")
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
                "float4 skinnedPosition=float4(qrotate(";
            source += positionName;
            source += ".xyz,dqReal)+dqTranslation,1);";
            if (includeNormal)
            {
                source += "float3 skinnedNormal=normalize(qrotate(";
                source += normalName;
                source += ",dqReal));";
            }
        }
        else
        {
            source += "float4 skinnedPosition=float4(skinPoint(";
            source += positionName;
            source += ",input.boneIndices.x)*input.boneWeights.x+skinPoint(";
            source += positionName;
            source += ",input.boneIndices.y)*input.boneWeights.y+skinPoint(";
            source += positionName;
            source += ",input.boneIndices.z)*input.boneWeights.z+skinPoint(";
            source += positionName;
            source += ",input.boneIndices.w)*input.boneWeights.w,1);";
            if (includeNormal)
            {
                source += "float3 skinnedNormal=normalize(skinVector(";
                source += normalName;
                source += ",input.boneIndices.x)*input.boneWeights.x+skinVector(";
                source += normalName;
                source += ",input.boneIndices.y)*input.boneWeights.y+skinVector(";
                source += normalName;
                source += ",input.boneIndices.z)*input.boneWeights.z+skinVector(";
                source += normalName;
                source += ",input.boneIndices.w)*input.boneWeights.w);";
            }
        }
    }
}
#endif
