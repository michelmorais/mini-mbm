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
#include "mesh-manager-impl.h"
#include <draw-compatibility.h>
#include <shader-var-cfg.h>
#include <texture-manager.h>
#include <renderizable.h>
#include <shader.h>
#include <device.h>
#include <util-interface.h>
#include <shapes.h>
#include <cr-static-local.h>
#include <miniz-wrap/miniz-wrap.h>
#include <header-mesh.h>
#include <header-mesh-legacy-disk.h>
#include "mesh-v8-io.h"
#include "mesh-v11-io.h"
#include "mesh-io-primitives.h"

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
                // MBM_DETAIL_TYPE_FONT/PARTICLE/TILE removed in milestone 5: this function is now only
                // called from saveV11/loadV11's SECTION_DETAIL_PHYSICS, which saveV11 never writes for
                // FONT/PARTICLE/TILE_MAP meshes (mesh-manager.cpp's saveV11 rejects them outright), so
                // those detail types can never appear here. v1-v10 handling of them moved to
                // mesh_deprecated.
                default:
                {
                    return log_util::onFailed(fp,__FILE__, __LINE__, "unknown type bounding box [%d] [%s]", detail.type, fileNamePath);
                }
            }
        }
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

    // Maps SUBSET_DEBUG::materialTextureSlots[].type (legacy util::MATERIAL_TEXTURE_SLOT_TYPE numbering,
    // 1-4) onto the v11 on-disk role byte (mbm::TEXTURE_ROLE numbering, 2-5) - the two enums exist for
    // different reasons (one is the in-memory editor's legacy slot id, the other is the runtime
    // shader/texture role) and were never given matching values. See docs/mesh-v11-plan.md Scope
    // Decision 2.
    mbm::TEXTURE_ROLE legacyMaterialSlotTypeToTextureRole(const uint16_t legacyType) noexcept
    {
        switch (legacyType)
        {
            case util::MATERIAL_TEXTURE_SLOT_NORMAL:   return mbm::TEXTURE_ROLE_NORMAL;
            case util::MATERIAL_TEXTURE_SLOT_SPECULAR: return mbm::TEXTURE_ROLE_SPECULAR;
            case util::MATERIAL_TEXTURE_SLOT_EMISSIVE: return mbm::TEXTURE_ROLE_EMISSIVE;
            case util::MATERIAL_TEXTURE_SLOT_MASK:     return mbm::TEXTURE_ROLE_MASK;
            default:                                   return mbm::TEXTURE_ROLE_DIFFUSE;
        }
    }

    // Inverse of legacyMaterialSlotTypeToTextureRole, for loadV11. Returns false (caller skips the
    // slot, with a log warning) for roles that have no legacy slot-type equivalent - DIFFUSE and
    // ANIMATION_EFFECT are never written as an *extra* slot by saveV11 (DIFFUSE is always the
    // subset's primaryTexture instead), so seeing one here means an unrecognized/future role byte.
    bool textureRoleToLegacyMaterialSlotType(const mbm::TEXTURE_ROLE role, uint16_t &outLegacyType) noexcept
    {
        switch (role)
        {
            case mbm::TEXTURE_ROLE_NORMAL:   outLegacyType = util::MATERIAL_TEXTURE_SLOT_NORMAL;   return true;
            case mbm::TEXTURE_ROLE_SPECULAR: outLegacyType = util::MATERIAL_TEXTURE_SLOT_SPECULAR; return true;
            case mbm::TEXTURE_ROLE_EMISSIVE: outLegacyType = util::MATERIAL_TEXTURE_SLOT_EMISSIVE; return true;
            case mbm::TEXTURE_ROLE_MASK:     outLegacyType = util::MATERIAL_TEXTURE_SLOT_MASK;     return true;
            default:                         return false;
        }
    }

    // Bridges an in-memory v11 section payload (as returned by util::readSectionV11) to a FILE*, so
    // loadV11 can reuse the project's existing FILE*-based readers (util::readXxxV11, and - for
    // SECTION_DETAIL_PHYSICS - the real read_detail_mesh_section template) without duplicating any
    // parsing logic. tmpfile() is standard C (unlike fmemopen, which is POSIX-only and this project
    // also builds for Windows/DirectX9) and is automatically cleaned up by the OS when closed.
    FILE *stage_payload_as_tmpfile(const std::vector<uint8_t> &payload)
    {
        FILE *tmp = std::tmpfile();
        if (!tmp)
            return nullptr;
        if (!payload.empty() && std::fwrite(payload.data(), payload.size(), 1, tmp) != 1)
        {
            std::fclose(tmp);
            return nullptr;
        }
        std::fseek(tmp, 0, SEEK_SET);
        return tmp;
    }

}

namespace mbm
{
    struct MESH_MANAGER::Impl
    {
        std::unordered_map<std::string, MESH_MBM *> lsMeshes;
        std::vector<MESH_MBM *> lsFakeRelease;
    };

    // Relocated from mesh-manager-legacy.cpp in milestone 5 (that file is gone - it held the
    // MBM_ENABLE_MESH_LEGACY_V7-gated v1-v7 path, but these two functions were always-compiled and
    // still needed: they're read_detail_mesh_section's TriangleReader callback for both
    // MESH_MBM_DEBUG::loadV11 and MESH_MBM::loadV11.
    bool MESH_MBM_DEBUG::readDebugTriangleDetailCompat(FILE *fp, const char *fileNamePath, const int totalBounding, const int fileVersion)
    {
        if (fileVersion >= MODE_DRAW_VERSION_MBM_HEADER)
        {
            for (int j = 0; j < totalBounding; j++)
            {
                auto triangle = new TRIANGLE();
                this->impl->infoPhysics.lsTriangle.push_back(triangle);
                if (!util::readTriangleV8(fp, *triangle))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
            }
        }
        else
        {
            for (int j = 0; j < totalBounding; j++)
            {
                auto triangle = new TRIANGLE();
                this->impl->infoPhysics.lsTriangle.push_back(triangle);
                if (!util::readTriangleLegacyNoPosV8(fp, *triangle))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
            }
        }
        return true;
    }

    bool MESH_MBM::readTriangleDetailCompat(FILE *fp, const char *fileNamePath, const int totalBounding, const int fileVersion)
    {
        if (fileVersion >= MODE_DRAW_VERSION_MBM_HEADER)
        {
            for (int j = 0; j < totalBounding; j++)
            {
                auto triangle = new TRIANGLE();
                this->impl->infoPhysics.lsTriangle.push_back(triangle);
                if (!util::readTriangleV8(fp, *triangle))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
            }
        }
        else
        {
            for (int j = 0; j < totalBounding; j++)
            {
                auto triangle = new TRIANGLE();
                this->impl->infoPhysics.lsTriangle.push_back(triangle);
                if (!util::readTriangleLegacyNoPosV8(fp, *triangle))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
            }
        }
        return true;
    }

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

    BUFFER_GL *BUFFER_MESH::getRenderBuffer() const noexcept
    {
        return this->pBufferGL;
    }

    bool BUFFER_MESH::hasLoadedRenderBuffer() const noexcept
    {
        return this->pBufferGL && this->pBufferGL->isLoadedBuffer();
    }

    uint32_t BUFFER_MESH::getTotalSubsets() const noexcept
    {
        return this->totalSubset;
    }

    util::SUBSET *BUFFER_MESH::getSubset(const uint32_t indexSubset) const noexcept
    {
        if (this->subset && indexSubset < this->totalSubset)
            return &this->subset[indexSubset];
        return nullptr;
    }



    MESH_MBM_DEBUG::MESH_MBM_DEBUG()
        : impl(std::make_unique<Impl>())
    {
        impl->positionOffset      = VEC3(0, 0, 0);
        impl->angleDefault        = VEC3(0, 0, 0);
        impl->coordTexFrame_0     = nullptr;
        impl->sizeCoordTexFrame_0 = 0;
        impl->typeMe              = util::TYPE_MESH_UNKNOWN;
        util::MATERIAL m;
        impl->headerMesh.material      = m;
        impl->headerMesh.hasNorText[0] = HAS_NOR_NO;
        impl->headerMesh.hasNorText[1] = HAS_TEX_EACH_FRAME;
        impl->zoomEditorSprite.x      = 1.0f;
        impl->zoomEditorSprite.y      = 1.0f;
        impl->extraInfo                = nullptr;
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
            this->impl->buffer.push_back(pBuffer);
            return static_cast<uint32_t>(this->impl->buffer.size());
        }
        return 0;
    }
    
    uint32_t MESH_MBM_DEBUG::addSubset(uint32_t indexFrame)
    {
        if (indexFrame < static_cast<uint32_t>(this->impl->buffer.size()))
        {
            this->impl->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)]->subset.push_back(new util::SUBSET_DEBUG());
            return static_cast<uint32_t>(this->impl->buffer[indexFrame]->subset.size());
        }
        return 0;
    }
    
    bool MESH_MBM_DEBUG::getInfo(util::HEADER_MESH &headerMeshMbmOut, util::TYPE_MESH &typeOut,
                              INFO_BOUND_FONT **datailFontOut, std::vector<util::STAGE_PARTICLE> & lsStageParticle)
    {
        if (this->impl->buffer.size())
        {
            headerMeshMbmOut = this->impl->headerMesh;
            typeOut          = this->impl->typeMe;

            if(this->impl->typeMe == util::TYPE_MESH_FONT)
            {
                *datailFontOut   = static_cast<INFO_BOUND_FONT *>(this->impl->extraInfo);
            }
            lsStageParticle.clear();
            if(this->impl->typeMe == util::TYPE_MESH_PARTICLE)
            {
                auto* lsParticleInfo = static_cast<std::vector<util::STAGE_PARTICLE*>*>(this->impl->extraInfo);
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
                isMesh = false;
                isUnknown = true;
                return ext;
            }
        }
        return nullptr;
    }

    util::TYPE_MESH MESH_MBM_DEBUG::getMeshType() const noexcept
    {
        return this->impl->typeMe;
    }

    void MESH_MBM_DEBUG::setMeshType(const util::TYPE_MESH type) noexcept
    {
        this->impl->typeMe = type;
    }

    // Peeks a mesh file's header without fully loading it (editor file-browser preview). Milestone 5:
    // v1-v10 support moved out of core_mbm, so this only understands v11 files now - a non-v11 file
    // (bad magic) returns false, no legacy fallback. v11 has no font/particle/animation section types
    // implemented yet, so datailFontOut/lsStageParticle/headerMeshMbmOut.totalAnimation always come
    // back empty/default for a v11 mesh peek - that's correct, not a bug, until those sections exist.
    bool MESH_MBM_DEBUG::getInfo(const char *fileNamePath, util::HEADER_MESH &headerMeshMbmOut,util::INFO_DRAW_MODE & info_mode,
                              util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut,
                              std::vector<util::STAGE_PARTICLE> & lsStageParticle, int *versionOut)
    {
        (void)datailFontOut;
        (void)lsStageParticle;
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

        FILE *fp = util::openFile(fileNamePath, "rb");
        if (!fp)
            return log_util::onFailed(fp,__FILE__, __LINE__, "Failed to open file [%s]", fileNamePath);

        util::FILE_HEADER_V11 fileHeader;
        if (!util::readFileHeaderV11(fp, fileHeader))
        {
            fclose(fp);
            return log_util::onFailed(nullptr,__FILE__, __LINE__, "failed to read v11 file header [%s]", fileNamePath);
        }
        typeOut = static_cast<util::TYPE_MESH>(fileHeader.typeMesh);
        util::HEADER_MESH tmp;
        headerMeshMbmOut = tmp;
        info_mode = util::INFO_DRAW_MODE();

        for (uint32_t i = 0; i < fileHeader.sectionCount; ++i)
        {
            util::SECTION_HEADER_V11 sectionHeader;
            std::vector<uint8_t> payload;
            if (!util::readSectionV11(fp, sectionHeader, payload))
                break;
            if (sectionHeader.type == util::SECTION_MATERIAL_TRANSFORM)
            {
                FILE *tmpFp = stage_payload_as_tmpfile(payload);
                if (tmpFp)
                {
                    util::MATERIAL_TRANSFORM_V11 materialTransform;
                    if (util::readMaterialTransformV11(tmpFp, materialTransform))
                    {
                        headerMeshMbmOut.material = materialTransform.material;
                        headerMeshMbmOut.angleX   = materialTransform.angleX;
                        headerMeshMbmOut.angleY   = materialTransform.angleY;
                        headerMeshMbmOut.angleZ   = materialTransform.angleZ;
                        headerMeshMbmOut.posX     = materialTransform.posX;
                        headerMeshMbmOut.posY     = materialTransform.posY;
                        headerMeshMbmOut.posZ     = materialTransform.posZ;
                        info_mode.mode_draw                 = materialTransform.mode_draw;
                        info_mode.mode_cull_face            = materialTransform.mode_cull_face;
                        info_mode.mode_front_face_direction = materialTransform.mode_front_face_direction;
                    }
                    fclose(tmpFp);
                }
                break;
            }
        }
        fclose(fp);
        if (versionOut) *versionOut = 11;
        return true;
    }

    util::TYPE_MESH MESH_MBM_DEBUG::getType() noexcept
    {
        if (this->impl->buffer.size())
            return this->impl->typeMe;
        return util::TYPE_MESH_UNKNOWN;
    }

    VEC3 MESH_MBM_DEBUG::getAngleDefault() const noexcept
    {
        return this->impl->angleDefault;
    }

    void MESH_MBM_DEBUG::setAngleDefault(const VEC3 &angle) noexcept
    {
        this->impl->angleDefault = angle;
        this->impl->headerMesh.angleX = angle.x;
        this->impl->headerMesh.angleY = angle.y;
        this->impl->headerMesh.angleZ = angle.z;
    }

    VEC3 MESH_MBM_DEBUG::getPositionOffset() const noexcept
    {
        return this->impl->positionOffset;
    }

    void MESH_MBM_DEBUG::setPositionOffset(const VEC3 &position) noexcept
    {
        this->impl->positionOffset = position;
        this->impl->headerMesh.posX = position.x;
        this->impl->headerMesh.posY = position.y;
        this->impl->headerMesh.posZ = position.z;
    }

    INFO_PHYSICS & MESH_MBM_DEBUG::getPhysicsInfo() noexcept
    {
        return impl->infoPhysics;
    }

    const INFO_PHYSICS & MESH_MBM_DEBUG::getPhysicsInfo() const noexcept
    {
        return impl->infoPhysics;
    }

    int MESH_MBM_DEBUG::getFileVersion() const noexcept
    {
        return impl->headerMain.version;
    }

    util::MATERIAL & MESH_MBM_DEBUG::getMaterial() noexcept
    {
        return impl->headerMesh.material;
    }

    const util::MATERIAL & MESH_MBM_DEBUG::getMaterial() const noexcept
    {
        return impl->headerMesh.material;
    }

    int16_t MESH_MBM_DEBUG::getHasNormal() const noexcept
    {
        return impl->headerMesh.hasNorText[0];
    }

    void MESH_MBM_DEBUG::setHasNormal(const int16_t hasNormalMode) noexcept
    {
        impl->headerMesh.hasNorText[0] = hasNormalMode;
    }

    int16_t MESH_MBM_DEBUG::getHasTexture() const noexcept
    {
        return impl->headerMesh.hasNorText[1];
    }

    void MESH_MBM_DEBUG::setHasTexture(const int16_t hasTextureMode) noexcept
    {
        impl->headerMesh.hasNorText[1] = hasTextureMode;
    }

    const char * MESH_MBM_DEBUG::getFilenameMesh() const noexcept
    {
        return impl->fileName.c_str();
    }

    unsigned int MESH_MBM_DEBUG::getModeDraw() const noexcept
    {
        return this->impl->info_mode.mode_draw;
    }

    void MESH_MBM_DEBUG::setModeDraw(const unsigned int modeDraw) noexcept
    {
        this->impl->info_mode.mode_draw = modeDraw;
    }

    unsigned int MESH_MBM_DEBUG::getModeCullFace() const noexcept
    {
        return this->impl->info_mode.mode_cull_face;
    }

    void MESH_MBM_DEBUG::setModeCullFace(const unsigned int modeCullFace) noexcept
    {
        this->impl->info_mode.mode_cull_face = modeCullFace;
    }

    unsigned int MESH_MBM_DEBUG::getModeFrontFaceDirection() const noexcept
    {
        return this->impl->info_mode.mode_front_face_direction;
    }

    void MESH_MBM_DEBUG::setModeFrontFaceDirection(const unsigned int modeFrontFaceDirection) noexcept
    {
        this->impl->info_mode.mode_front_face_direction = modeFrontFaceDirection;
    }

    void * MESH_MBM_DEBUG::getDetailInfo() const noexcept
    {
        return this->impl->extraInfo;
    }

    void MESH_MBM_DEBUG::replaceDetailInfo(void *detailInfo) noexcept
    {
        this->deleteExtraInfo();
        this->impl->extraInfo = detailInfo;
    }

    uint32_t MESH_MBM_DEBUG::getTotalAnimationHeaders() const noexcept
    {
        return static_cast<uint32_t>(this->impl->infoAnimation.lsHeaderAnim.size());
    }

    util::INFO_ANIMATION::INFO_HEADER_ANIM * MESH_MBM_DEBUG::getAnimationHeader(const uint32_t index) const noexcept
    {
        if (index < this->impl->infoAnimation.lsHeaderAnim.size())
            return this->impl->infoAnimation.lsHeaderAnim[index];
        return nullptr;
    }

    void MESH_MBM_DEBUG::appendAnimationHeader(util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead) noexcept
    {
        if (infoHead)
            this->impl->infoAnimation.lsHeaderAnim.push_back(infoHead);
    }

    void MESH_MBM_DEBUG::clearBlendOperations() noexcept
    {
        this->impl->lsBlendOperation.clear();
    }

    void MESH_MBM_DEBUG::resizeBlendOperations(const uint32_t totalAnimations)
    {
        this->impl->lsBlendOperation.resize(totalAnimations);
    }

    void MESH_MBM_DEBUG::setBlendOperation(const uint32_t index, const int blendOperation)
    {
        if (index < this->impl->lsBlendOperation.size())
            this->impl->lsBlendOperation[index] = blendOperation;
    }

    uint32_t MESH_MBM_DEBUG::getTotalFrames() const noexcept
    {
        return static_cast<uint32_t>(this->impl->buffer.size());
    }

    util::BUFFER_MESH_DEBUG *MESH_MBM_DEBUG::getFrameBuffer(const uint32_t indexFrame) const noexcept
    {
        if (indexFrame < this->impl->buffer.size())
            return this->impl->buffer[indexFrame];
        return nullptr;
    }

    uint32_t MESH_MBM_DEBUG::getTotalSubsets(const uint32_t indexFrame) const noexcept
    {
        const util::BUFFER_MESH_DEBUG *bufferCurrent = this->getFrameBuffer(indexFrame);
        if (bufferCurrent)
            return static_cast<uint32_t>(bufferCurrent->subset.size());
        return 0;
    }

    util::SUBSET_DEBUG *MESH_MBM_DEBUG::getSubset(const uint32_t indexFrame, const uint32_t indexSubset) const noexcept
    {
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->getFrameBuffer(indexFrame);
        if (bufferCurrent && indexSubset < bufferCurrent->subset.size())
            return bufferCurrent->subset[indexSubset];
        return nullptr;
    }

    bool MESH_MBM_DEBUG::hasIndexBuffer(const uint32_t indexFrame) const noexcept
    {
        const util::BUFFER_MESH_DEBUG *bufferCurrent = this->getFrameBuffer(indexFrame);
        return bufferCurrent && bufferCurrent->indexBuffer != nullptr;
    }

    VEC3 *MESH_MBM_DEBUG::getPositionArray(const uint32_t indexFrame) const noexcept
    {
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->getFrameBuffer(indexFrame);
        return bufferCurrent ? reinterpret_cast<VEC3 *>(bufferCurrent->position) : nullptr;
    }

    VEC3 *MESH_MBM_DEBUG::getNormalArray(const uint32_t indexFrame) const noexcept
    {
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->getFrameBuffer(indexFrame);
        return bufferCurrent ? reinterpret_cast<VEC3 *>(bufferCurrent->normal) : nullptr;
    }

    VEC2 *MESH_MBM_DEBUG::getUvArray(const uint32_t indexFrame) const noexcept
    {
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->getFrameBuffer(indexFrame);
        return bufferCurrent ? reinterpret_cast<VEC2 *>(bufferCurrent->uv) : nullptr;
    }

    uint16_t *MESH_MBM_DEBUG::getIndexArray(const uint32_t indexFrame) const noexcept
    {
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->getFrameBuffer(indexFrame);
        return bufferCurrent ? bufferCurrent->indexBuffer : nullptr;
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
        // Unrecognized extension - peek the v11 file header directly (milestone 5: v1-v10 is gone).
        FILE *fp = util::openFile(fileNamePath, "rb");
        if (!fp)
            return util::TYPE_MESH_UNKNOWN;
        util::FILE_HEADER_V11 fileHeader;
        const bool ok = util::readFileHeaderV11(fp, fileHeader);
        fclose(fp);
        if (!ok)
            return util::TYPE_MESH_UNKNOWN;
        return static_cast<util::TYPE_MESH>(fileHeader.typeMesh);
    }
    
    void MESH_MBM_DEBUG::calculateNormals()
    {
        impl->headerMesh.totalFrames = static_cast<int>(this->impl->buffer.size());
        for (int currentFrame = 0; currentFrame < impl->headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->impl->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(currentFrame)];
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
        for (std::vector<util::BUFFER_MESH_DEBUG *>::size_type i = 0; i < this->impl->buffer.size(); ++i)
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->impl->buffer[i];
            if (bufferCurrent->normal)
            {
                delete[] bufferCurrent->normal;
                bufferCurrent->normal = nullptr;
            }
        }
        impl->headerMesh.hasNorText[0] = HAS_NOR_NO;
    }
    
    void MESH_MBM_DEBUG::addNormals()
    {
        calculateNormals();
        impl->headerMesh.hasNorText[0] = HAS_NOR_IN_FILE;
    }

    void MESH_MBM_DEBUG::removeBuffer(uint32_t indexFrame)
    {
        if (indexFrame >= static_cast<uint32_t>(this->impl->buffer.size()))
            return;
        delete this->impl->buffer[indexFrame];
        this->impl->buffer.erase(this->impl->buffer.begin() + static_cast<ptrdiff_t>(indexFrame));
    }

    void MESH_MBM_DEBUG::removeSubset(uint32_t indexFrame, uint32_t indexSubset)
    {
        if (indexFrame >= static_cast<uint32_t>(this->impl->buffer.size()))
            return;
        util::BUFFER_MESH_DEBUG *buf = this->impl->buffer[indexFrame];
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
        if (srcFrameIdx >= static_cast<uint32_t>(src.impl->buffer.size()))
            return 0;
        util::BUFFER_MESH_DEBUG *srcBuf = src.impl->buffer[srcFrameIdx];
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
        this->impl->buffer.push_back(newBuf);
        return static_cast<uint32_t>(this->impl->buffer.size());
    }

    uint32_t MESH_MBM_DEBUG::copySubsetFrom(uint32_t targetFrame, MESH_MBM_DEBUG &src, uint32_t srcFrame, uint32_t srcSubsetIdx)
    {
        if (targetFrame >= static_cast<uint32_t>(this->impl->buffer.size()))
            return 0;
        if (srcFrame >= static_cast<uint32_t>(src.impl->buffer.size()))
            return 0;
        util::BUFFER_MESH_DEBUG *tgtBuf = this->impl->buffer[targetFrame];
        util::BUFFER_MESH_DEBUG *srcBuf = src.impl->buffer[srcFrame];
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
        if (index >= static_cast<uint32_t>(this->impl->infoAnimation.lsHeaderAnim.size()))
            return;
        delete this->impl->infoAnimation.lsHeaderAnim[index];
        this->impl->infoAnimation.lsHeaderAnim.erase(
            this->impl->infoAnimation.lsHeaderAnim.begin() + static_cast<ptrdiff_t>(index));
    }
    
    void MESH_MBM_DEBUG::calculateUV()
    {
        impl->headerMesh.totalFrames = static_cast<int>(this->impl->buffer.size());
        for (int currentFrame = 0; currentFrame < impl->headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->impl->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(currentFrame)];
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
    
    bool MESH_MBM_DEBUG::saveV11(const char *fileOut, const bool recalculateNormal, const bool recalculateUV, char *errorOut,const int lenErrorOut)
    {
        if (this->impl->buffer.size() == 0)
            return false;

        if (this->getTotalAnimationHeaders() > 0 ||
            impl->typeMe == util::TYPE_MESH_FONT ||
            impl->typeMe == util::TYPE_MESH_PARTICLE ||
            impl->typeMe == util::TYPE_MESH_TILE_MAP)
        {
            const char *message = "saveV11 does not support animated or FONT/PARTICLE/TILE_MAP meshes yet (milestone 3 core slice)";
            if (errorOut && lenErrorOut > 0)
                snprintf(errorOut, static_cast<size_t>(lenErrorOut), "%s", message);
            return log_util::onFailed(nullptr, __FILE__, __LINE__, message);
        }

        FILE *file = nullptr;
        impl->headerMesh.totalFrames = static_cast<int>(this->impl->buffer.size());

        if (errorOut)
        {
            if (!check(errorOut,lenErrorOut))
                return log_util::onFailed(file,__FILE__, __LINE__, "error on check mesh to save.");
        }
        else
        {
            char strError[255] = "";
            if (!check(strError,sizeof(strError)-1))
                return log_util::onFailed(file,__FILE__, __LINE__, strError);
        }

        if (recalculateNormal)
        {
            this->calculateNormals();
            this->impl->headerMesh.hasNorText[0] = HAS_NOR_IN_FILE;
        }
        if (recalculateUV)
        {
            this->calculateUV();
            this->impl->headerMesh.hasNorText[1] = HAS_TEX_EACH_FRAME;
        }
        {
            std::string which_mode;
            if (is_any_mode_valid(this->impl->info_mode,which_mode) == false)
                return log_util::onFailed(file,__FILE__, __LINE__, "Invalid mode [%s] for [%s]",which_mode.c_str(),fileOut);
        }

        std::vector<std::string> ls_paths = this->getKnowPathsToExtraHeader();

        int totalBounding = static_cast<int>(this->impl->infoPhysics.lsCube.size())
                           + static_cast<int>(this->impl->infoPhysics.lsSphere.size())
                           + static_cast<int>(this->impl->infoPhysics.lsCubeComplex.size())
                           + static_cast<int>(this->impl->infoPhysics.lsTriangle.size());
        if (totalBounding == 0)
            this->fillAtLeastOneBound();

        // "wb+" (not "wb"): writeSectionV11Streamed seeks back and reads each section's just-written
        // payload bytes to compute crc32Value, which needs the stream open for reading too.
        file = util::openFile(fileOut, "wb+");
        if (!file)
            return log_util::onFailed(file,__FILE__, __LINE__, "Failed to open file [%s]", fileOut);

        util::FILE_HEADER_V11 fileHeader;
        std::memcpy(fileHeader.magic, MBM_V11_MAGIC, sizeof(fileHeader.magic));
        fileHeader.formatVersion    = MBM_V11_FORMAT_VERSION;
        fileHeader.typeMesh         = static_cast<uint8_t>(impl->typeMe);
        fileHeader.backBufferWidth  = impl->headerMain.backBufferWidth;
        fileHeader.backBufferHeight = impl->headerMain.backBufferHeight;
        fileHeader.sectionCount     = 1u /*material*/ + 1u /*physics*/ + (ls_paths.empty() ? 0u : 1u)
                                     + static_cast<uint32_t>(impl->headerMesh.totalFrames);
        if (!util::writeFileHeaderV11(file, fileHeader))
            return log_util::onFailed(file,__FILE__, __LINE__, "failed to write v11 file header [%s]", fileOut);

        // SECTION_MATERIAL_TRANSFORM ---------------------------------------------------------------------
        {
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_MATERIAL_TRANSFORM;
            sectionHeader.sectionVersion = 1;
            util::MATERIAL_TRANSFORM_V11 materialTransform;
            materialTransform.material  = impl->headerMesh.material;
            materialTransform.angleX    = impl->headerMesh.angleX;
            materialTransform.angleY    = impl->headerMesh.angleY;
            materialTransform.angleZ    = impl->headerMesh.angleZ;
            materialTransform.posX      = impl->headerMesh.posX;
            materialTransform.posY      = impl->headerMesh.posY;
            materialTransform.posZ      = impl->headerMesh.posZ;
            materialTransform.mode_draw = impl->info_mode.mode_draw;
            materialTransform.mode_cull_face = impl->info_mode.mode_cull_face;
            materialTransform.mode_front_face_direction = impl->info_mode.mode_front_face_direction;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [&materialTransform](FILE *fp)
            {
                return util::writeMaterialTransformV11(fp, materialTransform);
            });
            if (!ok)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to write SECTION_MATERIAL_TRANSFORM [%s]", fileOut);
        }

        // SECTION_EXTRA_PATHS ------------------------------------------------------------------------------
        if (!ls_paths.empty())
        {
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_EXTRA_PATHS;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [&ls_paths](FILE *fp)
            {
                if (!util::le_io::writeU32LE(fp, static_cast<uint32_t>(ls_paths.size())))
                    return false;
                for (const auto &path : ls_paths)
                {
                    if (!util::writeStringV11(fp, path))
                        return false;
                }
                return true;
            });
            if (!ok)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to write SECTION_EXTRA_PATHS [%s]", fileOut);
        }

        // SECTION_DETAIL_PHYSICS - keeps v8's field layout verbatim, just moved into the TLV envelope ------
        {
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_DETAIL_PHYSICS;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [this](FILE *fp)
            {
                const int totalBoundingLocal = static_cast<int>(this->impl->infoPhysics.lsCube.size())
                                              + static_cast<int>(this->impl->infoPhysics.lsSphere.size())
                                              + static_cast<int>(this->impl->infoPhysics.lsCubeComplex.size())
                                              + static_cast<int>(this->impl->infoPhysics.lsTriangle.size());
                util::DETAIL_MESH detailHeader;
                detailHeader.totalBounding = totalBoundingLocal;
                detailHeader.type          = 'P';
                if (!util::writeDetailMeshV8(fp, detailHeader))
                    return false;

                if (!this->impl->infoPhysics.lsCube.empty())
                {
                    util::DETAIL_MESH detail;
                    detail.totalBounding = static_cast<int>(this->impl->infoPhysics.lsCube.size());
                    detail.type          = 1;
                    if (!util::writeDetailMeshV8(fp, detail))
                        return false;
                    for (auto *cube : this->impl->infoPhysics.lsCube)
                        if (!util::writeCubeV8(fp, *cube))
                            return false;
                }
                if (!this->impl->infoPhysics.lsSphere.empty())
                {
                    util::DETAIL_MESH detail;
                    detail.totalBounding = static_cast<int>(this->impl->infoPhysics.lsSphere.size());
                    detail.type          = 2;
                    if (!util::writeDetailMeshV8(fp, detail))
                        return false;
                    for (auto *sphere : this->impl->infoPhysics.lsSphere)
                        if (!util::writeSphereV8(fp, *sphere))
                            return false;
                }
                if (!this->impl->infoPhysics.lsCubeComplex.empty())
                {
                    util::DETAIL_MESH detail;
                    detail.totalBounding = static_cast<int>(this->impl->infoPhysics.lsCubeComplex.size());
                    detail.type          = 3;
                    if (!util::writeDetailMeshV8(fp, detail))
                        return false;
                    for (auto *complex : this->impl->infoPhysics.lsCubeComplex)
                        if (!util::writeCubeComplexV8(fp, *complex))
                            return false;
                }
                if (!this->impl->infoPhysics.lsTriangle.empty())
                {
                    util::DETAIL_MESH detail;
                    detail.totalBounding = static_cast<int>(this->impl->infoPhysics.lsTriangle.size());
                    detail.type          = 4;
                    if (!util::writeDetailMeshV8(fp, detail))
                        return false;
                    for (auto *triangle : this->impl->infoPhysics.lsTriangle)
                        if (!util::writeTriangleV8(fp, *triangle))
                            return false;
                }
                return true;
            });
            if (!ok)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to write SECTION_DETAIL_PHYSICS [%s]", fileOut);
        }

        // SECTION_FRAME_STATIC, one per frame --------------------------------------------------------------
        for (int currentFrame = 0; currentFrame < impl->headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer =
                this->impl->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(currentFrame)];
            const auto totalSubset = static_cast<uint32_t>(currentFrameBuffer->subset.size());
            util::HEADER_FRAME *headerFrame = &currentFrameBuffer->headerFrame;

            const bool isIndexBuffer = currentFrameBuffer->indexBuffer != nullptr;
            if (isIndexBuffer)
            {
                uint32_t sIndex = 0;
                for (uint32_t i = 0; i < totalSubset; ++i)
                    sIndex += static_cast<uint32_t>(currentFrameBuffer->subset[i]->indexCount);
                headerFrame->sizeIndexBuffer = static_cast<int>(sIndex);
            }
            else
            {
                headerFrame->sizeIndexBuffer = 0;
            }
            uint32_t sVertex = 0;
            for (uint32_t i = 0; i < totalSubset; ++i)
                sVertex += static_cast<uint32_t>(currentFrameBuffer->subset[i]->vertexCount);
            headerFrame->totalSubset      = static_cast<int>(totalSubset);
            headerFrame->sizeVertexBuffer = static_cast<int>(sVertex);

            if (headerFrame->sizeVertexBuffer == 0)
                return log_util::onFailed(file,__FILE__, __LINE__, "total of vertex is zero");

            util::FRAME_HEADER_V11 v11FrameHeader;
            v11FrameHeader.totalSubset = totalSubset;
            v11FrameHeader.vertexCount = static_cast<uint32_t>(headerFrame->sizeVertexBuffer);
            v11FrameHeader.indexWidth  = 16;
            v11FrameHeader.hasNormal   = (impl->headerMesh.hasNorText[0] != HAS_NOR_NO) ? 1 : 0;
            const bool ownUv    = (impl->headerMesh.hasNorText[1] == HAS_TEX_EACH_FRAME) ||
                                  (currentFrame == 0 && impl->headerMesh.hasNorText[1] == HAS_TEX_FIRST_FRAME);
            const bool sharedUv = (currentFrame != 0 && impl->headerMesh.hasNorText[1] == HAS_TEX_FIRST_FRAME);
            v11FrameHeader.hasUv      = (ownUv || sharedUv) ? 1 : 0;
            v11FrameHeader.uvSource   = sharedUv ? 1 : 0; // 0 = OWN, 1 = SHARED_WITH_FRAME_0
            v11FrameHeader.indexCount = isIndexBuffer ? static_cast<uint32_t>(headerFrame->sizeIndexBuffer) : 0;

            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_FRAME_STATIC;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [&](FILE *fp) -> bool
            {
                if (!util::writeFrameHeaderV11(fp, v11FrameHeader))
                    return false;

                // Position is always stored in memory as VEC3 regardless of headerFrame.stride - stride
                // only ever affected what saveDebug wrote to the v8 file (2 vs 3 floats on disk). v11 always
                // stores VEC3 (format doc §6 has no stride field), so this is a direct copy, not an upconversion.
                const auto *position3 = reinterpret_cast<const VEC3 *>(currentFrameBuffer->position);
                for (uint32_t i = 0; i < v11FrameHeader.vertexCount; ++i)
                {
                    if (!util::le_io::writeF32LE(fp, position3[i].x) ||
                        !util::le_io::writeF32LE(fp, position3[i].y) ||
                        !util::le_io::writeF32LE(fp, position3[i].z))
                        return false;
                }

                if (v11FrameHeader.hasNormal)
                {
                    const auto *normal3 = reinterpret_cast<const VEC3 *>(currentFrameBuffer->normal);
                    for (uint32_t i = 0; i < v11FrameHeader.vertexCount; ++i)
                    {
                        if (!util::le_io::writeF32LE(fp, normal3[i].x) ||
                            !util::le_io::writeF32LE(fp, normal3[i].y) ||
                            !util::le_io::writeF32LE(fp, normal3[i].z))
                            return false;
                    }
                }

                if (v11FrameHeader.hasUv && v11FrameHeader.uvSource == 0)
                {
                    const auto *uv2 = reinterpret_cast<const VEC2 *>(currentFrameBuffer->uv);
                    for (uint32_t i = 0; i < v11FrameHeader.vertexCount; ++i)
                    {
                        if (!util::le_io::writeF32LE(fp, uv2[i].x) || !util::le_io::writeF32LE(fp, uv2[i].y))
                            return false;
                    }
                }

                if (v11FrameHeader.indexCount > 0)
                {
                    for (uint32_t i = 0; i < v11FrameHeader.indexCount; ++i)
                        if (!util::le_io::writeU16LE(fp, currentFrameBuffer->indexBuffer[i]))
                            return false;
                }

                for (uint32_t i = 0; i < totalSubset; ++i)
                {
                    util::SUBSET_DEBUG *pSubset = currentFrameBuffer->subset[i];

                    util::SUBSET_DESC_V11 subsetDesc;
                    char nameTexture[64];
                    if (!fillTextureReferenceForHeader(fp, pSubset->texture, impl->typeMe, nameTexture))
                        return false;
                    subsetDesc.primaryTexture.storage = util::TEXTURE_REF_STORAGE_PATH;
                    subsetDesc.primaryTexture.path    = nameTexture;
                    subsetDesc.vertexCount = pSubset->vertexCount;
                    subsetDesc.vertexStart = pSubset->vertexStart;
                    subsetDesc.indexStart  = pSubset->indexStart;
                    subsetDesc.indexCount  = pSubset->indexCount;
                    subsetDesc.alphaColor[0] = 1;
                    subsetDesc.alphaColor[1] = subsetDesc.alphaColor[2] = subsetDesc.alphaColor[3] = 0;

                    std::vector<util::SUBSET_EXTRA_SLOT_V11> extraSlots;
                    if (this->impl->headerMain.version >= MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER)
                    {
                        for (const auto &slot : pSubset->materialTextureSlots)
                        {
                            if (slot.texture.empty())
                                continue;
                            util::SUBSET_EXTRA_SLOT_V11 extraSlot;
                            extraSlot.role = static_cast<uint8_t>(legacyMaterialSlotTypeToTextureRole(slot.type));
                            char slotNameTexture[64];
                            if (!fillTextureReferenceForHeader(fp, slot.texture, impl->typeMe, slotNameTexture))
                                return false;
                            extraSlot.texture.storage = util::TEXTURE_REF_STORAGE_PATH;
                            extraSlot.texture.path    = slotNameTexture;
                            extraSlots.push_back(extraSlot);
                        }
                    }
                    subsetDesc.extraSlotCount = static_cast<uint16_t>(extraSlots.size());

                    if (!util::writeSubsetDescV11(fp, subsetDesc))
                        return false;
                    for (const auto &extraSlot : extraSlots)
                        if (!util::writeSubsetExtraSlotV11(fp, extraSlot))
                            return false;
                }

                return true;
            });

            if (!ok)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to write SECTION_FRAME_STATIC for frame %d [%s]", currentFrame, fileOut);
        }

        if (file)
            fclose(file);
        file = nullptr;
        return true;
    }

    bool MESH_MBM_DEBUG::readFrameStaticV11Payload(FILE *fp, const util::BUFFER_MESH_DEBUG *frame0, util::BUFFER_MESH_DEBUG *&out,
                                                   util::FRAME_HEADER_V11 &outFrameHeader)
    {
        out = nullptr;

        util::FRAME_HEADER_V11 &frameHeader = outFrameHeader;
        if (!util::readFrameHeaderV11(fp, frameHeader))
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read FRAME_HEADER_V11");
        if (frameHeader.indexWidth != 16)
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "loadV11 only supports 16-bit indices (milestone 4 core slice)");

        auto pBuffer = new util::BUFFER_MESH_DEBUG();
        pBuffer->headerFrame.totalSubset      = static_cast<int>(frameHeader.totalSubset);
        pBuffer->headerFrame.sizeVertexBuffer = static_cast<int>(frameHeader.vertexCount);
        pBuffer->headerFrame.sizeIndexBuffer  = static_cast<int>(frameHeader.indexCount);
        pBuffer->headerFrame.stride           = (impl->typeMe == util::TYPE_MESH_SPRITE) ? 2 : 3;
        strncpy(pBuffer->headerFrame.typeBuffer, frameHeader.indexCount > 0 ? "IB" : "VB", sizeof(pBuffer->headerFrame.typeBuffer) - 1);

        const auto fail = [&]() -> bool
        {
            pBuffer->release();
            delete pBuffer;
            return false;
        };

        auto pPosition = new VEC3[frameHeader.vertexCount];
        pBuffer->position = reinterpret_cast<float *>(pPosition);
        for (uint32_t i = 0; i < frameHeader.vertexCount; ++i)
        {
            if (!util::le_io::readF32LE(fp, pPosition[i].x) ||
                !util::le_io::readF32LE(fp, pPosition[i].y) ||
                !util::le_io::readF32LE(fp, pPosition[i].z))
            {
                log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read frame position");
                return fail();
            }
        }

        if (frameHeader.hasNormal)
        {
            auto pNormal = new VEC3[frameHeader.vertexCount];
            pBuffer->normal = reinterpret_cast<float *>(pNormal);
            for (uint32_t i = 0; i < frameHeader.vertexCount; ++i)
            {
                if (!util::le_io::readF32LE(fp, pNormal[i].x) ||
                    !util::le_io::readF32LE(fp, pNormal[i].y) ||
                    !util::le_io::readF32LE(fp, pNormal[i].z))
                {
                    log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read frame normal");
                    return fail();
                }
            }
        }

        auto pUv = new VEC2[frameHeader.vertexCount];
        memset(static_cast<void *>(pUv), 0, sizeof(VEC2) * static_cast<size_t>(frameHeader.vertexCount));
        pBuffer->uv = reinterpret_cast<float *>(pUv);
        if (frameHeader.hasUv)
        {
            if (frameHeader.uvSource == 0) // OWN
            {
                for (uint32_t i = 0; i < frameHeader.vertexCount; ++i)
                {
                    if (!util::le_io::readF32LE(fp, pUv[i].x) || !util::le_io::readF32LE(fp, pUv[i].y))
                    {
                        log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read frame uv");
                        return fail();
                    }
                }
            }
            else if (frame0 && frame0->uv) // SHARED_WITH_FRAME_0
            {
                const auto *frame0Uv = reinterpret_cast<const VEC2 *>(frame0->uv);
                const uint32_t safeCopy = std::min(frameHeader.vertexCount, static_cast<uint32_t>(frame0->headerFrame.sizeVertexBuffer));
                memcpy(static_cast<void *>(pUv), frame0Uv, sizeof(VEC2) * static_cast<size_t>(safeCopy));
            }
        }

        if (frameHeader.indexCount > 0)
        {
            auto pIndex = new uint16_t[frameHeader.indexCount];
            pBuffer->indexBuffer = pIndex;
            for (uint32_t i = 0; i < frameHeader.indexCount; ++i)
            {
                if (!util::le_io::readU16LE(fp, pIndex[i]))
                {
                    log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read frame index");
                    return fail();
                }
            }
        }

        for (uint32_t s = 0; s < frameHeader.totalSubset; ++s)
        {
            util::SUBSET_DESC_V11 subsetDesc;
            if (!util::readSubsetDescV11(fp, subsetDesc))
            {
                log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read SUBSET_DESC_V11");
                return fail();
            }

            auto pSubset = new util::SUBSET_DEBUG();
            pSubset->vertexStart = subsetDesc.vertexStart;
            pSubset->vertexCount = subsetDesc.vertexCount;
            pSubset->indexStart  = subsetDesc.indexStart;
            pSubset->indexCount  = subsetDesc.indexCount;
            pSubset->texture     = subsetDesc.primaryTexture.path;

            for (uint16_t e = 0; e < subsetDesc.extraSlotCount; ++e)
            {
                util::SUBSET_EXTRA_SLOT_V11 extraSlot;
                if (!util::readSubsetExtraSlotV11(fp, extraSlot))
                {
                    delete pSubset;
                    log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read SUBSET_EXTRA_SLOT_V11");
                    return fail();
                }
                uint16_t legacyType = 0;
                if (textureRoleToLegacyMaterialSlotType(static_cast<mbm::TEXTURE_ROLE>(extraSlot.role), legacyType))
                {
                    util::MATERIAL_TEXTURE_SLOT_DEBUG slotDebug;
                    slotDebug.type    = legacyType;
                    slotDebug.texture = extraSlot.texture.path;
                    pSubset->materialTextureSlots.push_back(slotDebug);
                }
                else
                {
                    PRINT_IF_DEBUG("Warning! loadV11 skipped extra texture slot with unrecognized role [%d]\n", extraSlot.role);
                }
            }
            pBuffer->subset.push_back(pSubset);
        }

        out = pBuffer;
        return true;
    }

    bool MESH_MBM_DEBUG::loadV11(const char *fileNamePath)
    {
        this->release();
        FILE *fp = util::openFile(fileNamePath, "rb");
        if (!fp)
            return log_util::onFailed(fp, __FILE__, __LINE__, "Failed to open file [%s]", fileNamePath);

        util::FILE_HEADER_V11 fileHeader;
        if (!util::readFileHeaderV11(fp, fileHeader))
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read v11 file header [%s]", fileNamePath);

        impl->typeMe                      = static_cast<util::TYPE_MESH>(fileHeader.typeMesh);
        impl->headerMain.version          = CURRENT_VERSION_MBM_HEADER;
        impl->headerMain.backBufferWidth  = fileHeader.backBufferWidth;
        impl->headerMain.backBufferHeight = fileHeader.backBufferHeight;
        strncpy(impl->headerMain.name, MBM_HEADER_NAME_MBM, sizeof(impl->headerMain.name) - 1);
        const char *typeApp = get_type_app_from_mesh_type(impl->typeMe);
        if (typeApp)
            strncpy(impl->headerMain.typeApp, typeApp, sizeof(impl->headerMain.typeApp) - 1);

        int16_t hasNormalFlag  = HAS_NOR_NO;
        int16_t hasTextureFlag = HAS_TEX_NO;
        bool    sawFirstFrame  = false;
        util::BUFFER_MESH_DEBUG *frame0 = nullptr;

        for (uint32_t i = 0; i < fileHeader.sectionCount; ++i)
        {
            util::SECTION_HEADER_V11 sectionHeader;
            std::vector<uint8_t> payload;
            if (!util::readSectionV11(fp, sectionHeader, payload))
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read section %u [%s]", i, fileNamePath);

            if (sectionHeader.type == util::SECTION_MATERIAL_TRANSFORM)
            {
                FILE *tmp = stage_payload_as_tmpfile(payload);
                if (!tmp)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to stage SECTION_MATERIAL_TRANSFORM [%s]", fileNamePath);
                util::MATERIAL_TRANSFORM_V11 materialTransform;
                const bool ok = util::readMaterialTransformV11(tmp, materialTransform);
                fclose(tmp);
                if (!ok)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_MATERIAL_TRANSFORM [%s]", fileNamePath);

                impl->headerMesh.material = materialTransform.material;
                impl->headerMesh.angleX   = materialTransform.angleX;
                impl->headerMesh.angleY   = materialTransform.angleY;
                impl->headerMesh.angleZ   = materialTransform.angleZ;
                impl->headerMesh.posX     = materialTransform.posX;
                impl->headerMesh.posY     = materialTransform.posY;
                impl->headerMesh.posZ     = materialTransform.posZ;
                // setAngleDefault/setPositionOffset keep impl->angleDefault/positionOffset in sync with
                // headerMesh.angle*/pos* - loadV11 must mirror that, since getAngleDefault/
                // getPositionOffset read the impl-> copies, not headerMesh directly.
                impl->angleDefault = VEC3(materialTransform.angleX, materialTransform.angleY, materialTransform.angleZ);
                impl->positionOffset = VEC3(materialTransform.posX, materialTransform.posY, materialTransform.posZ);
                impl->info_mode.mode_draw = materialTransform.mode_draw;
                impl->info_mode.mode_cull_face = materialTransform.mode_cull_face;
                impl->info_mode.mode_front_face_direction = materialTransform.mode_front_face_direction;
            }
            else if (sectionHeader.type == util::SECTION_EXTRA_PATHS)
            {
                FILE *tmp = stage_payload_as_tmpfile(payload);
                if (!tmp)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to stage SECTION_EXTRA_PATHS [%s]", fileNamePath);
                uint32_t count = 0;
                bool ok = util::le_io::readU32LE(tmp, count);
                for (uint32_t p = 0; ok && p < count; ++p)
                {
                    std::string path;
                    ok = util::readStringV11(tmp, path);
                    if (ok)
                    {
#ifndef ANDROID
                        util::addPath(path.c_str());
#endif
                    }
                }
                fclose(tmp);
                if (!ok)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_EXTRA_PATHS [%s]", fileNamePath);
            }
            else if (sectionHeader.type == util::SECTION_DETAIL_PHYSICS)
            {
                FILE *tmp = stage_payload_as_tmpfile(payload);
                if (!tmp)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to stage SECTION_DETAIL_PHYSICS [%s]", fileNamePath);
                const bool ok = read_detail_mesh_section(tmp, fileNamePath, impl->headerMain, this->impl->infoPhysics, this->impl->extraInfo,
                                                         [this](FILE *f, const char *n, const int tb, const int fv)
                                                         {
                                                             return this->readDebugTriangleDetailCompat(f, n, tb, fv);
                                                         });
                fclose(tmp);
                if (!ok)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_DETAIL_PHYSICS [%s]", fileNamePath);
            }
            else if (sectionHeader.type == util::SECTION_FRAME_STATIC)
            {
                FILE *tmp = stage_payload_as_tmpfile(payload);
                if (!tmp)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to stage SECTION_FRAME_STATIC [%s]", fileNamePath);
                util::BUFFER_MESH_DEBUG *pBuffer = nullptr;
                util::FRAME_HEADER_V11 v11FrameHeader;
                const bool ok = this->readFrameStaticV11Payload(tmp, frame0, pBuffer, v11FrameHeader);
                fclose(tmp);
                if (!ok)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_FRAME_STATIC [%s]", fileNamePath);

                if (!sawFirstFrame)
                {
                    sawFirstFrame = true;
                    frame0        = pBuffer;
                    hasNormalFlag = v11FrameHeader.hasNormal ? HAS_NOR_IN_FILE : HAS_NOR_NO;
                    hasTextureFlag = !v11FrameHeader.hasUv ? HAS_TEX_NO
                                    : (v11FrameHeader.uvSource == 0 ? HAS_TEX_EACH_FRAME : HAS_TEX_FIRST_FRAME);
                }
                this->impl->buffer.push_back(pBuffer);
            }
            else
            {
                return log_util::onFailed(fp, __FILE__, __LINE__,
                                          "loadV11 does not support section type %u yet (milestone 4 core slice) [%s]",
                                          sectionHeader.type, fileNamePath);
            }
        }

        impl->headerMesh.totalFrames    = static_cast<int>(this->impl->buffer.size());
        impl->headerMesh.totalAnimation = 0;
        impl->headerMesh.hasNorText[0]  = hasNormalFlag;
        impl->headerMesh.hasNorText[1]  = hasTextureFlag;
        impl->fileName = fileNamePath;

        if (fp)
            fclose(fp);
        return true;
    }

    bool MESH_MBM_DEBUG::check(char *error,const int lenError)
    {
        if (this->impl->buffer.size() == 0)
        {
            if (error)
                strncpy(error, "Empty buffer",lenError);
            return false;
        }
        for (std::vector<util::BUFFER_MESH_DEBUG *>::size_type i = 0; i < this->impl->buffer.size(); ++i)
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->impl->buffer[i];
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

        for (uint32_t i = 0; i < this->impl->buffer.size(); ++i)
        {
            int                      iTotalVertex  = 0;
            int                      iTotalIndex   = 0;
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->impl->buffer[i];
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
            for (uint32_t i = 0; i < this->impl->buffer.size(); ++i)
            {
                centralizeFrame(static_cast<int>(i), indexSubset);
            }
        }
        else if (indexFrame < static_cast<int>(this->impl->buffer.size()))
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->impl->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG*>::size_type>(indexFrame)];
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
            for (uint32_t i = 0; i < this->impl->buffer.size(); ++i)
                rotateFrame(static_cast<int>(i), indexSubset, angleX, angleY, angleZ);
            return;
        }
        if (indexFrame >= static_cast<int>(this->impl->buffer.size())) return;
        const float radX = angleX * static_cast<float>(M_PI) / 180.0f;
        const float radY = angleY * static_cast<float>(M_PI) / 180.0f;
        const float radZ = angleZ * static_cast<float>(M_PI) / 180.0f;
        const float cosX = cosf(radX), sinX = sinf(radX);
        const float cosY = cosf(radY), sinY = sinf(radY);
        const float cosZ = cosf(radZ), sinZ = sinf(radZ);
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->impl->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)];
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
            for (uint32_t i = 0; i < this->impl->buffer.size(); ++i)
                scaleFrame(static_cast<int>(i), indexSubset, sx, sy, sz);
            return;
        }
        if (indexFrame >= static_cast<int>(this->impl->buffer.size())) return;
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->impl->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)];
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
            for (uint32_t i = 0; i < this->impl->buffer.size(); ++i)
                translateFrame(static_cast<int>(i), indexSubset, dx, dy, dz);
            return;
        }
        if (indexFrame >= static_cast<int>(this->impl->buffer.size())) return;
        util::BUFFER_MESH_DEBUG *bufferCurrent = this->impl->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)];
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
        if (indexFrame < this->impl->buffer.size() && indexSubset < this->impl->buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->impl->buffer[indexFrame];
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
                const auto tSubset = static_cast<int>(indexFrame < this->impl->buffer.size() ? this->impl->buffer[indexFrame]->subset.size() : 0);
                snprintf(strErrorOut, strErrorOutLen, "Out of bound[indexFrame(total %u),indexSubset(total %d)\n"
                                     "indexFrame %u indexSubset %u",
                        static_cast<uint32_t>(this->impl->buffer.size()), tSubset, indexFrame, indexSubset);
            }
            return false;
        }
    }
    
    bool MESH_MBM_DEBUG::addVertex(const uint32_t indexFrame, const uint32_t indexSubset, const uint32_t totalVertex)
    {
        if (indexFrame < this->impl->buffer.size() && indexSubset < this->impl->buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent    = this->impl->buffer[indexFrame];
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

            impl->headerMesh.hasNorText[0] = HAS_NOR_IN_FILE; // addVertex always allocates normals
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
        if (this->impl->buffer.size() == 0)
        {
            if (errorOut)
                snprintf(errorOut, errorOutLen, "there is no frame ");
            return 0;
        }
        if (initialFrame < 0 || initialFrame >= static_cast<int>(this->impl->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut, errorOutLen, "initial frame [%d] out of range ->[%d]", initialFrame, static_cast<int>(this->impl->buffer.size()));
            return 0;
        }
        if (finalFrame < 0 || finalFrame >= static_cast<int>(this->impl->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut, errorOutLen, "final frame [%d] out of range ->[%d]", finalFrame, static_cast<int>(this->impl->buffer.size()));
            return 0;
        }
        if (typeAnimation < 0 || typeAnimation > 6)
        {
            if (errorOut)
                snprintf(errorOut, errorOutLen, "type of animation [%d] out of range ->[0-6]", typeAnimation);
            return 0;
        }
        auto infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
        this->impl->infoAnimation.lsHeaderAnim.push_back(infoHead);
        impl->headerMesh.totalAnimation = static_cast<int>(this->impl->infoAnimation.lsHeaderAnim.size());
        infoHead->headerAnim     = new util::HEADER_ANIMATION();
        if (nameAnimation)
            strncpy(infoHead->headerAnim->nameAnimation, nameAnimation, sizeof(infoHead->headerAnim->nameAnimation) - 1);
        else
            strncpy(infoHead->headerAnim->nameAnimation, "default",sizeof(infoHead->headerAnim->nameAnimation) - 1);
        infoHead->headerAnim->initialFrame     = initialFrame;
        infoHead->headerAnim->finalFrame       = finalFrame;
        infoHead->headerAnim->timeBetweenFrame = timeBetweenFrame <= 0.0f ? 0.0f : timeBetweenFrame;
        infoHead->headerAnim->typeAnimation    = typeAnimation;
        return impl->headerMesh.totalAnimation;
    }

    bool MESH_MBM_DEBUG::updateAnimation(const uint32_t index, const char *nameAnimation, const int initialFrame, const int finalFrame,
                           const float timeBetweenFrame, const int typeAnimation, char *errorOut,const int lenError)
    {
        if (this->impl->buffer.size() == 0)
        {
            if (errorOut)
                snprintf(errorOut,lenError, "there is no frame ");
            return false;
        }
        if(index >= this->impl->infoAnimation.lsHeaderAnim.size())
        {
            if (errorOut)
                snprintf(errorOut, lenError,"index animation out of range. Total anim -> [%d] index -> [%d] ",static_cast<int>(this->impl->infoAnimation.lsHeaderAnim.size()),static_cast<int>(index));
            return false;
        }
        if (initialFrame < 0 || initialFrame >= static_cast<int>(this->impl->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut, lenError,"initial frame [%d] out of range ->[%d]", initialFrame, static_cast<int>(this->impl->buffer.size()));
            return false;
        }
        if (finalFrame < 0 || finalFrame >= static_cast<int>(this->impl->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut,lenError, "final frame [%d] out of range ->[%d]", finalFrame, static_cast<int>(this->impl->buffer.size()));
            return false;
        }
        if (typeAnimation < 0 || typeAnimation > 6)
        {
            if (errorOut)
                snprintf(errorOut,lenError, "type of animation [%d] out of range ->[0-6]", typeAnimation);
            return false;
        }
        util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead = this->impl->infoAnimation.lsHeaderAnim[index];
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
        if(index < this->impl->infoAnimation.lsHeaderAnim.size())
            return this->impl->infoAnimation.lsHeaderAnim[index];
        return nullptr;
    }

    void MESH_MBM_DEBUG::deleteExtraInfo()
    {
        switch(impl->typeMe)
        {
            case util::TYPE_MESH_FONT:
            {
                auto* infoFont = static_cast<mbm::INFO_BOUND_FONT*>(impl->extraInfo);
                if(infoFont)
                    delete infoFont;
            }
            break;
            case util::TYPE_MESH_PARTICLE:
            {
                auto* lsParticleInfo = static_cast<std::vector<util::STAGE_PARTICLE*>*>(impl->extraInfo);
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
                auto* infoTileMap = static_cast<util::BTILE_INFO*>(impl->extraInfo);
                if(infoTileMap)
                    delete infoTileMap;
            }
            break;
                        case util::TYPE_MESH_SHAPE:
            {
                auto* infoShape = static_cast<util::DYNAMIC_SHAPE*>(impl->extraInfo);
                if(infoShape)
                    delete infoShape;
            }
            break;
            default:
            {
                if (impl->extraInfo)
                {
                    auto * charExtraInfo = static_cast<char*>(impl->extraInfo);
                    delete[] charExtraInfo;
                }
            }
        }
        impl->extraInfo           = nullptr;
    }
    
    void MESH_MBM_DEBUG::fixDefaultBoud()
    {
        if (this->impl->infoPhysics.lsCube.size() == 0)
        {
            this->fillAtLeastOneBound();
            impl->headerMesh.deprecated_typePhysics = 1;
        }
    }
    
    void MESH_MBM_DEBUG::release()
    {
        deleteExtraInfo();
        if (this->impl->coordTexFrame_0)
            delete[] this->impl->coordTexFrame_0;
        this->impl->coordTexFrame_0 = nullptr;
        
        for (auto meshBuffer : this->impl->buffer)
        {
            if (meshBuffer)
                delete meshBuffer;
            meshBuffer = nullptr;
        }
        impl->buffer.clear();
        impl->angleDefault        = VEC3(0, 0, 0);
        impl->positionOffset      = VEC3(0, 0, 0);
        impl->sizeCoordTexFrame_0 = 0;
        impl->typeMe              = util::TYPE_MESH_UNKNOWN;
        memset(static_cast<void*>(&this->impl->headerMain), 0, sizeof(this->impl->headerMain));
        memset(static_cast<void*>(&this->impl->headerMesh), 0, sizeof(this->impl->headerMesh));
        impl->zoomEditorSprite.x = 1.0f;
        impl->zoomEditorSprite.y = 1.0f;
        util::MATERIAL m;
        this->impl->headerMesh.material      = m;
        this->impl->headerMesh.hasNorText[0] = HAS_NOR_NO;
        this->impl->headerMesh.hasNorText[1] = HAS_TEX_EACH_FRAME;
        this->impl->infoPhysics.release();
        this->impl->infoAnimation.release();
    }

    void MESH_MBM_DEBUG::fillAtLeastOneBound()
    {
        impl->headerMesh.deprecated_typePhysics = 1;
        auto base                       = new CUBE();
        this->impl->infoPhysics.release();
        this->impl->infoPhysics.lsCube.push_back(base);
        VEC3 vMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        VEC3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
        for (int currentFrame = 0; currentFrame < impl->headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->impl->buffer[std::vector<util::BUFFER_MESH_DEBUG *>::size_type(currentFrame)];
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
    



    VEC3 MESH_MBM::getPositionOffset() const noexcept
    {
        return impl->positionOffset;
    }

    void MESH_MBM::setPositionOffset(const VEC3 &position) noexcept
    {
        impl->positionOffset = position;
    }

    VEC3 MESH_MBM::getAngleDefault() const noexcept
    {
        return impl->angleDefault;
    }

    void MESH_MBM::setAngleDefault(const VEC3 &angle) noexcept
    {
        impl->angleDefault = angle;
    }

    BUFFER_MESH * MESH_MBM::getBuffer(const uint32_t index) const
    {
        if (index < this->impl->totalFramesMesh && impl->buffer)
            return &impl->buffer[index];
        return nullptr;
    }

    TEXTURE * MESH_MBM::getTexture(const uint32_t indexFrame, const uint32_t indexSubset)
    {
        if (indexFrame < impl->totalFramesMesh && impl->buffer)
        {
            if (indexSubset < impl->buffer[indexFrame].totalSubset)
                return impl->buffer[indexFrame].subset[indexSubset].texture;
        }
        return nullptr;
    }
    
    bool MESH_MBM::setTexture(const uint32_t indexFrame, const uint32_t indexSubset, const char *fileNameTexture,
                           const bool hasAlpha)
    {
        if (indexFrame < impl->totalFramesMesh && impl->buffer)
        {
            if (indexSubset < impl->buffer[indexFrame].totalSubset)
            {
                impl->buffer[indexFrame].subset[indexSubset].texture =
                    TEXTURE_MANAGER::getInstance()->load(fileNameTexture, hasAlpha);
                if (impl->buffer[indexFrame].pBufferGL && impl->buffer[indexFrame].subset[indexSubset].texture)
                {
                    for (uint32_t i = 0; i < impl->buffer[indexFrame].pBufferGL->totalSubset; ++i)
                    {
                        impl->buffer[indexFrame].pBufferGL->setTextureByStage(impl->buffer[indexFrame].subset[indexSubset].texture, 0, i);
                    }
                    return true;
                }
                return impl->buffer[indexFrame].subset[indexSubset].texture != nullptr;
            }
        }
        return false;
    }
    
    const char * MESH_MBM::getFilenameMesh() const
    {
        return impl->fileName.c_str();
    }

    INFO_PHYSICS & MESH_MBM::getPhysicsInfo() noexcept
    {
        return this->impl->infoPhysics;
    }

    const INFO_PHYSICS & MESH_MBM::getPhysicsInfo() const noexcept
    {
        return this->impl->infoPhysics;
    }

    void MESH_MBM::resetPhysicsInfo()
    {
        this->impl->infoPhysics.release();
    }

    void MESH_MBM::appendPhysicsCube(CUBE *cube) noexcept
    {
        if (cube)
            this->impl->infoPhysics.lsCube.push_back(cube);
    }

    void MESH_MBM::appendPhysicsSphere(SPHERE *sphere) noexcept
    {
        if (sphere)
            this->impl->infoPhysics.lsSphere.push_back(sphere);
    }

    void MESH_MBM::appendPhysicsCubeComplex(CUBE_COMPLEX *cubeComplex) noexcept
    {
        if (cubeComplex)
            this->impl->infoPhysics.lsCubeComplex.push_back(cubeComplex);
    }

    void MESH_MBM::appendPhysicsTriangle(TRIANGLE *triangle) noexcept
    {
        if (triangle)
            this->impl->infoPhysics.lsTriangle.push_back(triangle);
    }

    util::INFO_ANIMATION & MESH_MBM::getAnimationInfo() noexcept
    {
        return this->impl->infoAnimation;
    }

    const util::INFO_ANIMATION & MESH_MBM::getAnimationInfo() const noexcept
    {
        return this->impl->infoAnimation;
    }

    uint32_t MESH_MBM::getTotalAnimations() const noexcept
    {
        return static_cast<uint32_t>(this->impl->infoAnimation.lsHeaderAnim.size());
    }

    util::INFO_ANIMATION::INFO_HEADER_ANIM * MESH_MBM::getAnimationHeader(const uint32_t index) const noexcept
    {
        if (index < this->impl->infoAnimation.lsHeaderAnim.size())
            return this->impl->infoAnimation.lsHeaderAnim[index];
        return nullptr;
    }
    
    MESH_MBM::~MESH_MBM()
    {
        release();
    }

    void MESH_MBM::deleteExtraInfo()
    {
        switch(impl->typeMe)
        {
            case util::TYPE_MESH_FONT:
            {
                auto* infoFont = static_cast<mbm::INFO_BOUND_FONT*>(impl->extraInfo);
                if(infoFont)
                    delete infoFont;
            }
            break;
            case util::TYPE_MESH_PARTICLE:
            {
                auto* lsParticleInfo = static_cast<std::vector<util::STAGE_PARTICLE*>*>(impl->extraInfo);
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
                auto* infoTileMap = static_cast<util::BTILE_INFO*>(impl->extraInfo);
                if(infoTileMap)
                    delete infoTileMap;
            }
            break;
            case util::TYPE_MESH_SHAPE:
            {
                auto* infoShape = static_cast<util::DYNAMIC_SHAPE*>(impl->extraInfo);
                if(infoShape)
                    delete infoShape;
            }
            break;
            default:
            {
                if (impl->extraInfo)
                {
                    auto * charExtraInfo = static_cast<char*>(impl->extraInfo);
                    delete[] charExtraInfo;
                }
            }
        }
        impl->extraInfo           = nullptr;
    }
    
    void MESH_MBM::release() //
    {
        deleteExtraInfo();
        if (impl->buffer)
            delete[] impl->buffer;
        impl->buffer = nullptr;
        this->impl->infoPhysics.release();
        this->impl->infoAnimation.release();

        if (impl->coordTexFrame_0)
            delete[] impl->coordTexFrame_0;
        impl->coordTexFrame_0 = nullptr;

        impl->totalFramesMesh = 0;
        
        impl->zoomEditorSprite.x  = 0;
        impl->zoomEditorSprite.y  = 0;
        impl->typeMe              = util::TYPE_MESH_UNKNOWN;
        impl->hasNormTex[0]       = 0;
        impl->hasNormTex[1]       = 0;
        impl->depthUberImage      = 8;
        impl->sizeCoordTexFrame_0 = 0;
    }
    
    bool MESH_MBM::isLoaded() const
    {
        return this->impl->buffer != nullptr;
    }
    
    bool MESH_MBM::render(const uint32_t indexFrame,const SHADER *pShader,
                          const RENDERIZABLE *renderizableOwner)
    {
        if (indexFrame < impl->totalFramesMesh && impl->buffer)
        {
            DEVICE *device = DEVICE::getInstance();
            device->setRenderMaterial(this->impl->material);
            const bool ret = pShader->render(impl->buffer[indexFrame].pBufferGL, renderizableOwner);
            device->clearRenderMaterial();
            return ret;
        }
        return false;
    }

    bool MESH_MBM::renderDynamic(const uint32_t indexFrame, SHADER *pShader, VEC3 *vertex, VEC3 *normal,
                                    VEC2 *uv, const RENDERIZABLE *renderizableOwner)
    {
        if (indexFrame < impl->totalFramesMesh && impl->buffer)
        {
            DEVICE *device = DEVICE::getInstance();
            device->setRenderMaterial(this->impl->material);
            const bool ret = pShader->renderDynamic(impl->buffer[indexFrame].pBufferGL, vertex, normal, uv,
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
        return impl->typeMe;
    }
    
    VEC2 MESH_MBM::getZoomEditorSprite() const
    {
        return this->impl->zoomEditorSprite;
    }
    
    uint32_t MESH_MBM::getTotalFrame() const
    {
        return impl->totalFramesMesh;
    }
    
    uint32_t MESH_MBM::getTotalSubset(const uint32_t indexFrame) const
    {
        if (indexFrame < impl->totalFramesMesh && impl->buffer)
            return impl->buffer[indexFrame].totalSubset;
        return 0;
    }
    
    MESH_MBM::MESH_MBM()
        : impl(std::make_unique<Impl>())
    {
        impl->buffer          = nullptr;
        impl->extraInfo       = nullptr;
        impl->totalFramesMesh = 0;

        impl->coordTexFrame_0     = nullptr;
        impl->sizeCoordTexFrame_0 = 0;

        impl->zoomEditorSprite.x  = 0;
        impl->zoomEditorSprite.y  = 0;
        impl->typeMe              = util::TYPE_MESH_UNKNOWN;
        impl->hasNormTex[0]       = 0;
        impl->hasNormTex[1]       = 0;
        impl->depthUberImage      = 8;
    }
    
    bool MESH_MBM::load(const char *fileNamePath)
    {
        return this->load(fileNamePath, nullptr);
    }

    bool MESH_MBM::load(const char *fileNamePath, RENDERIZABLE *renderizable)
    {
        if (!this->loadV11(fileNamePath))
            return false;
        if (renderizable)
        {
            renderizable->getPosition() += this->impl->positionOffset;
            renderizable->setAngle(this->impl->angleDefault);
        }
        return true;
    }

    bool MESH_MBM::readFrameStaticV11Payload(FILE *fp, const char *fileNamePath, const uint32_t currentFrame)
    {
        util::FRAME_HEADER_V11 frameHeader;
        if (!util::readFrameHeaderV11(fp, frameHeader))
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read FRAME_HEADER_V11 [%s]", fileNamePath);
        if (frameHeader.indexWidth != 16)
            return log_util::onFailed(fp, __FILE__, __LINE__, "loadV11 only supports 16-bit indices (milestone 4 core slice) [%s]", fileNamePath);

        impl->buffer[currentFrame].subset      = new util::SUBSET[frameHeader.totalSubset];
        impl->buffer[currentFrame].totalSubset = frameHeader.totalSubset;

        auto pPosition = new VEC3[frameHeader.vertexCount];
        VEC3 *pNormal  = frameHeader.hasNormal ? new VEC3[frameHeader.vertexCount] : nullptr;
        auto pTexture  = new VEC2[frameHeader.vertexCount];
        memset(static_cast<void *>(pTexture), 0, sizeof(VEC2) * static_cast<size_t>(frameHeader.vertexCount));
        uint16_t *pIndex = nullptr;

        const auto cleanup = [&]()
        {
            delete[] pPosition;
            delete[] pNormal;
            delete[] pTexture;
            delete[] pIndex;
        };

        for (uint32_t i = 0; i < frameHeader.vertexCount; ++i)
        {
            if (!util::le_io::readF32LE(fp, pPosition[i].x) || !util::le_io::readF32LE(fp, pPosition[i].y) ||
                !util::le_io::readF32LE(fp, pPosition[i].z))
            {
                cleanup();
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read frame position [%s]", fileNamePath);
            }
        }

        if (frameHeader.hasNormal)
        {
            for (uint32_t i = 0; i < frameHeader.vertexCount; ++i)
            {
                if (!util::le_io::readF32LE(fp, pNormal[i].x) || !util::le_io::readF32LE(fp, pNormal[i].y) ||
                    !util::le_io::readF32LE(fp, pNormal[i].z))
                {
                    cleanup();
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read frame normal [%s]", fileNamePath);
                }
            }
        }

        if (frameHeader.hasUv)
        {
            if (frameHeader.uvSource == 0) // OWN
            {
                for (uint32_t i = 0; i < frameHeader.vertexCount; ++i)
                {
                    if (!util::le_io::readF32LE(fp, pTexture[i].x) || !util::le_io::readF32LE(fp, pTexture[i].y))
                    {
                        cleanup();
                        return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read frame uv [%s]", fileNamePath);
                    }
                }
                // mirrors load_from_separated_buffers_common's HAS_TEX_FIRST_FRAME caching (~line 388-409):
                // BUFFER_MESH does not keep CPU-side uv arrays around after GPU upload, so frame 0's uv is
                // cached on impl for later SHARED_WITH_FRAME_0 frames to copy from.
                if (!impl->coordTexFrame_0)
                {
                    impl->coordTexFrame_0     = new VEC2[frameHeader.vertexCount];
                    impl->sizeCoordTexFrame_0 = static_cast<int>(frameHeader.vertexCount);
                    memcpy(static_cast<void *>(impl->coordTexFrame_0), pTexture, sizeof(VEC2) * static_cast<size_t>(frameHeader.vertexCount));
                }
            }
            else if (impl->coordTexFrame_0) // SHARED_WITH_FRAME_0
            {
                const uint32_t safeCopy =
                    std::min(frameHeader.vertexCount, static_cast<uint32_t>(impl->sizeCoordTexFrame_0));
                memcpy(static_cast<void *>(pTexture), impl->coordTexFrame_0, sizeof(VEC2) * static_cast<size_t>(safeCopy));
            }
        }

        if (frameHeader.indexCount > 0)
        {
            pIndex = new uint16_t[frameHeader.indexCount];
            for (uint32_t i = 0; i < frameHeader.indexCount; ++i)
            {
                if (!util::le_io::readU16LE(fp, pIndex[i]))
                {
                    cleanup();
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read frame index [%s]", fileNamePath);
                }
            }
        }

        std::vector<TEXTURE *> lsIdTexture;
        std::vector<uint8_t>   lsHasColorKeying;
        TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
        for (uint32_t s = 0; s < frameHeader.totalSubset; ++s)
        {
            util::SUBSET_DESC_V11 subsetDesc;
            if (!util::readSubsetDescV11(fp, subsetDesc))
            {
                cleanup();
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read SUBSET_DESC_V11 [%s]", fileNamePath);
            }

            util::SUBSET &subset = impl->buffer[currentFrame].subset[s];
            subset.vertexStart   = subsetDesc.vertexStart;
            subset.vertexCount   = subsetDesc.vertexCount;
            subset.indexStart    = subsetDesc.indexStart;
            subset.indexCount    = subsetDesc.indexCount;
            subset.texture       = textureManager->load(subsetDesc.primaryTexture.path.c_str(), subsetDesc.alphaColor[0] != 0);
            if (subset.texture)
            {
                lsIdTexture.push_back(subset.texture);
                lsHasColorKeying.push_back(subset.texture->hasAlphaChannel() ? 1 : 0);
            }
            else
            {
                lsIdTexture.push_back(nullptr);
                lsHasColorKeying.push_back(0);
            }

            for (uint16_t e = 0; e < subsetDesc.extraSlotCount; ++e)
            {
                util::SUBSET_EXTRA_SLOT_V11 extraSlot;
                if (!util::readSubsetExtraSlotV11(fp, extraSlot))
                {
                    cleanup();
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read SUBSET_EXTRA_SLOT_V11 [%s]", fileNamePath);
                }
                uint16_t legacyType = 0;
                if (textureRoleToLegacyMaterialSlotType(static_cast<mbm::TEXTURE_ROLE>(extraSlot.role), legacyType))
                {
                    util::MATERIAL_TEXTURE_SLOT_HEADER slotHeader;
                    slotHeader.type = legacyType;
                    strncpy(slotHeader.nameTexture, extraSlot.texture.path.c_str(), sizeof(slotHeader.nameTexture) - 1);
                    slotHeader.nameTexture[sizeof(slotHeader.nameTexture) - 1] = 0;
                    slotHeader.payloadSizeInBytes = 0;
                    subset.materialTextureSlotHeaders.push_back(slotHeader);
                    subset.materialTextures.push_back(textureManager->load(extraSlot.texture.path.c_str(), true));
                }
                else
                {
                    PRINT_IF_DEBUG("Warning! loadV11 skipped extra texture slot with unrecognized role [%d]\n", extraSlot.role);
                }
            }
        }

        impl->buffer[currentFrame].pBufferGL = new BUFFER_GL();
        const bool hasIndex                  = frameHeader.indexCount > 0;
        bool       loadOk                    = false;
        if (hasIndex)
        {
            auto indexStartSubset = new int[impl->buffer[currentFrame].totalSubset];
            auto indexCountSubset = new int[impl->buffer[currentFrame].totalSubset];
            for (uint32_t s = 0; s < frameHeader.totalSubset; ++s)
            {
                indexStartSubset[s] = impl->buffer[currentFrame].subset[s].indexStart;
                indexCountSubset[s] = impl->buffer[currentFrame].subset[s].indexCount;
            }
            loadOk = impl->buffer[currentFrame].pBufferGL->loadBuffer(pPosition, pNormal, pTexture,
                                                                      static_cast<uint32_t>(frameHeader.vertexCount), pIndex,
                                                                      impl->buffer[currentFrame].totalSubset, indexStartSubset,
                                                                      indexCountSubset, &impl->info_mode);
            delete[] indexStartSubset;
            delete[] indexCountSubset;
        }
        else
        {
            auto vertexStartSubset = new int[impl->buffer[currentFrame].totalSubset];
            auto vertexCountSubset = new int[impl->buffer[currentFrame].totalSubset];
            for (uint32_t s = 0; s < frameHeader.totalSubset; ++s)
            {
                vertexStartSubset[s] = impl->buffer[currentFrame].subset[s].vertexStart;
                vertexCountSubset[s] = impl->buffer[currentFrame].subset[s].vertexCount;
            }
            constexpr bool isDynamic = false;
            loadOk = impl->buffer[currentFrame].pBufferGL->loadBuffer(pPosition, pNormal, pTexture,
                                                                      static_cast<uint32_t>(frameHeader.vertexCount),
                                                                      impl->buffer[currentFrame].totalSubset, vertexStartSubset,
                                                                      vertexCountSubset, &impl->info_mode, isDynamic);
            delete[] vertexStartSubset;
            delete[] vertexCountSubset;
        }
        cleanup();
        if (!loadOk)
            return log_util::onFailed(fp, __FILE__, __LINE__, "error on load buffer for frame %u [%s]", currentFrame, fileNamePath);

        const std::vector<TEXTURE *>::size_type totalIdTexture =
            (impl->buffer[currentFrame].pBufferGL->totalSubset > lsIdTexture.size())
                ? lsIdTexture.size()
                : impl->buffer[currentFrame].pBufferGL->totalSubset;
        for (std::vector<TEXTURE *>::size_type s = 0; s < totalIdTexture; ++s)
            impl->buffer[currentFrame].pBufferGL->setTextureByStage(lsIdTexture[s], 0, static_cast<uint32_t>(s));
        return true;
    }

    bool MESH_MBM::loadV11(const char *fileNamePath)
    {
        this->release();
        FILE *fp = util::openFile(fileNamePath, "rb");
        if (!fp)
            return log_util::onFailed(fp, __FILE__, __LINE__, "Failed to open file [%s]", fileNamePath);

        util::FILE_HEADER_V11 fileHeader;
        if (!util::readFileHeaderV11(fp, fileHeader))
            return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read v11 file header [%s]", fileNamePath);

        impl->fileName = fileNamePath;
        impl->typeMe   = static_cast<util::TYPE_MESH>(fileHeader.typeMesh);
        util::HEADER headerMain;
        headerMain.version = CURRENT_VERSION_MBM_HEADER;
        if (impl->typeMe == util::TYPE_MESH_TILE_MAP)
            mbm::TEXTURE::EnablePixelPerfectTexture(true);
        else
            mbm::TEXTURE::EnablePixelPerfectTexture(false);

        // First pass: stage every section's payload in memory and count SECTION_FRAME_STATIC entries,
        // since impl->buffer is a fixed-size array (unlike MESH_MBM_DEBUG's vector) and must be
        // allocated to the right size up front, matching loadImpl's `new BUFFER_MESH[headerMesh.totalFrames]`.
        struct StagedSection
        {
            util::SECTION_HEADER_V11 header;
            std::vector<uint8_t>     payload;
        };
        std::vector<StagedSection> sections;
        sections.reserve(fileHeader.sectionCount);
        uint32_t totalFrames = 0;
        for (uint32_t i = 0; i < fileHeader.sectionCount; ++i)
        {
            StagedSection staged;
            if (!util::readSectionV11(fp, staged.header, staged.payload))
            {
                fclose(fp);
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read section %u [%s]", i, fileNamePath);
            }
            if (staged.header.type == util::SECTION_FRAME_STATIC)
                ++totalFrames;
            sections.push_back(std::move(staged));
        }
        fclose(fp);

        impl->buffer          = new BUFFER_MESH[totalFrames];
        impl->totalFramesMesh = totalFrames;

        uint32_t currentFrame = 0;
        for (const auto &staged : sections)
        {
            FILE *tmp = stage_payload_as_tmpfile(staged.payload);
            if (!tmp)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to stage section %u [%s]", staged.header.type, fileNamePath);

            if (staged.header.type == util::SECTION_MATERIAL_TRANSFORM)
            {
                util::MATERIAL_TRANSFORM_V11 materialTransform;
                const bool ok = util::readMaterialTransformV11(tmp, materialTransform);
                fclose(tmp);
                if (!ok)
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to parse SECTION_MATERIAL_TRANSFORM [%s]", fileNamePath);
                impl->material        = materialTransform.material;
                impl->positionOffset  = VEC3(materialTransform.posX, materialTransform.posY, materialTransform.posZ);
                impl->angleDefault    = VEC3(materialTransform.angleX, materialTransform.angleY, materialTransform.angleZ);
                impl->info_mode.mode_draw                 = materialTransform.mode_draw;
                impl->info_mode.mode_cull_face            = materialTransform.mode_cull_face;
                impl->info_mode.mode_front_face_direction = materialTransform.mode_front_face_direction;
            }
            else if (staged.header.type == util::SECTION_EXTRA_PATHS)
            {
                uint32_t count = 0;
                bool ok = util::le_io::readU32LE(tmp, count);
                for (uint32_t p = 0; ok && p < count; ++p)
                {
                    std::string path;
                    ok = util::readStringV11(tmp, path);
                    if (ok)
                    {
#ifndef ANDROID
                        util::addPath(path.c_str());
#endif
                    }
                }
                fclose(tmp);
                if (!ok)
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to parse SECTION_EXTRA_PATHS [%s]", fileNamePath);
            }
            else if (staged.header.type == util::SECTION_DETAIL_PHYSICS)
            {
                const bool ok = read_detail_mesh_section(tmp, fileNamePath, headerMain, impl->infoPhysics, impl->extraInfo,
                                                         [this](FILE *f, const char *n, const int tb, const int fv)
                                                         {
                                                             return this->readTriangleDetailCompat(f, n, tb, fv);
                                                         });
                fclose(tmp);
                if (!ok)
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to parse SECTION_DETAIL_PHYSICS [%s]", fileNamePath);
            }
            else if (staged.header.type == util::SECTION_FRAME_STATIC)
            {
                const bool ok = this->readFrameStaticV11Payload(tmp, fileNamePath, currentFrame);
                fclose(tmp);
                if (!ok)
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to parse SECTION_FRAME_STATIC [%s]", fileNamePath);
                ++currentFrame;
            }
            else
            {
                fclose(tmp);
                return log_util::onFailed(nullptr, __FILE__, __LINE__,
                                          "loadV11 does not support section type %u yet (milestone 4 core slice) [%s]",
                                          staged.header.type, fileNamePath);
            }
        }

        impl->hasNormTex[0] = 0;
        impl->hasNormTex[1] = 0;
        return true;
    }

    const INFO_BOUND_FONT* MESH_MBM::getInfoFont()const
    {
        if(this->impl->typeMe == util::TYPE_MESH_FONT)
            return static_cast<INFO_BOUND_FONT*>(this->impl->extraInfo);
        return nullptr;
    }

    const std::vector<util::STAGE_PARTICLE*>* MESH_MBM::getInfoParticle()const
    {
        if(this->impl->typeMe == util::TYPE_MESH_PARTICLE)
            return static_cast<std::vector<util::STAGE_PARTICLE*>*>(this->impl->extraInfo);
        return nullptr;
    }

    const util::BTILE_INFO* MESH_MBM::getInfoTile()const
    {
        if(this->impl->typeMe == util::TYPE_MESH_TILE_MAP)
            return static_cast<util::BTILE_INFO*>(this->impl->extraInfo);
        return nullptr;
    }

        API_IMPL const util::DYNAMIC_SHAPE* MESH_MBM::getInfoShape()const
        {
            if(this->impl->typeMe == util::TYPE_MESH_SHAPE)
                 return static_cast<util::DYNAMIC_SHAPE*>(this->impl->extraInfo);
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
                renderizable->getPosition() += mesh->impl->positionOffset;
                renderizable->setAngle(mesh->impl->angleDefault);
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
        std::vector<FONT_GLYPH_QUAD> lsGlyphQuad;
        std::vector<VEC2>            lsWidthLetter;

        TEXTURE *texture = TEXTURE_MANAGER::getInstance()->loadTTF(fileNameTtf, &lsGlyphQuad, &lsWidthLetter, heightLetter,saveTextureAsPng);
        if (texture == nullptr || lsGlyphQuad.size() < 30)
        {
            delete mesh;
            return nullptr;
        }
        if(texture_loaded != nullptr)
            *texture_loaded = texture;
        auto tTotalSTB = static_cast<uint32_t>(lsGlyphQuad.size() - 30);
        VEC3         pPosition[4];
        VEC3*        pNormal = nullptr; // no normal for font, only position and texture
        VEC2         pTexture[4];

        /*for (auto & i : pNormal)
        {
            i.x = 0;
            i.y = 0;
            i.z = 1;
        }*/

        mesh->impl->buffer                       = new BUFFER_MESH[tTotalSTB];
        mesh->impl->totalFramesMesh              = tTotalSTB;
        uint16_t    indexQuad[6] = {0, 1, 2, 2, 1, 3};
        auto* infoFont          = new INFO_BOUND_FONT();
        mesh->impl->extraInfo					   = infoFont;
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
        
        for (uint32_t i = 30, index = 0; i < lsGlyphQuad.size(); ++i)
        {
            const FONT_GLYPH_QUAD &q = lsGlyphQuad[i];
            if (q.valid)
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

                pTexture[0].x = q.s0;
                pTexture[0].y = q.t1;
                pTexture[1].x = q.s0;
                pTexture[1].y = q.t0;
                pTexture[2].x = q.s1;
                pTexture[2].y = q.t1;
                pTexture[3].x = q.s1;
                pTexture[3].y = q.t0;

                mesh->impl->buffer[index].pBufferGL            = new BUFFER_GL();
                mesh->impl->buffer[index].subset               = new util::SUBSET[1];
                mesh->impl->buffer[index].totalSubset          = 1;
                mesh->impl->buffer[index].subset[0].indexCount = 6;
                
                if (mesh->impl->buffer[index].pBufferGL->loadBuffer(
                        pPosition, pNormal, pTexture, 4, indexQuad, mesh->impl->buffer[index].totalSubset,
                        &mesh->impl->buffer[index].subset[0].indexStart, &mesh->impl->buffer[index].subset[0].indexCount,nullptr))
                {

                    mesh->impl->buffer[index].subset[0].texture        = texture;
                    mesh->impl->buffer[index].pBufferGL->setTextureByStage(texture, 0, 0);
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
                    break;
                }
            }
        }
        if (mesh)
        {
            mesh->impl->positionOffset                    = VEC3(0, 0, 0);
            mesh->impl->angleDefault                      = VEC3(0, 0, 0);
            mesh->impl->typeMe                            = util::TYPE_MESH_FONT;
            auto header = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            mesh->impl->infoAnimation.lsHeaderAnim.push_back(header);
            header->headerAnim             = new util::HEADER_ANIMATION();
            header->headerAnim->hasShaderEffect = 1; // always will be 1
            this->impl->lsMeshes[fileNameBaseSuppose] = mesh;
            const char *fontps                 = "font.ps";
            header->headerAnim->typeAnimation  = 1;
            mesh->impl->hasNormTex[0] = HAS_NOR_IN_FILE;//has normal
            mesh->impl->hasNormTex[1] = HAS_TEX_EACH_FRAME;//uv each frame
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
            mesh->impl->fileName = std::move(fileNameBaseSuppose);
            
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
        mesh->impl->buffer                          = new BUFFER_MESH[1];
        mesh->impl->totalFramesMesh                 = 1;
        mesh->impl->buffer[0].pBufferGL             = new BUFFER_GL();
        mesh->impl->buffer[0].subset                = new util::SUBSET[1];
        mesh->impl->buffer[0].totalSubset           = 1;
        mesh->impl->buffer[0].subset[0].vertexCount = sizeVertexBuffer / 3;

        {
            std::string which_mode;
            if(info_mode && is_any_mode_valid(*info_mode,which_mode) == false)
            {
                ERROR_LOG( "Invalid mode %s detected:[%s]", which_mode.c_str(),nickName);
                delete mesh;
                return nullptr;
            }
        }

        if (!mesh->impl->buffer[0].pBufferGL->loadBuffer(
                reinterpret_cast<VEC3 *>(pPosition), reinterpret_cast<VEC3 *>(pNormal), reinterpret_cast<VEC2 *>(pTexture), sizeVertexBuffer / 3, mesh->impl->buffer[0].totalSubset,
                &mesh->impl->buffer[0].subset[0].vertexStart, &mesh->impl->buffer[0].subset[0].vertexCount,info_mode, isDynamic))
        {
            ERROR_LOG( "error on load buffer bufferTriangleList [%s]", nickName);
            delete mesh;
            return nullptr;
        }

        mesh->impl->positionOffset = VEC3(0, 0, 0);
        mesh->impl->angleDefault   = VEC3(0, 0, 0);
        mesh->impl->typeMe         = util::TYPE_MESH_SHAPE;
        if(info_mode)
        {
            mesh->impl->info_mode.mode_draw = info_mode->mode_draw;
            mesh->impl->info_mode.mode_cull_face = info_mode->mode_cull_face;
            mesh->impl->info_mode.mode_front_face_direction = info_mode->mode_front_face_direction;
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
        mesh->impl->buffer                          = new BUFFER_MESH[1];
        mesh->impl->totalFramesMesh                 = 1;
        mesh->impl->buffer[0].pBufferGL             = new BUFFER_GL();
        mesh->impl->buffer[0].subset                = new util::SUBSET[1];
        mesh->impl->buffer[0].totalSubset           = 1;
        mesh->impl->buffer[0].subset[0].vertexCount = sizeVertexBuffer / 3;
        mesh->impl->buffer[0].subset[0].indexCount  = static_cast<int>(sizeIndex);

        std::string which_mode;
        if(info_draw_mode && is_any_mode_valid(*info_draw_mode,which_mode) == false)
        {
            ERROR_LOG( "Invalid mode [%s] detected for [%s]", which_mode.c_str(),nickName);
            delete mesh;
            return nullptr;
        }

        if (!mesh->impl->buffer[0].pBufferGL->loadBuffer(reinterpret_cast<VEC3*>(pPosition),reinterpret_cast<VEC3 *>(pNormal), reinterpret_cast<VEC2 *>(pTexture),
                                                   sizeVertexBuffer / 3, index, mesh->impl->buffer[0].totalSubset,
                                                   &mesh->impl->buffer[0].subset[0].indexStart,
                                                   &mesh->impl->buffer[0].subset[0].indexCount,info_draw_mode))
        {
            ERROR_LOG( "error on load buffer bufferTriangleList [%s]", nickName);
            delete mesh;
            return nullptr;
        }

        mesh->impl->positionOffset = VEC3(0, 0, 0);
        mesh->impl->angleDefault   = VEC3(0, 0, 0);
        mesh->impl->typeMe         = util::TYPE_MESH_SHAPE;
        if(info_draw_mode)
        {
            mesh->impl->info_mode.mode_draw = info_draw_mode->mode_draw;
            mesh->impl->info_mode.mode_cull_face = info_draw_mode->mode_cull_face;
            mesh->impl->info_mode.mode_front_face_direction = info_draw_mode->mode_front_face_direction;
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
        mesh->impl->buffer                          = new BUFFER_MESH[1];
        mesh->impl->totalFramesMesh                 = 1;
        mesh->impl->buffer[0].pBufferGL             = new BUFFER_GL();
        mesh->impl->buffer[0].subset                = new util::SUBSET[1];
        mesh->impl->buffer[0].totalSubset           = 1;
        mesh->impl->buffer[0].subset[0].vertexCount = sizeVertexBuffer / 3;
        mesh->impl->buffer[0].subset[0].indexCount  = static_cast<int>(sizeIndex);
        const bool hasNormal                  = dynamic_shape_info.size_normal > 0;
        const bool hasUv                      = dynamic_shape_info.size_uv > 0;

        std::string which_mode;
        if(info_draw_mode && is_any_mode_valid(*info_draw_mode,which_mode) == false)
        {
            ERROR_LOG( "Invalid mode [%s] detected for [%s]", which_mode.c_str(),nickName);
            delete mesh;
            return nullptr;
        }

        if (!mesh->impl->buffer[0].pBufferGL->loadBufferDynamic(index, mesh->impl->buffer[0].totalSubset,
                                                          &mesh->impl->buffer[0].subset[0].indexStart,
                                                          &mesh->impl->buffer[0].subset[0].indexCount, 
                                                          hasNormal, hasUv, info_draw_mode))
        {
            ERROR_LOG( "error on load buffer bufferTriangleList [%s]", nickName);
            delete mesh;
            return nullptr;
        }
        mesh->impl->positionOffset = VEC3(0, 0, 0);
        mesh->impl->angleDefault   = VEC3(0, 0, 0);
        mesh->impl->typeMe         = util::TYPE_MESH_SHAPE;
        util::DYNAMIC_SHAPE * extra_info_shape = new util::DYNAMIC_SHAPE(dynamic_shape_info.dynamicVertex,dynamic_shape_info.dynamicNormal,dynamic_shape_info.dynamicUV,dynamic_shape_info.size_vertex,dynamic_shape_info.size_normal,dynamic_shape_info.size_uv);
        mesh->impl->extraInfo      = extra_info_shape;
        mesh->impl->fileName       = fileNameBase;
        if(info_draw_mode)
        {
            mesh->impl->info_mode.mode_draw = info_draw_mode->mode_draw;
            mesh->impl->info_mode.mode_cull_face = info_draw_mode->mode_cull_face;
            mesh->impl->info_mode.mode_front_face_direction = info_draw_mode->mode_front_face_direction;
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
        impl->fileName = meshMemory->getFilenameMesh();
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
                strncpy(impl->headerMain.typeApp, get_type_app_from_mesh_type(meshMemory->getTypeMesh()), sizeof(impl->headerMain.typeApp) - 1);
                break;
            default:
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Mesh invalid type");
                break;
        }
        strncpy(impl->headerMain.name, MBM_HEADER_NAME_MBM, sizeof(impl->headerMain.name) - 1);
        impl->headerMain.version = CURRENT_VERSION_MBM_HEADER;
        impl->headerMain.magic = 0x010203ff;
        impl->typeMe = meshMemory->getTypeMesh();
        // step 2: --------------------------------------------------------------------------------------------------
        const INFO_PHYSICS &meshPhysics = meshMemory->getPhysicsInfo();
        for (auto pCube : meshPhysics.lsCube)
        {
            auto cube = new CUBE(pCube->halfDim, pCube->absCenter);
            this->impl->infoPhysics.lsCube.push_back(cube);
        }
        for (auto pBase : meshPhysics.lsSphere)
        {
            auto base = new SPHERE();
            base->absCenter[0] = pBase->absCenter[0];
            base->absCenter[1] = pBase->absCenter[1];
            base->absCenter[2] = pBase->absCenter[2];
            base->ray = pBase->ray;
            this->impl->infoPhysics.lsSphere.push_back(base);
        }
        for (auto pComplex : meshPhysics.lsCubeComplex)
        {
            auto complex = new CUBE_COMPLEX();
            for (int k = 0; k < 8; k++)
                complex->p[k] = pComplex->p[k];
            this->impl->infoPhysics.lsCubeComplex.push_back(complex);
        }
        for (auto pTriangle : meshPhysics.lsTriangle)
        {
            auto triangle = new TRIANGLE();
            triangle->point[0] = pTriangle->point[0];
            triangle->point[1] = pTriangle->point[1];
            triangle->point[2] = pTriangle->point[2];
            this->impl->infoPhysics.lsTriangle.push_back(triangle);
        }
        if (meshMemory->getInfoFont() != nullptr)
        {
            const INFO_BOUND_FONT* pMemoryInfoFont = meshMemory->getInfoFont();
            impl->headerMain.backBufferHeight = pMemoryInfoFont->heightLetter;
            this->impl->extraInfo = new INFO_BOUND_FONT();
            auto* infoFont = static_cast<INFO_BOUND_FONT*>(this->impl->extraInfo);
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
            this->impl->extraInfo = lsParticleInfo;
            for (auto thatStage : *thatParticleInfo)
            {
                auto* stage = new util::STAGE_PARTICLE(thatStage);
                lsParticleInfo->push_back(stage);
            }
        }
        if (meshMemory->getInfoTile() != nullptr)
        {
            const util::BTILE_INFO* thatInfoTile = meshMemory->getInfoTile();
            this->impl->extraInfo = thatInfoTile->clone();
        }
        impl->headerMesh.totalAnimation = static_cast<int32_t>(meshMemory->getAnimationInfo().lsHeaderAnim.size());
        for (int i = 0; i < impl->headerMesh.totalAnimation; ++i)
        {
            const util::INFO_ANIMATION::INFO_HEADER_ANIM* pInfoAnim = meshMemory->getAnimationInfo().lsHeaderAnim[i];
            auto  infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            infoHead->headerAnim = new util::HEADER_ANIMATION();
            this->impl->infoAnimation.lsHeaderAnim.push_back(infoHead);
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
                infoStepShader->setTextureAnimationEffectFileName(pInfoStepShader->getTextureAnimationEffectFileName());
                const char *textureAnimationEffect = pInfoStepShader->getTextureAnimationEffectFileName();

                if (pInfoStepShader->dataPS)
                {
                    infoStepShader->dataPS = new util::INFO_SHADER_DATA(
                        pInfoStepShader->dataPS->lenVars * 4,
                        static_cast<int>(strlen(pInfoStepShader->dataPS->fileNameShader) + 1),
                        textureAnimationEffect ? static_cast<int>(strlen(textureAnimationEffect) + 1) : 0);
                    strcpy(infoStepShader->dataPS->fileNameShader, pInfoStepShader->dataPS->fileNameShader);
                    if (infoStepShader->dataPS->fileNameTextureStage2 && textureAnimationEffect)
                        strcpy(infoStepShader->dataPS->fileNameTextureStage2, textureAnimationEffect);
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
                        textureAnimationEffect ? static_cast<int>(strlen(textureAnimationEffect) + 1) : 0);
                    strcpy(infoStepShader->dataVS->fileNameShader, pInfoStepShader->dataVS->fileNameShader);
                    if (infoStepShader->dataVS->fileNameTextureStage2 && textureAnimationEffect)
                        strcpy(infoStepShader->dataVS->fileNameTextureStage2, textureAnimationEffect);
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

        impl->headerMesh.totalFrames = meshMemory->getTotalFrame();
        {
            const BUFFER_MESH* pBufferMesh0 = meshMemory->getBuffer(0);
            const BUFFER_GL* pGl0 = pBufferMesh0 ? pBufferMesh0->pBufferGL : nullptr;
            const bool hasNormals = pGl0 && (pGl0->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR || pGl0->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
            impl->headerMesh.hasNorText[0] = hasNormals ? HAS_NOR_IN_FILE : HAS_NOR_NO;
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
        for (int currentFrame = 0; currentFrame < impl->headerMesh.totalFrames; ++currentFrame)
        {
            auto pBuffer = new util::BUFFER_MESH_DEBUG();
            this->impl->buffer.push_back(pBuffer);
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
        impl->positionOffset = VEC3(impl->headerMesh.posX, impl->headerMesh.posY, impl->headerMesh.posZ);
        impl->angleDefault = VEC3(impl->headerMesh.angleX, impl->headerMesh.angleY, impl->headerMesh.angleZ);
        this->impl->sizeCoordTexFrame_0 = 0;
        if (this->impl->coordTexFrame_0)
            delete[] this->impl->coordTexFrame_0;
        this->impl->coordTexFrame_0 = nullptr;
        return true;
    }
}

mbm::MESH_MANAGER *    mbm::MESH_MANAGER::instanceMeshManager        = nullptr;
