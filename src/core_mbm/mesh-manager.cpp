/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <mesh-manager.h>
#include <draw-compatibility.h>
#include <shader-var-cfg.h>
#include <texture-manager.h>
#include <renderizable.h>
#include <shader.h>
#include <device.h>
#include <util-interface.h>
#include <shapes.h>
#include <deprecated.h>
#include <cr-static-local.h>
#include <miniz-wrap/miniz-wrap.h>
#include <header-mesh.h>
#include "mesh-v8-io.h"

#include <cfloat>
#include <string>
#include <algorithm> // std::sort
#include <unordered_map>
#include <unordered_set>


const bool is_any_mode_valid(const util::INFO_DRAW_MODE & info_mode,std::string & which_mode_is_invalid)noexcept
{
    if(util::is_mode_draw_valid(info_mode.mode_draw) == false)
    {
        which_mode_is_invalid = "mode draw:";
        which_mode_is_invalid += std::to_string(info_mode.mode_draw);
        return false;
    }
    if(util::is_mode_cull_face_valid(info_mode.mode_cull_face) == false)
    {
        which_mode_is_invalid = "mode cull face:";
        which_mode_is_invalid += std::to_string(info_mode.mode_cull_face);
        return false;
    }
    if(util::is_mode_front_face_direction_valid(info_mode.mode_front_face_direction) == false)
    {
        which_mode_is_invalid = "mode front face direction:";
        which_mode_is_invalid += std::to_string(info_mode.mode_front_face_direction);
        return false;
    }
    return true;
}

namespace
{
    const char* get_type_app_from_mesh_type(const util::TYPE_MESH type) noexcept
    {
        switch (type)
        {
            case util::TYPE_MESH_3D:       return MBM_TYPE_APP_MESH_3D;
            case util::TYPE_MESH_USER:     return MBM_TYPE_APP_USER;
            case util::TYPE_MESH_SPRITE:   return MBM_TYPE_APP_SPRITE;
            case util::TYPE_MESH_FONT:     return MBM_TYPE_APP_FONT;
            case util::TYPE_MESH_TEXTURE:  return MBM_TYPE_APP_TEXTURE;
            case util::TYPE_MESH_SHAPE:    return MBM_TYPE_APP_SHAPE;
            case util::TYPE_MESH_PARTICLE: return MBM_TYPE_APP_PARTICLE;
            case util::TYPE_MESH_TILE_MAP: return MBM_TYPE_APP_TILE;
            default:                       return nullptr;
        }
    }

    util::TYPE_MESH get_mesh_type_from_type_app(const char* typeApp) noexcept
    {
        if (strncmp(typeApp, MBM_TYPE_APP_MESH_3D, MBM_HEADER_TYPE_APP_COMPARE_LENGTH) == 0)
            return util::TYPE_MESH_3D;
        if (strncmp(typeApp, MBM_TYPE_APP_USER, MBM_HEADER_TYPE_APP_COMPARE_LENGTH) == 0)
            return util::TYPE_MESH_USER;
        if (strncmp(typeApp, MBM_TYPE_APP_FONT, MBM_HEADER_TYPE_APP_COMPARE_LENGTH) == 0)
            return util::TYPE_MESH_FONT;
        if (strncmp(typeApp, MBM_TYPE_APP_SPRITE, MBM_HEADER_TYPE_APP_COMPARE_LENGTH) == 0)
            return util::TYPE_MESH_SPRITE;
        if (strncmp(typeApp, MBM_TYPE_APP_TILE, MBM_HEADER_TYPE_APP_COMPARE_LENGTH) == 0)
            return util::TYPE_MESH_TILE_MAP;
        if (strncmp(typeApp, MBM_TYPE_APP_PARTICLE, MBM_HEADER_TYPE_APP_COMPARE_LENGTH) == 0)
            return util::TYPE_MESH_PARTICLE;
        if (strncmp(typeApp, MBM_TYPE_APP_SHAPE, MBM_HEADER_TYPE_APP_COMPARE_LENGTH) == 0)
            return util::TYPE_MESH_SHAPE;
        return util::TYPE_MESH_UNKNOWN;
    }

    bool try_decode_type_from_header(const util::HEADER& header, util::TYPE_MESH& typeOut) noexcept
    {
        if (strncmp(header.name, MBM_HEADER_NAME_MBM, MBM_HEADER_NAME_COMPARE_LENGTH) != 0)
            return false;
        typeOut = get_mesh_type_from_type_app(header.typeApp);
        return typeOut != util::TYPE_MESH_UNKNOWN;
    }

    bool open_decompressed_mesh_file(const char *fileNamePath, FILE *&fp, const char *openErrorFormat)
    {
        fp = util::openFile(fileNamePath, "rb");
        if (!fp)
            return log_util::onFailed(fp, __FILE__, __LINE__, openErrorFormat, fileNamePath ? fileNamePath : "nullptr");
        fclose(fp);
        fp = nullptr;

        mbm::MINIZ minz;
        char errorDesc[MBM_ERROR_DESCRIPTION_BUFFER_SIZE] = "";
        if (!minz.decompressFile(fileNamePath, util::getDecompressModelFileName(), errorDesc, sizeof(errorDesc) - 1))
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to uncompress file [%s]\n%s", fileNamePath, errorDesc);

        fp = util::openFile(util::getDecompressModelFileName(), "rb");
        if (!fp)
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to open file [%s]", fileNamePath);
        return true;
    }

    bool readHeaderDescSubsetVersioned(FILE *fp, util::HEADER_DESC_SUBSET &out, const int version);
    bool writeHeaderDescSubsetVersioned(FILE *fp, const util::HEADER_DESC_SUBSET &in, const int version);

    bool read_main_header_and_type(FILE *fp, const char *fileNamePath, util::HEADER &headerOut, util::TYPE_MESH &typeOut)
    {
        if (!util::readHeaderV8(fp, headerOut))
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read header file [%s]", fileNamePath);
        if (!try_decode_type_from_header(headerOut, typeOut))
        {
            char strTemp[MBM_ERROR_DESCRIPTION_BUFFER_SIZE];
            snprintf(strTemp, MBM_ERROR_DESCRIPTION_BUFFER_SIZE, "[%s] is not a mbm file!!\ntype of file: %s", fileNamePath, headerOut.typeApp);
            return log_util::onFailed(fp, __FILE__, __LINE__, strTemp);
        }
        if (headerOut.version < INITIAL_VERSION_MBM_HEADER || headerOut.version > CURRENT_VERSION_MBM_HEADER)
            return log_util::onFailed(fp, __FILE__, __LINE__, "incompatible version [%s] version [%d]", fileNamePath, headerOut.version);
        return true;
    }

    bool read_extra_headers(FILE *fp, const char *fileNamePath, const int extraHeaderCount, const bool registerPaths)
    {
        for (int i = 0; i < extraHeaderCount; ++i)
        {
            util::EXTRA_HEADER extra;
            if (!util::readExtraHeaderV8(fp, extra))
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read info EXTRA_HEADER [%s]", fileNamePath);
            if (extra.type == MBM_EXTRA_HEADER_TYPE_PATHS)
            {
                std::string path(extra.sizeExtraHeader + 1, 0);
                if (!fread(&path[0], extra.sizeExtraHeader, 1, fp))
                    return log_util::onFailed(fp, __FILE__, __LINE__, "Failed to read string from EXTRA_HEADER [%s] size -> [%d]", fileNamePath, extra.sizeExtraHeader);
#ifndef ANDROID
                if (registerPaths)
                    util::addPath(path.c_str());
#endif
            }
            else
            {
                return log_util::onFailed(fp, __FILE__, __LINE__, "Unsuported type of EXTRA_HEADER [%s] -> type [%d]", fileNamePath, extra.type);
            }
        }
        return true;
    }

    bool read_info_mode_if_needed(FILE *fp, const char *fileNamePath, const int version, util::INFO_DRAW_MODE &infoMode,
                                  const bool validateModes)
    {
        if (version < MODE_DRAW_VERSION_MBM_HEADER)
            return true;
        if (!util::readInfoDrawModeV8(fp, infoMode))
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read info INFO_DRAW_MODE [%s]", fileNamePath);
        if (validateModes)
        {
            std::string which_mode;
            if (is_any_mode_valid(infoMode, which_mode) == false)
                return log_util::onFailed(fp, __FILE__, __LINE__, "Invalid mode %s detected:[%s]", which_mode.c_str(), fileNamePath);
        }
        return true;
    }

    template <typename OnHeaderRead, typename OnSubsetRead>
    bool read_frame_headers_and_subsets(FILE *fp,
                                        const char *fileNamePath,
                                        const int fileVersion,
                                        util::HEADER_FRAME &headerFrame,
                                        OnHeaderRead onHeaderRead,
                                        OnSubsetRead onSubsetRead)
    {
        if (!util::readHeaderFrameV8(fp, headerFrame))
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read header of frame [%s]", fileNamePath);
        if (!onHeaderRead(headerFrame))
            return false;

        util::HEADER_DESC_SUBSET headerDescSubset;
        for (int i = 0; i < headerFrame.totalSubset; ++i)
        {
            if (!readHeaderDescSubsetVersioned(fp, headerDescSubset, fileVersion))
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read header of subset [%s]", fileNamePath);
            if (!onSubsetRead(headerFrame, i, headerDescSubset))
                return false;
        }
        return true;
    }

    template <typename LoadFromSeparatedBuffersFn, typename OnIndexedData, typename OnVertexData>
    bool read_frame_geometry(FILE *fp,
                             const char *fileNamePath,
                             const util::HEADER_FRAME &headerFrame,
                             int16_t hasNorText[2],
                             const int fileVersion,
                             LoadFromSeparatedBuffersFn loadFromSeparatedBuffers,
                             OnIndexedData onIndexedData,
                             OnVertexData onVertexData)
    {
        if (headerFrame.sizeIndexBuffer && strcmp(headerFrame.typeBuffer, "IB") == 0)
        {
            auto indexArray = new uint16_t[headerFrame.sizeIndexBuffer];
            if (!util::readU16ArrayV8(fp, indexArray, static_cast<uint32_t>(headerFrame.sizeIndexBuffer)))
            {
                delete[] indexArray;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read index buffer of frame [%s]", fileNamePath);
            }
            mbm::VEC3 *pPosition = nullptr;
            mbm::VEC3 *pNormal   = nullptr;
            mbm::VEC2 *pTexture  = nullptr;
            if (!loadFromSeparatedBuffers(fp,
                                 headerFrame.sizeVertexBuffer,
                                 &pPosition,
                                 &pNormal,
                                 &pTexture,
                                 hasNorText,
                                 indexArray,
                                 headerFrame.sizeIndexBuffer,
                                 headerFrame.stride,
                                 fileVersion))
            {
                delete[] indexArray;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex buffer of frame [%s]", fileNamePath);
            }
            if (!onIndexedData(pPosition, pNormal, pTexture, indexArray))
            {
                delete[] pPosition;
                delete[] pNormal;
                delete[] pTexture;
                delete[] indexArray;
                return false;
            }
            delete[] pPosition;
            delete[] pNormal;
            delete[] pTexture;
            delete[] indexArray;
            return true;
        }
        if (strcmp(headerFrame.typeBuffer, "VB") == 0)
        {
            mbm::VEC3 *pPosition = nullptr;
            mbm::VEC3 *pNormal   = nullptr;
            mbm::VEC2 *pTexture  = nullptr;
            if (!loadFromSeparatedBuffers(fp,
                                 headerFrame.sizeVertexBuffer,
                                 &pPosition,
                                 &pNormal,
                                 &pTexture,
                                 hasNorText,
                                 nullptr,
                                 0,
                                 headerFrame.stride,
                                 fileVersion))
            {
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex buffer of frame [%s]", fileNamePath);
            }
            if (!onVertexData(pPosition, pNormal, pTexture))
            {
                delete[] pPosition;
                delete[] pNormal;
                delete[] pTexture;
                return false;
            }
            delete[] pPosition;
            delete[] pNormal;
            delete[] pTexture;
            return true;
        }
        return log_util::onFailed(fp,__FILE__, __LINE__, "unknown buffer type [%s]", fileNamePath);
    }

    bool load_from_separated_buffers_common(FILE *fp,
                                  const int sizeVertexBuffer,
                                  mbm::VEC3 **positionOut,
                                  mbm::VEC3 **normalOut,
                                  mbm::VEC2 **textureOut,
                                  int16_t hasNorText[2],
                                  uint16_t *indexArray,
                                  const int sizeArrayIndex,
                                  const int stride,
                                  const int fileVersion,
                                  mbm::VEC2 *&coordTexFrame0,
                                  int &sizeCoordTexFrame0)
    {
        (void)fileVersion;
        const bool noNormals = (hasNorText[0] == HAS_NOR_NO);
        const bool hasNormalsFromFile = (hasNorText[0] == HAS_NOR_IN_FILE);
        const bool calculateNormals = (hasNorText[0] == HAS_NOR_CALCULATE);

        auto pPosition = new mbm::VEC3[sizeVertexBuffer];
        mbm::VEC3* pNormal = noNormals ? nullptr : new mbm::VEC3[sizeVertexBuffer];
        auto pTexture  = new mbm::VEC2[sizeVertexBuffer];
        *positionOut            = pPosition;
        *normalOut              = pNormal;
        *textureOut             = pTexture;
        if (stride == 3)
        {
            if (!util::readVec3ArrayV8(fp, pPosition, static_cast<uint32_t>(sizeVertexBuffer)))
            {
                delete[] pPosition;
                if (pNormal) delete[] pNormal;
                delete[] pTexture;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
            }
            if (hasNormalsFromFile)
            {
                if (!util::readVec3ArrayV8(fp, pNormal, static_cast<uint32_t>(sizeVertexBuffer)))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
                }
            }
            else if (calculateNormals)
            {
                if (indexArray && sizeArrayIndex)
                {
                    for (int i = 0; i < sizeArrayIndex; i += 3)
                    {
                        const int index0 = indexArray[i];
                        const int index1 = indexArray[i + 1];
                        const int index2 = indexArray[i + 2];
                        if (index0 >= sizeVertexBuffer || index1 >= sizeVertexBuffer || index2 >= sizeVertexBuffer)
                        {
                            delete[] pPosition;
                            delete[] pNormal;
                            delete[] pTexture;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "inconsistent index to normal");
                        }
                        mbm::VEC3 a(pPosition[index1] - pPosition[index0]);
                        mbm::VEC3 b(pPosition[index2] - pPosition[index1]);
                        mbm::vec3Cross(&pNormal[index0], &a, &b);
                        mbm::vec3Normalize(&pNormal[index0], &pNormal[index0]);
                        pNormal[index1] = pNormal[index0];
                        pNormal[index2] = pNormal[index0];
                    }
                }
                else
                {
                    for (int i = 0; i < sizeVertexBuffer; i += 3)
                    {
                        mbm::VEC3 a(pPosition[i + 1] - pPosition[i]);
                        mbm::VEC3 b(pPosition[i + 2] - pPosition[i + 1]);
                        mbm::vec3Cross(&pNormal[i], &a, &b);
                        mbm::vec3Normalize(&pNormal[i], &pNormal[i]);
                        pNormal[i + 1] = pNormal[i];
                        pNormal[i + 2] = pNormal[i];
                    }
                }
            }
            if (hasNorText[1] == HAS_TEX_EACH_FRAME)
            {
                if (!util::readVec2ArrayV8(fp, pTexture, static_cast<uint32_t>(sizeVertexBuffer)))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                }
            }
            else if (hasNorText[1] == HAS_TEX_FIRST_FRAME)
            {
                if (coordTexFrame0)
                {
                    if (sizeVertexBuffer != sizeCoordTexFrame0)
                        memset(static_cast<void*>(pTexture), 0, sizeof(mbm::VEC2) * static_cast<size_t>(sizeVertexBuffer));
                    int safeCopy = std::min(sizeVertexBuffer, sizeCoordTexFrame0);
                    memcpy(static_cast<void*>(pTexture), coordTexFrame0, sizeof(mbm::VEC2) * static_cast<size_t>(safeCopy));
                }
                else
                {
                    coordTexFrame0 = new mbm::VEC2[sizeVertexBuffer];
                    sizeCoordTexFrame0 = sizeVertexBuffer;
                    if (!util::readVec2ArrayV8(fp, pTexture, static_cast<uint32_t>(sizeVertexBuffer)))
                    {
                        delete[] pPosition;
                        delete[] pNormal;
                        delete[] pTexture;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                    }
                    memcpy(static_cast<void*>(coordTexFrame0), pTexture, sizeof(mbm::VEC2) * static_cast<size_t>(sizeVertexBuffer));
                }
            }
            else
            {
                for (int i = 0, j = 0; i < sizeVertexBuffer; i += 3, ++j)
                {
                    if (j % 2)
                    {
                        pTexture[i].x = 0;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 1;
                    }
                    else
                    {
                        pTexture[i].x = 1;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 0;
                    }
                }
            }
            return true;
        }
        if (stride == 2)
        {
            auto pStridePosition = new mbm::VEC2[sizeVertexBuffer];
            if (!util::readVec2ArrayV8(fp, pStridePosition, static_cast<uint32_t>(sizeVertexBuffer)))
            {
                delete[] pPosition;
                if (pNormal) delete[] pNormal;
                delete[] pTexture;
                delete[] pStridePosition;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
            }
            for (int i = 0; i < sizeVertexBuffer; ++i)
            {
                pPosition[i].x = pStridePosition[i].x;
                pPosition[i].y = pStridePosition[i].y;
                pPosition[i].z = 0.0f;
            }
            delete[] pStridePosition;
            pStridePosition = nullptr;
            if (hasNormalsFromFile)
            {
                if (!util::readVec3ArrayV8(fp, pNormal, static_cast<uint32_t>(sizeVertexBuffer)))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
                }
            }
            else if (calculateNormals)
            {
                if (indexArray && sizeArrayIndex)
                {
                    for (int i = 0; i < sizeArrayIndex; i += 3)
                    {
                        const int index0 = indexArray[i];
                        const int index1 = indexArray[i + 1];
                        const int index2 = indexArray[i + 2];
                        if (index0 >= sizeVertexBuffer || index1 >= sizeVertexBuffer || index2 >= sizeVertexBuffer)
                        {
                            delete[] pPosition;
                            delete[] pNormal;
                            delete[] pTexture;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "inconsistent index to normal");
                        }
                        mbm::VEC3 a(pPosition[index1] - pPosition[index0]);
                        mbm::VEC3 b(pPosition[index2] - pPosition[index1]);
                        mbm::vec3Cross(&pNormal[index0], &a, &b);
                        mbm::vec3Normalize(&pNormal[index0], &pNormal[index0]);
                        pNormal[index1] = pNormal[index0];
                        pNormal[index2] = pNormal[index0];
                    }
                }
                else
                {
                    for (int i = 0; i < sizeVertexBuffer; i += 3)
                    {
                        mbm::VEC3 a(pPosition[i + 1] - pPosition[i]);
                        mbm::VEC3 b(pPosition[i + 2] - pPosition[i + 1]);
                        mbm::vec3Cross(&pNormal[i], &a, &b);
                        mbm::vec3Normalize(&pNormal[i], &pNormal[i]);
                        pNormal[i + 1] = pNormal[i];
                        pNormal[i + 2] = pNormal[i];
                    }
                }
            }
            if (hasNorText[1] == HAS_TEX_EACH_FRAME)
            {
                if (!util::readVec2ArrayV8(fp, pTexture, static_cast<uint32_t>(sizeVertexBuffer)))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                }
            }
            else if (hasNorText[1] == HAS_TEX_FIRST_FRAME)
            {
                if (coordTexFrame0)
                {
                    if (sizeVertexBuffer != sizeCoordTexFrame0)
                        memset(static_cast<void*>(pTexture), 0, sizeof(mbm::VEC2) * static_cast<size_t>(sizeVertexBuffer));
                    int safeCopy = std::min(sizeVertexBuffer, sizeCoordTexFrame0);
                    memcpy(static_cast<void*>(pTexture), coordTexFrame0, sizeof(mbm::VEC2) * static_cast<size_t>(safeCopy));
                }
                else
                {
                    coordTexFrame0 = new mbm::VEC2[sizeVertexBuffer];
                    sizeCoordTexFrame0 = sizeVertexBuffer;
                    if (!util::readVec2ArrayV8(fp, pTexture, static_cast<uint32_t>(sizeVertexBuffer)))
                    {
                        delete[] pPosition;
                        delete[] pNormal;
                        delete[] pTexture;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                    }
                    memcpy(static_cast<void*>(coordTexFrame0), pTexture, sizeof(mbm::VEC2) * static_cast<size_t>(sizeVertexBuffer));
                }
            }
            else
            {
                for (int i = 0, j = 0; i < sizeVertexBuffer; i += 3, ++j)
                {
                    if (j % 2)
                    {
                        pTexture[i].x = 0;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 1;
                    }
                    else
                    {
                        pTexture[i].x = 1;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 0;
                    }
                }
            }
            return true;
        }
        return log_util::onFailed(fp,__FILE__, __LINE__, "stride unknown. must be 2 or 3");
    }

    bool fill_animation_headers_common(FILE *fp,
                                       const char *fileNamePath,
                                       const int totalAnimation,
                                       util::INFO_ANIMATION &infoAnimation)
    {
        for (int i = 0; i < totalAnimation; ++i)
        {
            auto headerAnim = new util::HEADER_ANIMATION();
            if (!util::readHeaderAnimationV8(fp, *headerAnim))
            {
                delete headerAnim;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load animation 's mesh [%s]", fileNamePath);
            }
            auto infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            infoAnimation.lsHeaderAnim.push_back(infoHead);
            infoHead->headerAnim = headerAnim;

            if(headerAnim->hasShaderEffect == 1)
            {
                infoHead->effectShader = new util::INFO_FX();
                util::HEADER_INFO_SHADER_STEP headerPS_VS;
                if (!util::readHeaderInfoShaderStepV8(fp, headerPS_VS))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header step [%s]", fileNamePath);
                if(headerPS_VS.blendOperation != 0)
                    infoHead->effectShader->blendOperation = headerPS_VS.blendOperation;
                else
                    infoHead->effectShader->blendOperation = 1;
                if (headerPS_VS.lenNameShader)
                {
                    auto dataInfo = new util::INFO_SHADER_DATA(headerPS_VS.sizeArrayVarInBytes,
                                                               static_cast<short>(headerPS_VS.lenNameShader),
                                                               static_cast<short>(headerPS_VS.lenTextureStage2));
                    infoHead->effectShader->blendOperation = headerPS_VS.blendOperation;
                    infoHead->effectShader->dataPS     = (dataInfo);
                    dataInfo->typeAnimation    = headerPS_VS.typeAnimation;
                    dataInfo->timeAnimation    = headerPS_VS.timeAnimation;
                    if (!fread(dataInfo->fileNameShader, static_cast<size_t>(headerPS_VS.lenNameShader), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read shader's name [%s]", fileNamePath);

                    if (headerPS_VS.lenTextureStage2)
                    {
                        if (!fread(dataInfo->fileNameTextureStage2, static_cast<size_t>(headerPS_VS.lenTextureStage2), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name's texture [%s]", fileNamePath);
                    }
                    if (headerPS_VS.sizeArrayVarInBytes)
                    {
                        if (!fread(dataInfo->typeVars, static_cast<size_t>(dataInfo->lenVars), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!util::readFloatArrayV8(fp,
                                                    dataInfo->min,
                                                    static_cast<uint32_t>(dataInfo->lenVars) * 4))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!util::readFloatArrayV8(fp,
                                                    dataInfo->max,
                                                    static_cast<uint32_t>(dataInfo->lenVars) * 4))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    }
                }
                if (!util::readHeaderInfoShaderStepV8(fp, headerPS_VS))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header step [%s]", fileNamePath);
                if (headerPS_VS.lenNameShader)
                {
                    auto dataInfo = new util::INFO_SHADER_DATA(headerPS_VS.sizeArrayVarInBytes,
                                                               static_cast<short>(headerPS_VS.lenNameShader),
                                                               static_cast<short>(headerPS_VS.lenTextureStage2));
                    infoHead->effectShader->blendOperation = headerPS_VS.blendOperation;
                    infoHead->effectShader->dataVS     = (dataInfo);
                    dataInfo->typeAnimation    = headerPS_VS.typeAnimation;
                    dataInfo->timeAnimation    = headerPS_VS.timeAnimation;
                    if (!fread(dataInfo->fileNameShader, static_cast<size_t>(headerPS_VS.lenNameShader), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read shader's name [%s]", fileNamePath);

                    if (headerPS_VS.lenTextureStage2)
                    {
                        if (!fread(dataInfo->fileNameTextureStage2, static_cast<size_t>(headerPS_VS.lenTextureStage2), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name's texture [%s]", fileNamePath);
                    }
                    if (headerPS_VS.sizeArrayVarInBytes)
                    {
                        if (!fread(dataInfo->typeVars, static_cast<size_t>(dataInfo->lenVars), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!util::readFloatArrayV8(fp,
                                                    dataInfo->min,
                                                    static_cast<uint32_t>(dataInfo->lenVars) * 4))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!util::readFloatArrayV8(fp,
                                                    dataInfo->max,
                                                    static_cast<uint32_t>(dataInfo->lenVars) * 4))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    }
                }
            }
        }
        return true;
    }

    bool write_shader_step_to_file(const char *fileOut,
                                   FILE **file,
                                   util::INFO_FX *infoShaderStep,
                                   util::INFO_SHADER_DATA *infoShader,
                                   const bool writeTextureStage2,
                                   const char *shaderNameError)
    {
        util::HEADER_INFO_SHADER_STEP headerPS_VS;
        headerPS_VS.lenNameShader = static_cast<short>(strlen(infoShader->fileNameShader) + 1);
        if (writeTextureStage2 && infoShader->fileNameTextureStage2)
            headerPS_VS.lenTextureStage2 = static_cast<short>(strlen(infoShader->fileNameTextureStage2) + 1);
        else
            headerPS_VS.lenTextureStage2 = 0;
        headerPS_VS.sizeArrayVarInBytes  = static_cast<short>(infoShader->lenVars * 4);
        headerPS_VS.typeAnimation        = static_cast<short>(infoShader->typeAnimation);
        headerPS_VS.blendOperation       = infoShaderStep->blendOperation;
        headerPS_VS.timeAnimation        = infoShader->timeAnimation;

        if (!util::writeHeaderInfoShaderStepV8(*file, headerPS_VS))
            return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add header shader to file");

        if (headerPS_VS.lenNameShader)
        {
            if (!util::addToFileBinary(fileOut,
                                       infoShader->fileNameShader,
                                       static_cast<size_t>(headerPS_VS.lenNameShader),
                                       file))
                return log_util::onFailed(*file,__FILE__, __LINE__, shaderNameError);
        }
        if (headerPS_VS.lenTextureStage2)
        {
            if (!util::addToFileBinary(fileOut,
                                       infoShader->fileNameTextureStage2,
                                       static_cast<size_t>(headerPS_VS.lenTextureStage2),
                                       file))
                return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add textures name stage 2");
        }
        if (headerPS_VS.sizeArrayVarInBytes)
        {
            std::vector<char> lsSizeVar;
            lsSizeVar.reserve(infoShader->lenVars);
            for (int j = 0; j < infoShader->lenVars; ++j)
            {
                lsSizeVar.push_back(static_cast<char>(infoShader->typeVars[j]));
            }
            if (!util::addToFileBinary(fileOut, &lsSizeVar[0], lsSizeVar.size(), file))
                return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add var to file!!");
            for (int j = 0; j < infoShader->lenVars; ++j)
            {
                float d[4];
                memcpy(d, &infoShader->min[j * 4], sizeof(d));
                if (!util::writeFloatArrayV8(*file, d, 4))
                    return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add var to file");
            }
            for (int j = 0; j < infoShader->lenVars; ++j)
            {
                float d[4];
                memcpy(d, &infoShader->max[j * 4], sizeof(d));
                if (!util::writeFloatArrayV8(*file, d, 4))
                    return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add var to file");
            }
        }
        return true;
    }

    bool write_empty_shader_step(FILE **file, const int blendOperation)
    {
        util::HEADER_INFO_SHADER_STEP headerPS_VS;
        headerPS_VS.blendOperation = blendOperation;
        if (!util::writeHeaderInfoShaderStepV8(*file, headerPS_VS))
            return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add header shader to file");
        return true;
    }

    bool write_empty_shader_steps_pair(FILE **file, const int blendOperation)
    {
        if (!write_empty_shader_step(file, blendOperation))
            return false;
        if (!write_empty_shader_step(file, blendOperation))
            return false;
        return true;
    }

    template <typename TriangleReader>
    bool read_detail_mesh_section(FILE *fp,
                                  const char *fileNamePath,
                                  const util::HEADER &headerMain,
                                  mbm::INFO_PHYSICS &infoPhysics,
                                  void *&extraInfo,
                                  TriangleReader readTriangleDetail)
    {
        util::DETAIL_MESH detailInfo;
        if (headerMain.version == DETAIL_MESH_VERSION_MBM_HEADER)
        {
            if (!util::readDetailMeshV8(fp, detailInfo))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
            if (detailInfo.type != MBM_DEPRECATED_DETAIL_TYPE_SCRIPT && detailInfo.type != MBM_DEPRECATED_DETAIL_TYPE_SHADER)
                return log_util::onFailed(fp,__FILE__, __LINE__,"expected first DETAIL_MESH [%s] as size info extra information at version == DETAIL_MESH_VERSION_MBM_HEADER",fileNamePath);
            if (detailInfo.totalBounding)
            {
                const auto extraInfoSize = static_cast<uint32_t>(detailInfo.totalBounding);
                auto * extra     = new char[extraInfoSize];
                if (!fread(extra, static_cast<size_t>(extraInfoSize), 1, fp))
                {
                    delete [] extra;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read old and deprected extra info [%s]", fileNamePath);
                }
                delete [] extra;
            }
        }

        if (!util::readDetailMeshV8(fp, detailInfo))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
        if (headerMain.version == DETAIL_MESH_VERSION_MBM_HEADER)
        {
            if (detailInfo.type != 'H')
                return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'H' at DETAIL_MESH [%s]", fileNamePath);
        }
        else
        {
            if (detailInfo.type != 'P')
                return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'P' from Physics at DETAIL_MESH [%s]", fileNamePath);
        }
        for (int i = 0; i < detailInfo.totalBounding; )
        {
            util::DETAIL_MESH detail;
            if (!util::readDetailMeshV8(fp, detail))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read DETAIL_MESH [%s]", fileNamePath);
            switch (detail.type)
            {
                case MBM_DETAIL_TYPE_CUBE:
                {
                    for(int j=0; j< detail.totalBounding; j++)
                    {
                        auto cube = new mbm::CUBE();
                        infoPhysics.lsCube.push_back(cube);
                        if (!util::readCubeV8(fp, *cube))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                    }
                    i += detail.totalBounding;
                }
                break;
                case MBM_DETAIL_TYPE_SPHERE:
                {
                    for(int j=0; j< detail.totalBounding; j++)
                    {
                        auto base = new mbm::SPHERE();
                        infoPhysics.lsSphere.push_back(base);
                        if (!util::readSphereV8(fp, *base))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                    }
                    i += detail.totalBounding;
                }
                break;
                case MBM_DETAIL_TYPE_CUBE_COMPLEX:
                {
                    for(int j=0; j< detail.totalBounding; j++)
                    {
                        auto complex = new mbm::CUBE_COMPLEX();
                        infoPhysics.lsCubeComplex.push_back(complex);
                        if (!util::readCubeComplexV8(fp, *complex))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                    }
                    i += detail.totalBounding;
                }
                break;
                case MBM_DETAIL_TYPE_TRIANGLE:
                {
                    if (!readTriangleDetail(fp, fileNamePath, detail.totalBounding, headerMain.version))
                        return false;
                    i += detail.totalBounding;
                }
                break;
                case MBM_DETAIL_TYPE_FONT:
                {
                    auto *infoFont = new mbm::INFO_BOUND_FONT();
                    extraInfo = infoFont;
                    util::DETAIL_HEADER_FONT headerFont;
                    if (!util::readDetailHeaderFontV8(fp, headerFont))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]", fileNamePath);
                    auto strNameFont = new char[headerFont.sizeNameFonte];
                    if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                    {
                        delete [] strNameFont;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                    }
                    infoFont->fontName        = strNameFont;
                    infoFont->heightLetter    = headerFont.heightLetter;
                    infoFont->spaceXCharacter = headerFont.spaceXCharacter;
                    infoFont->spaceYCharacter = headerFont.spaceYCharacter;
                    delete[] strNameFont;
                    for (int j = 0; j < headerFont.totalDetailFont; ++j)
                    {
                        auto detailFont = new util::DETAIL_LETTER();
                        if (!util::readDetailLetterV8(fp, *detailFont))
                        {
                            delete detailFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER [%s]", fileNamePath);
                        }
                        if (detailFont->indexFrame >= headerFont.totalDetailFont)
                        {
                            delete detailFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER!! frame out of bound [%s]", fileNamePath);
                        }
                        infoFont->letter[detailFont->letter].detail = detailFont;
                    }
                    i += 1;
                }
                break;
                case MBM_DETAIL_TYPE_PARTICLE:
                {
                    auto* lsParticleInfo = new std::vector<util::STAGE_PARTICLE*>();
                    extraInfo = lsParticleInfo;
                    for (int j = 0; j< detail.totalBounding; j++)
                    {
                        auto stage = new util::STAGE_PARTICLE();
                        lsParticleInfo->push_back(stage);
                        if (!util::readStageParticleV8(fp, *stage))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read stage particle [%s]", fileNamePath);
                    }
                    i += 1;
                }
                break;
                case MBM_DETAIL_TYPE_TILE:
                {
                    auto* infoTileMap = new util::BTILE_INFO();
                    extraInfo = infoTileMap;

                    if (!util::readBtileHeaderMapV8(fp, infoTileMap->map))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read from  detail tile!");

                    if(infoTileMap->map.layerCount != static_cast<uint32_t>(detail.totalBounding))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "expected layerCount == totalBounding. [%d]!=[%d]",infoTileMap->map.layerCount,detail.totalBounding);

                    infoTileMap->infoBrickEditor = new util::BTILE_BRICK_INFO[infoTileMap->map.countRawTiles];
                    if (!util::readBtileBrickInfoArrayV8(fp, infoTileMap->infoBrickEditor, infoTileMap->map.countRawTiles))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read brick info editor tile from  detail tile!");

                    const uint32_t tileCount = infoTileMap->map.count_height_tile * infoTileMap->map.count_width_tile;
                    infoTileMap->layers      = new util::BTILE_LAYER[infoTileMap->map.layerCount];
                    for (uint32_t  j = 0; j < infoTileMap->map.layerCount; ++j)
                    {
                        if (!util::readFloat3ArrayV8(fp, infoTileMap->layers[j].offset))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read layer offset from tile!");

                        infoTileMap->layers[j].lsIndexTiles = new util::BTILE_INDEX_TILE[tileCount];
                        if (!util::readBtileIndexTileArrayV8(fp, infoTileMap->layers[j].lsIndexTiles, tileCount))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read indexed tile from  detail tile!");
                    }

                    util::BTILE_DETAIL_HEADER detailObjsProperty;
                    if (!util::readBtileDetailHeaderV8(fp, detailObjsProperty))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");

                    for(uint32_t j= 0 ; j < detailObjsProperty.totalObj; ++j)
                    {
                        util::BTILE_OBJ_HEADER objHeader;
                        if (!util::readBtileObjHeaderV8(fp, objHeader))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                        auto* obj = new util::BTILE_OBJ(static_cast<util::BTILE_OBJ_TYPE>(objHeader.type));
                        infoTileMap->lsObj.push_back(obj);
                        if(objHeader.sizeName > 0)
                        {
                            obj->name.resize(objHeader.sizeName);
                            auto * name = const_cast<char*>(obj->name.data());
                            if (!fread(name, objHeader.sizeName,1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name at detail tile header!");
                        }
                        for(uint32_t k= 0 ; k < objHeader.sizePoints; ++k)
                        {
                            auto* point = new mbm::VEC2();
                            obj->lsPoints.push_back(point);
                            if (!util::readVec2V8(fp, *point))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                        }
                    }

                    for(uint32_t j= 0 ; j < detailObjsProperty.totalProperties; ++j)
                    {
                        util::BTILE_PROPERTY_HEADER propertyHeader;
                        if (!util::readBtilePropertyHeaderV8(fp, propertyHeader))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail property tile header!");
                        auto* property = new util::BTILE_PROPERTY(static_cast<util::BTILE_PROPERTY_TYPE>(propertyHeader.type));
                        infoTileMap->lsProperty.push_back(property);
                        if(propertyHeader.nameLength > 0)
                        {
                            property->name.resize(propertyHeader.nameLength);
                            auto * name = const_cast<char*>(property->name.data());
                            if (!fread(name,propertyHeader.nameLength,1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property name at detail tile header!");
                        }
                        if(propertyHeader.valueLength > 0)
                        {
                            property->value.resize(propertyHeader.valueLength);
                            auto * value = const_cast<char*>(property->value.data());
                            if (!fread(value,propertyHeader.valueLength,1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property value at detail tile header!");
                        }
                        if(propertyHeader.ownerLength > 0)
                        {
                            property->owner.resize(propertyHeader.ownerLength);
                            auto * owner = const_cast<char*>(property->owner.data());
                            if (!fread(owner,propertyHeader.ownerLength,1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property owner at detail tile header!");
                        }
                    }

                    i += 1;
                }
                break;
                default:
                {
                    return log_util::onFailed(fp,__FILE__, __LINE__, "unknown type bounding box [%d] [%s]", detail.type, fileNamePath);
                }
            }
        }
        return true;
    }

    bool readHeaderDescSubsetVersioned(FILE *fp, util::HEADER_DESC_SUBSET &out, const int version)
    {
        if (version >= MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER)
            return util::readHeaderDescSubsetV9(fp, out);
        return util::readHeaderDescSubsetV8(fp, out);
    }

    bool writeHeaderDescSubsetVersioned(FILE *fp, const util::HEADER_DESC_SUBSET &in, const int version)
    {
        if (version >= MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER)
            return util::writeHeaderDescSubsetV9(fp, in);
        return util::writeHeaderDescSubsetV8(fp, in);
    }

    bool isKnownMaterialTextureSlotType(const uint16_t type) noexcept
    {
        switch (type)
        {
            case util::MATERIAL_TEXTURE_SLOT_NORMAL:
            case util::MATERIAL_TEXTURE_SLOT_SPECULAR:
            case util::MATERIAL_TEXTURE_SLOT_EMISSIVE:
            case util::MATERIAL_TEXTURE_SLOT_MASK:
            {
                return true;
            }
            default:
            {
                return false;
            }
        }
    }

    bool skipBytes(FILE *fp, const uint32_t totalBytes, const char *fileNamePath, const char *context)
    {
        if (totalBytes == 0)
            return true;
        if (fseek(fp, static_cast<long>(totalBytes), SEEK_CUR) != 0)
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to skip %s [%s]", context, fileNamePath);
        return true;
    }

    bool fillTextureReferenceForHeader(FILE *file,
                                       const std::string &textureReference,
                                       const util::TYPE_MESH typeMe,
                                       char outNameTexture[64])
    {
        memset(outNameTexture, 0, 64);
        if (textureReference.empty())
        {
            strncpy(outNameTexture, "default", 63);
            return true;
        }

        std::vector<std::string> lsRet;
        util::split(lsRet, textureReference.c_str(), '\\');
        const std::vector<std::string>::size_type s = lsRet.size();
        if (s)
        {
            const std::string newOne(lsRet[s - 1]);
            util::split(lsRet, newOne.c_str(), '/');
            const std::vector<std::string>::size_type s2 = lsRet.size();
            if (s2)
                strncpy(outNameTexture, lsRet[s2 - 1].c_str(), 63);
            else
                strncpy(outNameTexture, textureReference.c_str(), 63);
        }
        else
        {
            strncpy(outNameTexture, textureReference.c_str(), 63);
        }

        if (typeMe == util::TYPE_MESH_FONT)
        {
            const std::string fontNameTexture(outNameTexture);
            if (fontNameTexture.find(".ttf") != std::string::npos)
                return log_util::onFailed(file, __FILE__, __LINE__,
                                          "You must to load the font with 'save' flag enabled to save as png otherwise will not work...");
        }

        bool exists = false;
        std::string fullPathTexture = util::getFullPath(outNameTexture, &exists);
        if (exists && fullPathTexture.size() < 64u)
            strncpy(outNameTexture, fullPathTexture.c_str(), 63);
        return true;
    }

    bool readMaterialTextureSlotDebug(FILE *fp,
                                      const char *fileNamePath,
                                      const util::MATERIAL_TEXTURE_SLOT_HEADER &slotHeader,
                                      util::MATERIAL_TEXTURE_SLOT_DEBUG &slotDebug)
    {
        slotDebug.type = slotHeader.type;
        if (slotHeader.payloadSizeInBytes == 0)
        {
            slotDebug.texture = slotHeader.nameTexture;
            return true;
        }

        char nameTexture[sizeof(slotHeader.nameTexture)];
        strncpy(nameTexture, slotHeader.nameTexture, sizeof(nameTexture) - 1);
        nameTexture[sizeof(nameTexture) - 1] = 0;
        char *pch = strchr(nameTexture, '#');
        if (!(pch && pch[0] == '#' && pch[1] == 'u'))
            return log_util::onFailed(fp, __FILE__, __LINE__, "invalid embedded material texture slot payload [%s]", fileNamePath);
        pch[0] = 0;

        util::HEADER_IMG headerImg;
        headerImg.lenght = 0;
        if (!util::readHeaderImgV8(fp, headerImg))
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material slot image header [%s]", fileNamePath);
        if (headerImg.lenght == 0)
            return log_util::onFailed(fp, __FILE__, __LINE__, "invalid material slot image header [%s]", fileNamePath);

        auto data = new uint8_t[headerImg.lenght];
        if (!fread(data, static_cast<size_t>(headerImg.lenght), 1, fp))
        {
            delete[] data;
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material slot image [%s]", fileNamePath);
        }

        uint32_t sizeOfImage = 0;
        if (headerImg.channel != 4 && headerImg.channel != 3 && headerImg.channel != 0)
        {
            delete[] data;
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material slot image channel [%s]", fileNamePath);
        }
        headerImg.channel = headerImg.channel == 4 ? 4 : 3;
        switch (headerImg.depth)
        {
            case 3:
            {
                sizeOfImage = 3 * headerImg.channel * headerImg.width * headerImg.height;
                while (sizeOfImage % 8)
                {
                    sizeOfImage++;
                }
                sizeOfImage = sizeOfImage / 8;
            }
            break;
            case 4:
            {
                sizeOfImage = 4 * headerImg.channel * headerImg.width * headerImg.height;
                while (sizeOfImage % 8)
                {
                    sizeOfImage++;
                }
                sizeOfImage = sizeOfImage / 8;
            }
            break;
            case 8:
            {
                sizeOfImage = headerImg.width * headerImg.height * headerImg.channel;
            }
            break;
            default:
            {
                delete[] data;
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material slot image [%s]", fileNamePath);
            }
        }

        mbm::MINIZ miniz;
        const bool ok = miniz.decompressStream(data, headerImg.lenght, sizeOfImage);
        delete[] data;
        if (!ok)
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to uncompress material slot image [%s]", fileNamePath);

        const uint32_t consumedPayload = static_cast<uint32_t>(sizeof(util::HEADER_IMG_DISK_V8)) + headerImg.lenght;
        if (consumedPayload > slotHeader.payloadSizeInBytes)
            return log_util::onFailed(fp, __FILE__, __LINE__, "invalid material slot payload size [%s]", fileNamePath);
        if (!skipBytes(fp, slotHeader.payloadSizeInBytes - consumedPayload, fileNamePath, "material texture slot payload"))
            return false;

        slotDebug.texture = nameTexture;
        return true;
    }

    bool readMaterialTextureSlotRuntime(FILE *fp,
                                        const char *fileNamePath,
                                        const util::MATERIAL_TEXTURE_SLOT_HEADER &slotHeader,
                                        util::MATERIAL_TEXTURE_SLOT_HEADER &slotHeaderOut,
                                        mbm::TEXTURE *&textureOut)
    {
        slotHeaderOut = slotHeader;
        textureOut = nullptr;
        mbm::TEXTURE_MANAGER *textureManager = mbm::TEXTURE_MANAGER::getInstance();
        if (slotHeader.payloadSizeInBytes == 0)
        {
            textureOut = textureManager->load(slotHeader.nameTexture, true);
            return true;
        }

        char nameTexture[sizeof(slotHeader.nameTexture)];
        strncpy(nameTexture, slotHeader.nameTexture, sizeof(nameTexture) - 1);
        nameTexture[sizeof(nameTexture) - 1] = 0;
        char *pch = strchr(nameTexture, '#');
        if (!(pch && pch[0] == '#' && pch[1] == 'u'))
            return log_util::onFailed(fp, __FILE__, __LINE__, "invalid embedded material texture slot payload [%s]", fileNamePath);
        pch[0] = 0;

        util::HEADER_IMG headerImg;
        headerImg.lenght = 0;
        if (!util::readHeaderImgV8(fp, headerImg))
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material slot image header [%s]", fileNamePath);
        if (headerImg.lenght == 0)
            return log_util::onFailed(fp, __FILE__, __LINE__, "invalid material slot image header [%s]", fileNamePath);

        auto data = new uint8_t[headerImg.lenght];
        if (!fread(data, static_cast<size_t>(headerImg.lenght), 1, fp))
        {
            delete[] data;
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material slot image [%s]", fileNamePath);
        }

        uint32_t sizeOfImage = 0;
        if (headerImg.channel != 4 && headerImg.channel != 3 && headerImg.channel != 0)
        {
            delete[] data;
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material slot image channel [%s]", fileNamePath);
        }
        headerImg.channel = headerImg.channel == 4 ? 4 : 3;
        switch (headerImg.depth)
        {
            case 3:
            {
                sizeOfImage = 3 * headerImg.channel * headerImg.width * headerImg.height;
                while (sizeOfImage % 8)
                {
                    sizeOfImage++;
                }
                sizeOfImage = sizeOfImage / 8;
            }
            break;
            case 4:
            {
                sizeOfImage = 4 * headerImg.channel * headerImg.width * headerImg.height;
                while (sizeOfImage % 8)
                {
                    sizeOfImage++;
                }
                sizeOfImage = sizeOfImage / 8;
            }
            break;
            case 8:
            {
                sizeOfImage = headerImg.width * headerImg.height * headerImg.channel;
            }
            break;
            default:
            {
                delete[] data;
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material slot image [%s]", fileNamePath);
            }
        }

        mbm::MINIZ miniz;
        if (!miniz.decompressStream(data, headerImg.lenght, sizeOfImage))
        {
            delete[] data;
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to uncompress material slot image [%s]", fileNamePath);
        }
        delete[] data;

        const uint32_t consumedPayload = static_cast<uint32_t>(sizeof(util::HEADER_IMG_DISK_V8)) + headerImg.lenght;
        if (consumedPayload > slotHeader.payloadSizeInBytes)
            return log_util::onFailed(fp, __FILE__, __LINE__, "invalid material slot payload size [%s]", fileNamePath);
        if (!skipBytes(fp, slotHeader.payloadSizeInBytes - consumedPayload, fileNamePath, "material texture slot payload"))
            return false;

        textureOut = textureManager->load(headerImg.width,
                                          headerImg.height,
                                          miniz.getDataStreamOut(),
                                          nameTexture,
                                          headerImg.depth,
                                          headerImg.channel,
                                          headerImg.hasAlpha ? true : false);
        strncpy(slotHeaderOut.nameTexture, nameTexture, sizeof(slotHeaderOut.nameTexture) - 1);
        slotHeaderOut.nameTexture[sizeof(slotHeaderOut.nameTexture) - 1] = 0;
        slotHeaderOut.payloadSizeInBytes = 0;
        return true;
    }

    bool readMaterialTextureSlotsDebug(FILE *fp,
                                       const char *fileNamePath,
                                       const int version,
                                       const uint16_t slotCount,
                                       util::SUBSET_DEBUG &subsetDebug)
    {
        if (version < MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER)
            return true;
        for (uint16_t i = 0; i < slotCount; ++i)
        {
            util::MATERIAL_TEXTURE_SLOT_HEADER slotHeader;
            if (!util::readMaterialTextureSlotHeaderV9(fp, slotHeader))
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material texture slot header [%s]", fileNamePath);
            if (!isKnownMaterialTextureSlotType(slotHeader.type))
            {
                if (!skipBytes(fp, slotHeader.payloadSizeInBytes, fileNamePath, "unknown material texture slot payload"))
                    return false;
                continue;
            }
            util::MATERIAL_TEXTURE_SLOT_DEBUG slotDebug;
            if (!readMaterialTextureSlotDebug(fp, fileNamePath, slotHeader, slotDebug))
                return false;
            subsetDebug.materialTextureSlots.push_back(slotDebug);
        }
        return true;
    }

    bool readMaterialTextureSlotsRuntime(FILE *fp,
                                         const char *fileNamePath,
                                         const int version,
                                         const uint16_t slotCount,
                                         util::SUBSET &subsetRuntime)
    {
        if (version < MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER)
            return true;
        for (uint16_t i = 0; i < slotCount; ++i)
        {
            util::MATERIAL_TEXTURE_SLOT_HEADER slotHeader;
            if (!util::readMaterialTextureSlotHeaderV9(fp, slotHeader))
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read material texture slot header [%s]", fileNamePath);
            if (!isKnownMaterialTextureSlotType(slotHeader.type))
            {
                if (!skipBytes(fp, slotHeader.payloadSizeInBytes, fileNamePath, "unknown material texture slot payload"))
                    return false;
                continue;
            }
            util::MATERIAL_TEXTURE_SLOT_HEADER slotHeaderOut;
            mbm::TEXTURE *texture = nullptr;
            if (!readMaterialTextureSlotRuntime(fp, fileNamePath, slotHeader, slotHeaderOut, texture))
                return false;
            subsetRuntime.materialTextureSlotHeaders.push_back(slotHeaderOut);
            subsetRuntime.materialTextures.push_back(texture);
        }
        return true;
    }
}

namespace mbm
{
    struct MESH_MANAGER::Impl
    {
        std::unordered_map<std::string, MESH_MBM *> lsMeshes;
        std::vector<MESH_MBM *> lsFakeRelease;
    };

    constexpr BUFFER_MESH::BUFFER_MESH() noexcept : pBufferGL(nullptr), subset(nullptr), totalSubset(0)
    {
    }
    
    BUFFER_MESH::~BUFFER_MESH()
    {
        release();
    }
    
    void BUFFER_MESH::release()
    {
        if (pBufferGL)
            delete pBufferGL;
        pBufferGL = nullptr;

        if (subset)
            delete[] subset;
        subset      = nullptr;
        totalSubset = 0;
    }



    MESH_MBM_DEBUG::MESH_MBM_DEBUG() noexcept
    {
        positionOffset      = VEC3(0, 0, 0);
        angleDefault        = VEC3(0, 0, 0);
        coordTexFrame_0     = nullptr;
        sizeCoordTexFrame_0 = 0;
        typeMe              = util::TYPE_MESH_UNKNOWN;
        util::MATERIAL m;
        this->headerMesh.material      = m;
        this->headerMesh.hasNorText[0] = HAS_NOR_NO;
        this->headerMesh.hasNorText[1] = HAS_TEX_EACH_FRAME;
        zoomEditorSprite.x            = 1.0f;
        zoomEditorSprite.y            = 1.0f;
        extraInfo                     = nullptr;
    }

    MESH_MBM_DEBUG::~MESH_MBM_DEBUG()
    {
        this->release();
    }
    
    uint32_t MESH_MBM_DEBUG::addBuffer(const int stride )
    {
        if ((stride == 3 || stride == 2))
        {
            auto pBuffer = new util::BUFFER_MESH_DEBUG();
            pBuffer->headerFrame.stride      = stride;
            this->buffer.push_back(pBuffer);
            return static_cast<uint32_t>(this->buffer.size());
        }
        return 0;
    }
    
    uint32_t MESH_MBM_DEBUG::addSubset(uint32_t indexFrame)
    {
        if (indexFrame < static_cast<uint32_t>(this->buffer.size()))
        {
            this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)]->subset.push_back(new util::SUBSET_DEBUG());
            return static_cast<uint32_t>(this->buffer[indexFrame]->subset.size());
        }
        return 0;
    }
    
    bool MESH_MBM_DEBUG::getInfo(util::HEADER_MESH &headerMeshMbmOut, util::TYPE_MESH &typeOut,
                              INFO_BOUND_FONT **datailFontOut, std::vector<util::STAGE_PARTICLE> & lsStageParticle)
    {
        if (this->buffer.size())
        {
            headerMeshMbmOut = this->headerMesh;
            typeOut          = this->typeMe;

            if(this->typeMe == util::TYPE_MESH_FONT)
            {
                *datailFontOut   = static_cast<INFO_BOUND_FONT *>(this->extraInfo);
            }
            lsStageParticle.clear();
            if(this->typeMe == util::TYPE_MESH_PARTICLE)
            {
                auto* lsParticleInfo = static_cast<std::vector<util::STAGE_PARTICLE*>*>(this->extraInfo);
                if(lsParticleInfo)
                {
                    for (auto & i : *lsParticleInfo)
                    {
                        lsStageParticle.push_back(*i);
                    }		
                }
            }
            return true;
        }
        return false;
    }

    std::string MESH_MBM_DEBUG::getExtension(const char* fileName)
    {
        std::string ret;
        if(fileName)
        {
            const std::string file(fileName);
            const std::string::size_type p = file.find_last_of('.');
            if(p != std::string::npos && (p+1) < file.size())
            {
                std::string newExt(file.substr(p + 1));
                const std::string::size_type l = newExt.size();
                for (std::string::size_type i=0; i < l; ++i )
                {
                    newExt[i] = static_cast<char>(toupper(newExt[i]));
                }
                ret = std::move(newExt);
            }
        }
        return ret;
    }

    const char* MESH_MBM_DEBUG::getValidExtension(const char* fileName,bool &isImage,bool &isMesh,bool &isUnknown)
    {
        if(fileName)
        {
            const std::string file(fileName);
            const std::string::size_type p = file.find_last_of('.');
            if(p != std::string::npos && (p+1) < file.size())
            {
                const char* ext = &fileName[p+1];
                std::string                  newExt(file.substr(p + 1));
                const std::string::size_type l = newExt.size();
                for (std::string::size_type i=0; i < l; ++i )
                {
                    newExt[i] = static_cast<char>(toupper(newExt[i]));
                }
                isImage = true;
                if(newExt.compare("PNG") == 0)
                    return ext;
                if(newExt.compare("BMP") == 0)
                    return ext;
                if(newExt.compare("TIF") == 0)
                    return ext;
                if(newExt.compare("JPEG") == 0)
                    return ext;
                if(newExt.compare("JPG") == 0)
                    return ext;
                if(newExt.compare("GIF") == 0)
                    return ext;
                if(newExt.compare("TIFF") == 0)
                    return ext;
                if(newExt.compare("UBERIMG") == 0)
                    return ext;
                isImage = false;
                isMesh = true;
                if(newExt.compare("SPT") == 0)
                    return ext;
                if(newExt.compare("MSH") == 0)
                    return ext;
                if(newExt.compare("FNT") == 0)
                    return ext;
                if(newExt.compare("PTL") == 0)
                    return ext;
                if(newExt.compare("TILE") == 0)
                    return ext;
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
                if(newExt.compare("MBM") == 0)
                    return ext;
#endif
                isMesh = false;
                isUnknown = true;
                return ext;
            }
        }
        return nullptr;
    }
    
    bool MESH_MBM_DEBUG::getInfo(const char *fileNamePath, util::HEADER_MESH &headerMeshMbmOut,util::INFO_DRAW_MODE & info_mode,
                              util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut, 
                              std::vector<util::STAGE_PARTICLE> & lsStageParticle, int *versionOut)
    {
        bool isImage    = false;
        bool isMesh     = false;
        bool isUnknown  = false;
        const char* ext = getValidExtension(fileNamePath,isImage,isMesh,isUnknown);
        if(isUnknown)
        {
            typeOut = util::TYPE_MESH_UNKNOWN;
            return false;
        }
        if(ext && (isImage))
        {
            typeOut = util::TYPE_MESH_TEXTURE;
            datailFontOut.fontName = ext;
            const std::string::size_type l = datailFontOut.fontName.size();
            for (std::string::size_type i=0; i < l; ++i )
            {
                datailFontOut.fontName[i] = static_cast<char>(toupper(datailFontOut.fontName[i]));
            }
            util::HEADER_MESH tmp;
            headerMeshMbmOut = tmp;
            if (versionOut) *versionOut = 0;
            return true;    
        }
        if(!isMesh)
            return false;
        util::HEADER headerMbmOut;
        FILE * fp = nullptr;
        if (!open_decompressed_mesh_file(fileNamePath, fp, "Failed to open file [%s]"))
            return false;
        // step 1: Verificação do header  MBM principal
        // -------------------------------------------------------------------------------
        if (!read_main_header_and_type(fp, fileNamePath, headerMbmOut, typeOut))
            return false;

        if (headerMbmOut.version < STRONG_TYPES_VERSION_MBM_HEADER)
        {
    #if defined(MBM_ENABLE_MESH_LEGACY_V7)
            return MESH_MBM_DEBUG::getInfoLegacyCompat(fp, fileNamePath, headerMbmOut, headerMeshMbmOut, info_mode,
                                   typeOut, datailFontOut, lsStageParticle, versionOut);
    #else
            return log_util::onFailed(fp,__FILE__, __LINE__, "legacy mesh version [%d] disabled at compile time. Rebuild with MBM_ENABLE_MESH_LEGACY_V7", headerMbmOut.version);
    #endif
        }

        if (!read_extra_headers(fp, fileNamePath, headerMbmOut.extraHeader, false))
            return false;

        if (!read_info_mode_if_needed(fp, fileNamePath, headerMbmOut.version, info_mode, false))
            return false;

        // step 2: --------------------------------------------------------------------------------------------------
        if (headerMbmOut.version >= DETAIL_MESH_VERSION_MBM_HEADER)
        {
            util::DETAIL_MESH detailInfo;
            if (!util::readDetailMeshV8(fp, detailInfo))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
            if (detailInfo.type != 'P')
                return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'P' from Physics at DETAIL_MESH [%s]", fileNamePath);
            for (int i = 0; i < detailInfo.totalBounding; )
            {
                util::DETAIL_MESH detail;
                if (!util::readDetailMeshV8(fp, detail))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read DETAIL_MESH [%s]", fileNamePath);
                switch (detail.type)
                {
                    case MBM_DETAIL_TYPE_CUBE:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            CUBE cube;
                            if (!util::readCubeV8(fp, cube))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case MBM_DETAIL_TYPE_SPHERE:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            SPHERE base;
                            if (!util::readSphereV8(fp, base))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case MBM_DETAIL_TYPE_CUBE_COMPLEX:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            CUBE_COMPLEX complex;
                            if (!util::readCubeComplexV8(fp, complex))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case MBM_DETAIL_TYPE_TRIANGLE:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            TRIANGLE triangle;
                            if (!util::readTriangleV8(fp, triangle))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case MBM_DETAIL_TYPE_FONT:
                    {
                        util::DETAIL_HEADER_FONT headerFont;
                        if (!util::readDetailHeaderFontV8(fp, headerFont))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]",
                                                     fileNamePath);
                        auto strNameFont = new char[headerFont.sizeNameFonte];
                        if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                        {
                            delete [] strNameFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                        }
                        datailFontOut.fontName        = strNameFont;
                        datailFontOut.heightLetter    = headerFont.heightLetter;
                        datailFontOut.spaceXCharacter = headerFont.spaceXCharacter;
                        datailFontOut.spaceYCharacter = headerFont.spaceYCharacter;
                        delete[] strNameFont;
                        for (int j = 0; j < headerFont.totalDetailFont; ++j)
                        {
                            auto detailFont = new util::DETAIL_LETTER();
                            if (!util::readDetailLetterV8(fp, *detailFont))
                            {
                                delete detailFont;
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER [%s]", fileNamePath);
                            }
                            if (detailFont->indexFrame >= headerFont.totalDetailFont)
                            {
                                delete detailFont;
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER!! frame out of bound [%s]", fileNamePath);
                            }
                            datailFontOut.letter[detailFont->letter].detail = detailFont;
                        }
                        i += 1;
                    }
                    break;
                    case MBM_DETAIL_TYPE_PARTICLE:
                    {
                        for (int j = 0; j< detail.totalBounding; j++)
                        {
                            util::STAGE_PARTICLE stage;
                            if (!util::readStageParticleV8(fp, stage))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read stage particle [%s]", fileNamePath);
                            lsStageParticle.push_back(stage);
                        }
                        i += 1;
                    }
                    break;
                    case MBM_DETAIL_TYPE_TILE:
                    {
                        util::BTILE_INFO infoTileMap;
                        
                        if (!util::readBtileHeaderMapV8(fp, infoTileMap.map))
                        {
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read from  detail tile!");
                        }

                        if(infoTileMap.map.layerCount != static_cast<uint32_t>(detail.totalBounding))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "expected layerCount == totalBounding. [%d]!=[%d]",infoTileMap.map.layerCount,detail.totalBounding);


                        infoTileMap.infoBrickEditor = new util::BTILE_BRICK_INFO[infoTileMap.map.countRawTiles];
                        if (!util::readBtileBrickInfoArrayV8(fp, infoTileMap.infoBrickEditor, infoTileMap.map.countRawTiles))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read brick info editor tile from  detail tile!");

                        const uint32_t tileCount = infoTileMap.map.count_height_tile * infoTileMap.map.count_width_tile;
                        infoTileMap.layers = new util::BTILE_LAYER[infoTileMap.map.layerCount];

                        for (uint32_t  j = 0; j < infoTileMap.map.layerCount; ++j)
                        {
                            if (!util::readFloat3ArrayV8(fp, infoTileMap.layers[j].offset))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read layer offset from tile!");

                            infoTileMap.layers[j].lsIndexTiles = new util::BTILE_INDEX_TILE[tileCount];

                            if (!util::readBtileIndexTileArrayV8(fp, infoTileMap.layers[j].lsIndexTiles, tileCount))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read indexed tile from  detail tile!");
                        }

                        util::BTILE_DETAIL_HEADER detailObjsProperty;
                        if (!util::readBtileDetailHeaderV8(fp, detailObjsProperty))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");

                        for(uint32_t j= 0 ; j < detailObjsProperty.totalObj; ++j)
                        {
                            util::BTILE_OBJ_HEADER objHeader;
                            if (!util::readBtileObjHeaderV8(fp, objHeader))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            auto* obj = new util::BTILE_OBJ(static_cast<util::BTILE_OBJ_TYPE>(objHeader.type));
                            infoTileMap.lsObj.push_back(obj);
                            if(objHeader.sizeName > 0)
                            {
                                obj->name.resize(objHeader.sizeName);
                                auto * name = const_cast<char*>(obj->name.data());
                                if (!fread(name, objHeader.sizeName,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name at detail tile header!");
                            }
                            for(uint32_t k= 0 ; k < objHeader.sizePoints; ++k)
                            {
                                auto* point = new mbm::VEC2();
                                obj->lsPoints.push_back(point);
                                if (!util::readVec2V8(fp, *point))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            }
                        }

                        for(uint32_t j= 0 ; j < detailObjsProperty.totalProperties; ++j)
                        {
                            util::BTILE_PROPERTY_HEADER propertyHeader;
                            if (!util::readBtilePropertyHeaderV8(fp, propertyHeader))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail property tile header!");
                            auto* property = new util::BTILE_PROPERTY();
                            infoTileMap.lsProperty.push_back(property);
                            if(propertyHeader.nameLength > 0)
                            {
                                property->name.resize(propertyHeader.nameLength);
                                auto * name = const_cast<char*>(property->name.data());
                                if (!fread(name,propertyHeader.nameLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property name at detail tile header!");
                            }
                            if(propertyHeader.valueLength > 0)
                            {
                                property->value.resize(propertyHeader.valueLength);
                                auto * value = const_cast<char*>(property->value.data());
                                if (!fread(value,propertyHeader.valueLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property value at detail tile header!");
                            }
                            if(propertyHeader.ownerLength > 0)
                            {
                                property->owner.resize(propertyHeader.ownerLength);
                                auto * owner = const_cast<char*>(property->owner.data());
                                if (!fread(owner,propertyHeader.ownerLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property owner at detail tile header!");
                            }
                        }
                        i += 1;
                    }
                    break;
                    default:
                    {
                        return log_util::onFailed(fp,__FILE__, __LINE__, "unknown type bounding box [%d] [%s]", detail.type,
                                                 fileNamePath);
                    }
                }
            }
        }
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "Imcompatible version [%d]", headerMbmOut.version);
        }
        // 3 headerMesh MBM -------------------------------------------------------------------------------
        if (!util::readHeaderMeshV8(fp, headerMeshMbmOut))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read HEADER_MESH [%s]", fileNamePath);
        fclose(fp);
        fp = nullptr;
        if (versionOut) *versionOut = headerMbmOut.version;
        return true;
    }
    
    util::TYPE_MESH MESH_MBM_DEBUG::getType() noexcept
    {
        if (this->buffer.size())
            return this->typeMe;
        return util::TYPE_MESH_UNKNOWN;
    }
    
    util::TYPE_MESH MESH_MBM_DEBUG::getType(const char *fileNamePath)
    {
        bool isImage    = false;
        bool isMesh     = false;
        bool isUnknown  = false;
        const char* ext = getValidExtension(fileNamePath,isImage,isMesh,isUnknown);
        if(ext == nullptr)
            return util::TYPE_MESH_UNKNOWN;
        if(isImage)
            return util::TYPE_MESH_TEXTURE;
        if(isUnknown)
            return util::TYPE_MESH_UNKNOWN;
        if(!isMesh)
            return util::TYPE_MESH_UNKNOWN;
        if(strcasecmp(ext,"SPT") == 0)
            return util::TYPE_MESH_SPRITE;
        if(strcasecmp(ext,"FNT") == 0)
            return util::TYPE_MESH_FONT;
        if(strcasecmp(ext,"MSH") == 0)
            return util::TYPE_MESH_3D;
        if(strcasecmp(ext,"PTL") == 0)
            return util::TYPE_MESH_PARTICLE;
        if (strcasecmp(ext, "TILE") == 0)
            return util::TYPE_MESH_TILE_MAP;
        util::TYPE_MESH typeOut = util::TYPE_MESH_UNKNOWN;
        util::HEADER    headerMbmOut;
        FILE * fp = nullptr;
        if (!open_decompressed_mesh_file(fileNamePath, fp, "Failed to open file [%s]"))
            return util::TYPE_MESH_UNKNOWN;
        // step 1: Verificação do header  MBM principal
        // -------------------------------------------------------------------------------
        if (!read_main_header_and_type(fp, fileNamePath, headerMbmOut, typeOut))
            return util::TYPE_MESH_UNKNOWN;
        fclose(fp);
        fp = nullptr;
        return typeOut;
    }
    
    void MESH_MBM_DEBUG::calculateNormals()
    {
        headerMesh.totalFrames = static_cast<int>(this->buffer.size());
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(currentFrame)];
            auto *                   position           = reinterpret_cast<VEC3 *>(currentFrameBuffer->position);
            if (currentFrameBuffer->normal == nullptr)
            {
                currentFrameBuffer->normal = new float[currentFrameBuffer->headerFrame.sizeVertexBuffer * 3];
            }
            auto *                   normal             = reinterpret_cast<VEC3 *>(currentFrameBuffer->normal);
            const auto       sSub               = static_cast<uint32_t>(currentFrameBuffer->subset.size());
            if (currentFrameBuffer->indexBuffer == nullptr) // vertex
            {
                for (uint32_t indexSubset = 0; indexSubset < sSub; ++indexSubset)
                {
                    util::SUBSET_DEBUG *subset      = currentFrameBuffer->subset[indexSubset];
                    int                 countSubset = subset->vertexStart + subset->vertexCount;
                    countSubset -= (countSubset % 3);
                    for (int i = subset->vertexStart; (i + 3) < countSubset; i += 3)
                    {
                        VEC3      a, b, n;
                        const int index0 = i;
                        const int index1 = i + 1;
                        const int index2 = i + 2;

                        a.x = position[index1].x - position[index0].x;
                        a.y = position[index1].y - position[index0].y;
                        a.z = position[index1].z - position[index0].z;

                        b.x = position[index2].x - position[index0].x;
                        b.y = position[index2].y - position[index0].y;
                        b.z = position[index2].z - position[index0].z;

                        vec3Cross(&n, &a, &b);
                        vec3Normalize(&n, &n);

                        normal[index0] = n;
                        normal[index1] = n;
                        normal[index2] = n;
                    }
                }
            }
            else
            {
                for (uint32_t indexSubset = 0; indexSubset < sSub; ++indexSubset)
                {
                    util::SUBSET_DEBUG *subset           = currentFrameBuffer->subset[indexSubset];
                    int                 countIndexSubset = subset->vertexStart + subset->vertexCount;
                    countIndexSubset -= (countIndexSubset % 3);
                    for (int i = subset->indexStart; (i + 3) < countIndexSubset; i += 3)
                    {
                        VEC3      a, b, n;
                        const auto index0 = static_cast<int>(currentFrameBuffer->indexBuffer[i]);
                        const auto index1 = static_cast<int>(currentFrameBuffer->indexBuffer[i + 1]);
                        const auto index2 = static_cast<int>(currentFrameBuffer->indexBuffer[i + 2]);

                        a.x = position[index1].x - position[index0].x;
                        a.y = position[index1].y - position[index0].y;
                        a.z = position[index1].z - position[index0].z;

                        b.x = position[index2].x - position[index0].x;
                        b.y = position[index2].y - position[index0].y;
                        b.z = position[index2].z - position[index0].z;

                        vec3Cross(&n, &a, &b);
                        vec3Normalize(&n, &n);

                        normal[index0] = n;
                        normal[index1] = n;
                        normal[index2] = n;
                    }
                }
            }
        }
    }
    
    void MESH_MBM_DEBUG::removeNormals()
    {
        for (std::vector<util::BUFFER_MESH_DEBUG *>::size_type i = 0; i < this->buffer.size(); ++i)
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[i];
            if (bufferCurrent->normal)
            {
                delete[] bufferCurrent->normal;
                bufferCurrent->normal = nullptr;
            }
        }
        headerMesh.hasNorText[0] = HAS_NOR_NO;
    }
    
    void MESH_MBM_DEBUG::addNormals()
    {
        calculateNormals();
        headerMesh.hasNorText[0] = HAS_NOR_IN_FILE;
    }

    void MESH_MBM_DEBUG::removeBuffer(uint32_t indexFrame)
    {
        if (indexFrame >= static_cast<uint32_t>(this->buffer.size()))
            return;
        delete this->buffer[indexFrame];
        this->buffer.erase(this->buffer.begin() + static_cast<ptrdiff_t>(indexFrame));
    }

    void MESH_MBM_DEBUG::removeSubset(uint32_t indexFrame, uint32_t indexSubset)
    {
        if (indexFrame >= static_cast<uint32_t>(this->buffer.size()))
            return;
        util::BUFFER_MESH_DEBUG *buf = this->buffer[indexFrame];
        if (indexSubset >= static_cast<uint32_t>(buf->subset.size()))
            return;
        util::SUBSET_DEBUG *sub = buf->subset[indexSubset];
        const int vStart  = sub->vertexStart;
        const int vCount  = sub->vertexCount;
        const int iStart  = sub->indexStart;
        const int iCount  = sub->indexCount;
        const int stride  = buf->headerFrame.stride;
        const int totalV  = buf->headerFrame.sizeVertexBuffer;
        const int totalI  = buf->headerFrame.sizeIndexBuffer;
        // Compact position
        if (buf->position && vCount > 0 && (vStart + vCount) < totalV)
        {
            float *dst       = buf->position + (vStart * stride);
            const float *src = buf->position + ((vStart + vCount) * stride);
            const int n      = (totalV - vStart - vCount) * stride;
            memmove(dst, src, static_cast<size_t>(n) * sizeof(float));
        }
        // Compact normals
        if (buf->normal && vCount > 0 && (vStart + vCount) < totalV)
        {
            float *dst       = buf->normal + (vStart * 3);
            const float *src = buf->normal + ((vStart + vCount) * 3);
            const int n      = (totalV - vStart - vCount) * 3;
            memmove(dst, src, static_cast<size_t>(n) * sizeof(float));
        }
        // Compact UVs
        if (buf->uv && vCount > 0 && (vStart + vCount) < totalV)
        {
            float *dst       = buf->uv + (vStart * 2);
            const float *src = buf->uv + ((vStart + vCount) * 2);
            const int n      = (totalV - vStart - vCount) * 2;
            memmove(dst, src, static_cast<size_t>(n) * sizeof(float));
        }
        // Fix index buffer: adjust vertex references, then compact
        if (buf->indexBuffer)
        {
            for (int i = 0; i < totalI; ++i)
            {
                if (i >= iStart && i < iStart + iCount)
                    continue; // skip entries being removed
                if (buf->indexBuffer[i] >= static_cast<uint16_t>(vStart + vCount))
                    buf->indexBuffer[i] -= static_cast<uint16_t>(vCount);
            }
            if (iCount > 0 && (iStart + iCount) < totalI)
            {
                uint16_t *dst       = buf->indexBuffer + iStart;
                const uint16_t *src = buf->indexBuffer + iStart + iCount;
                memmove(dst, src, static_cast<size_t>(totalI - iStart - iCount) * sizeof(uint16_t));
            }
            buf->headerFrame.sizeIndexBuffer -= iCount;
        }
        // Adjust remaining subsets
        for (size_t i = 0; i < buf->subset.size(); ++i)
        {
            if (i == static_cast<size_t>(indexSubset))
                continue;
            util::SUBSET_DEBUG *s = buf->subset[i];
            if (s->vertexStart > vStart)
                s->vertexStart -= vCount;
            if (s->indexStart > iStart)
                s->indexStart -= iCount;
        }
        delete sub;
        buf->subset.erase(buf->subset.begin() + static_cast<ptrdiff_t>(indexSubset));
        buf->headerFrame.totalSubset--;
        buf->headerFrame.sizeVertexBuffer -= vCount;
    }

    uint32_t MESH_MBM_DEBUG::copyBufferFrom(MESH_MBM_DEBUG &src, uint32_t srcFrameIdx)
    {
        if (srcFrameIdx >= static_cast<uint32_t>(src.buffer.size()))
            return 0;
        util::BUFFER_MESH_DEBUG *srcBuf = src.buffer[srcFrameIdx];
        auto *newBuf                    = new util::BUFFER_MESH_DEBUG();
        newBuf->headerFrame             = srcBuf->headerFrame;
        const int stride  = srcBuf->headerFrame.stride;
        const int totalV  = srcBuf->headerFrame.sizeVertexBuffer;
        const int totalI  = srcBuf->headerFrame.sizeIndexBuffer;
        if (srcBuf->position && totalV > 0)
        {
            const size_t n    = static_cast<size_t>(totalV * stride);
            newBuf->position  = new float[n];
            memcpy(newBuf->position, srcBuf->position, n * sizeof(float));
        }
        if (srcBuf->normal && totalV > 0)
        {
            const size_t n  = static_cast<size_t>(totalV * 3);
            newBuf->normal  = new float[n];
            memcpy(newBuf->normal, srcBuf->normal, n * sizeof(float));
        }
        if (srcBuf->uv && totalV > 0)
        {
            const size_t n = static_cast<size_t>(totalV * 2);
            newBuf->uv     = new float[n];
            memcpy(newBuf->uv, srcBuf->uv, n * sizeof(float));
        }
        if (srcBuf->indexBuffer && totalI > 0)
        {
            const size_t n      = static_cast<size_t>(totalI);
            newBuf->indexBuffer = new uint16_t[n];
            memcpy(newBuf->indexBuffer, srcBuf->indexBuffer, n * sizeof(uint16_t));
        }
        for (auto *srcSub : srcBuf->subset)
        {
            auto *newSub = new util::SUBSET_DEBUG();
            *newSub      = *srcSub;
            newBuf->subset.push_back(newSub);
        }
        this->buffer.push_back(newBuf);
        return static_cast<uint32_t>(this->buffer.size());
    }

    uint32_t MESH_MBM_DEBUG::copySubsetFrom(uint32_t targetFrame, MESH_MBM_DEBUG &src, uint32_t srcFrame, uint32_t srcSubsetIdx)
    {
        if (targetFrame >= static_cast<uint32_t>(this->buffer.size()))
            return 0;
        if (srcFrame >= static_cast<uint32_t>(src.buffer.size()))
            return 0;
        util::BUFFER_MESH_DEBUG *tgtBuf = this->buffer[targetFrame];
        util::BUFFER_MESH_DEBUG *srcBuf = src.buffer[srcFrame];
        if (srcSubsetIdx >= static_cast<uint32_t>(srcBuf->subset.size()))
            return 0;
        const int tgtStride = tgtBuf->headerFrame.stride;
        const int srcStride = srcBuf->headerFrame.stride;
        if (tgtStride != srcStride)
            return 0; // incompatible strides
        const bool tgtHasIB = (tgtBuf->indexBuffer != nullptr);
        const bool srcHasIB = (srcBuf->indexBuffer != nullptr);
        if (tgtHasIB != srcHasIB)
            return 0; // incompatible VB/IB modes
        util::SUBSET_DEBUG *srcSub = srcBuf->subset[srcSubsetIdx];
        const int srcVStart  = srcSub->vertexStart;
        const int srcVCount  = srcSub->vertexCount;
        const int srcIStart  = srcSub->indexStart;
        const int srcICount  = srcSub->indexCount;
        const int tgtOldV    = tgtBuf->headerFrame.sizeVertexBuffer;
        const int tgtOldI    = tgtBuf->headerFrame.sizeIndexBuffer;
        // Append vertex data
        if (srcVCount > 0)
        {
            const int newTotalV = tgtOldV + srcVCount;
            // position
            {
                const size_t oldN = static_cast<size_t>(tgtOldV * tgtStride);
                const size_t addN = static_cast<size_t>(srcVCount * srcStride);
                auto *newPos      = new float[oldN + addN];
                if (tgtBuf->position) memcpy(newPos, tgtBuf->position, oldN * sizeof(float));
                memcpy(newPos + oldN, srcBuf->position + srcVStart * srcStride, addN * sizeof(float));
                delete[] tgtBuf->position;
                tgtBuf->position = newPos;
            }
            // normals
            if (srcBuf->normal)
            {
                const size_t oldN = static_cast<size_t>(tgtOldV * 3);
                const size_t addN = static_cast<size_t>(srcVCount * 3);
                auto *newNrm      = new float[oldN + addN]();
                if (tgtBuf->normal) memcpy(newNrm, tgtBuf->normal, oldN * sizeof(float));
                memcpy(newNrm + oldN, srcBuf->normal + srcVStart * 3, addN * sizeof(float));
                delete[] tgtBuf->normal;
                tgtBuf->normal = newNrm;
            }
            // UVs
            if (srcBuf->uv)
            {
                const size_t oldN = static_cast<size_t>(tgtOldV * 2);
                const size_t addN = static_cast<size_t>(srcVCount * 2);
                auto *newUv       = new float[oldN + addN]();
                if (tgtBuf->uv) memcpy(newUv, tgtBuf->uv, oldN * sizeof(float));
                memcpy(newUv + oldN, srcBuf->uv + srcVStart * 2, addN * sizeof(float));
                delete[] tgtBuf->uv;
                tgtBuf->uv = newUv;
            }
            tgtBuf->headerFrame.sizeVertexBuffer = newTotalV;
        }
        // Append index data (adjust vertex indices by tgtOldV)
        if (srcHasIB && srcICount > 0)
        {
            const int newTotalI = tgtOldI + srcICount;
            auto *newIdx        = new uint16_t[static_cast<size_t>(newTotalI)];
            if (tgtBuf->indexBuffer) memcpy(newIdx, tgtBuf->indexBuffer, static_cast<size_t>(tgtOldI) * sizeof(uint16_t));
            for (int i = 0; i < srcICount; ++i)
                newIdx[tgtOldI + i] = static_cast<uint16_t>(srcBuf->indexBuffer[srcIStart + i] - srcVStart + tgtOldV);
            delete[] tgtBuf->indexBuffer;
            tgtBuf->indexBuffer              = newIdx;
            tgtBuf->headerFrame.sizeIndexBuffer = newTotalI;
            strncpy(tgtBuf->headerFrame.typeBuffer, "IB", sizeof(tgtBuf->headerFrame.typeBuffer) - 1);
        }
        // Create new subset entry
        auto *newSub       = new util::SUBSET_DEBUG();
        newSub->texture    = srcSub->texture;
        newSub->materialTextureSlots = srcSub->materialTextureSlots;
        newSub->vertexStart = tgtOldV;
        newSub->vertexCount = srcVCount;
        newSub->indexStart  = tgtOldI;
        newSub->indexCount  = srcICount;
        tgtBuf->subset.push_back(newSub);
        tgtBuf->headerFrame.totalSubset = static_cast<int>(tgtBuf->subset.size());
        return static_cast<uint32_t>(tgtBuf->subset.size());
    }

    void MESH_MBM_DEBUG::removeAnimation(uint32_t index)
    {
        if (index >= static_cast<uint32_t>(this->infoAnimation.lsHeaderAnim.size()))
            return;
        delete this->infoAnimation.lsHeaderAnim[index];
        this->infoAnimation.lsHeaderAnim.erase(
            this->infoAnimation.lsHeaderAnim.begin() + static_cast<ptrdiff_t>(index));
    }
    
    void MESH_MBM_DEBUG::calculateUV()
    {
        headerMesh.totalFrames = static_cast<int>(this->buffer.size());
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(currentFrame)];
            const auto  sSub   = static_cast<uint32_t>(currentFrameBuffer->subset.size());
            VEC2                     vMin(FLT_MAX, FLT_MAX);
            VEC2                     vMax(-FLT_MAX, -FLT_MAX);
            const auto *             pPosition = reinterpret_cast<VEC3 *>(currentFrameBuffer->position);
            auto *                   pUv       = reinterpret_cast<VEC2 *>(currentFrameBuffer->uv);
            for (uint32_t indexSubset = 0; indexSubset < sSub; ++indexSubset)
            {
                const util::SUBSET_DEBUG *subset = currentFrameBuffer->subset[indexSubset];
                int                 countSubset  = subset->vertexStart + subset->vertexCount;
                countSubset -= (countSubset % 3);
                for (int i = subset->vertexStart; i < countSubset; ++i)
                {
                    const VEC3 *position = &pPosition[i];
                    if (position->x > vMax.x)
                        vMax.x = position->x;
                    if (position->y > vMax.y)
                        vMax.y = position->y;

                    if (position->x < vMin.x)
                        vMin.x = position->x;
                    if (position->y < vMin.y)
                        vMin.y = position->y;
                }
            }
            const float tmp[2] = {(vMax.x - vMin.x),(vMax.y - vMin.y)};
            const float width  = (tmp[0] == 0.0f ? 1.0f : tmp[0]);
            const float height = (tmp[1] == 0.0f ? 1.0f : tmp[1]);
            vMin.x = -vMin.x;
            vMin.y = -vMin.y;
            for (uint32_t indexSubset = 0; indexSubset < sSub; ++indexSubset)
            {
                const util::SUBSET_DEBUG *subset = currentFrameBuffer->subset[indexSubset];
                const int           countSubset  = subset->vertexStart + subset->vertexCount;
                for (int i = subset->vertexStart; i < countSubset; ++i)
                {
                    const VEC3 *position = &pPosition[i];
                    VEC2 *uv       = &pUv[i];
                    uv->x          = (position->x + vMin.x) / width;
                    uv->y          = 1.0f - ((position->y + vMin.y) / height);
                }
            }
        }
    }
    
    bool MESH_MBM_DEBUG::saveDebug(const char *fileOut, const bool recalculateNormal, const bool recalculateUV, char *errorOut,const int lenErrorOut)
    {
        if (this->buffer.size() == 0)
            return false;
        FILE *file = nullptr;
        strncpy(headerMain.name, MBM_HEADER_NAME_MBM,sizeof(headerMain.name)-1);
        headerMesh.totalFrames  = static_cast<int>(this->buffer.size());
        headerMain.version     = CURRENT_VERSION_MBM_HEADER;
        headerMain.reserved    = 0;
        headerMain.extraHeader = 0;
        headerMain.magic       = 0x010203ff;
        const char* typeApp = get_type_app_from_mesh_type(typeMe);
        if (typeApp)
        {
            strncpy(headerMain.typeApp, typeApp, sizeof(headerMain.typeApp)-1);
        }
        else
        {
            bool allstride2 = true;
            for (auto & i : this->buffer)
            {
                if (i->headerFrame.stride != 2)
                {
                    allstride2 = false;
                    break;
                }
            }
            if (allstride2)
            {
                typeMe = util::TYPE_MESH_SPRITE;
            }
            else
            {
                typeMe = util::TYPE_MESH_3D;
            }
            strncpy(headerMain.typeApp, get_type_app_from_mesh_type(typeMe), sizeof(headerMain.typeApp)-1);
        }
        if (errorOut)
        {
            if (!check(errorOut,lenErrorOut))
            {
                return log_util::onFailed(file,__FILE__, __LINE__, "error on check mesh to save.");
            }
        }
        else
        {
            char strError[255] = "";
            if (!check(strError,sizeof(strError)-1))
            {
                return log_util::onFailed(file,__FILE__, __LINE__, strError);
            }
        }
        if (recalculateNormal)
        {
            this->calculateNormals();
            this->headerMesh.hasNorText[0] = HAS_NOR_IN_FILE;
        }
        if (recalculateUV)
        {
            this->calculateUV();
            this->headerMesh.hasNorText[1] = HAS_TEX_EACH_FRAME;
        }
        {
            std::string which_mode;
            if(is_any_mode_valid(this->info_mode,which_mode) == false)
            {
                return log_util::onFailed(file,__FILE__, __LINE__, "Invalid mode [%s] for [%s]",which_mode.c_str(),fileOut);
            }
        }

        // 1 header MBM -------------------------------------------------------------------------------
        std::vector<std::string> ls_paths = this->getKnowPathsToExtraHeader();
        headerMain.extraHeader = static_cast<int>(ls_paths.size());
        file = util::openFile(fileOut, "wb");
        if (!file)
            return log_util::onFailed(file,__FILE__, __LINE__, "Failed to open file [%s]", fileOut);
        if (!util::writeHeaderV8(file, this->headerMain))
            return log_util::onFailed(file,__FILE__, __LINE__, "Failed to save file [%s]", fileOut);


        // 2 extra header MBM -------------------------------------------------------------------------------
        for (size_t i = 0; i < ls_paths.size(); i++)
        {
            util::EXTRA_HEADER extra;
            const std::string& path = ls_paths[i];
            extra.type = 1;
            extra.sizeExtraHeader = static_cast<int32_t>(path.size());
            if (!util::writeExtraHeaderV8(file, extra))
                return log_util::onFailed(file, __FILE__, __LINE__, "failed to save EXTRA_HEADER [%s]", fileOut);
            if (!util::addToFileBinary(fileOut, path.data(), path.size(), &file))
                return log_util::onFailed(file, __FILE__, __LINE__, "failed to save path for EXTRA_HEADER [%s]", fileOut);
        }

        if (!util::writeInfoDrawModeV8(file, this->info_mode))
            return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail INFO_DRAW_MODE [%s]", fileOut);

        int totalBounding = ((typeMe == util::TYPE_MESH_FONT) || (typeMe == util::TYPE_MESH_PARTICLE) || (typeMe == util::TYPE_MESH_TILE_MAP)) ? 1 : 0;
        totalBounding += static_cast<int>(this->infoPhysics.lsCube.size());
        totalBounding += static_cast<int>(this->infoPhysics.lsSphere.size());
        totalBounding += static_cast<int>(this->infoPhysics.lsCubeComplex.size());
        totalBounding += static_cast<int>(this->infoPhysics.lsTriangle.size());
        if (totalBounding == 0)
        {
            this->fillAtLeastOneBound();
            totalBounding = 1;
        }
        util::DETAIL_MESH detailHeader;
        detailHeader.totalBounding = totalBounding;
        detailHeader.type          = 'P'; //Physics
        if (!util::writeDetailMeshV8(file, detailHeader))
            return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail header bounding box!!");

        if (this->infoPhysics.lsCube.size())
        {
            util::DETAIL_MESH detail;
            detail.totalBounding = static_cast<int>(this->infoPhysics.lsCube.size());
            detail.type          = 1;
            if (!util::writeDetailMeshV8(file, detail))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail bounding box!!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                CUBE *cube = this->infoPhysics.lsCube[static_cast<std::vector<CUBE*>::size_type>(i)];
                if (!util::writeCubeV8(file, *cube))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save bounding box!");
            }
        }
        if (this->infoPhysics.lsSphere.size())
        {
            util::DETAIL_MESH detail;
            detail.totalBounding = static_cast<int>(this->infoPhysics.lsSphere.size());
            detail.type          = 2;
            if (!util::writeDetailMeshV8(file, detail))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail bounding box!!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                SPHERE *sphere = this->infoPhysics.lsSphere[static_cast<std::vector<SPHERE*>::size_type>(i)];
                if (!util::writeSphereV8(file, *sphere))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save bounding box!");
            }
        }
        if (this->infoPhysics.lsCubeComplex.size())
        {
            util::DETAIL_MESH detail;
            detail.totalBounding = static_cast<int>(this->infoPhysics.lsCubeComplex.size());
            detail.type          = 3;
            if (!util::writeDetailMeshV8(file, detail))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail bounding box!!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                CUBE_COMPLEX *complex = this->infoPhysics.lsCubeComplex[static_cast<std::vector<CUBE_COMPLEX*>::size_type>(i)];
                if (!util::writeCubeComplexV8(file, *complex))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save bounding box!");
            }
        }
        if (this->infoPhysics.lsTriangle.size())
        {
            util::DETAIL_MESH detail;
            detail.totalBounding = static_cast<int>(this->infoPhysics.lsTriangle.size());
            detail.type          = 4;
            if (!util::writeDetailMeshV8(file, detail))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail bounding box!!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                TRIANGLE *triangle = this->infoPhysics.lsTriangle[static_cast<std::vector<TRIANGLE*>::size_type>(i)];
                if (!util::writeTriangleV8(file, *triangle))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save bounding box!");
            }
        }
        if (typeMe == util::TYPE_MESH_FONT)
        {
            util::DETAIL_MESH detail;
            auto *infoFont = static_cast<INFO_BOUND_FONT *>(this->extraInfo);
            if(infoFont == nullptr)
              return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail font, infoFont is null!");
            detail.totalBounding = 1;
            detail.type          = 5;
            if (!util::writeDetailMeshV8(file, detail))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail font!");
            util::DETAIL_HEADER_FONT headerFont;
            headerFont.totalDetailFont = 0;
            for (int i = 0; i < 255; ++i)
            {
                LETTER *letter = &infoFont->letter[static_cast<std::vector<LETTER*>::size_type>(i)];
                if (letter->detail)
                {
                    headerFont.totalDetailFont++;
                }
            }
            headerFont.sizeNameFonte   = static_cast<uint16_t>(infoFont->fontName.size() + 1);
            headerFont.spaceXCharacter = static_cast<char>(infoFont->spaceXCharacter);
            headerFont.spaceYCharacter = static_cast<char>(infoFont->spaceYCharacter);
            headerFont.heightLetter    = static_cast<uint16_t>(infoFont->heightLetter);
            if (!util::writeDetailHeaderFontV8(file, headerFont))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to add header DETAIL_HEADER_FONT!!");

            if (!util::addToFileBinary(fileOut,infoFont->fontName.c_str(), infoFont->fontName.size() + 1, &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save font's name!!");

            int totalLetters = 0;
            for (int i = 0; i < 255; ++i)
            {
                LETTER *letter = &infoFont->letter[static_cast<std::vector<LETTER*>::size_type>(i)];
                if (letter->detail)
                {
                    util::DETAIL_LETTER *detailFont = letter->detail;
                    if (!util::writeDetailLetterV8(file, *detailFont))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to include detail DETAIL_LETTER!!");
                    totalLetters++;
                }
            }
            if (totalLetters != this->headerMesh.totalFrames)
                return log_util::onFailed(
                    file, __FILE__,__LINE__, "failed to include detail DETAIL_LETTER!!\ntotal of frames different of letters!!!");
        }
        else if (typeMe == util::TYPE_MESH_PARTICLE)
        {
            util::DETAIL_MESH detail;
            const auto* lsParticleInfo = static_cast<const std::vector<util::STAGE_PARTICLE*>*>(this->extraInfo);
            if(lsParticleInfo == nullptr)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to include detail particle!!");
            detail.totalBounding = static_cast<int>(lsParticleInfo->size());
            detail.type = 6;
            if (!util::writeDetailMeshV8(file, detail))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail particle!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                util::STAGE_PARTICLE* stage = lsParticleInfo->at(static_cast<std::vector<util::STAGE_PARTICLE*>::size_type>(i));
                if (!util::writeStageParticleV8(file, *stage))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to include detail particle!!");
            }
        }
        else if (typeMe == util::TYPE_MESH_TILE_MAP)
        {
            util::DETAIL_MESH detail;
            
            auto* infoTileMap = static_cast<util::BTILE_INFO*>(this->extraInfo);
            if(infoTileMap == nullptr)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile!");
            detail.totalBounding = infoTileMap->map.layerCount;
            detail.type = 7;
            if (!util::writeDetailMeshV8(file, detail))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile!");

            if (!util::writeBtileHeaderMapV8(file, infoTileMap->map))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile!");

            if (!util::writeBtileBrickInfoArrayV8(file, infoTileMap->infoBrickEditor, infoTileMap->map.countRawTiles))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to include brick editor info tile!!");

            const uint32_t tileCount = infoTileMap->map.count_height_tile * infoTileMap->map.count_width_tile;
            
            for (uint32_t  i = 0; i < infoTileMap->map.layerCount; ++i)
            {
                const util::BTILE_INDEX_TILE* lsIndexTiles = infoTileMap->layers[i].lsIndexTiles;
                
                if (!util::writeFloat3ArrayV8(file, infoTileMap->layers[i].offset))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to include offset of layer detail tile!!");

                if (!util::writeBtileIndexTileArrayV8(file, lsIndexTiles, tileCount))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to include index detail tile!!");
            }

            util::BTILE_DETAIL_HEADER detailObjsProperty;
            detailObjsProperty.totalObj			= static_cast<uint32_t>(infoTileMap->lsObj.size());
            detailObjsProperty.totalProperties	= static_cast<uint32_t>(infoTileMap->lsProperty.size());
            if (!util::writeBtileDetailHeaderV8(file, detailObjsProperty))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile header!");

            for(uint32_t i= 0 ; i < detailObjsProperty.totalObj; ++i)
            {
                util::BTILE_OBJ* obj = infoTileMap->lsObj[i];
                util::BTILE_OBJ_HEADER objHeader(obj);
                if (!util::writeBtileObjHeaderV8(file, objHeader))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile header!");
                if(obj->name.size() > 0)
                {
                    if (!util::addToFileBinary(fileOut, obj->name.c_str(), obj->name.size(), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save name at detail tile header!");
                }
                for(auto & lsPoint : obj->lsPoints)
                {
                    if (!util::writeVec2V8(file, *lsPoint))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile header!");
                }
            }

            for(uint32_t i= 0 ; i < detailObjsProperty.totalProperties; ++i)
            {
                util::BTILE_PROPERTY* property = infoTileMap->lsProperty[i];
                util::BTILE_PROPERTY_HEADER propertyHeader(property);
                if (!util::writeBtilePropertyHeaderV8(file, propertyHeader))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail property tile header!");
                if(property->name.size() > 0)
                {
                    if (!util::addToFileBinary(fileOut, property->name.c_str(), property->name.size(), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save property name at detail tile header!");
                }
                if(property->value.size() > 0)
                {
                    if (!util::addToFileBinary(fileOut, property->value.c_str(), property->value.size(), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save property value at detail tile header!");
                }
                if(property->owner.size() > 0)
                {
                    if (!util::addToFileBinary(fileOut, property->owner.c_str(), property->owner.size(), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save property owner at detail tile header!");
                }
            }
        }
        headerMesh.totalAnimation = static_cast<int>(this->infoAnimation.lsHeaderAnim.size());
        if (headerMesh.totalAnimation == 0)
        {
            auto infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            this->infoAnimation.lsHeaderAnim.push_back(infoHead);
            headerMesh.totalAnimation = static_cast<int>(this->infoAnimation.lsHeaderAnim.size());
            infoHead->headerAnim     = new util::HEADER_ANIMATION();
            strncpy(infoHead->headerAnim->nameAnimation, "default",sizeof(infoHead->headerAnim->nameAnimation)-1);
        }

        // 3 headerMesh MBM -------------------------------------------------------------------------------
        if (!util::writeHeaderMeshV8(file, headerMesh))
            return log_util::onFailed(file,__FILE__, __LINE__, "failed to save header of file!!");

        // 4 header anim -- Todas as animações -----------------------------------------------------------
        if (!this->saveAnimationHeaders(fileOut, &file))
            return false;

        // Loop principal atraves de todos os frames deste arquivo -----------------------------------------------
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(currentFrame)];
            auto             totalSubset        = static_cast<uint32_t>(currentFrameBuffer->subset.size());
            // 5 Cada header Frame
            // --------------------------------------------------------------------------------------------------
            // Grava cada estrutura de frame para cada loop indicando os atributos do objeto
            // ----------------------------------------
            util::HEADER_FRAME *headerFrame = &currentFrameBuffer->headerFrame;

            const bool isIndexBuffer = currentFrameBuffer->indexBuffer != nullptr;

            // Verfifica se utiliza index buffer ou vertex buffer
            // -------------------------------------------------------------------
            if (!isIndexBuffer)
            {
                strncpy(headerFrame->typeBuffer, "VB",sizeof(headerFrame->typeBuffer)-1); // Opta por vertex buffer
                headerFrame->sizeIndexBuffer = 0;
            }
            else
            {
                strncpy(headerFrame->typeBuffer, "IB",sizeof(headerFrame->typeBuffer)-1); // Opta por index buffer
                uint32_t sIndex = 0;
                for (uint32_t i = 0; i < totalSubset; ++i)
                {
                    sIndex += static_cast<uint32_t>(currentFrameBuffer->subset[i]->indexCount);
                }
                headerFrame->sizeIndexBuffer = static_cast<int>(sIndex);
            }
            uint32_t sVertex = 0;
            for (uint32_t i = 0; i < totalSubset; ++i)
            {
                sVertex += static_cast<uint32_t>(currentFrameBuffer->subset[i]->vertexCount);
            }
            headerFrame->totalSubset      = static_cast<int>(totalSubset);
            headerFrame->sizeVertexBuffer = static_cast<int>(sVertex);
            if (!util::writeHeaderFrameV8(file, *headerFrame))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to add header of frame!");
            // 6 Todos os headers subset deste frame
            // -------------------------------------------------------------------------------
            for (int i = 0; i < headerFrame->totalSubset; ++i)
            {
                util::HEADER_DESC_SUBSET headerDescSubset;
                util::SUBSET_DEBUG *     pSubset = currentFrameBuffer->subset[static_cast<std::vector<util::SUBSET_DEBUG *>::size_type>(i)];
                if (!fillTextureReferenceForHeader(file, pSubset->texture, typeMe, headerDescSubset.nameTexture))
                    return false;
                headerDescSubset.vertexStart = pSubset->vertexStart;
                headerDescSubset.indexStart  = pSubset->indexStart;
                headerDescSubset.vertexCount = pSubset->vertexCount;
                headerDescSubset.indexCount  = pSubset->indexCount;
                headerDescSubset.hasAlphaColor  = 1;
                std::vector<util::MATERIAL_TEXTURE_SLOT_HEADER> materialSlotHeaders;
                if (this->headerMain.version >= MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER)
                {
                    materialSlotHeaders.reserve(pSubset->materialTextureSlots.size());
                    for (const auto &slot : pSubset->materialTextureSlots)
                    {
                        if (slot.texture.empty())
                            continue;
                        util::MATERIAL_TEXTURE_SLOT_HEADER slotHeader;
                        slotHeader.type = slot.type;
                        if (!fillTextureReferenceForHeader(file, slot.texture, typeMe, slotHeader.nameTexture))
                            return false;
                        materialSlotHeaders.push_back(slotHeader);
                    }
                    headerDescSubset.materialTextureSlotCount =
                        static_cast<uint16_t>(materialSlotHeaders.size());
                }

                if (!writeHeaderDescSubsetVersioned(file, headerDescSubset, this->headerMain.version))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to add header of subset!");
                for (const auto &slotHeader : materialSlotHeaders)
                {
                    if (!util::writeMaterialTextureSlotHeaderV9(file, slotHeader))
                        return log_util::onFailed(file, __FILE__, __LINE__, "failed to add material texture slot header!");
                }
            }

            // 6 index buffer se houver -----------------------------------------------------------------------------
            if (headerFrame->sizeIndexBuffer && strcmp(headerFrame->typeBuffer, "IB") == 0)
            {
                if (!util::writeU16ArrayV8(file,
                                           currentFrameBuffer->indexBuffer,
                                           static_cast<uint32_t>(headerFrame->sizeIndexBuffer)))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to add index buffer!!! !!");
            }
            // 6 Vertex buffer -----------------------------------------------------------------------------
            if (headerFrame->sizeVertexBuffer)
            {
                if (headerFrame->stride == 2)
                {
                    auto pPosition = new VEC2[headerFrame->sizeVertexBuffer];
                    auto *position  = reinterpret_cast<VEC3 *>(currentFrameBuffer->position);
                    for (int i = 0; i < headerFrame->sizeVertexBuffer; ++i)
                    {
                        pPosition[i].x = position[i].x;
                        pPosition[i].y = position[i].y;
                    }
                    if (!util::writeVec2ArrayV8(file, pPosition, static_cast<uint32_t>(headerFrame->sizeVertexBuffer)))
                    {
                        delete[] pPosition;
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                    }
                    delete[] pPosition;
                }
                else
                {
                    if (!util::writeVec3ArrayV8(file,
                                                reinterpret_cast<VEC3*>(currentFrameBuffer->position),
                                                static_cast<uint32_t>(headerFrame->sizeVertexBuffer)))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                }
                if (headerMesh.hasNorText[0] != HAS_NOR_NO)
                {
                    if (!util::writeVec3ArrayV8(file,
                                                reinterpret_cast<VEC3*>(currentFrameBuffer->normal),
                                                static_cast<uint32_t>(headerFrame->sizeVertexBuffer)))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                }
                if (headerMesh.hasNorText[1] == HAS_TEX_EACH_FRAME)
                {
                    if (!util::addToFileBinary(fileOut, currentFrameBuffer->uv,
                                               static_cast<size_t>(headerFrame->sizeVertexBuffer) * sizeof(VEC2), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                }
                else if (currentFrame == 0 && headerMesh.hasNorText[1] == HAS_TEX_FIRST_FRAME)
                {
                    if (!util::addToFileBinary(fileOut, currentFrameBuffer->uv,
                                               static_cast<size_t>(headerFrame->sizeVertexBuffer) * sizeof(VEC2), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                }
            }
            else
                return log_util::onFailed(file,__FILE__, __LINE__, "total of vertex is zero");
        }
        if (file)
            fclose(file);
        file = nullptr;
        return this->compressFile(fileOut,errorOut,lenErrorOut);
    }

    bool MESH_MBM_DEBUG::loadDebug(const char *fileNamePath)
    {
        return this->loadDebugImpl(fileNamePath, true);
    }

    bool MESH_MBM_DEBUG::loadDebugImpl(const char *fileNamePath, const bool allowLegacyDispatch)
    {
        this->release();
        FILE *fp = nullptr;
        if (!open_decompressed_mesh_file(fileNamePath, fp, "failed to open file [%s]"))
            return false;
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
        deprecated_mbm::INFO_SPRITE deprectedInfoSprite; // version <=SPRITE_INFO_VERSION_MBM_HEADER
#endif
        fileName = fileNamePath;
        // step 1: Verificação do header  MBM principal
        // -------------------------------------------------------------------------------
        if (!read_main_header_and_type(fp, fileNamePath, headerMain, typeMe))
            return false;
        if (allowLegacyDispatch && headerMain.version < STRONG_TYPES_VERSION_MBM_HEADER)
        {
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
            if (fp)
                fclose(fp);
            fp = nullptr;
            return this->loadDebugLegacyCompat(fileNamePath);
#else
            return log_util::onFailed(fp,__FILE__, __LINE__, "legacy mesh version [%d] disabled at compile time. Rebuild with MBM_ENABLE_MESH_LEGACY_V7", headerMain.version);
#endif
        }

        if (headerMain.version >= EXTRA_MBM_HEADER_PATH_TEXTURE)
        {
            if (!read_extra_headers(fp, fileNamePath, headerMain.extraHeader, true))
                return false;
        }
        if (!read_info_mode_if_needed(fp, fileNamePath, headerMain.version, info_mode, false))
            return false;
        if(typeMe == util::TYPE_MESH_TILE_MAP)
        {
            mbm::TEXTURE::EnablePixelPerfectTexture(true);
        }
        else
        {
            mbm::TEXTURE::EnablePixelPerfectTexture(false);
        }
        // step 2: --------------------------------------------------------------------------------------------------
        if (headerMain.version >= DETAIL_MESH_VERSION_MBM_HEADER)
        {
            if (!read_detail_mesh_section(fp,
                                          fileNamePath,
                                          headerMain,
                                          this->infoPhysics,
                                          this->extraInfo,
                                          [this](FILE *file, const char *name, const int totalBounding, const int fileVersion)
                                          {
                                              return this->readDebugTriangleDetailCompat(file, name, totalBounding, fileVersion);
                                          }))
                return false;
        }
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
        else
        {
            if (!this->loadDebugLegacyDetailStep(fp, fileNamePath, deprectedInfoSprite))
                return false;
        }
#else
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "Imcompatible version [%d]", headerMain.version);
        }
#endif
        // 3 headerMesh MBM -------------------------------------------------------------------------------
        if (!util::readHeaderMeshV8(fp, headerMesh))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read HEADER_MESH [%s]", fileNamePath);
        if (headerMesh.totalAnimation == 0)
            return log_util::onFailed(fp,__FILE__, __LINE__, "there is no animation [%s]", fileNamePath);

        // 4 header anim -- Todas as animações -----------------------------------------------------------
        if (headerMain.version < STRONG_TYPES_VERSION_MBM_HEADER)
        {
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
            if (!this->loadDebugLegacyAnimationStep(fp, fileNamePath, deprectedInfoSprite))
                return false;
#else
            return log_util::onFailed(fp,__FILE__, __LINE__, "unexpected version [%s] Version [%d]", fileNamePath, headerMain.version);
#endif
        }
        else if (!this->fillAnimation_2(fileNamePath, fp))
        {
            return false;
        }
        // Loop principal atraves de todos os frames deste arquivo -----------------------------------------------
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            auto pBuffer = new util::BUFFER_MESH_DEBUG();
            this->buffer.push_back(pBuffer);
            // 5 Sequencia lógica dos frames --------------------------------------------------------------------------
            // Cada header Frame
            // --------------------------------------------------------------------------------------------------
            util::HEADER_FRAME *headerFrame = &pBuffer->headerFrame;
            if (!read_frame_headers_and_subsets(
                    fp,
                    fileNamePath,
                    headerMain.version,
                    *headerFrame,
                    [](util::HEADER_FRAME &) -> bool
                    {
                        return true;
                    },
                    [&](const util::HEADER_FRAME &, const int, util::HEADER_DESC_SUBSET &headerDescSubset) -> bool
                    {
                        auto pSubset = new util::SUBSET_DEBUG();
                        pBuffer->subset.push_back(pSubset);
                        pSubset->vertexStart = headerDescSubset.vertexStart;
                        pSubset->indexStart  = headerDescSubset.indexStart;
                        pSubset->indexCount  = headerDescSubset.indexCount;
                        pSubset->vertexCount = headerDescSubset.vertexCount;
                        if (strcmp(headerDescSubset.nameTexture, "default") != 0)
                        {
                            char *pch = strchr(headerDescSubset.nameTexture, '#');
                            if (pch && pch[0] == '#' && pch[1] == 'u')
                            {
                                pch[0] = 0;
                                util::HEADER_IMG headerImg;
                                headerImg.lenght = 0;
                                if (!util::readHeaderImgV8(fp, headerImg))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header image [%s]", fileNamePath);
                                if (headerImg.lenght == 0)
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header image [%s]", fileNamePath);
                                auto data = new uint8_t[headerImg.lenght];
                                if (!fread(data, static_cast<size_t>(headerImg.lenght), 1, fp))
                                {
                                    delete [] data;
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image [%s]", fileNamePath);
                                }
                                uint32_t sizeOfImage = 0;
                                if (headerImg.channel != 4 && headerImg.channel != 3 && headerImg.channel != 0)
                                {
                                    delete [] data;
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image! Channel != [3 || 4] [%s]",
                                                             fileNamePath);
                                }
                                headerImg.channel = headerImg.channel == 4 ? 4 : 3;
                                switch (headerImg.depth)
                                {
                                    case 3:
                                    {
                                        sizeOfImage = 3 * headerImg.channel * headerImg.width * headerImg.height;
                                        while (sizeOfImage % 8)
                                        {
                                            sizeOfImage++;
                                        }
                                        sizeOfImage = sizeOfImage / 8;
                                    }
                                    break;
                                    case 4:
                                    {

                                        sizeOfImage = 4 * headerImg.channel * headerImg.width * headerImg.height;
                                        while (sizeOfImage % 8)
                                        {
                                            sizeOfImage++;
                                        }
                                        sizeOfImage = sizeOfImage / 8;
                                    }
                                    break;
                                    case 8: { sizeOfImage = headerImg.width * headerImg.height * headerImg.channel;}
                                    break;
                                    default: {delete [] data; return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image [%s]", fileNamePath);}
                                }
                                MINIZ miniz;
                                if (miniz.decompressStream(data, headerImg.lenght, sizeOfImage))
                                {
                                    delete[] data;
                                    pSubset->texture = headerDescSubset.nameTexture;
                                    if (!pSubset->texture.size())
                                    {
#if defined _DEBUG
                                        PRINT_IF_DEBUG( "error on create texture: %s \n Linha %d",
                                                     fileName.c_str());
#endif
                                    }
                                }
                                else
                                {
                                    delete[] data;
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to uncompress file [%s]", fileNamePath);
                                }
                            }
                            else
                            {
                                pch = strchr(headerDescSubset.nameTexture, '#');
                                if (pch && pch[0] == '#' && pch[1] == 'M') // Material apenas .. cor como string
                                {
                                    pch = &pch[2];
                                    //util::MATERIAL mat;
                                    //memset(&mat, 0, sizeof(mat));
                                    std::vector<std::string> result;
                                    util::split(result, pch, '|');

                                    if (result.size() == 5)
                                    {
                                        COLOR colorAculm(0.0f, 0.0f, 0.0f, 0.0f);
                                        int        totalSum = 0;
                                        for (auto & n : result)
                                        {
                                            const char *strTemp   = n.c_str();
                                            //char        letter    = strTemp[0];
                                            const char *strNumber = &strTemp[1];
                                            COLOR  color     = static_cast<uint32_t>(strtol(strNumber, nullptr, 16));
                                            if (color.r > 0.0f || color.g > 0.0f || color.b > 0.0f)
                                            {
                                                colorAculm.r += color.r;
                                                colorAculm.g += color.g;
                                                colorAculm.b += color.b;
                                                totalSum++;
                                            }
                                            //switch (letter)
                                            //{
                                            //    case 'A': { mat.Ambient = color;
                                            //    }
                                            //    break;
                                            //    case 'D': { mat.Diffuse = color;
                                            //    }
                                            //    break;
                                            //    case 'E': { mat.Emissive = color;
                                            //    }
                                            //    break;
                                            //    case 'S': { mat.Specular = color;
                                            //    }
                                            //    break;
                                            //    case 'P': { mat.Power = static_cast<float>(atof(strNumber));}
                                            //    break;
                                            //    default: break;
                                            //}
                                        }
                                        if (totalSum)
                                        { // potências de 2 (2,4,8,16,32,64,128,256, 512,1024)...
                                            /*colorAculm.r  =   colorAculm.r / (float)totalSum;
                                            colorAculm.g    =   colorAculm.g / (float)totalSum;
                                            colorAculm.b    =   colorAculm.b / (float)totalSum;
                                            uint8_t dataARGB[16];
                                            uint32_t dwR = colorAculm.r >= 1.0f ? 0xff : colorAculm.r <= 0.0f ? 0x00 :
                                            (uint32_t) (colorAculm.r * 255.0f + 0.5f);
                                            uint32_t dwG = colorAculm.g >= 1.0f ? 0xff : colorAculm.g <= 0.0f ? 0x00 :
                                            (uint32_t) (colorAculm.g * 255.0f + 0.5f);
                                            uint32_t dwB = colorAculm.b >= 1.0f ? 0xff : colorAculm.b <= 0.0f ? 0x00 :
                                            (uint32_t) (colorAculm.b * 255.0f + 0.5f);
                                            for(int pixel=0; pixel< 12; pixel+=4)
                                            {
                                                dataARGB[pixel]     =   (uint8_t)255;
                                                dataARGB[pixel+1]   =   (uint8_t)dwR;
                                                dataARGB[pixel+2]   =   (uint8_t)dwG;
                                                dataARGB[pixel+3]   =   (uint8_t)dwB;
                                            }*/
                                            pSubset->texture = headerDescSubset.nameTexture;
                                        }
                                    }
                                    else
                                    {
                                        pSubset->texture.clear();
                                    }
                                }
                                else if (pch && pch[0] == '#')//solid color  as texture
                                {
                                    const char * nickName = pch;
                                    mbm::TEXTURE_MANAGER * man = mbm::TEXTURE_MANAGER::getInstance();
                                    if(man->existTexture(nickName) == false)
                                    {
                                        COLOR color;
                                        const char* sColor = &pch[1];
                                        int len = static_cast<int>(strlen(sColor));
                                        if (len == 8)
                                        {
                                            char alpha[3] = {0,0,0};
                                            alpha[0] = *sColor;
                                            sColor++;
                                            alpha[1] = *sColor;
                                            sColor++;
                                            const int n = static_cast<int>(strtol(sColor,nullptr, 16));
                                            color = COLOR(n);
                                            color.a = strtol(alpha, nullptr, 16) * 1.0f / 255.0f;
                                        }
                                        else if (len == 6)
                                        {
                                            const int n = static_cast<int>(strtol(sColor, nullptr, 16));
                                            color = COLOR(n);
                                            color.a = 1.0f;
                                        }
                                        const uint32_t width = 4;
                                        const uint32_t height = 4;
                                        uint8_t pixel[4 * 4* 3];
                                        uint8_t r = 0;
                                        uint8_t g = 0;
                                        uint8_t b = 0;
                                        color.get(&r,&g,&b);
                                        for (uint32_t j = 0; j < 4 * 4 * 3; j += 3)
                                        {
                                            pixel[j] = r;
                                            pixel[j+1] = g;
                                            pixel[j+2] = b;
                                        }

                                        TEXTURE *solidTexture = man->load(width, height, &pixel[0], nickName, 8,3);
                                        if(solidTexture == nullptr)
                                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed on create solid texture [%s][%s]",nickName, fileNamePath);
                                    }
                                    pSubset->texture = headerDescSubset.nameTexture;
                                }
                                else
                                {
                                    pSubset->texture = headerDescSubset.nameTexture;
                                }
                            }
                        }
                        if (!readMaterialTextureSlotsDebug(fp,
                                                           fileNamePath,
                                                           headerMain.version,
                                                           headerDescSubset.materialTextureSlotCount,
                                                           *pSubset))
                            return false;
                        return true;
                    }))
                return false;

            if (!read_frame_geometry(
                    fp,
                    fileNamePath,
                    *headerFrame,
                    headerMesh.hasNorText,
                    this->headerMain.version,
                    [this](FILE *file,
                           const int sizeVertexBuffer,
                           VEC3 **positionOut,
                           VEC3 **normalOut,
                           VEC2 **textureOut,
                           int16_t hasNorText[2],
                           uint16_t *indexArray,
                           const int sizeArrayIndex,
                           const int stride,
                           const int fileVersion) -> bool
                    {
                        return this->loadFromSeparatedBuffers(file,
                                                     sizeVertexBuffer,
                                                     positionOut,
                                                     normalOut,
                                                     textureOut,
                                                     hasNorText,
                                                     indexArray,
                                                     sizeArrayIndex,
                                                     stride,
                                                     fileVersion);
                    },
                    [&](VEC3 *&pPosition, VEC3 *&pNormal, VEC2 *&pTexture, uint16_t *&indexArray) -> bool
                    {
                        pBuffer->position    = reinterpret_cast<float *>(pPosition);
                        pBuffer->normal      = reinterpret_cast<float *>(pNormal);
                        pBuffer->uv          = reinterpret_cast<float *>(pTexture);
                        pBuffer->indexBuffer = indexArray;
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
                        this->fillDebugLegacyPhysicsIfNeeded(headerMain.version,
                                                             deprectedInfoSprite,
                                                             pPosition,
                                                             static_cast<uint32_t>(currentFrame),
                                                             pBuffer->subset);
#endif
                        pPosition = nullptr;
                        pNormal   = nullptr;
                        pTexture  = nullptr;
                        indexArray = nullptr;
                        return true;
                    },
                    [&](VEC3 *&pPosition, VEC3 *&pNormal, VEC2 *&pTexture) -> bool
                    {
                        pBuffer->position = reinterpret_cast<float *>(pPosition);
                        pBuffer->normal   = reinterpret_cast<float *>(pNormal);
                        pBuffer->uv       = reinterpret_cast<float *>(pTexture);
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
                        this->fillDebugLegacyPhysicsIfNeeded(headerMain.version,
                                                             deprectedInfoSprite,
                                                             pPosition,
                                                             static_cast<uint32_t>(currentFrame),
                                                             pBuffer->subset);
#endif
                        pPosition = nullptr;
                        pNormal   = nullptr;
                        pTexture  = nullptr;
                        return true;
                    }))
                return false;
        }
        fclose(fp);
        fp             = nullptr;
        positionOffset = VEC3(headerMesh.posX, headerMesh.posY, headerMesh.posZ);
        angleDefault   = VEC3(headerMesh.angleX, headerMesh.angleY, headerMesh.angleZ);
        remove(util::getDecompressModelFileName());

        this->sizeCoordTexFrame_0 = 0;
        if (this->coordTexFrame_0)
            delete[] this->coordTexFrame_0;
        this->coordTexFrame_0 = nullptr;
        return true;
    }
    
    bool MESH_MBM_DEBUG::check(char *error,const int lenError)
    {
        if (this->buffer.size() == 0)
        {
            if (error)
                strncpy(error, "Empty buffer",lenError);
            return false;
        }
        for (std::vector<util::BUFFER_MESH_DEBUG *>::size_type i = 0; i < this->buffer.size(); ++i)
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[i];
            const std::vector<util::SUBSET_DEBUG *>::size_type s = bufferCurrent->subset.size();
            if (s == 0)
            {
                if (error)
                    snprintf(error,lenError, "Empty subset at frame [%d]",static_cast<int>(i));
                return false;
            }
            for (std::vector<util::SUBSET_DEBUG *>::size_type j = 0; j < s; ++j)
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[j];
                if (pTmpSubset->vertexCount == 0)
                {
                    if (error)
                        snprintf(error,lenError, "vertex count [0] in subset [%d] at frame [%d]", static_cast<int>(j),static_cast<int>(i));
                    return false;
                }
                if (pTmpSubset->indexCount > 0 && bufferCurrent->indexBuffer == nullptr)
                {
                    pTmpSubset->indexCount = 0;
                }
                if (bufferCurrent->indexBuffer)
                {
                    if (pTmpSubset->indexCount == 0)
                    {
                        if (error)
                            snprintf(error,lenError,"there is index in buffer but 'index count' has [0] in subset [%d] at frame [%d]", static_cast<int>(j),static_cast<int>(i));
                        return false;
                    }
                }
            }
        }

        for (uint32_t i = 0; i < this->buffer.size(); ++i)
        {
            int                      iTotalVertex  = 0;
            int                      iTotalIndex   = 0;
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[i];
            const std::vector<util::BUFFER_MESH_DEBUG *>::size_type s = bufferCurrent->subset.size();
            for (std::vector<util::BUFFER_MESH_DEBUG *>::size_type j = 0; j < s; ++j)
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[j];
                iTotalVertex += pTmpSubset->vertexCount;
                iTotalIndex += pTmpSubset->indexCount;
            }
            if (bufferCurrent->indexBuffer)
            {
                int rest = iTotalIndex % 3;
                if (rest)
                {
                    if((iTotalIndex - rest) >=3)
                    {
                        util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[s-1];
                        pTmpSubset->indexCount -= rest;
                        PRINT_IF_DEBUG("index buffer must be dividible by 3 (mode triangle list indexed)\nindex total [%d]\nDoing work around to work",iTotalIndex);
                    }
                    else
                    {
                        if (error)
                        snprintf(error, lenError, "index buffer must be dividible by 3 (mode triangle list indexed)\nindex total [%d]",iTotalIndex);
                        return false;
                    }
                }
            }
            else
            {
                int rest = iTotalVertex % 3;
                if (rest)
                {
                    if((iTotalVertex - rest) >=3)
                    {
                        util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[s-1];
                        pTmpSubset->vertexCount-= rest;
                        PRINT_IF_DEBUG("vertex buffer must be dividible by 3 (mode triangle list indexed)\nvertex total [%d]\n Doing work around to work",iTotalVertex);
                    }
                    else
                    {
                        if (error)
                            snprintf(error, lenError, "vertex buffer must be dividible by 3 (mode triangle list indexed)\nvertex total [%d]",iTotalVertex);
                        return false;
                    }
                }
            }
            for (uint32_t j = 0; j < s; ++j)
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[j];
                if ((pTmpSubset->vertexStart + pTmpSubset->vertexCount) > iTotalVertex)
                {
                    if (error)
                        snprintf(error, lenError, "vertex start [%d] + vertex count [%d] = [%d] > total vertex [%d] in subset [%u] "
                                       "at frame [%u]",
                                pTmpSubset->vertexStart, pTmpSubset->vertexCount,
                                pTmpSubset->vertexStart + pTmpSubset->vertexCount, iTotalVertex, j, i);
                    return false;
                }
                if ((pTmpSubset->indexStart + pTmpSubset->indexCount) > iTotalIndex)
                {
                    if (error)
                        snprintf(
                            error,
                            lenError,
                            "index start [%d] + index count [%d] = [%d] > total index [%d] in subset [%u] at frame [%u]",
                            pTmpSubset->indexStart, pTmpSubset->indexCount,
                            pTmpSubset->indexStart + pTmpSubset->indexCount, iTotalIndex, j, i);
                    return false;
                }
            }
            if (bufferCurrent->indexBuffer)
            {
                for (uint32_t j = 0; j < s; ++j)
                {
                    util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[j];
                    if (pTmpSubset->indexCount == 0)
                    {
                        if (error)
                            snprintf(error, lenError, "index length is [0] in subset [%u] at frame [%u]", j, i);
                        return false;
                    }
                    const int indexStart = pTmpSubset->indexStart;
                    const int indexEnd   = pTmpSubset->indexStart + pTmpSubset->indexCount;
                    for (int k = indexStart; k < indexEnd; ++k)
                    {
                        uint16_t index = bufferCurrent->indexBuffer[k];
                        if (index > iTotalVertex)
                        {
                            if (error)
                                snprintf(error, lenError, "index [%d] value [%u] invalid in subset [%u] at frame [%u]", k, index, j,
                                        i);
                            return false;
                        }
                    }
                }
            }
        }
        return true;
    }
    
    void MESH_MBM_DEBUG::centralizeFrame(const int indexFrame, const int indexSubset)
    {
        if (indexFrame < 0) //-1
        {
            for (uint32_t i = 0; i < this->buffer.size(); ++i)
            {
                centralizeFrame(static_cast<int>(i), indexSubset);
            }
        }
        else if (indexFrame < static_cast<int>(this->buffer.size()))
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG*>::size_type>(indexFrame)];
            auto *                   pPosition     = reinterpret_cast<VEC3 *>(bufferCurrent->position);
            const auto       s             = static_cast<uint32_t>(bufferCurrent->subset.size());
            VEC3                     maxSize(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            VEC3                     minSize(FLT_MAX, FLT_MAX, FLT_MAX);
            if (indexSubset < 0)
            {
                for (uint32_t i = 0; i < s; ++i)
                {
                    const util::SUBSET_DEBUG * pTmpSubset = bufferCurrent->subset[static_cast<std::vector<util::SUBSET_DEBUG *>::size_type>(i)];
                    const auto        n          = static_cast<uint32_t>(pTmpSubset->vertexStart + pTmpSubset->vertexCount);
                    for (auto j = static_cast<uint32_t>(pTmpSubset->vertexStart); j < n; ++j)
                    {
                        VEC3 *pos = &pPosition[j];
                        if (pos->x < minSize.x)
                            minSize.x = pos->x;
                        if (pos->y < minSize.y)
                            minSize.y = pos->y;
                        if (pos->z < minSize.z)
                            minSize.z = pos->z;

                        if (pos->x > maxSize.x)
                            maxSize.x = pos->x;
                        if (pos->y > maxSize.y)
                            maxSize.y = pos->y;
                        if (pos->z > maxSize.z)
                            maxSize.z = pos->z;
                    }
                }
            }
            else if (indexSubset < static_cast<int>(s))
            {
                const util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[static_cast<std::vector<util::SUBSET_DEBUG *>::size_type>(indexSubset)];
                const auto        n          = static_cast<uint32_t>(pTmpSubset->vertexStart + pTmpSubset->vertexCount);
                for (auto j = static_cast<uint32_t>(pTmpSubset->vertexStart); j < n; ++j)
                {
                    VEC3 *pos = &pPosition[j];
                    if (pos->x < minSize.x)
                        minSize.x = pos->x;
                    if (pos->y < minSize.y)
                        minSize.y = pos->y;
                    if (pos->z < minSize.z)
                        minSize.z = pos->z;

                    if (pos->x > maxSize.x)
                        maxSize.x = pos->x;
                    if (pos->y > maxSize.y)
                        maxSize.y = pos->y;
                    if (pos->z > maxSize.z)
                        maxSize.z = pos->z;
                }
            }
            else
            {
                return;
            }
            VEC3 dist(maxSize - minSize);
            // O Seguinte calculo inibe uns pontos perdidos
            float xDif = maxSize.x < 0.0f ? -maxSize.x : maxSize.x;
            float yDif = maxSize.y < 0.0f ? -maxSize.y : maxSize.y;
            float zDif = maxSize.z < 0.0f ? -maxSize.z : maxSize.z;

            float xDiff = minSize.x < 0.0f ? -minSize.x : minSize.x;
            float yDiff = minSize.y < 0.0f ? -minSize.y : minSize.y;
            float zDiff = minSize.z < 0.0f ? -minSize.z : minSize.z;

            float xMin = xDiff < xDif ? xDiff : xDif;
            float xMax = xDiff > xDif ? xDiff : xDif;

            float yMin = yDiff < yDif ? yDiff : yDif;
            float yMax = yDiff > yDif ? yDiff : yDif;

            float zMin = zDiff < zDif ? zDiff : zDif;
            float zMax = zDiff > zDif ? zDiff : zDif;

            float xDiv = xMin / xMax;
            float yDiv = yMin / yMax;
            float zDiv = zMin / zMax;
            if (xDiv < 0.001f)
            {
                dist.x    = xMin;
                minSize.x = 0;
            }
            if (yDiv < 0.001f)
            {
                dist.y    = yMin;
                minSize.y = 0;
            }
            if (zDiv < 0.001f)
            {
                dist.z    = zMin;
                minSize.z = 0;
            }
            const VEC3 middle(dist.x * 0.5f, dist.y * 0.5f, dist.z * 0.5f);
            const VEC3 offset(minSize.x + middle.x, minSize.y + middle.y, minSize.z + middle.z);
            if (indexSubset < 0)
            {
                for (uint32_t i = 0; i < s; ++i)
                {
                    util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[i];
                    const auto  n          = static_cast<uint32_t >(pTmpSubset->vertexStart + pTmpSubset->vertexCount);
                    for (auto j = static_cast<uint32_t >(pTmpSubset->vertexStart); j < n; ++j)
                    {
                        VEC3 *pos = &pPosition[j];
                        pos->x -= offset.x;
                        pos->y -= offset.y;
                        pos->z -= offset.z;
                    }
                }
            }
            else if (indexSubset < static_cast<int>(s))
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[static_cast<std::vector<util::SUBSET_DEBUG *>::size_type>(indexSubset)];
                const int  n          = pTmpSubset->vertexStart + pTmpSubset->vertexCount;
                for (int j = pTmpSubset->vertexStart; j < n; ++j)
                {
                    VEC3 *pos = &pPosition[j];
                    pos->x -= offset.x;
                    pos->y -= offset.y;
                    pos->z -= offset.z;
                }
            }
        }
    }

    void MESH_MBM_DEBUG::rotateFrame(const int indexFrame, const int indexSubset, const float angleX, const float angleY, const float angleZ)
    {
        if (indexFrame < 0)
        {
            for (uint32_t i = 0; i < this->buffer.size(); ++i)
                rotateFrame(static_cast<int>(i), indexSubset, angleX, angleY, angleZ);
            return;
        }
        if (indexFrame >= static_cast<int>(this->buffer.size())) return;
        const float radX = angleX * static_cast<float>(M_PI) / 180.0f;
        const float radY = angleY * static_cast<float>(M_PI) / 180.0f;
        const float radZ = angleZ * static_cast<float>(M_PI) / 180.0f;
        const float cosX = cosf(radX), sinX = sinf(radX);
        const float cosY = cosf(radY), sinY = sinf(radY);
        const float cosZ = cosf(radZ), sinZ = sinf(radZ);
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)];
        auto *const              pPosition     = reinterpret_cast<VEC3 *>(bufferCurrent->position);
        const auto               s             = static_cast<uint32_t>(bufferCurrent->subset.size());
        auto applyRotation                      = [&](const uint32_t vertexStart, const uint32_t vertexCount) {
            const uint32_t n = vertexStart + vertexCount;
            for (uint32_t j = vertexStart; j < n; ++j)
            {
                VEC3 *p = &pPosition[j];
                if (angleX != 0.0f)
                {
                    const float y = p->y * cosX - p->z * sinX;
                    const float z = p->y * sinX + p->z * cosX;
                    p->y = y; p->z = z;
                }
                if (angleY != 0.0f)
                {
                    const float x =  p->x * cosY + p->z * sinY;
                    const float z = -p->x * sinY + p->z * cosY;
                    p->x = x; p->z = z;
                }
                if (angleZ != 0.0f)
                {
                    const float x = p->x * cosZ - p->y * sinZ;
                    const float y = p->x * sinZ + p->y * cosZ;
                    p->x = x; p->y = y;
                }
            }
        };
        if (indexSubset < 0)
        {
            for (uint32_t i = 0; i < s; ++i)
            {
                const util::SUBSET_DEBUG *pSub = bufferCurrent->subset[i];
                applyRotation(static_cast<uint32_t>(pSub->vertexStart), static_cast<uint32_t>(pSub->vertexCount));
            }
        }
        else if (indexSubset < static_cast<int>(s))
        {
            const util::SUBSET_DEBUG *pSub = bufferCurrent->subset[static_cast<uint32_t>(indexSubset)];
            applyRotation(static_cast<uint32_t>(pSub->vertexStart), static_cast<uint32_t>(pSub->vertexCount));
        }
    }

    void MESH_MBM_DEBUG::scaleFrame(const int indexFrame, const int indexSubset, const float sx, const float sy, const float sz)
    {
        if (indexFrame < 0)
        {
            for (uint32_t i = 0; i < this->buffer.size(); ++i)
                scaleFrame(static_cast<int>(i), indexSubset, sx, sy, sz);
            return;
        }
        if (indexFrame >= static_cast<int>(this->buffer.size())) return;
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)];
        auto *const              pPosition     = reinterpret_cast<VEC3 *>(bufferCurrent->position);
        const auto               s             = static_cast<uint32_t>(bufferCurrent->subset.size());
        auto applyScale                         = [&](const uint32_t vertexStart, const uint32_t vertexCount) {
            const uint32_t n = vertexStart + vertexCount;
            for (uint32_t j = vertexStart; j < n; ++j)
            {
                VEC3 *p = &pPosition[j];
                p->x *= sx; p->y *= sy; p->z *= sz;
            }
        };
        if (indexSubset < 0)
        {
            for (uint32_t i = 0; i < s; ++i)
            {
                const util::SUBSET_DEBUG *pSub = bufferCurrent->subset[i];
                applyScale(static_cast<uint32_t>(pSub->vertexStart), static_cast<uint32_t>(pSub->vertexCount));
            }
        }
        else if (indexSubset < static_cast<int>(s))
        {
            const util::SUBSET_DEBUG *pSub = bufferCurrent->subset[static_cast<uint32_t>(indexSubset)];
            applyScale(static_cast<uint32_t>(pSub->vertexStart), static_cast<uint32_t>(pSub->vertexCount));
        }
    }

    void MESH_MBM_DEBUG::translateFrame(const int indexFrame, const int indexSubset, const float dx, const float dy, const float dz)
    {
        if (indexFrame < 0)
        {
            for (uint32_t i = 0; i < this->buffer.size(); ++i)
                translateFrame(static_cast<int>(i), indexSubset, dx, dy, dz);
            return;
        }
        if (indexFrame >= static_cast<int>(this->buffer.size())) return;
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)];
        auto *const              pPosition     = reinterpret_cast<VEC3 *>(bufferCurrent->position);
        const auto               s             = static_cast<uint32_t>(bufferCurrent->subset.size());
        auto applyTranslate                     = [&](const uint32_t vertexStart, const uint32_t vertexCount) {
            const uint32_t n = vertexStart + vertexCount;
            for (uint32_t j = vertexStart; j < n; ++j)
            {
                VEC3 *p = &pPosition[j];
                p->x += dx; p->y += dy; p->z += dz;
            }
        };
        if (indexSubset < 0)
        {
            for (uint32_t i = 0; i < s; ++i)
            {
                const util::SUBSET_DEBUG *pSub = bufferCurrent->subset[i];
                applyTranslate(static_cast<uint32_t>(pSub->vertexStart), static_cast<uint32_t>(pSub->vertexCount));
            }
        }
        else if (indexSubset < static_cast<int>(s))
        {
            const util::SUBSET_DEBUG *pSub = bufferCurrent->subset[static_cast<uint32_t>(indexSubset)];
            applyTranslate(static_cast<uint32_t>(pSub->vertexStart), static_cast<uint32_t>(pSub->vertexCount));
        }
    }

    bool MESH_MBM_DEBUG::addIndex(const uint32_t indexFrame, const uint32_t indexSubset,
                        const uint16_t *newIndexPart, const uint32_t sizeArrayNewIndexPart,
                        char *strErrorOut, const int strErrorOutLen)
    {
        if (indexFrame < this->buffer.size() && indexSubset < this->buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[indexFrame];
            util::SUBSET_DEBUG *     pSubset       = bufferCurrent->subset[indexSubset];
            if (pSubset->vertexCount == 0)
            {
                if (strErrorOut)
                    snprintf(strErrorOut, strErrorOutLen, "vertex count is zero [0] to subset [%u] at frame [%u]\nBefore set index you "
                                         "must set the vertex.",
                            indexSubset, indexFrame);
                return false;
            }
            for (uint32_t i = 0; i < sizeArrayNewIndexPart; ++i)
            {
                const uint16_t index = newIndexPart[i];
                if (index >= pSubset->vertexCount)
                {
                    if (strErrorOut)
                        snprintf(strErrorOut, strErrorOutLen, "index [%u] value [%u] out of bound. max vertex [%d] for this subset", i,
                                index, pSubset->vertexCount);
                    return false;
                }
            }
            const std::vector<util::SUBSET_DEBUG *>::size_type  sizeSubset       = bufferCurrent->subset.size();
            uint32_t        indexCountTotal  = 0;
            uint32_t        indexCountAfter  = 0;
            uint32_t        indexCountBefore = 0;
            uint16_t *oldIndex         = bufferCurrent->indexBuffer;
            auto        oldSizeIndex     = static_cast<uint32_t>(pSubset->indexCount);
            for (uint32_t i = 0; i < sizeSubset; ++i)
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[i];
                indexCountTotal += static_cast<uint32_t>(pTmpSubset->indexCount);
                if (i < indexSubset)
                {
                    indexCountBefore += static_cast<uint32_t>(pTmpSubset->indexCount);
                }
                if (i > indexSubset)
                {
                    indexCountAfter += static_cast<uint32_t>(pTmpSubset->indexCount);
                }
            }

            const uint32_t newSizeIndex = indexCountTotal - oldSizeIndex + sizeArrayNewIndexPart;

            if (oldIndex)
            {
                auto newIndex   = new unsigned short[newSizeIndex];
                bufferCurrent->indexBuffer = newIndex;
                if (indexCountBefore)
                {
                    memcpy(newIndex, oldIndex, sizeof(unsigned short) * static_cast<size_t>(indexCountBefore));
                }
                memcpy(&newIndex[indexCountBefore], newIndexPart, sizeof(unsigned short) * static_cast<size_t>(sizeArrayNewIndexPart));
                if (indexCountAfter)
                {
                    uint32_t s = indexCountBefore + sizeArrayNewIndexPart;
                    memcpy(&newIndex[s], oldIndex, sizeof(unsigned short) * static_cast<size_t>(indexCountAfter));
                }
                int diff = static_cast<int>(oldSizeIndex) - static_cast<int>(sizeArrayNewIndexPart);
                for (uint32_t i = (indexSubset + 1); i < sizeSubset; ++i)
                {
                    util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[std::vector<util::SUBSET_DEBUG *>::size_type(i)];
                    pTmpSubset->indexStart += diff;
                }

                for (int i = pSubset->indexStart; i < (pSubset->indexStart + static_cast<int>(sizeArrayNewIndexPart)); ++i)
                {
                    newIndex[i] +=  static_cast<unsigned short>(pSubset->vertexStart);
                }
                pSubset->indexCount = static_cast<int>(sizeArrayNewIndexPart);
                delete[] oldIndex;
            }
            else
            {
                pSubset->indexStart        = static_cast<int>(indexCountBefore);
                pSubset->indexCount        = static_cast<int>(sizeArrayNewIndexPart);
                bufferCurrent->indexBuffer = new uint16_t[newSizeIndex];
                memcpy(bufferCurrent->indexBuffer, newIndexPart, sizeof(uint16_t) * static_cast<size_t>(sizeArrayNewIndexPart));
                int diff = static_cast<int>(oldSizeIndex) - static_cast<int>(sizeArrayNewIndexPart);
                for (uint32_t i = (indexSubset + 1); i < sizeSubset; ++i)
                {
                    util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[i];
                    pTmpSubset->indexStart += diff;
                }
                for (int i = pSubset->indexStart; i < (pSubset->indexStart + static_cast<int>(sizeArrayNewIndexPart)); ++i)
                {
                    bufferCurrent->indexBuffer[i] += static_cast<unsigned short>(pSubset->vertexStart);
                }
            }
            // update
            uint32_t lastCountIndex = 0;

            for (auto & i : bufferCurrent->subset)
            {
                pSubset             = i;
                pSubset->indexStart = static_cast<int>(lastCountIndex);
                lastCountIndex += static_cast<uint32_t>(pSubset->indexCount);
            }
            return true;
        }
        else
        {
            if (strErrorOut)
            {
                const auto tSubset = static_cast<int>(indexFrame < this->buffer.size() ? this->buffer[indexFrame]->subset.size() : 0);
                snprintf(strErrorOut, strErrorOutLen, "Out of bound[indexFrame(total %u),indexSubset(total %d)\n"
                                     "indexFrame %u indexSubset %u",
                        static_cast<uint32_t>(this->buffer.size()), tSubset, indexFrame, indexSubset);
            }
            return false;
        }
    }
    
    bool MESH_MBM_DEBUG::addVertex(const uint32_t indexFrame, const uint32_t indexSubset, const uint32_t totalVertex)
    {
        if (indexFrame < this->buffer.size() && indexSubset < this->buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent    = this->buffer[indexFrame];
            util::SUBSET_DEBUG *     pSubset          = nullptr;
            unsigned  int            vertexCountTotal = 0;
            unsigned  int            vertexEndSubset  = 0;

            for (std::vector<util::SUBSET_DEBUG *>::size_type i = 0; i < bufferCurrent->subset.size(); ++i)
            {
                pSubset = bufferCurrent->subset[i];
                vertexCountTotal += static_cast<uint32_t>(pSubset->vertexCount);
                if (i <= indexSubset)
                {
                    vertexEndSubset += static_cast<uint32_t>(pSubset->vertexCount);
                }
            }
            pSubset = bufferCurrent->subset[indexSubset];
            if (pSubset->indexCount)
            {
                PRINT_IF_DEBUG( "Warning! you are adding vertex to a subset [%d] that has index at "
                                                 "frame [%d]\n the index will be deleted.");
                if (bufferCurrent->indexBuffer)
                {
                    delete[] bufferCurrent->indexBuffer;
                    bufferCurrent->indexBuffer = nullptr;
                }
                for (auto & i : bufferCurrent->subset)
                {
                    pSubset             = i;
                    pSubset->indexCount = 0;
                    pSubset->indexStart = 0;
                }
            }
            auto *oldPosition = reinterpret_cast<VEC3 *>(bufferCurrent->position);
            auto *oldNormal   = reinterpret_cast<VEC3 *>(bufferCurrent->normal);
            auto *oldUv       = reinterpret_cast<VEC2 *>(bufferCurrent->uv);

            auto newPosition = new VEC3[vertexCountTotal + totalVertex];
            auto newNormal   = new VEC3[vertexCountTotal + totalVertex];
            auto newUv       = new VEC2[vertexCountTotal + totalVertex];

            if (vertexEndSubset)
            {
                if (oldPosition)
                    memcpy(static_cast<void*>(newPosition), static_cast<void*>(oldPosition), sizeof(VEC3) * static_cast<size_t>(vertexEndSubset));
                if (oldNormal)
                    memcpy(static_cast<void*>(newNormal), static_cast<void*>(oldNormal), sizeof(VEC3) * static_cast<size_t>(vertexEndSubset));
                if (oldUv)
                    memcpy(static_cast<void*>(newUv), static_cast<void*>(oldUv), sizeof(VEC2) * static_cast<size_t>(vertexEndSubset));
            }
            memset(static_cast<void*>(&newPosition[vertexEndSubset]), 0, sizeof(VEC3) * static_cast<size_t>(totalVertex)); // new vertex comes 0.0f
            memset(static_cast<void*>(&newNormal[vertexEndSubset]), 0, sizeof(VEC3) * static_cast<size_t>(totalVertex));   // new vertex comes 0.0f
            memset(static_cast<void*>(&newUv[vertexEndSubset]), 0, sizeof(VEC2) * static_cast<size_t>(totalVertex));       // new vertex comes 0.0f
            uint32_t lenLastVertex = vertexCountTotal - (vertexEndSubset);
            if (lenLastVertex > 0)
            {
                memcpy(static_cast<void*>(&newPosition[vertexEndSubset + totalVertex]), &oldPosition[vertexEndSubset],sizeof(VEC3) * static_cast<size_t>(lenLastVertex));
                if (oldNormal)
                    memcpy(static_cast<void*>(&newNormal[vertexEndSubset + totalVertex]), &oldNormal[vertexEndSubset],sizeof(VEC3) * static_cast<size_t>(lenLastVertex));
                else
                    memset(static_cast<void*>(&newNormal[vertexEndSubset + totalVertex]), 0, sizeof(VEC3) * static_cast<size_t>(lenLastVertex));
                memcpy(static_cast<void*>(&newUv[vertexEndSubset + totalVertex]), &oldUv[vertexEndSubset], sizeof(VEC2) * static_cast<size_t>(lenLastVertex));
            }

            bufferCurrent->position = reinterpret_cast<float *>(newPosition);
            bufferCurrent->normal   = reinterpret_cast<float *>(newNormal);
            bufferCurrent->uv       = reinterpret_cast<float *>(newUv);

            if (oldPosition)
                delete[] oldPosition;
            if (oldNormal)
                delete[] oldNormal;
            if (oldUv)
                delete[] oldUv;

            headerMesh.hasNorText[0] = HAS_NOR_IN_FILE; // addVertex always allocates normals
            pSubset->vertexCount += totalVertex;
            // update
            uint32_t lastCountVertex = 0;
            for (auto & i : bufferCurrent->subset)
            {
                pSubset              = i;
                pSubset->vertexStart = static_cast<int>(lastCountVertex);
                lastCountVertex += static_cast<uint32_t>(pSubset->vertexCount);
            }
            return true;
        }
        return false;
    }
    
    int MESH_MBM_DEBUG::addAnimation(const char *nameAnimation, const int initialFrame, const int finalFrame,
                           const float timeBetweenFrame, const int typeAnimation, char *errorOut, const int errorOutLen)
    {
        if (this->buffer.size() == 0)
        {
            if (errorOut)
                snprintf(errorOut, errorOutLen, "there is no frame ");
            return 0;
        }
        if (initialFrame < 0 || initialFrame >= static_cast<int>(this->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut, errorOutLen, "initial frame [%d] out of range ->[%d]", initialFrame, static_cast<int>(this->buffer.size()));
            return 0;
        }
        if (finalFrame < 0 || finalFrame >= static_cast<int>(this->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut, errorOutLen, "final frame [%d] out of range ->[%d]", finalFrame, static_cast<int>(this->buffer.size()));
            return 0;
        }
        if (typeAnimation < 0 || typeAnimation > 6)
        {
            if (errorOut)
                snprintf(errorOut, errorOutLen, "type of animation [%d] out of range ->[0-6]", typeAnimation);
            return 0;
        }
        auto infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
        this->infoAnimation.lsHeaderAnim.push_back(infoHead);
        headerMesh.totalAnimation = static_cast<int>(this->infoAnimation.lsHeaderAnim.size());
        infoHead->headerAnim     = new util::HEADER_ANIMATION();
        if (nameAnimation)
            strncpy(infoHead->headerAnim->nameAnimation, nameAnimation, sizeof(infoHead->headerAnim->nameAnimation) - 1);
        else
            strncpy(infoHead->headerAnim->nameAnimation, "default",sizeof(infoHead->headerAnim->nameAnimation) - 1);
        infoHead->headerAnim->initialFrame     = initialFrame;
        infoHead->headerAnim->finalFrame       = finalFrame;
        infoHead->headerAnim->timeBetweenFrame = timeBetweenFrame <= 0.0f ? 0.0f : timeBetweenFrame;
        infoHead->headerAnim->typeAnimation    = typeAnimation;
        return headerMesh.totalAnimation;
    }

    bool MESH_MBM_DEBUG::updateAnimation(const uint32_t index, const char *nameAnimation, const int initialFrame, const int finalFrame,
                           const float timeBetweenFrame, const int typeAnimation, char *errorOut,const int lenError)
    {
        if (this->buffer.size() == 0)
        {
            if (errorOut)
                snprintf(errorOut,lenError, "there is no frame ");
            return false;
        }
        if(index >= this->infoAnimation.lsHeaderAnim.size())
        {
            if (errorOut)
                snprintf(errorOut, lenError,"index animation out of range. Total anim -> [%d] index -> [%d] ",static_cast<int>(this->infoAnimation.lsHeaderAnim.size()),static_cast<int>(index));
            return false;
        }
        if (initialFrame < 0 || initialFrame >= static_cast<int>(this->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut, lenError,"initial frame [%d] out of range ->[%d]", initialFrame, static_cast<int>(this->buffer.size()));
            return false;
        }
        if (finalFrame < 0 || finalFrame >= static_cast<int>(this->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut,lenError, "final frame [%d] out of range ->[%d]", finalFrame, static_cast<int>(this->buffer.size()));
            return false;
        }
        if (typeAnimation < 0 || typeAnimation > 6)
        {
            if (errorOut)
                snprintf(errorOut,lenError, "type of animation [%d] out of range ->[0-6]", typeAnimation);
            return false;
        }
        util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead = this->infoAnimation.lsHeaderAnim[index];
        if(infoHead->headerAnim == nullptr)
        {
            if (errorOut)
                strncpy(errorOut, "headerAnim null",lenError);
            return false;
        }
        if (nameAnimation)
            strncpy(infoHead->headerAnim->nameAnimation, nameAnimation, sizeof(infoHead->headerAnim->nameAnimation) - 1);
        else
            strncpy(infoHead->headerAnim->nameAnimation, "default",sizeof(infoHead->headerAnim->nameAnimation)-1);
        infoHead->headerAnim->initialFrame     = initialFrame;
        infoHead->headerAnim->finalFrame       = finalFrame;
        infoHead->headerAnim->timeBetweenFrame = timeBetweenFrame <= 0.0f ? 0.0f : timeBetweenFrame;
        infoHead->headerAnim->typeAnimation    = typeAnimation;
        return true;
    }

    const util::INFO_ANIMATION::INFO_HEADER_ANIM *MESH_MBM_DEBUG::getAnim(const uint32_t index)const
    {
        if(index < this->infoAnimation.lsHeaderAnim.size())
            return this->infoAnimation.lsHeaderAnim[index];
        return nullptr;
    }

    void MESH_MBM_DEBUG::deleteExtraInfo()
    {
        switch(typeMe)
        {
            case util::TYPE_MESH_FONT:
            {
                auto* infoFont = static_cast<mbm::INFO_BOUND_FONT*>(extraInfo);
                if(infoFont)
                    delete infoFont;
            }
            break;
            case util::TYPE_MESH_PARTICLE:
            {
                auto* lsParticleInfo = static_cast<std::vector<util::STAGE_PARTICLE*>*>(extraInfo);
                if(lsParticleInfo)
                {
                    for (auto stage : *lsParticleInfo)
                    {
                        delete stage;
                    }
                    lsParticleInfo->clear();
                    delete lsParticleInfo;
                }
            }
            break;
            case util::TYPE_MESH_TILE_MAP:
            {
                auto* infoTileMap = static_cast<util::BTILE_INFO*>(extraInfo);
                if(infoTileMap)
                    delete infoTileMap;
            }
            break;
                        case util::TYPE_MESH_SHAPE:
            {
                auto* infoShape = static_cast<util::DYNAMIC_SHAPE*>(extraInfo);
                if(infoShape)
                    delete infoShape;
            }
            break;
            default:
            {
                if (extraInfo)
                {
                    auto * charExtraInfo = static_cast<char*>(extraInfo);
                    delete[] charExtraInfo;
                }
            }
        }
        extraInfo           = nullptr;
    }
    
    void MESH_MBM_DEBUG::fixDefaultBoud()
    {
        if (this->infoPhysics.lsCube.size() == 0)
        {
            this->fillAtLeastOneBound();
            headerMesh.deprecated_typePhysics = 1;
        }
    }
    
    void MESH_MBM_DEBUG::release()
    {
        deleteExtraInfo();
        if (this->coordTexFrame_0)
            delete[] this->coordTexFrame_0;
        this->coordTexFrame_0 = nullptr;
        
        for (auto meshBuffer : this->buffer)
        {
            if (meshBuffer)
                delete meshBuffer;
            meshBuffer = nullptr;
        }
        buffer.clear();
        angleDefault        = VEC3(0, 0, 0);
        positionOffset      = VEC3(0, 0, 0);
        sizeCoordTexFrame_0 = 0;
        typeMe              = util::TYPE_MESH_UNKNOWN;
        memset(static_cast<void*>(&this->headerMain), 0, sizeof(this->headerMain));
        memset(static_cast<void*>(&this->headerMesh), 0, sizeof(this->headerMesh));
        zoomEditorSprite.x = 1.0f;
        zoomEditorSprite.y = 1.0f;
        util::MATERIAL m;
        this->headerMesh.material      = m;
        this->headerMesh.hasNorText[0] = HAS_NOR_NO;
        this->headerMesh.hasNorText[1] = HAS_TEX_EACH_FRAME;
        this->infoPhysics.release();
        this->infoAnimation.release();
    }

    void MESH_MBM_DEBUG::fillAtLeastOneBound()
    {
        headerMesh.deprecated_typePhysics = 1;
        auto base                       = new CUBE();
        this->infoPhysics.release();
        this->infoPhysics.lsCube.push_back(base);
        VEC3 vMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        VEC3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->buffer[std::vector<util::BUFFER_MESH_DEBUG *>::size_type(currentFrame)];
            auto *                   pPosition          = reinterpret_cast<VEC3 *>(currentFrameBuffer->position);
            for (auto pSubset : currentFrameBuffer->subset)
            {
                const int           t       = pSubset->vertexCount + pSubset->vertexStart;
                for (int i = pSubset->vertexStart; i < t; ++i)
                {
                    VEC3 *p = &pPosition[i];
                    if (p->x < vMin.x)
                        vMin.x = p->x;
                    if (p->y < vMin.y)
                        vMin.y = p->y;
                    if (p->z < vMin.z)
                        vMin.z = p->z;

                    if (p->x > vMax.x)
                        vMax.x = p->x;
                    if (p->y > vMax.y)
                        vMax.y = p->y;
                    if (p->z > vMax.z)
                        vMax.z = p->z;
                }
            }
        }
        base->halfDim.x   = (vMax.x - vMin.x) * 0.5f;
        base->halfDim.y   = (vMax.y - vMin.y) * 0.5f;
        base->halfDim.z   = (vMax.z - vMin.z) * 0.5f;
        base->absCenter.x = vMin.x + (base->halfDim.x);
        base->absCenter.y = vMin.y + (base->halfDim.y);
        base->absCenter.z = vMin.z + (base->halfDim.z);
    }

    std::vector<std::string> MESH_MBM_DEBUG::getKnowPathsToExtraHeader()
    {
        mbm::TEXTURE_MANAGER* textureManager = mbm::TEXTURE_MANAGER::getInstance();
        std::vector<std::string> allTexturesFullPaths;
        textureManager->getAllTexturesFullPaths(allTexturesFullPaths);

        std::unordered_set<std::string> uniquePaths;
        for(const auto& fullPath : allTexturesFullPaths)
        {
            std::string path = util::getPathFromFullPathName(fullPath.c_str());
            if (!path.empty())
            {
                uniquePaths.insert(path);
            }
        }
        std::vector<std::string> result;
        for (const auto & path : uniquePaths)
        {
            result.insert(result.end(), path);
        }
        return result;
    }
    
    bool MESH_MBM_DEBUG::fillAnimation_2(const char *fileNamePath, FILE *fp)
    {
        return fill_animation_headers_common(fp,
                                             fileNamePath,
                                             this->headerMesh.totalAnimation,
                                             this->infoAnimation);
    }
    
    bool MESH_MBM_DEBUG::loadFromSeparatedBuffers(FILE *fp, const int sizeVertexBuffer, VEC3 **positionOut,
                                VEC3 **normalOut, VEC2 **textureOut, int16_t hasNorText[2],
                                uint16_t *indexArray, const int sizeArrayIndex, const int stride,
                                int fileVersion)
    {
        return load_from_separated_buffers_common(fp,
                                        sizeVertexBuffer,
                                        positionOut,
                                        normalOut,
                                        textureOut,
                                        hasNorText,
                                        indexArray,
                                        sizeArrayIndex,
                                        stride,
                                        fileVersion,
                                        this->coordTexFrame_0,
                                        this->sizeCoordTexFrame_0);
    }
    
    bool MESH_MBM_DEBUG::saveAnimationHeaders(const char *fileOut, FILE **file)
    {
        // 4 header anim -- Todas as animações -----------------------------------------------------------
        for (int i = 0; i < this->headerMesh.totalAnimation; ++i)
        {
            util::INFO_ANIMATION::INFO_HEADER_ANIM * infoHead   = this->infoAnimation.lsHeaderAnim[ std::vector<util::INFO_ANIMATION::INFO_HEADER_ANIM *>::size_type(i)];
            util::HEADER_ANIMATION       headerAnim = *infoHead->headerAnim;
            if (headerAnim.hasShaderEffect == 0)
                headerAnim.hasShaderEffect = 1;
            if (!util::writeHeaderAnimationV8(*file, headerAnim))
                return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add animation header", *file);

            util::INFO_FX *effectShader = infoHead->effectShader;
            if(effectShader == nullptr)
            {
                int blendOperation = 0;
                if(i < static_cast<int>(lsBlendOperation.size()) && lsBlendOperation[i] != 0)
                    blendOperation = lsBlendOperation[i];
                if (!write_empty_shader_steps_pair(file, blendOperation))
                    return false;
                continue;
            }

            bool shouldWriteVertexTextureStage2 = true;
            // Pixel Shader
            util::INFO_SHADER_DATA *pixelShaderData = effectShader->dataPS;
            if (pixelShaderData)
            {
                shouldWriteVertexTextureStage2 = false;
                if (!write_shader_step_to_file(fileOut,
                                               file,
                                               effectShader,
                                               pixelShaderData,
                                               true,
                                               "failed to add name of pixel shader"))
                    return false;
            }
            else
            {
                if (!write_empty_shader_step(file, effectShader->blendOperation))
                    return false;
            }

            // Vertex Shader
            util::INFO_SHADER_DATA *vertexShaderData = effectShader->dataVS;
            if (vertexShaderData)
            {
                if (!write_shader_step_to_file(fileOut,
                                               file,
                                               effectShader,
                                               vertexShaderData,
                                               shouldWriteVertexTextureStage2,
                                               "failed to add name of shader to file"))
                    return false;
            }
            else
            {
                if (!write_empty_shader_step(file, effectShader->blendOperation))
                    return false;
            }
        }
        return true;
    }
    
    bool MESH_MBM_DEBUG::compressFile(const char *fileNameIn, char *stringStatus,const int lenStatus)
    {
        if (!fileNameIn)
        {
            if (stringStatus)
                strncpy(stringStatus, "name of file empty",lenStatus);
            return false;
        }
        MINIZ  miniz;
        std::string fileNameTemp(fileNameIn);
        fileNameTemp += ".tmp";
        if (rename(fileNameIn, fileNameTemp.c_str()))
        {
            if (stringStatus)
                strncpy(stringStatus, "failed to rename file",lenStatus);
            return false;
        }
        if (miniz.compressFile(fileNameTemp.c_str(), fileNameIn))
        {
            remove(fileNameTemp.c_str());
            if (stringStatus && stringStatus[0] == 0)
                strncpy(stringStatus, "File successfully saved",lenStatus);
            return true;
        }
        else
        {
            rename(fileNameTemp.c_str(), fileNameIn);
            if (stringStatus)
                strncpy(stringStatus, "error on compress file",lenStatus);
            return false;
        }
    }



    BUFFER_MESH * MESH_MBM::getBuffer(const uint32_t index) const
    {
        if (index < this->totalFramesMesh && buffer)
            return &buffer[index];
        return nullptr;
    }
    
    TEXTURE * MESH_MBM::getTexture(const uint32_t indexFrame, const uint32_t indexSubset)
    {
        if (indexFrame < totalFramesMesh && buffer)
        {
            if (indexSubset < buffer[indexFrame].totalSubset)
                return buffer[indexFrame].subset[indexSubset].texture;
        }
        return nullptr;
    }
    
    bool MESH_MBM::setTexture(const uint32_t indexFrame, const uint32_t indexSubset, const char *fileNameTexture,
                           const bool hasAlpha)
    {
        if (indexFrame < totalFramesMesh && buffer)
        {
            if (indexSubset < buffer[indexFrame].totalSubset)
            {
                buffer[indexFrame].subset[indexSubset].texture =
                    TEXTURE_MANAGER::getInstance()->load(fileNameTexture, hasAlpha);
                if (buffer[indexFrame].pBufferGL && buffer[indexFrame].subset[indexSubset].texture)
                {
                    for (uint32_t i = 0; i < buffer[indexFrame].pBufferGL->totalSubset; ++i)
                    {
                        buffer[indexFrame].pBufferGL->setTextureByStage(buffer[indexFrame].subset[indexSubset].texture, 0, i);
                    }
                    return true;
                }
                return buffer[indexFrame].subset[indexSubset].texture != nullptr;
            }
        }
        return false;
    }
    
    const char * MESH_MBM::getFilenameMesh() const
    {
        return fileName.c_str();
    }
    
    MESH_MBM::~MESH_MBM()
    {
        release();
    }

    void MESH_MBM::deleteExtraInfo()
    {
        switch(typeMe)
        {
            case util::TYPE_MESH_FONT:
            {
                auto* infoFont = static_cast<mbm::INFO_BOUND_FONT*>(extraInfo);
                if(infoFont)
                    delete infoFont;
            }
            break;
            case util::TYPE_MESH_PARTICLE:
            {
                auto* lsParticleInfo = static_cast<std::vector<util::STAGE_PARTICLE*>*>(extraInfo);
                if(lsParticleInfo)
                {
                    for (auto stage : *lsParticleInfo)
                    {
                        delete stage;
                    }
                    lsParticleInfo->clear();
                    delete lsParticleInfo;
                }
            }
            break;
            case util::TYPE_MESH_TILE_MAP:
            {
                auto* infoTileMap = static_cast<util::BTILE_INFO*>(extraInfo);
                if(infoTileMap)
                    delete infoTileMap;
            }
            break;
            case util::TYPE_MESH_SHAPE:
            {
                auto* infoShape = static_cast<util::DYNAMIC_SHAPE*>(extraInfo);
                if(infoShape)
                    delete infoShape;
            }
            break;
            default:
            {
                if (extraInfo)
                {
                    auto * charExtraInfo = static_cast<char*>(extraInfo);
                    delete[] charExtraInfo;
                }
            }
        }
        extraInfo           = nullptr;
    }
    
    void MESH_MBM::release() //
    {
        deleteExtraInfo();
        if (buffer)
            delete[] buffer;
        buffer = nullptr;
        this->infoPhysics.release();
        this->infoAnimation.release();

        if (coordTexFrame_0)
            delete[] coordTexFrame_0;
        coordTexFrame_0 = nullptr;

        totalFramesMesh = 0;
        
        zoomEditorSprite.x  = 0;
        zoomEditorSprite.y  = 0;
        typeMe              = util::TYPE_MESH_UNKNOWN;
        hasNormTex[0]       = 0;
        hasNormTex[1]       = 0;
        depthUberImage      = 8;
        sizeCoordTexFrame_0 = 0;
    }
    
    bool MESH_MBM::isLoaded() const
    {
        return this->buffer != nullptr;
    }
    
    bool MESH_MBM::render(const uint32_t indexFrame,const SHADER *pShader,TEXTURE* ptrTexture1,
                          const RENDERIZABLE *renderizableOwner)
    {
        if (indexFrame < totalFramesMesh && buffer)
        {
            DEVICE *device = DEVICE::getInstance();
            device->setRenderMaterial(this->material);
            buffer[indexFrame].pBufferGL->setTextureByStage(ptrTexture1, 1, 0);
            const bool ret = pShader->render(buffer[indexFrame].pBufferGL, renderizableOwner);
            device->clearRenderMaterial();
            return ret;
        }
        return false;
    }

    bool MESH_MBM::renderDynamic(const uint32_t indexFrame, SHADER *pShader, VEC3 *vertex, VEC3 *normal,
                                    VEC2 *uv, TEXTURE* ptrTexture1, const RENDERIZABLE *renderizableOwner)
    {
        if (indexFrame < totalFramesMesh && buffer)
        {
            DEVICE *device = DEVICE::getInstance();
            device->setRenderMaterial(this->material);
            buffer[indexFrame].pBufferGL->setTextureByStage(ptrTexture1, 1, 0);
            const bool ret = pShader->renderDynamic(buffer[indexFrame].pBufferGL, vertex, normal, uv,
                                                    renderizableOwner);
            device->clearRenderMaterial();
            return ret;
        }
        return false;
    }
    
    /*const bool drawSubset(    const uint32_t          indexFrame,
                                    std::vector<uint32_t>   &lsIndexSubset,
                                    SHADER*             pShader,
                                    const uint32_t          idTexture1)//Renderiza o frame indicado retorna true se
    foi possivel renderizar
    {
        if(indexFrame < totalFramesMesh && buffer && lsIndexSubset.size())
        {
            for(uint32_t i=0, s = lsIndexSubset.size(); i< s; ++i)
            {
                const uint32_t index = lsIndexSubset[i];
                if(index < buffer[indexFrame].totalSubset)
                {
                    buffer[indexFrame].pBufferGL->idTexture1 = idTexture1;
                    if(!pShader->drawSubset(buffer[indexFrame].pBufferGL,index))
                        return false;
                }
            }
            return true;
        }
        return false;
    }*/
    
    util::TYPE_MESH MESH_MBM::getTypeMesh() const
    {
        return typeMe;
    }
    
    VEC2 MESH_MBM::getZoomEditorSprite() const
    {
        return this->zoomEditorSprite;
    }
    
    uint32_t MESH_MBM::getTotalFrame() const
    {
        return totalFramesMesh;
    }
    
    uint32_t MESH_MBM::getTotalSubset(const uint32_t indexFrame) const
    {
        if (indexFrame < totalFramesMesh && buffer)
            return buffer[indexFrame].totalSubset;
        return 0;
    }
    
    MESH_MBM::MESH_MBM() noexcept
    {
        buffer          = nullptr;
        extraInfo       = nullptr;
        totalFramesMesh = 0;
        
        coordTexFrame_0     = nullptr;
        sizeCoordTexFrame_0 = 0;

        zoomEditorSprite.x  = 0;
        zoomEditorSprite.y  = 0;
        typeMe              = util::TYPE_MESH_UNKNOWN;
        hasNormTex[0]       = 0;
        hasNormTex[1]       = 0;
        depthUberImage      = 8;
    }
    
    bool MESH_MBM::load(const char *fileNamePath)
    {
        return this->load(fileNamePath, nullptr);
    }

    bool MESH_MBM::load(const char *fileNamePath, RENDERIZABLE *renderizable)
    {
        return this->loadImpl(fileNamePath, true, renderizable);
    }

    bool MESH_MBM::loadImpl(const char *fileNamePath, const bool allowLegacyDispatch, RENDERIZABLE *renderizable)
    {
        this->release();
        util::HEADER       headerMain;
        util::HEADER_MESH  headerMesh;
        FILE *                 fp             = nullptr;
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
        deprecated_mbm::INFO_SPRITE deprectedInfoSprite; // version <=SPRITE_INFO_VERSION_MBM_HEADER
#endif
        if (!open_decompressed_mesh_file(fileNamePath, fp, "Not found or failure to open file [%s]"))
            return false;
        this->fileName = fileNamePath;
        // step 1: Verificação do header  principal
        // -------------------------------------------------------------------------------
        if (!read_main_header_and_type(fp, fileNamePath, headerMain, typeMe))
            return false;
        if (allowLegacyDispatch && headerMain.version < STRONG_TYPES_VERSION_MBM_HEADER)
        {
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
            if (fp)
                fclose(fp);
            fp = nullptr;
            return this->loadLegacyCompat(fileNamePath, renderizable);
#else
            return log_util::onFailed(fp,__FILE__, __LINE__, "legacy mesh version [%d] disabled at compile time. Rebuild with MBM_ENABLE_MESH_LEGACY_V7", headerMain.version);
#endif
        }

        if (!read_extra_headers(fp, fileNamePath, headerMain.extraHeader, true))
            return false;

        if (!read_info_mode_if_needed(fp, fileNamePath, headerMain.version, info_mode, true))
            return false;
        if(typeMe == util::TYPE_MESH_TILE_MAP)
        {
            mbm::TEXTURE::EnablePixelPerfectTexture(true);
        }
        else
        {
            mbm::TEXTURE::EnablePixelPerfectTexture(false);
        }
        // step 2: --------------------------------------------------------------------------------------------------
        if (headerMain.version >= DETAIL_MESH_VERSION_MBM_HEADER)
        {
            if (!read_detail_mesh_section(fp,
                                          fileNamePath,
                                          headerMain,
                                          this->infoPhysics,
                                          this->extraInfo,
                                          [this](FILE *file, const char *name, const int totalBounding, const int fileVersion)
                                          {
                                              return this->readTriangleDetailCompat(file, name, totalBounding, fileVersion);
                                          }))
                return false;
        }
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
        else
        {
            if (!this->loadLegacyDetailStep(fp, fileNamePath, headerMain, deprectedInfoSprite))
                return false;
        }
#else
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "Imcompatible version [%d]", headerMain.version);
        }
#endif

        // 3 headerMesh MBM -------------------------------------------------------------------------------
        if (!util::readHeaderMeshV8(fp, headerMesh))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read HEADER_MESH [%s]", fileNamePath);
        this->hasNormTex[0]     = headerMesh.hasNorText[0];
        this->hasNormTex[1]     = headerMesh.hasNorText[1];
        this->material.Ambient  = headerMesh.material.Ambient;
        this->material.Diffuse  = headerMesh.material.Diffuse;
        this->material.Emissive = headerMesh.material.Emissive;
        this->material.Specular = headerMesh.material.Specular;
        this->material.Power    = headerMesh.material.Power;
        if (headerMesh.totalAnimation == 0)
            return log_util::onFailed(fp,__FILE__, __LINE__, "there is no animation [%s]", fileNamePath);

        // 4 header anim -- Todas as animações -----------------------------------------------------------
        if (headerMain.version < STRONG_TYPES_VERSION_MBM_HEADER)
        {
    #if defined(MBM_ENABLE_MESH_LEGACY_V7)
            if (!this->loadLegacyAnimationStep(fp, fileNamePath, headerMain, headerMesh, deprectedInfoSprite))
            return false;
    #else
            return log_util::onFailed(fp,__FILE__, __LINE__, "unexpected version [%s] V[%d]", fileNamePath, headerMain.version);
    #endif
        }
        else if (!this->fillAnimation_2(headerMesh, fileNamePath, fp))
        {
            return false;
        }
        this->buffer          = new BUFFER_MESH[headerMesh.totalFrames];
        this->totalFramesMesh = static_cast<uint32_t>(headerMesh.totalFrames);

        // Loop principal atraves de todos os frames deste arquivo -----------------------------------------------
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            std::vector<TEXTURE*>  lsIdTexture;
            std::vector<uint8_t> lsHasColorKeying;
            // 5 Sequencia lógica dos frames --------------------------------------------------------------------------
            // Cada header Frame
            // --------------------------------------------------------------------------------------------------
            util::HEADER_FRAME headerFrame;
            if (!read_frame_headers_and_subsets(
                    fp,
                    fileNamePath,
                    headerMain.version,
                    headerFrame,
                    [&](util::HEADER_FRAME &header) -> bool
                    {
                        buffer[currentFrame].subset      = new util::SUBSET[header.totalSubset];
                        buffer[currentFrame].totalSubset = static_cast<uint32_t>(header.totalSubset);
                        return true;
                    },
                    [&](const util::HEADER_FRAME &, const int i, util::HEADER_DESC_SUBSET &headerDescSubset) -> bool
                    {
                        buffer[currentFrame].subset[i].vertexStart = headerDescSubset.vertexStart;
                        buffer[currentFrame].subset[i].indexStart  = headerDescSubset.indexStart;
                        buffer[currentFrame].subset[i].indexCount  = headerDescSubset.indexCount;
                        buffer[currentFrame].subset[i].vertexCount = headerDescSubset.vertexCount;
                        if (strcmp(headerDescSubset.nameTexture, "default") != 0)
                        {
                            char *pch = strchr(headerDescSubset.nameTexture, '#');
                            if (pch && pch[0] == '#' && pch[1] == 'u')
                            {
                                pch[0] = 0;
                                util::HEADER_IMG headerImg;
                                headerImg.lenght = 0;
                                if (!util::readHeaderImgV8(fp, headerImg))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header image [%s]", fileNamePath);
                                if (headerImg.lenght == 0)
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header image [%s]", fileNamePath);
                                auto data = new uint8_t[headerImg.lenght];
                                if (!fread(data, static_cast<size_t>(headerImg.lenght), 1, fp))
                                {
                                    delete [] data;
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image [%s]", fileNamePath);
                                }
                                uint32_t sizeOfImage = 0;
                                this->depthUberImage     = static_cast<uint8_t>(headerImg.depth);
                                if (headerImg.channel != 4 && headerImg.channel != 3 && headerImg.channel != 0)
                                {
                                    delete [] data;
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image! Channel != [3 || 4] [%s]",
                                                             fileNamePath);
                                }
                                headerImg.channel = headerImg.channel == 4 ? 4 : 3;
                                switch (headerImg.depth)
                                {
                                    case 3:
                                    {
                                        sizeOfImage = 3 * headerImg.channel * headerImg.width * headerImg.height;
                                        while (sizeOfImage % 8)
                                        {
                                            sizeOfImage++;
                                        }
                                        sizeOfImage = sizeOfImage / 8;
                                    }
                                    break;
                                    case 4:
                                    {

                                        sizeOfImage = 4 * headerImg.channel * headerImg.width * headerImg.height;
                                        while (sizeOfImage % 8)
                                        {
                                            sizeOfImage++;
                                        }
                                        sizeOfImage = sizeOfImage / 8;
                                    }
                                    break;
                                    case 8: { sizeOfImage = headerImg.width * headerImg.height * headerImg.channel;}
                                    break;
                                    default:
                                    {
                                        delete [] data;
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image [%s]", fileNamePath);
                                    }
                                }
                                MINIZ miniz;
                                if (miniz.decompressStream(data, headerImg.lenght, sizeOfImage))
                                {
                                    delete[] data;
                                    TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
                                    buffer[currentFrame].subset[i].texture = textureManager->load(
                                        headerImg.width, headerImg.height, miniz.getDataStreamOut(), headerDescSubset.nameTexture,
                                        headerImg.depth, headerImg.channel, headerImg.hasAlpha ? true : false);
                                    if (buffer[currentFrame].subset[i].texture == nullptr)
                                    {
                                        lsIdTexture.push_back(nullptr);
                                        lsHasColorKeying.push_back(0);
#if defined _DEBUG
                                        PRINT_IF_DEBUG( "error on creating texture: %s", fileName.c_str());
#endif
                                    }
                                    else
                                    {
                                        lsIdTexture.push_back(buffer[currentFrame].subset[i].texture);
                                        lsHasColorKeying.push_back(buffer[currentFrame].subset[i].texture->useAlphaChannel ? 1: 0);
                                    }
                                }
                                else
                                {
                                    delete[] data;
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to uncompress file [%s]", fileNamePath);
                                }
                            }
                            else
                            {
                                pch = strchr(headerDescSubset.nameTexture, '#');
                                if (pch && pch[0] == '#' && pch[1] == 'M') // Material apenas .. cor como string
                                {
                                    pch = &pch[2];
                                    //util::MATERIAL mat;
                                    //memset(&mat, 0, sizeof(mat));
                                    std::vector<std::string> result;
                                    util::split(result, pch, '|');

                                    if (result.size() == 5)
                                    {
                                        COLOR colorAculm(0.0f, 0.0f, 0.0f, 0.0f);
                                        int        totalSum = 0;
                                        for (auto & n : result)
                                        {
                                            const char *strTemp   = n.c_str();
                                            //char        letter    = strTemp[0];
                                            const char *strNumber = &strTemp[1];
                                            COLOR  color     = static_cast<uint32_t>(strtol(strNumber, nullptr, 16));
                                            if (color.r > 0.0f || color.g > 0.0f || color.b > 0.0f)
                                            {
                                                colorAculm.r += color.r;
                                                colorAculm.g += color.g;
                                                colorAculm.b += color.b;
                                                totalSum++;
                                            }
                                            //switch (letter)
                                            //{
                                            //    case 'A': { mat.Ambient = color;
                                            //    }
                                            //    break;
                                            //    case 'D': { mat.Diffuse = color;
                                            //    }
                                            //    break;
                                            //    case 'E': { mat.Emissive = color;
                                            //    }
                                            //    break;
                                            //    case 'S': { mat.Specular = color;
                                            //    }
                                            //    break;
                                            //    case 'P': { mat.Power = static_cast<float>(atof(strNumber));
                                            //    }
                                            //    break;
                                            //    default: break;
                                            //}
                                        }
                                        if (totalSum)
                                        { // potências de 2 (2,4,8,16,32,64,128,256, 512,1024)...
                                            TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
                                            colorAculm.r = colorAculm.r / static_cast<float>(totalSum);
                                            colorAculm.g = colorAculm.g / static_cast<float>(totalSum);
                                            colorAculm.b = colorAculm.b / static_cast<float>(totalSum);
                                            uint8_t dataARGB[16];
                                            uint32_t  dwR =
                                                colorAculm.r >= 1.0f
                                                    ? 0xff
                                                    : colorAculm.r <= 0.0f ? 0x00 : static_cast<uint32_t>(colorAculm.r * 255.0f + 0.5f);
                                            uint32_t dwG =
                                                colorAculm.g >= 1.0f
                                                    ? 0xff
                                                    : colorAculm.g <= 0.0f ? 0x00 : static_cast<uint32_t>(colorAculm.g * 255.0f + 0.5f);
                                            uint32_t dwB =
                                                colorAculm.b >= 1.0f
                                                    ? 0xff
                                                    : colorAculm.b <= 0.0f ? 0x00 : static_cast<uint32_t>(colorAculm.b * 255.0f + 0.5f);
                                            for (int pixel = 0; pixel < 12; pixel += 4)
                                            {
                                                dataARGB[pixel]     = static_cast<uint8_t>(255);
                                                dataARGB[pixel + 1] = static_cast<uint8_t>(dwR);
                                                dataARGB[pixel + 2] = static_cast<uint8_t>(dwG);
                                                dataARGB[pixel + 3] = static_cast<uint8_t>(dwB);
                                            }
                                            buffer[currentFrame].subset[i].texture =
                                                textureManager->load(2, 2, dataARGB, headerDescSubset.nameTexture, 8, 4);
                                            if (buffer[currentFrame].subset[i].texture)
                                            {
                                                lsIdTexture.push_back(buffer[currentFrame].subset[i].texture);
                                                lsHasColorKeying.push_back(
                                                    buffer[currentFrame].subset[i].texture->useAlphaChannel ? 1 : 0);
                                            }
                                            else
                                            {
                                                lsIdTexture.push_back(nullptr);
                                                lsHasColorKeying.push_back(0);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        buffer[currentFrame].subset[i].texture = nullptr;
                                        lsIdTexture.push_back(nullptr);
                                        lsHasColorKeying.push_back(0);
                                    }
                                }
                                else if(this->getInfoFont() != nullptr)
                                {
                                    TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
                                    const INFO_BOUND_FONT* pInfoFont = this->getInfoFont();
                                    const size_t len = strlen(headerDescSubset.nameTexture);
                                    if(len > 4 && strcasecmp(&headerDescSubset.nameTexture[len-4],".ttf")==0)
                                    {
                                        buffer[currentFrame].subset[i].texture = textureManager->loadTTF(headerDescSubset.nameTexture,nullptr,nullptr,pInfoFont->heightLetter,true);
                                    }
                                    else
                                    {
                                        buffer[currentFrame].subset[i].texture = textureManager->load(
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
                                            headerDescSubset.nameTexture, true);
                                            headerDescSubset.hasAlphaColor = 1;
#else
                                            headerDescSubset.nameTexture, headerDescSubset.hasAlphaColor ? true : false);
#endif
                                    }
                                    if (!buffer[currentFrame].subset[i].texture)
                                    {
                                        lsIdTexture.push_back(0);
                                        lsHasColorKeying.push_back(0);
                                    }
                                    else
                                    {
                                        lsIdTexture.push_back(buffer[currentFrame].subset[i].texture);
                                        lsHasColorKeying.push_back(buffer[currentFrame].subset[i].texture->useAlphaChannel ? 1: 0);
                                    }
                                }
                                else
                                {
                                    TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
                                    buffer[currentFrame].subset[i].texture = textureManager->load(
                                        headerDescSubset.nameTexture, headerDescSubset.hasAlphaColor ? true : false);
                                    if (!buffer[currentFrame].subset[i].texture)
                                    {
                                        lsIdTexture.push_back(nullptr);
                                        lsHasColorKeying.push_back(0);
                                    }
                                    else
                                    {
                                        lsIdTexture.push_back(buffer[currentFrame].subset[i].texture);
                                        lsHasColorKeying.push_back(buffer[currentFrame].subset[i].texture->useAlphaChannel ? 1: 0);
                                    }
                                }
                            }
                        }
                        if (!readMaterialTextureSlotsRuntime(fp,
                                                             fileNamePath,
                                                             headerMain.version,
                                                             headerDescSubset.materialTextureSlotCount,
                                                             buffer[currentFrame].subset[i]))
                            return false;
                        return true;
                    }))
                return false;

            if (!read_frame_geometry(
                    fp,
                    fileNamePath,
                    headerFrame,
                    headerMesh.hasNorText,
                    headerMain.version,
                    [this](FILE *file,
                           const int sizeVertexBuffer,
                           VEC3 **positionOut,
                           VEC3 **normalOut,
                           VEC2 **textureOut,
                           int16_t hasNorText[2],
                           uint16_t *indexArray,
                           const int sizeArrayIndex,
                           const int stride,
                           const int fileVersion) -> bool
                    {
                        return this->loadFromSeparatedBuffers(file,
                                                     sizeVertexBuffer,
                                                     positionOut,
                                                     normalOut,
                                                     textureOut,
                                                     hasNorText,
                                                     indexArray,
                                                     sizeArrayIndex,
                                                     stride,
                                                     fileVersion);
                    },
                    [&](VEC3 *&pPosition, VEC3 *&pNormal, VEC2 *&pTexture, uint16_t *&indexArray) -> bool
                    {
                        buffer[currentFrame].pBufferGL = new BUFFER_GL();
                        auto indexStart = new int[buffer[currentFrame].totalSubset];
                        auto indexCount = new int[buffer[currentFrame].totalSubset];
                        for (int subIndex = 0; subIndex < headerFrame.totalSubset; ++subIndex)
                        {
                            indexStart[subIndex] = buffer[currentFrame].subset[subIndex].indexStart;
                            indexCount[subIndex] = buffer[currentFrame].subset[subIndex].indexCount;
                        }
                        if (!buffer[currentFrame].pBufferGL->loadBuffer(pPosition,
                                                                        pNormal,
                                                                        pTexture,
                                                                        static_cast<uint32_t>(headerFrame.sizeVertexBuffer),
                                                                        indexArray,
                                                                        buffer[currentFrame].totalSubset,
                                                                        indexStart,
                                                                        indexCount,
                                                                        &this->info_mode))

                        {
                            delete[] indexStart;
                            delete[] indexCount;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "error on load buffer bufferTriangleList [%s]", fileNamePath);
                        }
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
                        this->fillLegacyPhysicsIfNeeded(headerMain.version,
                                                        deprectedInfoSprite,
                                                        pPosition,
                                                        static_cast<uint32_t>(currentFrame),
                                                        buffer[currentFrame].subset);
#endif
                        delete[] indexStart;
                        delete[] indexCount;
                        return true;
                    },
                    [&](VEC3 *&pPosition, VEC3 *&pNormal, VEC2 *&pTexture) -> bool
                    {
                        constexpr bool isDynamic = false;
                        buffer[currentFrame].pBufferGL = new BUFFER_GL();
                        auto vertexStart               = new int[buffer[currentFrame].totalSubset];
                        auto vertexCount               = new int[buffer[currentFrame].totalSubset];
                        for (int subIndex = 0; subIndex < headerFrame.totalSubset; ++subIndex)
                        {
                            vertexStart[subIndex] = buffer[currentFrame].subset[subIndex].vertexStart;
                            vertexCount[subIndex] = buffer[currentFrame].subset[subIndex].vertexCount;
                        }
                        if (!buffer[currentFrame].pBufferGL->loadBuffer(
                                pPosition,
                                pNormal,
                                pTexture,
                                static_cast<uint32_t>(headerFrame.sizeVertexBuffer),
                                buffer[currentFrame].totalSubset,
                                vertexStart,
                                vertexCount,
                                &this->info_mode,
                                isDynamic))
                        {
                            delete[] vertexStart;
                            delete[] vertexCount;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "error on load buffer bufferTriangleList [%s]", fileNamePath);
                        }
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
                        this->fillLegacyPhysicsIfNeeded(headerMain.version,
                                                        deprectedInfoSprite,
                                                        pPosition,
                                                        static_cast<uint32_t>(currentFrame),
                                                        buffer[currentFrame].subset);
#endif
                        delete[] vertexStart;
                        delete[] vertexCount;
                        return true;
                    }))
                return false;
            const std::vector<int>::size_type  totalIdTexture = ((buffer[currentFrame].pBufferGL->totalSubset > lsIdTexture.size())
                                                     ? lsIdTexture.size()
                                                     : buffer[currentFrame].pBufferGL->totalSubset);
            for (std::vector<int>::size_type i = 0; i < totalIdTexture; ++i)
            {
                buffer[currentFrame].pBufferGL->setTextureByStage(lsIdTexture[i], 0, static_cast<uint32_t>(i));
                const util::SUBSET &subsetRuntime = buffer[currentFrame].subset[i];
                const std::vector<util::MATERIAL_TEXTURE_SLOT_HEADER>::size_type totalMaterialTextures =
                    subsetRuntime.materialTextureSlotHeaders.size() < subsetRuntime.materialTextures.size()
                        ? subsetRuntime.materialTextureSlotHeaders.size()
                        : subsetRuntime.materialTextures.size();
                for (std::vector<util::MATERIAL_TEXTURE_SLOT_HEADER>::size_type materialIndex = 0;
                     materialIndex < totalMaterialTextures;
                     ++materialIndex)
                {
                    if (subsetRuntime.materialTextureSlotHeaders[materialIndex].type == util::MATERIAL_TEXTURE_SLOT_NORMAL)
                    {
                        buffer[currentFrame].pBufferGL->setTextureByStage(
                            subsetRuntime.materialTextures[materialIndex], 2, static_cast<uint32_t>(i));
                        break;
                    }
                }
            }
        }
        fclose(fp);
        fp             = nullptr;
        positionOffset = VEC3(headerMesh.posX, headerMesh.posY, headerMesh.posZ);
        angleDefault   = VEC3(headerMesh.angleX, headerMesh.angleY, headerMesh.angleZ);
        remove(util::getDecompressModelFileName());

        this->sizeCoordTexFrame_0 = 0;
        if (this->coordTexFrame_0)
            delete[] this->coordTexFrame_0;
        this->coordTexFrame_0 = nullptr;

        if (renderizable)
        {
            renderizable->getPosition() += this->positionOffset;
            renderizable->setAngle(this->angleDefault);
        }
        return true;
    }
    
    void MESH_MBM::invertMap(const bool u, const bool v, VEC2 *pTexture, const uint32_t arraySize)
    {
        float maxU = -FLT_MAX;
        float maxV = -FLT_MAX;
        float minU = FLT_MAX;
        float minV = FLT_MAX;
        for (uint32_t k = 0; k < arraySize; ++k)
        {
            if (pTexture[k].x > maxU)
                maxU = pTexture[k].x;
            if (pTexture[k].y > maxV)
                maxV = pTexture[k].y;

            if (pTexture[k].x < minU)
                minU = pTexture[k].x;
            if (pTexture[k].y < minV)
                minV = pTexture[k].y;
        }
        const float diffU = maxU - minU;
        const float diffV = maxV - minV;
        for (uint32_t k = 0; k < arraySize; ++k)
        {
            if (u)
            {
                float perc    = (pTexture[k].x - minU) / diffU;
                pTexture[k].x = ((1.0f - perc) * diffU) + minU;
            }
            if (v)
            {
                float perc    = (pTexture[k].y - minV) / diffV;
                pTexture[k].y = ((1.0f - perc) * diffV) + minV;
            }
        }
    }
    
    bool MESH_MBM::loadFromSeparatedBuffers(FILE *fp, const int sizeVertexBuffer, VEC3 **positionOut,
                                VEC3 **normalOut, VEC2 **textureOut, int16_t hasNorText[2],
                                uint16_t *indexArray, const int sizeArrayIndex, const int stride,
                                int fileVersion)
    {
        return load_from_separated_buffers_common(fp,
                                        sizeVertexBuffer,
                                        positionOut,
                                        normalOut,
                                        textureOut,
                                        hasNorText,
                                        indexArray,
                                        sizeArrayIndex,
                                        stride,
                                        fileVersion,
                                        this->coordTexFrame_0,
                                        this->sizeCoordTexFrame_0);
    }
    
    bool MESH_MBM::fillAnimation_2(util::HEADER_MESH &headerMesh, const char *fileNamePath, FILE *fp)
    {
        return fill_animation_headers_common(fp,
                                             fileNamePath,
                                             headerMesh.totalAnimation,
                                             this->infoAnimation);
    }

    const INFO_BOUND_FONT* MESH_MBM::getInfoFont()const
    {
        if(this->typeMe == util::TYPE_MESH_FONT)
            return static_cast<INFO_BOUND_FONT*>(this->extraInfo);
        return nullptr;
    }

    const std::vector<util::STAGE_PARTICLE*>* MESH_MBM::getInfoParticle()const
    {
        if(this->typeMe == util::TYPE_MESH_PARTICLE)
            return static_cast<std::vector<util::STAGE_PARTICLE*>*>(this->extraInfo);
        return nullptr;
    }

    const util::BTILE_INFO* MESH_MBM::getInfoTile()const
    {
        if(this->typeMe == util::TYPE_MESH_TILE_MAP)
            return static_cast<util::BTILE_INFO*>(this->extraInfo);
        return nullptr;
    }

        API_IMPL const util::DYNAMIC_SHAPE* MESH_MBM::getInfoShape()const
        {
            if(this->typeMe == util::TYPE_MESH_SHAPE)
                 return static_cast<util::DYNAMIC_SHAPE*>(this->extraInfo);
            return nullptr;
        }

    MESH_MANAGER::MESH_MANAGER()
        : impl(std::make_unique<Impl>())
    {
    }

    MESH_MANAGER * MESH_MANAGER::getInstance()
    {
        if (instanceMeshManager == nullptr)
        {
            instanceMeshManager = new MESH_MANAGER();
        }
        return instanceMeshManager;
    }
    
    void MESH_MANAGER::release()
    {
        if (instanceMeshManager)
            delete instanceMeshManager;
        instanceMeshManager = nullptr;
    }
    
    void MESH_MANAGER::fakeRelease(const char* fileName)
    {
        const std::string fileNameBase = util::getBaseName(fileName);
        MESH_MBM *ptr = this->impl->lsMeshes[fileNameBase];
        if (ptr)
        {
            this->impl->lsFakeRelease.push_back(ptr);
            this->impl->lsMeshes[fileNameBase] = nullptr;
        }
    }

    MESH_MBM * MESH_MANAGER::getIfExists(const char* fileName)
    {
        std::string fileNameBase = util::getBaseName(fileName);
        auto mesh = this->impl->lsMeshes[fileNameBase];
        return mesh;
    }
    
    MESH_MBM * MESH_MANAGER::load(const char *fileName)
    {
        return this->load(fileName, nullptr);
    }

    MESH_MBM * MESH_MANAGER::load(const char *fileName, RENDERIZABLE *renderizable)
    {
        std::string fileNameBase = util::getBaseName(fileName);
        auto mesh = this->impl->lsMeshes[fileNameBase];
        if(mesh)
        {
            if (renderizable)
            {
                renderizable->getPosition() += mesh->positionOffset;
                renderizable->setAngle(mesh->angleDefault);
            }
            return mesh;
        }
        mesh = new MESH_MBM();
        if (mesh->load(fileName, renderizable))
        {
            this->impl->lsMeshes[fileNameBase] = mesh;
            return mesh;
        }
        else
        {
            delete mesh;
#if defined _DEBUG
            PRINT_IF_DEBUG( "failed to load mesh");
#endif
            return nullptr;
        }
    }
    
    MESH_MBM * MESH_MANAGER::loadTrueTypeFont(const char *fileNameTtf, const float heightLetter, const short spaceWidth,
                                      const short spaceHeight,const bool saveTextureAsPng, TEXTURE ** texture_loaded)
    {
        if (fileNameTtf == nullptr)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG( "filename null.");
#endif
            return nullptr;
        }

        auto fillvertexQuadTrueFont = [](VEC3 *_position, const float width, const float height, const float diffY) -> void
        {
            const float x  = width * 0.5f;
            const float y  = height * 0.5f;
            _position[0].x = -x;
            _position[0].y = -y - diffY;
            _position[0].z = 0;

            _position[1].x = -x;
            _position[1].y = y - diffY;
            _position[1].z = 0;

            _position[2].x = x;
            _position[2].y = -y - diffY;
            _position[2].z = 0;

            _position[3].x = x;
            _position[3].y = y - diffY;
            _position[3].z = 0;
        };

        std::string fileNameBase = util::getBaseName(fileNameTtf);
        char measure[255]="";
        snprintf(measure,sizeof(measure),"%0.2f|%d|%d#",heightLetter,spaceWidth,spaceHeight);
        std::string fileNameBaseSuppose(measure);
        fileNameBaseSuppose += fileNameBase;
        auto mesh = this->impl->lsMeshes[fileNameBaseSuppose];
        if(mesh)
            return mesh;
        mesh = new MESH_MBM();
        std::vector<stbtt_aligned_quad *> lsStbFont;
        std::vector<VEC2>                 lsWidthLetter;

        TEXTURE *texture = TEXTURE_MANAGER::getInstance()->loadTTF(fileNameTtf, &lsStbFont, &lsWidthLetter, heightLetter,saveTextureAsPng);
        if (texture == nullptr || lsStbFont.size() < 30)
        {
            delete mesh;
            return nullptr;
        }
        if(texture_loaded != nullptr)
            *texture_loaded = texture;
        auto tTotalSTB = static_cast<uint32_t>(lsStbFont.size() - 30);
        VEC3         pPosition[4];
        VEC3*        pNormal = nullptr; // no normal for font, only position and texture
        VEC2         pTexture[4];

        /*for (auto & i : pNormal)
        {
            i.x = 0;
            i.y = 0;
            i.z = 1;
        }*/

        mesh->buffer                       = new BUFFER_MESH[tTotalSTB];
        mesh->totalFramesMesh              = tTotalSTB;
        uint16_t    indexQuad[6] = {0, 1, 2, 2, 1, 3};
        auto* infoFont          = new INFO_BOUND_FONT();
        mesh->extraInfo					   = infoFont;
        infoFont->spaceXCharacter          = spaceWidth;
        infoFont->spaceYCharacter          = spaceHeight;
        infoFont->heightLetter             = static_cast<unsigned short>(heightLetter);

        infoFont->fontName = fileNameTtf;
        std::size_t p      = infoFont->fontName.find_last_of(util::getCharDirSeparator());
        if (p != std::string::npos)
            infoFont->fontName.erase(0, p + 1);
        const float middleHeight = 'M' <= lsWidthLetter.size() ? lsWidthLetter['M'].y : 0;
        if(lsWidthLetter.size())
        {
            lsWidthLetter[' '].x  = lsWidthLetter['M'].x;
            lsWidthLetter[' '].y  = lsWidthLetter['M'].y;

            lsWidthLetter['\t'].x  = lsWidthLetter['M'].x * 4.0f;
            lsWidthLetter['\t'].y  = lsWidthLetter['M'].y * 4.0f;
        }
        
        for (uint32_t i = 30, index = 0; i < lsStbFont.size(); ++i)
        {
            stbtt_aligned_quad *q = lsStbFont[i];
            if (q)
            {
                const float y  = lsWidthLetter[i].y;
                float       dy = (middleHeight - y) * 0.5f;
                switch (i)
                {
                    case '*': dy = 0; break;
                    case '-': dy = 0; break;
                    case '+': dy = 0; break;
                    case '=': dy = 0; break;
                    case '<': dy = 0; break;
                    case '>': dy = 0; break;
                    case ':': dy = 0; break;
                    case '|': dy = 0; break;
                    case '~': dy = 0; break;
                    case '\'':
                        dy = -dy;
                        break; //'
                    case 22:
                        dy = -dy;
                        break; //"
                    case '\"':
                        dy = -dy;
                        break; //"
                    case ';':
                        dy = y * 0.5f;
                        break; //;
                    case 162:
                        dy = y * 0.10f;
                        break; //¢
                    case 185:
                        dy = -dy;
                        break; //¹
                    case 186:
                        dy = -dy;
                        break; //º
                    case 187:
                        dy = 0;
                        break; //»
                    case 170:
                        dy = -dy;
                        break; //ª
                    case 171:
                        dy = 0;
                        break; //«
                    case 172:
                        dy *= -0.75f;
                        break; //¬
                    case 176:
                        dy = -dy;
                        break; //°
                    case 178:
                        dy = -dy;
                        break; //²
                    case 179:
                        dy = -dy;
                        break; //³
                    default: break;
                }
                fillvertexQuadTrueFont(pPosition, lsWidthLetter[i].x, y, dy);

                pTexture[0].x = q->s0;
                pTexture[0].y = q->t1;
                pTexture[1].x = q->s0;
                pTexture[1].y = q->t0;
                pTexture[2].x = q->s1;
                pTexture[2].y = q->t1;
                pTexture[3].x = q->s1;
                pTexture[3].y = q->t0;

                mesh->buffer[index].pBufferGL            = new BUFFER_GL();
                mesh->buffer[index].subset               = new util::SUBSET[1];
                mesh->buffer[index].totalSubset          = 1;
                mesh->buffer[index].subset[0].indexCount = 6;
                
                if (mesh->buffer[index].pBufferGL->loadBuffer(
                        pPosition, pNormal, pTexture, 4, indexQuad, mesh->buffer[index].totalSubset,
                        &mesh->buffer[index].subset[0].indexStart, &mesh->buffer[index].subset[0].indexCount,nullptr))
                {

                    mesh->buffer[index].subset[0].texture        = texture;
                    mesh->buffer[index].pBufferGL->setTextureByStage(texture, 0, 0);
                    infoFont->letter[i].detail                   = new util::DETAIL_LETTER();
                    infoFont->letter[i].detail->indexFrame       = static_cast<uint8_t>(index);
                    infoFont->letter[i].detail->widthLetter      = static_cast<uint8_t>(lsWidthLetter[i].x);
                    infoFont->letter[i].detail->heightLetter     = static_cast<uint8_t>(lsWidthLetter[i].y);
                    infoFont->letter[i].detail->letter           = static_cast<uint8_t>(i);
                    ++index;
                }
                else
                {

                    PRINT_IF_DEBUG( "error on load buffer bufferTriangleList [%s]", fileNameTtf);
                    delete mesh;
                    mesh = nullptr;
                    for(auto qq : lsStbFont)
                    {
                        if(qq)
                            delete qq;
                    }
                    break;
                }
            }
        }
        for (auto q : lsStbFont)
        {
            if (q)
                delete q;
        }
        if (mesh)
        {
            mesh->positionOffset                    = VEC3(0, 0, 0);
            mesh->angleDefault                      = VEC3(0, 0, 0);
            mesh->typeMe                            = util::TYPE_MESH_FONT;
            auto header = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            mesh->infoAnimation.lsHeaderAnim.push_back(header);
            header->headerAnim             = new util::HEADER_ANIMATION();
            header->headerAnim->hasShaderEffect = 1; // always will be 1
            this->impl->lsMeshes[fileNameBaseSuppose] = mesh;
            const char *fontps                 = "font.ps";
            header->headerAnim->typeAnimation  = 1;
            mesh->hasNormTex[0] = HAS_NOR_IN_FILE;//has normal
            mesh->hasNormTex[1] = HAS_TEX_EACH_FRAME;//uv each frame
            strncpy(header->headerAnim->nameAnimation,"font-1",sizeof(header->headerAnim->nameAnimation)-1);
            auto effectFont = new util::INFO_FX();
            header->effectShader = effectFont;
            effectFont->dataPS = new util::INFO_SHADER_DATA(4, static_cast<int>(strlen(fontps) + 1), 0);
            strcpy(effectFont->dataPS->fileNameShader, fontps);
            const float mmin[4]               = {1.0f, 1.0f, 0.0f, 0.0f};
            const float mmax[4]               = {1.0f, 1.0f, 0.0f, 0.0f};
            effectFont->dataPS->typeAnimation = 6; // recursive loop
            effectFont->dataPS->timeAnimation = 1; // seconds
            effectFont->dataPS->typeVars[0]   = VAR_COLOR_RGB;
            memcpy(effectFont->dataPS->min, mmin, sizeof(mmin));
            memcpy(effectFont->dataPS->max, mmax, sizeof(mmax));
            mesh->fileName = std::move(fileNameBaseSuppose);
            
        }
        return mesh;
    }
    
    MESH_MBM * MESH_MANAGER::load(const char *nickName, float *pPosition, float *pNormal, float *pTexture,
                          const uint32_t sizeVertexBuffer,const util::INFO_DRAW_MODE * info_mode)
    {
        const std::string fileNameBase = util::getBaseName(nickName);
        auto mesh = this->impl->lsMeshes[fileNameBase];
        if(mesh)
            return mesh;
        constexpr bool isDynamic              = false;
        mesh                                  = new MESH_MBM();
        mesh->buffer                          = new BUFFER_MESH[1];
        mesh->totalFramesMesh                 = 1;
        mesh->buffer[0].pBufferGL             = new BUFFER_GL();
        mesh->buffer[0].subset                = new util::SUBSET[1];
        mesh->buffer[0].totalSubset           = 1;
        mesh->buffer[0].subset[0].vertexCount = sizeVertexBuffer / 3;

        {
            std::string which_mode;
            if(info_mode && is_any_mode_valid(*info_mode,which_mode) == false)
            {
                ERROR_LOG( "Invalid mode %s detected:[%s]", which_mode.c_str(),nickName);
                delete mesh;
                return nullptr;
            }
        }

        if (!mesh->buffer[0].pBufferGL->loadBuffer(
                reinterpret_cast<VEC3 *>(pPosition), reinterpret_cast<VEC3 *>(pNormal), reinterpret_cast<VEC2 *>(pTexture), sizeVertexBuffer / 3, mesh->buffer[0].totalSubset,
                &mesh->buffer[0].subset[0].vertexStart, &mesh->buffer[0].subset[0].vertexCount,info_mode, isDynamic))
        {
            ERROR_LOG( "error on load buffer bufferTriangleList [%s]", nickName);
            delete mesh;
            return nullptr;
        }

        mesh->positionOffset = VEC3(0, 0, 0);
        mesh->angleDefault   = VEC3(0, 0, 0);
        mesh->typeMe         = util::TYPE_MESH_SHAPE;
        if(info_mode)
        {
            mesh->info_mode.mode_draw = info_mode->mode_draw;
            mesh->info_mode.mode_cull_face = info_mode->mode_cull_face;
            mesh->info_mode.mode_front_face_direction = info_mode->mode_front_face_direction;
        }
        this->impl->lsMeshes[fileNameBase] = mesh;
        return mesh;
    }
    
    MESH_MBM * MESH_MANAGER::loadIndex(const char *nickName, float *pPosition, float *pNormal, float *pTexture,
                               const uint32_t sizeVertexBuffer, uint16_t *index,
                               const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        const std::string fileNameBase = util::getBaseName(nickName);
        auto mesh = this->impl->lsMeshes[fileNameBase];
        if(mesh)
            return mesh;
        mesh                                  = new MESH_MBM();
        mesh->buffer                          = new BUFFER_MESH[1];
        mesh->totalFramesMesh                 = 1;
        mesh->buffer[0].pBufferGL             = new BUFFER_GL();
        mesh->buffer[0].subset                = new util::SUBSET[1];
        mesh->buffer[0].totalSubset           = 1;
        mesh->buffer[0].subset[0].vertexCount = sizeVertexBuffer / 3;
        mesh->buffer[0].subset[0].indexCount  = static_cast<int>(sizeIndex);

        std::string which_mode;
        if(info_draw_mode && is_any_mode_valid(*info_draw_mode,which_mode) == false)
        {
            ERROR_LOG( "Invalid mode [%s] detected for [%s]", which_mode.c_str(),nickName);
            delete mesh;
            return nullptr;
        }

        if (!mesh->buffer[0].pBufferGL->loadBuffer(reinterpret_cast<VEC3*>(pPosition),reinterpret_cast<VEC3 *>(pNormal), reinterpret_cast<VEC2 *>(pTexture),
                                                   sizeVertexBuffer / 3, index, mesh->buffer[0].totalSubset,
                                                   &mesh->buffer[0].subset[0].indexStart,
                                                   &mesh->buffer[0].subset[0].indexCount,info_draw_mode))
        {
            ERROR_LOG( "error on load buffer bufferTriangleList [%s]", nickName);
            delete mesh;
            return nullptr;
        }

        mesh->positionOffset = VEC3(0, 0, 0);
        mesh->angleDefault   = VEC3(0, 0, 0);
        mesh->typeMe         = util::TYPE_MESH_SHAPE;
        if(info_draw_mode)
        {
            mesh->info_mode.mode_draw = info_draw_mode->mode_draw;
            mesh->info_mode.mode_cull_face = info_draw_mode->mode_cull_face;
            mesh->info_mode.mode_front_face_direction = info_draw_mode->mode_front_face_direction;
        }
        this->impl->lsMeshes[fileNameBase] = mesh;
        return mesh;
    }
    
    MESH_MBM * MESH_MANAGER::loadDynamicIndex(const char *nickName, const uint32_t sizeVertexBuffer,uint16_t *index, const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode,const util::DYNAMIC_SHAPE & dynamic_shape_info)
    {
        const std::string fileNameBase = util::getBaseName(nickName);
        auto mesh = this->impl->lsMeshes[fileNameBase];
        if (mesh == nullptr)
            mesh = new MESH_MBM();
        else
            mesh->release();
        mesh->buffer                          = new BUFFER_MESH[1];
        mesh->totalFramesMesh                 = 1;
        mesh->buffer[0].pBufferGL             = new BUFFER_GL();
        mesh->buffer[0].subset                = new util::SUBSET[1];
        mesh->buffer[0].totalSubset           = 1;
        mesh->buffer[0].subset[0].vertexCount = sizeVertexBuffer / 3;
        mesh->buffer[0].subset[0].indexCount  = static_cast<int>(sizeIndex);
        const bool hasNormal                  = dynamic_shape_info.size_normal > 0;
        const bool hasUv                      = dynamic_shape_info.size_uv > 0;

        std::string which_mode;
        if(info_draw_mode && is_any_mode_valid(*info_draw_mode,which_mode) == false)
        {
            ERROR_LOG( "Invalid mode [%s] detected for [%s]", which_mode.c_str(),nickName);
            delete mesh;
            return nullptr;
        }

        if (!mesh->buffer[0].pBufferGL->loadBufferDynamic(index, mesh->buffer[0].totalSubset,
                                                          &mesh->buffer[0].subset[0].indexStart,
                                                          &mesh->buffer[0].subset[0].indexCount, 
                                                          hasNormal, hasUv, info_draw_mode))
        {
            ERROR_LOG( "error on load buffer bufferTriangleList [%s]", nickName);
            delete mesh;
            return nullptr;
        }
        mesh->positionOffset = VEC3(0, 0, 0);
        mesh->angleDefault   = VEC3(0, 0, 0);
        mesh->typeMe         = util::TYPE_MESH_SHAPE;
        util::DYNAMIC_SHAPE * extra_info_shape = new util::DYNAMIC_SHAPE(dynamic_shape_info.dynamicVertex,dynamic_shape_info.dynamicNormal,dynamic_shape_info.dynamicUV,dynamic_shape_info.size_vertex,dynamic_shape_info.size_normal,dynamic_shape_info.size_uv);
        mesh->extraInfo      = extra_info_shape;
        mesh->fileName       = fileNameBase;
        if(info_draw_mode)
        {
            mesh->info_mode.mode_draw = info_draw_mode->mode_draw;
            mesh->info_mode.mode_cull_face = info_draw_mode->mode_cull_face;
            mesh->info_mode.mode_front_face_direction = info_draw_mode->mode_front_face_direction;
        }
        this->impl->lsMeshes[fileNameBase] = mesh;
        return mesh;
    }

    MESH_MANAGER::~MESH_MANAGER()
    {
        for (const auto & i : this->impl->lsMeshes)
        {
            MESH_MBM *ptr = i.second;
            if (ptr)
            {
                ptr->release();
                delete ptr;
                ptr = nullptr;
            }
        }
        for (auto ptr : this->impl->lsFakeRelease)
        {
            if (ptr)
            {
                delete ptr;
            }
        }
        this->impl->lsFakeRelease.clear();
    }

    const char * MESH_MANAGER::typeClassName(const util::TYPE_MESH type) noexcept
    {
        switch (type)
        {
            case util::TYPE_MESH_3D           : return "mesh";
            case util::TYPE_MESH_USER         : return "mesh user";
            case util::TYPE_MESH_SPRITE       : return "sprite";
            case util::TYPE_MESH_FONT         : return "font";
            case util::TYPE_MESH_TEXTURE      : return "texture";
            case util::TYPE_MESH_UNKNOWN      : return "unknown";
            case util::TYPE_MESH_SHAPE        : return "shape";
            case util::TYPE_MESH_PARTICLE     : return "particle";
            case util::TYPE_MESH_TILE_MAP     : return "tile map";
            default                           : return "unknown";
        }
    }

    bool MESH_MBM_DEBUG::loadDebugFromMemory(const MESH_MBM* meshMemory)
    {
        if (meshMemory == nullptr || meshMemory->isLoaded() == false)
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "Mesh empty or not loaded...");
//        auto* extensionString = (char*)glGetString(GL_EXTENSIONS);
//        if (strstr(extensionString, "GL_OES_mapbuffer") == nullptr)
//            return log_util::onFailed(nullptr, __FILE__, __LINE__, "extension [GL_OES_mapbuffer] not supported!");
//#if defined ANDROID //ANDROID //TODO fix issue not found EGL lib on ANDOID 
//        PRINT_IF_DEBUG("loadDebugFromMemory is not working on ANDOID");
//        PRINT_IF_DEBUG("TODO: fix issue not found EGL lib on ANDOID");
//        PFNGLMAPBUFFEROESPROC_TODO* glMapBufferOES = nullptr;
//        PFNGLUNMAPBUFFEROESPROC_TODO* glUnmapBufferOES = nullptr;
//#else //ANDROID //TODO fix issue not found EGL lib on ANDOID 
//        auto glMapBufferOES = (PFNGLMAPBUFFEROESPROC)eglGetProcAddress("glMapBufferOES");
//        auto glUnmapBufferOES = (PFNGLUNMAPBUFFEROESPROC)eglGetProcAddress("glUnmapBufferOES");
//#endif
//        if (glMapBufferOES == nullptr)
//            return log_util::onFailed(nullptr, __FILE__, __LINE__, "extension [glMapBufferOES] not supported!");
//        if (glUnmapBufferOES == nullptr)
//            return log_util::onFailed(nullptr, __FILE__, __LINE__, "extension [glUnmapBufferOES] not supported!");
        this->release();
        fileName = meshMemory->getFilenameMesh();
        // step 1: Verificação do header
        // -------------------------------------------------------------------------------
        switch (meshMemory->getTypeMesh())
        {
            case util::TYPE_MESH_3D:
            case util::TYPE_MESH_SHAPE:
            case util::TYPE_MESH_USER:
            case util::TYPE_MESH_SPRITE:
            case util::TYPE_MESH_TILE_MAP:
            case util::TYPE_MESH_FONT:
            case util::TYPE_MESH_PARTICLE:
                strncpy(headerMain.typeApp, get_type_app_from_mesh_type(meshMemory->getTypeMesh()), sizeof(headerMain.typeApp) - 1);
                break;
            default:
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Mesh invalid type");
                break;
        }
        strncpy(headerMain.name, MBM_HEADER_NAME_MBM, sizeof(headerMain.name) - 1);
        headerMain.version = CURRENT_VERSION_MBM_HEADER;
        headerMain.magic = 0x010203ff;
        typeMe = meshMemory->getTypeMesh();
        // step 2: --------------------------------------------------------------------------------------------------
        for (auto pCube : meshMemory->infoPhysics.lsCube)
        {
            auto cube = new CUBE(pCube->halfDim, pCube->absCenter);
            this->infoPhysics.lsCube.push_back(cube);
        }
        for (auto pBase : meshMemory->infoPhysics.lsSphere)
        {
            auto base = new SPHERE();
            base->absCenter[0] = pBase->absCenter[0];
            base->absCenter[1] = pBase->absCenter[1];
            base->absCenter[2] = pBase->absCenter[2];
            base->ray = pBase->ray;
            this->infoPhysics.lsSphere.push_back(base);
        }
        for (auto pComplex : meshMemory->infoPhysics.lsCubeComplex)
        {
            auto complex = new CUBE_COMPLEX();
            for (int k = 0; k < 8; k++)
                complex->p[k] = pComplex->p[k];
            this->infoPhysics.lsCubeComplex.push_back(complex);
        }
        for (auto pTriangle : meshMemory->infoPhysics.lsTriangle)
        {
            auto triangle = new TRIANGLE();
            triangle->point[0] = pTriangle->point[0];
            triangle->point[1] = pTriangle->point[1];
            triangle->point[2] = pTriangle->point[2];
            this->infoPhysics.lsTriangle.push_back(triangle);
        }
        if (meshMemory->getInfoFont() != nullptr)
        {
            const INFO_BOUND_FONT* pMemoryInfoFont = meshMemory->getInfoFont();
            headerMain.backBufferHeight = pMemoryInfoFont->heightLetter;
            this->extraInfo = new INFO_BOUND_FONT();
            auto* infoFont = static_cast<INFO_BOUND_FONT*>(this->extraInfo);
            util::DETAIL_HEADER_FONT headerFont;
            infoFont->fontName = pMemoryInfoFont->fontName;
            infoFont->heightLetter = pMemoryInfoFont->heightLetter;
            infoFont->spaceXCharacter = pMemoryInfoFont->spaceXCharacter;
            infoFont->spaceYCharacter = pMemoryInfoFont->spaceYCharacter;
            for (std::vector<util::DETAIL_LETTER*>::size_type j = 0; j < 255; ++j)
            {
                const util::DETAIL_LETTER* pDetailFont = pMemoryInfoFont->letter[j].detail;
                if (pMemoryInfoFont->letter[j].detail)
                {
                    auto detailFont = new util::DETAIL_LETTER();
                    detailFont->heightLetter = pDetailFont->heightLetter;
                    detailFont->indexFrame = pDetailFont->indexFrame;
                    detailFont->letter = pDetailFont->letter;
                    detailFont->widthLetter = pDetailFont->widthLetter;
                    infoFont->letter[j].detail = detailFont;
                }
            }
        }
        if (meshMemory->getInfoParticle() != nullptr)
        {
            const std::vector<util::STAGE_PARTICLE*>* thatParticleInfo = meshMemory->getInfoParticle();
            auto* lsParticleInfo = new std::vector<util::STAGE_PARTICLE*>();
            this->extraInfo = lsParticleInfo;
            for (auto thatStage : *thatParticleInfo)
            {
                auto* stage = new util::STAGE_PARTICLE(thatStage);
                lsParticleInfo->push_back(stage);
            }
        }
        if (meshMemory->getInfoTile() != nullptr)
        {
            const util::BTILE_INFO* thatInfoTile = meshMemory->getInfoTile();
            this->extraInfo = thatInfoTile->clone();
        }
        headerMesh.totalAnimation = static_cast<int32_t>(meshMemory->infoAnimation.lsHeaderAnim.size());
        for (int i = 0; i < headerMesh.totalAnimation; ++i)
        {
            const util::INFO_ANIMATION::INFO_HEADER_ANIM* pInfoAnim = meshMemory->infoAnimation.lsHeaderAnim[i];
            auto  infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            infoHead->headerAnim = new util::HEADER_ANIMATION();
            this->infoAnimation.lsHeaderAnim.push_back(infoHead);
            util::HEADER_ANIMATION* headerAnim = infoHead->headerAnim;
            headerAnim->hasShaderEffect = pInfoAnim->headerAnim->hasShaderEffect;
            headerAnim->blendState = pInfoAnim->headerAnim->blendState;
            headerAnim->initialFrame = pInfoAnim->headerAnim->initialFrame;
            headerAnim->finalFrame = pInfoAnim->headerAnim->finalFrame;
            headerAnim->timeBetweenFrame = pInfoAnim->headerAnim->timeBetweenFrame;
            headerAnim->typeAnimation = pInfoAnim->headerAnim->typeAnimation;
            strncpy(headerAnim->nameAnimation, pInfoAnim->headerAnim->nameAnimation, sizeof(headerAnim->nameAnimation));
            headerAnim->hasShaderEffect = (uint16_t)(infoHead->effectShader ? 1 : 0);
            infoHead->headerAnim->blendState = (uint16_t)headerAnim->blendState;
            //for(auto pInfoStepShader : pInfoAnim->lsStepEffectShader)
            if (infoHead->effectShader)
            {
                auto pInfoStepShader = pInfoAnim->effectShader;
                //each step may has two shaders (PS and VS)
                auto infoStepShader = new util::INFO_FX();
                infoHead->effectShader = infoStepShader;
                infoStepShader->blendOperation = pInfoStepShader->blendOperation;

                if (pInfoStepShader->dataPS)
                {
                    infoStepShader->dataPS = new util::INFO_SHADER_DATA(
                        pInfoStepShader->dataPS->lenVars * 4,
                        static_cast<int>(strlen(pInfoStepShader->dataPS->fileNameShader) + 1),
                        pInfoStepShader->dataPS->fileNameTextureStage2 ? static_cast<int>(strlen(pInfoStepShader->dataPS->fileNameTextureStage2) + 1) : 0);
                    strcpy(infoStepShader->dataPS->fileNameShader, pInfoStepShader->dataPS->fileNameShader);
                    if (infoStepShader->dataPS->fileNameTextureStage2)
                        strcpy(infoStepShader->dataPS->fileNameTextureStage2, pInfoStepShader->dataPS->fileNameTextureStage2);
                    infoStepShader->dataPS->timeAnimation = pInfoStepShader->dataPS->timeAnimation;
                    infoStepShader->dataPS->typeAnimation = pInfoStepShader->dataPS->typeAnimation;
                    for (int k = 0; k < infoStepShader->dataPS->lenVars; ++k)
                    {
                        const int index = k * 4;
                        memcpy(&infoStepShader->dataPS->max[index], &pInfoStepShader->dataPS->max[index], sizeof(float) * 4);
                        memcpy(&infoStepShader->dataPS->min[index], &pInfoStepShader->dataPS->min[index], sizeof(float) * 4);
                        infoStepShader->dataPS->typeVars[k] = pInfoStepShader->dataPS->typeVars[k];
                    }
                }
                if (pInfoStepShader->dataVS)
                {
                    infoStepShader->dataVS = new util::INFO_SHADER_DATA(
                        pInfoStepShader->dataVS->lenVars * 4,
                        static_cast<int>(strlen(pInfoStepShader->dataVS->fileNameShader) + 1),
                        pInfoStepShader->dataVS->fileNameTextureStage2 ? static_cast<int>(strlen(pInfoStepShader->dataVS->fileNameTextureStage2) + 1) : 0);
                    strcpy(infoStepShader->dataVS->fileNameShader, pInfoStepShader->dataVS->fileNameShader);
                    if (infoStepShader->dataVS->fileNameTextureStage2)
                        strcpy(infoStepShader->dataVS->fileNameTextureStage2, pInfoStepShader->dataVS->fileNameTextureStage2);
                    infoStepShader->dataVS->timeAnimation = pInfoStepShader->dataVS->timeAnimation;
                    infoStepShader->dataVS->typeAnimation = pInfoStepShader->dataVS->typeAnimation;
                    for (int k = 0; k < infoStepShader->dataVS->lenVars; ++k)
                    {
                        const int index = k * 4;
                        memcpy(&infoStepShader->dataVS->max[index], &pInfoStepShader->dataVS->max[index], sizeof(float) * 4);
                        memcpy(&infoStepShader->dataVS->min[index], &pInfoStepShader->dataVS->min[index], sizeof(float) * 4);
                        infoStepShader->dataVS->typeVars[k] = pInfoStepShader->dataVS->typeVars[k];
                    }
                }
            }
        }

        headerMesh.totalFrames = meshMemory->getTotalFrame();
        {
            const BUFFER_MESH* pBufferMesh0 = meshMemory->getBuffer(0);
            const BUFFER_GL* pGl0 = pBufferMesh0 ? pBufferMesh0->pBufferGL : nullptr;
            const bool hasNormals = pGl0 && (pGl0->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR || pGl0->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
            headerMesh.hasNorText[0] = hasNormals ? HAS_NOR_IN_FILE : HAS_NOR_NO;
        }
        //std::map<int, float> lsLetterChangedValuesByLetterX;
        std::map<int, float> lsLetterChangedValuesByCurFrameX;
        //std::map<int, float> lsLetterChangedValuesByLetterY;
        std::map<int, float> lsLetterChangedValuesByCurFrameY;
        if (meshMemory->getInfoFont() != nullptr)//TODO
        {
            const INFO_BOUND_FONT* pMemoryInfoFont = meshMemory->getInfoFont();
            const auto sL = static_cast<int>(sizeof(mbm::INFO_BOUND_FONT::letterDiffY) / sizeof(float));

            for (int i = 0; i < sL; ++i)
            {
                lsLetterChangedValuesByCurFrameX[i] = pMemoryInfoFont->letterDiffX[i];
                lsLetterChangedValuesByCurFrameY[i] = pMemoryInfoFont->letterDiffY[i];
                //if (pMemoryInfoFont->letterDiffY[i] != 0.0f)
                //{
                //    lsLetterChangedValuesByLetterY[i] = pMemoryInfoFont->letterDiffY[i];
                //}
                //if (pMemoryInfoFont->letterDiffX[i] != 0.0f)
                //{
                //    lsLetterChangedValuesByLetterX[i] = pMemoryInfoFont->letterDiffX[i];
                //    lsLetterChangedValuesByCurFrameX[pDetailFont->indexFrame] = pMemoryInfoFont->letterDiffX[i];
                //}
            }
            //for (auto& j : pMemoryInfoFont->letter)
            //{
            //    const util::DETAIL_LETTER* pDetailFont = j.detail;
            //    if (pDetailFont)
            //    {
            //        const float x = lsLetterChangedValuesByLetterX[pDetailFont->letter];
            //        if (x != 0.0f)
            //        {
            //            lsLetterChangedValuesByCurFrameX[pDetailFont->indexFrame] = x;
            //        }
            //        const float y = lsLetterChangedValuesByLetterY[pDetailFont->letter];
            //        if (y != 0.0f)
            //        {
            //            lsLetterChangedValuesByCurFrameY[pDetailFont->indexFrame] = y;
            //        }
            //    }
            //}
        }
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            auto pBuffer = new util::BUFFER_MESH_DEBUG();
            this->buffer.push_back(pBuffer);
            // 5 Sequencia lógica dos frames --------------------------------------------------------------------------
            // Cada header Frame
            // --------------------------------------------------------------------------------------------------
            util::HEADER_FRAME* headerFrame = &pBuffer->headerFrame;
            const BUFFER_MESH* pBufferMesh = meshMemory->getBuffer(currentFrame);
            const BUFFER_GL* pGl = pBufferMesh->pBufferGL;
            if (pGl->isIndexBuffer())
            {
                strncpy(headerFrame->typeBuffer, "IB", sizeof(headerFrame->typeBuffer) - 1);
                for (uint32_t i = 0; i < pBufferMesh->pBufferGL->totalSubset; ++i)
                {
                    headerFrame->sizeIndexBuffer += pBufferMesh->pBufferGL->indexCountIB[i];
                }
            }
            else
            {
                strncpy(headerFrame->typeBuffer, "VB", sizeof(headerFrame->typeBuffer) - 1);
                for (uint32_t i = 0; i < pBufferMesh->pBufferGL->totalSubset; ++i)
                {
                    headerFrame->sizeVertexBuffer += pBufferMesh->pBufferGL->vertexCountVB[i];
                }
            }
            headerFrame->stride = 3;
            // 6 Todos os headers subset deste frame
            // -------------------------------------------------------------------------------
            headerFrame->totalSubset = pBufferMesh->pBufferGL->totalSubset;
            if (fillInSubsetDebug(meshMemory,
                                  currentFrame,
                                  lsLetterChangedValuesByCurFrameX,
                                  lsLetterChangedValuesByCurFrameY,
                                  headerFrame,
                                  pBuffer) == false)
            {
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Failed to fill subset in specific backend engine for mesh %s", meshMemory->getFilenameMesh());
            }
            // moved to MESH_MBM_DEBUG::fillInSubsetDebug
            // 
        }
        positionOffset = VEC3(headerMesh.posX, headerMesh.posY, headerMesh.posZ);
        angleDefault = VEC3(headerMesh.angleX, headerMesh.angleY, headerMesh.angleZ);
        this->sizeCoordTexFrame_0 = 0;
        if (this->coordTexFrame_0)
            delete[] this->coordTexFrame_0;
        this->coordTexFrame_0 = nullptr;
        return true;
    }
}

mbm::MESH_MANAGER *    mbm::MESH_MANAGER::instanceMeshManager        = nullptr;
