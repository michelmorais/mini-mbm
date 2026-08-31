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
#include "private/skeletal-parity-asset.h"
#include "private/mesh-simplifier.h"
#include <skeletal-gpu-upload.h>
#include <draw-compatibility.h>
#include <shader-var-cfg.h>
#include <texture-manager.h>
#include <renderizable.h>
#include <shader.h>
#include <device.h>
#include <util-interface.h>
#include <shapes.h>
#include <miniz-wrap/miniz-wrap.h>
#include <header-mesh.h>
#include "mesh-v8-io.h"
#include "mesh-v11-io.h"
#include "mesh-io-primitives.h"

#include <cfloat>
#include <cmath>
#include <string>
#include <algorithm> // std::sort
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <limits>
#include <set>
#include <memory>


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

namespace mbm
{
    // The pure-CPU result of parsing a v11 file, safe to build on a worker thread and hand to the
    // main thread - no BUFFER_GL/TEXTURE_MANAGER/addPath/global-state calls anywhere in here (those
    // must stay main-thread-only). MESH_MBM::finishLoadFromIntermediate (main thread) consumes this.
    // Declared with external linkage (not in the anonymous namespace below) so it can be
    // forward-declared in mesh-manager.h for the MESH_MBM::finishLoadFromIntermediate declaration -
    // same PIMPL-style "forward declare in the header, define in the .cpp" pattern already used for
    // MESH_MBM::Impl/MESH_MANAGER::Impl.
    struct IntermediateExtraSlotV11
    {
        uint16_t    legacyType = 0; // already mapped via textureRoleToLegacyMaterialSlotType
        std::string path;
    };

    struct IntermediateSubsetV11
    {
        int32_t     vertexStart = 0;
        int32_t     vertexCount = 0;
        int32_t     indexStart  = 0;
        int32_t     indexCount  = 0;
        std::string primaryTexturePath;
        bool        hasAlphaColor = false;
        std::vector<IntermediateExtraSlotV11> extraSlots;
    };

    struct IntermediateFrameV11
    {
        uint32_t                    vertexCount = 0;
        bool                        hasNormal   = false;
        std::unique_ptr<VEC3[]>     position;
        std::unique_ptr<VEC3[]>     normal; // nullptr when !hasNormal
        std::unique_ptr<VEC2[]>     uv;
        uint32_t                    indexCount = 0;
        std::unique_ptr<uint16_t[]> index; // nullptr when indexCount == 0
        std::vector<IntermediateSubsetV11> subsets;
    };

    struct MESH_LOAD_INTERMEDIATE_V11
    {
        util::TYPE_MESH          typeMe = util::TYPE_MESH_UNKNOWN;
        util::MATERIAL           material;
        // Deprecated -- see MESH_MBM::Impl::positionOffset_deprecated (mesh-manager-impl.h) for
        // the rationale. Kept only so the shared parse loop can populate SECTION_MATERIAL_TRANSFORM's
        // full payload for round-trip fidelity; never applied to a loaded renderizable.
        VEC3                     positionOffset_deprecated;
        VEC3                     angleDefault_deprecated;
        util::INFO_DRAW_MODE     info_mode;
        mbm::INFO_PHYSICS        infoPhysics;
        util::INFO_ANIMATION     infoAnimation;
        std::vector<std::string> extraPaths;
        std::vector<IntermediateFrameV11> frames;
        std::vector<util::ARTICULATED_PART_V11> articulatedParts;
        std::vector<ARTICULATED_CLIP_DATA> articulatedClips;
        skeletal::CANONICAL_SKELETON canonicalSkeleton;
        skeletal::CANONICAL_WEIGHTS canonicalWeights;
        skeletal::CANONICAL_ANIMATIONS canonicalAnimations;
        // FONT (INFO_BOUND_FONT*) or PARTICLE (std::vector<util::STAGE_PARTICLE*>*) detail data,
        // tagged by `typeMe` - same opaque-by-type shape as MESH_MBM_DEBUG/MESH_MBM's impl->extraInfo.
        // A trivial void* (no destructor pitfall like infoPhysics/infoAnimation above), but still
        // needs explicit cleanup here if a load is abandoned before finishLoadFromIntermediate moves
        // it out, otherwise it leaks.
        void *extraInfo = nullptr;

        MESH_LOAD_INTERMEDIATE_V11() = default;
        ~MESH_LOAD_INTERMEDIATE_V11()
        {
            if (!extraInfo)
                return;
            if (typeMe == util::TYPE_MESH_FONT)
                delete static_cast<INFO_BOUND_FONT*>(extraInfo);
            else if (typeMe == util::TYPE_MESH_PARTICLE)
            {
                auto *lsStage = static_cast<std::vector<util::STAGE_PARTICLE*>*>(extraInfo);
                for (auto *stage : *lsStage)
                    delete stage;
                delete lsStage;
            }
            else if (typeMe == util::TYPE_MESH_TILE_MAP)
                delete static_cast<util::BTILE_INFO*>(extraInfo);
        }
        // mbm::INFO_PHYSICS/util::INFO_ANIMATION both have user-declared destructors, so neither has
        // an implicit move ctor/assignment (only the implicit copy ops, which would shallow-copy
        // their owning raw-pointer vectors and double-free on destruction) - move their vectors
        // individually instead of letting the compiler fall back to copying them, which would also
        // force every container holding this struct (e.g. the worker-pool completion queue) to copy
        // rather than move the whole thing, and that fails outright since IntermediateFrameV11 holds
        // unique_ptr's and genuinely can't be copied.
        MESH_LOAD_INTERMEDIATE_V11(MESH_LOAD_INTERMEDIATE_V11 &&other) noexcept
            : typeMe(other.typeMe), material(other.material),
              positionOffset_deprecated(other.positionOffset_deprecated),
              angleDefault_deprecated(other.angleDefault_deprecated), info_mode(other.info_mode),
              extraPaths(std::move(other.extraPaths)), frames(std::move(other.frames)),
              articulatedParts(std::move(other.articulatedParts)),
              articulatedClips(std::move(other.articulatedClips)),
              canonicalSkeleton(std::move(other.canonicalSkeleton)),
              canonicalWeights(std::move(other.canonicalWeights)),
              canonicalAnimations(std::move(other.canonicalAnimations))
        {
            infoPhysics.lsCube        = std::move(other.infoPhysics.lsCube);
            infoPhysics.lsCubeComplex = std::move(other.infoPhysics.lsCubeComplex);
            infoPhysics.lsSphere      = std::move(other.infoPhysics.lsSphere);
            infoPhysics.lsTriangle    = std::move(other.infoPhysics.lsTriangle);
            infoAnimation.lsHeaderAnim = std::move(other.infoAnimation.lsHeaderAnim);
            extraInfo       = other.extraInfo;
            other.extraInfo = nullptr;
        }
        MESH_LOAD_INTERMEDIATE_V11 &operator=(MESH_LOAD_INTERMEDIATE_V11 &&other) noexcept
        {
            if (this == &other)
                return *this;
            typeMe         = other.typeMe;
            material       = other.material;
            positionOffset_deprecated = other.positionOffset_deprecated;
            angleDefault_deprecated   = other.angleDefault_deprecated;
            info_mode      = other.info_mode;
            extraPaths     = std::move(other.extraPaths);
            frames         = std::move(other.frames);
            articulatedParts = std::move(other.articulatedParts);
            articulatedClips = std::move(other.articulatedClips);
            canonicalSkeleton = std::move(other.canonicalSkeleton);
            canonicalWeights = std::move(other.canonicalWeights);
            canonicalAnimations = std::move(other.canonicalAnimations);
            infoPhysics.lsCube        = std::move(other.infoPhysics.lsCube);
            infoPhysics.lsCubeComplex = std::move(other.infoPhysics.lsCubeComplex);
            infoPhysics.lsSphere      = std::move(other.infoPhysics.lsSphere);
            infoPhysics.lsTriangle    = std::move(other.infoPhysics.lsTriangle);
            infoAnimation.lsHeaderAnim = std::move(other.infoAnimation.lsHeaderAnim);
            extraInfo       = other.extraInfo;
            other.extraInfo = nullptr;
            return *this;
        }
        MESH_LOAD_INTERMEDIATE_V11(const MESH_LOAD_INTERMEDIATE_V11 &)            = delete;
        MESH_LOAD_INTERMEDIATE_V11 &operator=(const MESH_LOAD_INTERMEDIATE_V11 &) = delete;
    };
}

namespace
{

    template <typename TriangleReader>
    bool read_detail_mesh_section(util::MEM_CURSOR_V11 &fp,
                                  const char *fileNamePath,
                                  mbm::INFO_PHYSICS &infoPhysics,
                                  TriangleReader readTriangleDetail)
    {
        util::DETAIL_MESH detailInfo;
        if (!util::readDetailMeshV8(fp, detailInfo))
            return log_util::onFailed(nullptr,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
        if (detailInfo.type != 'P')
            return log_util::onFailed(nullptr,__FILE__, __LINE__, "expected 'P' from Physics at DETAIL_MESH [%s]", fileNamePath);
        for (int i = 0; i < detailInfo.totalBounding; )
        {
            util::DETAIL_MESH detail;
            if (!util::readDetailMeshV8(fp, detail))
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "failed to read DETAIL_MESH [%s]", fileNamePath);
            switch (detail.type)
            {
                case MBM_DETAIL_TYPE_CUBE:
                {
                    for(int j=0; j< detail.totalBounding; j++)
                    {
                        auto cube = new mbm::CUBE();
                        infoPhysics.lsCube.push_back(cube);
                        if (!util::readCubeV8(fp, *cube))
                            return log_util::onFailed(nullptr,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
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
                            return log_util::onFailed(nullptr,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
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
                            return log_util::onFailed(nullptr,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                    }
                    i += detail.totalBounding;
                }
                break;
                case MBM_DETAIL_TYPE_TRIANGLE:
                {
                    if (!readTriangleDetail(fp, fileNamePath, detail.totalBounding))
                        return false;
                    i += detail.totalBounding;
                }
                break;
                // MBM_DETAIL_TYPE_FONT/PARTICLE/TILE never appear here: SECTION_DETAIL_PHYSICS is
                // written for every mesh type (including FONT/PARTICLE/TILE_MAP), but its own
                // DETAIL_MESH.type sub-dispatch only ever carries physics bounding volumes.
                // FONT/PARTICLE/TILE_MAP detail data lives in its own top-level
                // SECTION_DETAIL_FONT/PARTICLE/TILE section instead (docs/mesh-v11-format.md §6c/§6d).
                default:
                {
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "unknown type bounding box [%d] [%s]", detail.type, fileNamePath);
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
    // shader/texture role) and were never given matching values.
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

    // Wraps an in-memory v11 section payload (as returned by util::readSectionV11) in a
    // MEM_CURSOR_V11, so loadV11 can reuse the project's existing payload-level readers
    // (util::readXxxV11, and - for SECTION_DETAIL_PHYSICS - the real read_detail_mesh_section
    // template) without duplicating any parsing logic, and without copying the payload through a
    // real OS temp file first (superseded an earlier tmpfile()-per-section design, which had a real
    // per-section syscall cost and a Windows risk, since MSVC's tmpfile() defaults to a root
    // directory non-admin users can't write to).
    util::MEM_CURSOR_V11 stage_payload_as_cursor(const std::vector<uint8_t> &payload)
    {
        return util::MEM_CURSOR_V11{payload.data(), payload.size(), 0};
    }

    // Parses one SECTION_ANIMATION payload (already staged as `tmp`, a cursor positioned at its
    // start) into a freshly allocated INFO_HEADER_ANIM, shared by MESH_MBM_DEBUG::loadV11 and
    // parse_v11_intermediate (worker-thread-safe - no impl-> access) so the two don't duplicate this
    // construction logic. Returns nullptr on any read failure; callers format their own error
    // message/context.
    util::INFO_ANIMATION::INFO_HEADER_ANIM *parse_animation_section_v11(util::MEM_CURSOR_V11 &tmp)
    {
        util::ANIMATION_HEADER_V11 v11Anim;
        bool ok = util::readAnimationHeaderV11(tmp, v11Anim);

        util::FX_HEADER_V11 v11Fx;
        std::vector<util::SHADER_VAR_V11> psVars, vsVars;
        if (ok && v11Anim.hasFx)
        {
            ok = util::readFxHeaderV11(tmp, v11Fx);
            if (ok && v11Fx.hasPS)
            {
                psVars.resize(v11Fx.ps.varCount);
                for (uint16_t v = 0; ok && v < v11Fx.ps.varCount; ++v)
                    ok = util::readShaderVarV11(tmp, psVars[v]);
            }
            if (ok && v11Fx.hasVS)
            {
                vsVars.resize(v11Fx.vs.varCount);
                for (uint16_t v = 0; ok && v < v11Fx.vs.varCount; ++v)
                    ok = util::readShaderVarV11(tmp, vsVars[v]);
            }
        }
        if (!ok)
            return nullptr;

        auto *infoHead   = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
        auto *headerAnim = new util::HEADER_ANIMATION();
        strncpy(headerAnim->nameAnimation, v11Anim.name.c_str(), sizeof(headerAnim->nameAnimation) - 1);
        headerAnim->nameAnimation[sizeof(headerAnim->nameAnimation) - 1] = '\0';
        headerAnim->initialFrame     = v11Anim.initialFrame;
        headerAnim->finalFrame       = v11Anim.finalFrame;
        headerAnim->timeBetweenFrame = v11Anim.timeBetweenFrame;
        headerAnim->typeAnimation    = v11Anim.typeAnimation;
        headerAnim->hasShaderEffect  = v11Anim.hasFx;
        headerAnim->blendState       = v11Anim.blendState;
        infoHead->headerAnim = headerAnim;

        if (v11Anim.hasFx)
        {
            auto *fxOut = new util::INFO_FX();
            fxOut->blendOperation = v11Fx.blendOperation;
            if (v11Fx.hasFxTexture)
                fxOut->setTextureAnimationEffectFileName(v11Fx.fxTexture.path.c_str());

            const auto buildStep = [](const util::SHADER_STEP_V11 &step,
                                      const std::vector<util::SHADER_VAR_V11> &vars) -> util::INFO_SHADER_DATA *
            {
                auto *data = new util::INFO_SHADER_DATA(static_cast<int>(vars.size()) * 4,
                                                        static_cast<int>(step.name.size()) + 1, 0);
                std::memcpy(data->fileNameShader, step.name.c_str(), step.name.size() + 1);
                data->timeAnimation = step.timeAnimation;
                data->typeAnimation = static_cast<int16_t>(step.typeAnimation);
                for (size_t v = 0; v < vars.size(); ++v)
                {
                    data->typeVars[v] = static_cast<char>(vars[v].typeVar);
                    for (int c = 0; c < 4; ++c)
                    {
                        data->min[v * 4 + c] = vars[v].min[c];
                        data->max[v * 4 + c] = vars[v].max[c];
                    }
                }
                return data;
            };
            if (v11Fx.hasPS)
                fxOut->dataPS = buildStep(v11Fx.ps, psVars);
            if (v11Fx.hasVS)
                fxOut->dataVS = buildStep(v11Fx.vs, vsVars);
            infoHead->effectShader = fxOut;
        }
        return infoHead;
    }

    bool parse_articulated_parts_section_v11(util::MEM_CURSOR_V11 &tmp,
                                             std::vector<util::ARTICULATED_PART_V11> &out)
    {
        util::ARTICULATED_PARTS_HEADER_V11 header;
        if (!util::readArticulatedPartsHeaderV11(tmp, header))
            return false;
        out.clear();
        out.reserve(header.partCount);
        for (uint32_t i = 0; i < header.partCount; ++i)
        {
            util::ARTICULATED_PART_V11 part;
            if (!util::readArticulatedPartV11(tmp, part))
                return false;
            out.push_back(std::move(part));
        }

        std::unordered_map<uint64_t, const util::ARTICULATED_PART_V11 *> partsById;
        std::unordered_set<uint64_t> occurrences;
        partsById.reserve(out.size());
        occurrences.reserve(out.size());
        for (const auto &part : out)
        {
            const uint64_t occurrence = (static_cast<uint64_t>(part.frameIndex) << 32) | part.subsetIndex;
            if (part.partId == 0 || !partsById.emplace(part.partId, &part).second ||
                !occurrences.insert(occurrence).second)
                return false;
        }
        for (const auto &part : out)
        {
            uint64_t parentId = part.parentPartId;
            for (size_t depth = 0; parentId != 0 && depth <= out.size(); ++depth)
            {
                const auto found = partsById.find(parentId);
                if (found == partsById.end() || found->second->frameIndex != part.frameIndex ||
                    parentId == part.partId)
                    return false;
                parentId = found->second->parentPartId;
                if (depth == out.size() && parentId != 0)
                    return false;
            }
        }
        return true;
    }

    bool parse_articulated_animation_section_v11(util::MEM_CURSOR_V11 &tmp,
                                                 std::vector<mbm::ARTICULATED_CLIP_DATA> &out)
    {
        util::ARTICULATED_ANIMATION_HEADER_V11 header;
        if (!util::readArticulatedAnimationHeaderV11(tmp, header))
            return false;
        out.clear();
        out.reserve(header.clipCount);
        for (uint32_t c = 0; c < header.clipCount; ++c)
        {
            mbm::ARTICULATED_CLIP_DATA clip;
            if (!util::readArticulatedClipV11(tmp, clip.header))
                return false;
            uint32_t trackCount = 0;
            if (!util::le_io::readU32LE(tmp, trackCount))
                return false;
            clip.tracks.reserve(trackCount);
            for (uint32_t t = 0; t < trackCount; ++t)
            {
                mbm::ARTICULATED_TRACK_DATA track;
                if (!util::readArticulatedTrackV11(tmp, track.header))
                    return false;
                track.keys.reserve(track.header.keyCount);
                for (uint32_t k = 0; k < track.header.keyCount; ++k)
                {
                    util::ARTICULATED_KEY_V11 key;
                    if (!util::readArticulatedKeyV11(tmp, key))
                        return false;
                    track.keys.push_back(key);
                }
                clip.tracks.push_back(std::move(track));
            }
            out.push_back(std::move(clip));
        }
        return true;
    }

    // Parses one SECTION_DETAIL_PARTICLE payload (already staged as `tmp`) into a freshly allocated
    // vector of STAGE_PARTICLE*, shared by MESH_MBM_DEBUG::loadV11 and parse_v11_intermediate.
    // Returns nullptr on any read failure.
    std::vector<util::STAGE_PARTICLE*> *parse_particle_detail_section_v11(util::MEM_CURSOR_V11 &tmp)
    {
        uint16_t stageCount = 0;
        bool ok = util::le_io::readU16LE(tmp, stageCount);
        auto *lsStage = new std::vector<util::STAGE_PARTICLE*>();
        lsStage->reserve(stageCount);
        for (uint16_t i = 0; ok && i < stageCount; ++i)
        {
            auto *stage = new util::STAGE_PARTICLE();
            lsStage->push_back(stage);
            ok = util::readStageParticleV11(tmp, *stage);
        }
        if (!ok)
        {
            for (auto *stage : *lsStage)
                delete stage;
            delete lsStage;
            return nullptr;
        }
        return lsStage;
    }

    // Parses one SECTION_DETAIL_FONT payload (already staged as `tmp`) into a freshly allocated
    // INFO_BOUND_FONT, shared by MESH_MBM_DEBUG::loadV11 and parse_v11_intermediate. Returns
    // nullptr on any read failure.
    mbm::INFO_BOUND_FONT *parse_font_detail_section_v11(util::MEM_CURSOR_V11 &tmp)
    {
        util::FONT_DETAIL_HEADER_V11 v11Font;
        bool ok = util::readFontDetailHeaderV11(tmp, v11Font);

        auto *font = new mbm::INFO_BOUND_FONT();
        if (ok)
        {
            font->fontName        = v11Font.name;
            font->spaceXCharacter = v11Font.spaceXCharacter;
            font->spaceYCharacter = v11Font.spaceYCharacter;
            font->heightLetter    = v11Font.heightLetter;
        }
        for (uint16_t i = 0; ok && i < v11Font.letterCount; ++i)
        {
            util::DETAIL_LETTER letterEntry;
            ok = util::readDetailLetterV11(tmp, letterEntry);
            if (ok && letterEntry.letter < 255)
                font->letter[letterEntry.letter].detail = new util::DETAIL_LETTER(letterEntry);
        }
        if (!ok)
        {
            delete font;
            return nullptr;
        }
        return font;
    }

    // Parses one SECTION_DETAIL_TILE payload (already staged as `tmp`) into a freshly allocated
    // util::BTILE_INFO, shared by MESH_MBM_DEBUG::loadV11 and parse_v11_intermediate. Returns
    // nullptr on any read failure - ~BTILE_INFO() already deep-cleans whatever was partially built
    // (layers/lsIndexTiles/infoBrickEditor/lsObj/lsProperty), so a single `delete` on the
    // in-progress object is enough at every failure point.
    util::BTILE_INFO *parse_tile_detail_section_v11(util::MEM_CURSOR_V11 &tmp)
    {
        util::TILE_HEADER_MAP_V11 v11Header;
        bool ok = util::readTileHeaderMapV11(tmp, v11Header);
        if (!ok)
            return nullptr;

        auto *tileInfo = new util::BTILE_INFO();
        tileInfo->map.count_width_tile  = v11Header.count_width_tile;
        tileInfo->map.count_height_tile = v11Header.count_height_tile;
        tileInfo->map.size_width_tile   = v11Header.size_width_tile;
        tileInfo->map.size_height_tile  = v11Header.size_height_tile;
        tileInfo->map.layerCount        = v11Header.layerCount;
        tileInfo->map.countRawTiles     = v11Header.countRawTiles;
        tileInfo->map.objectCount       = v11Header.objectCount;
        tileInfo->map.propertyCount     = v11Header.propertyCount;
        tileInfo->map.typeMap           = static_cast<util::BTILE_TYPE_MAP>(v11Header.typeMap);
        tileInfo->map.background        = v11Header.background;
        strncpy(tileInfo->map.background_texture, v11Header.backgroundTexture.c_str(),
                sizeof(tileInfo->map.background_texture) - 1);
        tileInfo->map.background_texture[sizeof(tileInfo->map.background_texture) - 1] = '\0';
        tileInfo->map.renderDirection[0] = static_cast<char>(v11Header.renderDirectionLeftToRight);
        tileInfo->map.renderDirection[1] = static_cast<char>(v11Header.renderDirectionTopToDown);

        tileInfo->infoBrickEditor = new util::BTILE_BRICK_INFO[v11Header.countRawTiles];
        for (uint32_t i = 0; ok && i < v11Header.countRawTiles; ++i)
            ok = util::readBtileBrickInfoV11(tmp, tileInfo->infoBrickEditor[i]);

        if (ok)
            tileInfo->layers = new util::BTILE_LAYER[v11Header.layerCount];
        const uint32_t cellsPerLayer = v11Header.count_width_tile * v11Header.count_height_tile;
        for (uint32_t l = 0; ok && l < v11Header.layerCount; ++l)
        {
            util::TILE_LAYER_HEADER_V11 v11Layer;
            ok = util::readTileLayerHeaderV11(tmp, v11Layer);
            if (!ok)
                break;
            tileInfo->layers[l].offset[0] = v11Layer.offsetX;
            tileInfo->layers[l].offset[1] = v11Layer.offsetY;
            tileInfo->layers[l].offset[2] = v11Layer.offsetZ;
            tileInfo->layers[l].lsIndexTiles = new util::BTILE_INDEX_TILE[cellsPerLayer];
            for (uint32_t c = 0; ok && c < cellsPerLayer; ++c)
                ok = util::readBtileIndexTileV11(tmp, tileInfo->layers[l].lsIndexTiles[c]);
        }

        for (uint32_t o = 0; ok && o < v11Header.objectCount; ++o)
        {
            util::TILE_OBJ_HEADER_V11 v11Obj;
            ok = util::readTileObjHeaderV11(tmp, v11Obj);
            if (!ok)
                break;
            auto *obj = new util::BTILE_OBJ(static_cast<util::BTILE_OBJ_TYPE>(v11Obj.type), v11Obj.name);
            tileInfo->lsObj.push_back(obj);
            for (uint16_t p = 0; ok && p < v11Obj.pointCount; ++p)
            {
                float x = 0, y = 0;
                ok = util::le_io::readF32LE(tmp, x) && util::le_io::readF32LE(tmp, y);
                if (ok)
                    obj->lsPoints.push_back(new mbm::VEC2(x, y));
            }
        }

        for (uint32_t p = 0; ok && p < v11Header.propertyCount; ++p)
        {
            util::TILE_PROPERTY_V11 v11Prop;
            ok = util::readTilePropertyV11(tmp, v11Prop);
            if (!ok)
                break;
            auto *prop = new util::BTILE_PROPERTY(static_cast<util::BTILE_PROPERTY_TYPE>(v11Prop.type));
            prop->owner = v11Prop.owner;
            prop->name  = v11Prop.name;
            prop->value = v11Prop.value;
            tileInfo->lsProperty.push_back(prop);
        }

        if (!ok)
        {
            delete tileInfo;
            return nullptr;
        }
        return tileInfo;
    }

    // Worker-thread-safe equivalent of MESH_MBM_DEBUG::readDebugTriangleDetailCompat - same logic,
    // takes INFO_PHYSICS directly instead of going through `this->impl` (parse_v11_intermediate has
    // no MESH_MBM_DEBUG instance).
    bool read_triangle_detail_v11(util::MEM_CURSOR_V11 &fp, const char *fileNamePath, const int totalBounding,
                                  mbm::INFO_PHYSICS &infoPhysics)
    {
        for (int j = 0; j < totalBounding; j++)
        {
            auto triangle = new mbm::TRIANGLE();
            infoPhysics.lsTriangle.push_back(triangle);
            if (!util::readTriangleV8(fp, *triangle))
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
        }
        return true;
    }

    // Worker-thread-safe equivalent of MESH_MBM::readFrameStaticV11Payload's CPU-side parsing (the
    // per-subset TEXTURE_MANAGER::load + BUFFER_GL upload moves to finishLoadFromIntermediate,
    // main-thread only). frame0Uv/frame0UvCount let later SHARED_WITH_FRAME_0 frames copy frame 0's
    // already-parsed uv array, mirroring readFrameStaticV11Payload's impl->coordTexFrame_0 cache.
    bool parse_v11_frame_intermediate(util::MEM_CURSOR_V11 &fp, const char *fileNamePath, mbm::IntermediateFrameV11 &outFrame,
                                      const mbm::VEC2 *frame0Uv, const int frame0UvCount)
    {
        util::FRAME_HEADER_V11 frameHeader;
        if (!util::readFrameHeaderV11(fp, frameHeader))
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read FRAME_HEADER_V11 [%s]", fileNamePath);
        if (frameHeader.indexWidth != 16)
            return log_util::onFailed(nullptr, __FILE__, __LINE__,
                                      "loadV11 only supports 16-bit indices [%s]", fileNamePath);

        outFrame.vertexCount = frameHeader.vertexCount;
        outFrame.hasNormal   = frameHeader.hasNormal != 0;
        outFrame.position    = std::make_unique<mbm::VEC3[]>(frameHeader.vertexCount);
        if (outFrame.hasNormal)
            outFrame.normal = std::make_unique<mbm::VEC3[]>(frameHeader.vertexCount);
        outFrame.uv = std::make_unique<mbm::VEC2[]>(frameHeader.vertexCount);
        memset(static_cast<void *>(outFrame.uv.get()), 0, sizeof(mbm::VEC2) * static_cast<size_t>(frameHeader.vertexCount));

        for (uint32_t i = 0; i < frameHeader.vertexCount; ++i)
        {
            if (!util::le_io::readF32LE(fp, outFrame.position[i].x) || !util::le_io::readF32LE(fp, outFrame.position[i].y) ||
                !util::le_io::readF32LE(fp, outFrame.position[i].z))
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read frame position [%s]", fileNamePath);
        }

        if (outFrame.hasNormal)
        {
            for (uint32_t i = 0; i < frameHeader.vertexCount; ++i)
            {
                if (!util::le_io::readF32LE(fp, outFrame.normal[i].x) || !util::le_io::readF32LE(fp, outFrame.normal[i].y) ||
                    !util::le_io::readF32LE(fp, outFrame.normal[i].z))
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read frame normal [%s]", fileNamePath);
            }
        }

        if (frameHeader.hasUv)
        {
            if (frameHeader.uvSource == 0) // OWN
            {
                for (uint32_t i = 0; i < frameHeader.vertexCount; ++i)
                {
                    if (!util::le_io::readF32LE(fp, outFrame.uv[i].x) || !util::le_io::readF32LE(fp, outFrame.uv[i].y))
                        return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read frame uv [%s]", fileNamePath);
                }
            }
            else if (frame0Uv) // SHARED_WITH_FRAME_0
            {
                const uint32_t safeCopy = std::min(frameHeader.vertexCount, static_cast<uint32_t>(frame0UvCount));
                memcpy(static_cast<void *>(outFrame.uv.get()), frame0Uv, sizeof(mbm::VEC2) * static_cast<size_t>(safeCopy));
            }
        }

        outFrame.indexCount = frameHeader.indexCount;
        if (frameHeader.indexCount > 0)
        {
            outFrame.index = std::make_unique<uint16_t[]>(frameHeader.indexCount);
            for (uint32_t i = 0; i < frameHeader.indexCount; ++i)
            {
                if (!util::le_io::readU16LE(fp, outFrame.index[i]))
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read frame index [%s]", fileNamePath);
            }
        }

        outFrame.subsets.resize(frameHeader.totalSubset);
        for (uint32_t s = 0; s < frameHeader.totalSubset; ++s)
        {
            util::SUBSET_DESC_V11 subsetDesc;
            if (!util::readSubsetDescV11(fp, subsetDesc))
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read SUBSET_DESC_V11 [%s]", fileNamePath);

            mbm::IntermediateSubsetV11 &subset = outFrame.subsets[s];
            subset.vertexStart        = subsetDesc.vertexStart;
            subset.vertexCount        = subsetDesc.vertexCount;
            subset.indexStart         = subsetDesc.indexStart;
            subset.indexCount         = subsetDesc.indexCount;
            subset.primaryTexturePath = subsetDesc.primaryTexture.path;
            subset.hasAlphaColor      = subsetDesc.alphaColor[0] != 0;

            for (uint16_t e = 0; e < subsetDesc.extraSlotCount; ++e)
            {
                util::SUBSET_EXTRA_SLOT_V11 extraSlot;
                if (!util::readSubsetExtraSlotV11(fp, extraSlot))
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read SUBSET_EXTRA_SLOT_V11 [%s]", fileNamePath);
                uint16_t legacyType = 0;
                if (textureRoleToLegacyMaterialSlotType(static_cast<mbm::TEXTURE_ROLE>(extraSlot.role), legacyType))
                {
                    mbm::IntermediateExtraSlotV11 slot;
                    slot.legacyType = legacyType;
                    slot.path       = extraSlot.texture.path;
                    subset.extraSlots.push_back(std::move(slot));
                }
                else
                {
                    PRINT_IF_DEBUG("Warning! loadV11 skipped extra texture slot with unrecognized role [%d]\n", extraSlot.role);
                }
            }
        }
        return true;
    }

    bool parse_canonical_skeleton_section_v11(util::MEM_CURSOR_V11 &fp, const uint16_t sectionVersion,
                                               mbm::skeletal::CANONICAL_SKELETON &out)
    {
        if (sectionVersion != 1 && sectionVersion != 2 && sectionVersion != 3)
            return false;
        out = {};
        uint32_t boneCount = 0;
        if (!util::le_io::readU64LE(fp, out.skeletonId) || out.skeletonId == 0 ||
            !util::le_io::readU32LE(fp, boneCount) || boneCount == 0)
            return false;
        out.sourceBones.reserve(boneCount);
        for (uint32_t index = 0; index < boneCount; ++index)
        {
            mbm::skeletal::CANONICAL_BONE bone;
            if (!util::le_io::readU64LE(fp, bone.boneId) ||
                !util::le_io::readU64LE(fp, bone.parentBoneId) ||
                !util::readStringV11(fp, bone.name) ||
                !util::le_io::readF32LE(fp, bone.localBind.translation.x) ||
                !util::le_io::readF32LE(fp, bone.localBind.translation.y) ||
                !util::le_io::readF32LE(fp, bone.localBind.translation.z) ||
                !util::le_io::readF32LE(fp, bone.localBind.rotation.x) ||
                !util::le_io::readF32LE(fp, bone.localBind.rotation.y) ||
                !util::le_io::readF32LE(fp, bone.localBind.rotation.z) ||
                !util::le_io::readF32LE(fp, bone.localBind.rotation.w) ||
                !util::le_io::readF32LE(fp, bone.localBind.scale.x) ||
                !util::le_io::readF32LE(fp, bone.localBind.scale.y) ||
                !util::le_io::readF32LE(fp, bone.localBind.scale.z) ||
                !util::le_io::readF32LE(fp, bone.radius) ||
                !util::le_io::readF32LE(fp, bone.length))
                return false;
            if (sectionVersion >= 2)
            {
                uint8_t explicitTail = 0;
                if (!util::le_io::readF32LE(fp, bone.tailOffset.x) ||
                    !util::le_io::readF32LE(fp, bone.tailOffset.y) ||
                    !util::le_io::readF32LE(fp, bone.tailOffset.z) ||
                    !util::le_io::readBytes(fp, &explicitTail, sizeof(explicitTail)) || explicitTail > 1)
                    return false;
                bone.hasExplicitTail = explicitTail != 0;
                if (sectionVersion >= 3)
                {
                    uint8_t connected = 0;
                    if (!util::le_io::readBytes(fp, &connected, sizeof(connected)) || connected > 1)
                        return false;
                    bone.connectedToParent = connected != 0;
                    if (bone.parentBoneId == 0 && bone.connectedToParent) return false;
                }
            }
            else
            {
                bone.tailOffset = mbm::VEC3(0.0f, bone.length, 0.0f);
                bone.hasExplicitTail = false;
            }
            out.sourceBones.push_back(std::move(bone));
        }
        return fp.pos == fp.size && mbm::skeletal::compileCanonicalSkeleton(out.sourceBones, out.compiled);
    }

    bool parse_canonical_weights_section_v11(util::MEM_CURSOR_V11 &fp, const uint16_t sectionVersion,
                                              const mbm::skeletal::CANONICAL_SKELETON &skeleton,
                                              const uint32_t expectedVertexCount,
                                              mbm::skeletal::CANONICAL_WEIGHTS &out)
    {
        if (sectionVersion != 1)
            return false;
        out = {};
        uint32_t vertexCount = 0, paletteCount = 0;
        if (!util::le_io::readU64LE(fp, out.skeletonId) ||
            !util::le_io::readU32LE(fp, out.frameIndex) ||
            !util::le_io::readU32LE(fp, vertexCount) ||
            !util::le_io::readU32LE(fp, paletteCount) || paletteCount > 65535)
            return false;
        out.paletteBoneIds.resize(paletteCount);
        for (uint64_t &boneId : out.paletteBoneIds)
            if (!util::le_io::readU64LE(fp, boneId)) return false;
        out.vertices.resize(vertexCount);
        for (mbm::skeletal::CANONICAL_VERTEX_WEIGHT &vertex : out.vertices)
        {
            for (uint16_t &paletteIndex : vertex.paletteIndex)
                if (!util::le_io::readU16LE(fp, paletteIndex)) return false;
            for (float &weight : vertex.weight)
                if (!util::le_io::readF32LE(fp, weight)) return false;
        }
        return fp.pos == fp.size &&
            mbm::skeletal::validateCanonicalWeights(skeleton, out, expectedVertexCount);
    }

    bool read_zero_reserved3(util::MEM_CURSOR_V11 &fp)
    {
        uint8_t reserved[3] = {0, 0, 0};
        return util::le_io::readBytes(fp, reserved, sizeof(reserved)) &&
            reserved[0] == 0 && reserved[1] == 0 && reserved[2] == 0;
    }

    bool parse_canonical_animation_section_v11(util::MEM_CURSOR_V11 &fp, const uint16_t sectionVersion,
                                                const mbm::skeletal::CANONICAL_SKELETON &skeleton,
                                                mbm::skeletal::CANONICAL_ANIMATIONS &out)
    {
        if (sectionVersion != 1)
            return false;
        out = {};
        uint32_t clipCount = 0;
        if (!util::le_io::readU64LE(fp, out.skeletonId) ||
            !util::le_io::readU32LE(fp, clipCount) || clipCount > (fp.size - fp.pos) / 22u)
            return false;
        out.clips.reserve(clipCount);
        for (uint32_t clipIndex = 0; clipIndex < clipCount; ++clipIndex)
        {
            mbm::skeletal::SKELETAL_CLIP clip;
            uint8_t loop = 0;
            uint32_t trackCount = 0;
            if (!util::le_io::readU64LE(fp, clip.clipId) || !util::readStringV11(fp, clip.name) ||
                !util::le_io::readF32LE(fp, clip.duration) ||
                !util::le_io::readBytes(fp, &loop, sizeof(loop)) || loop > 1 ||
                !read_zero_reserved3(fp) || !util::le_io::readU32LE(fp, trackCount) ||
                trackCount > (fp.size - fp.pos) / 16u)
                return false;
            clip.loop = loop != 0;
            clip.tracks.reserve(trackCount);
            for (uint32_t trackIndex = 0; trackIndex < trackCount; ++trackIndex)
            {
                mbm::skeletal::SKELETAL_TRACK track;
                uint32_t keyCount = 0;
                if (!util::le_io::readU64LE(fp, track.boneId) ||
                    !util::le_io::readBytes(fp, &track.channelMask, sizeof(track.channelMask)) ||
                    !read_zero_reserved3(fp) || !util::le_io::readU32LE(fp, keyCount) ||
                    keyCount == 0 || keyCount > (fp.size - fp.pos) / 64u)
                    return false;
                track.keys.reserve(keyCount);
                for (uint32_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                {
                    mbm::skeletal::SKELETAL_KEY key;
                    uint8_t easing = 0;
                    if (!util::le_io::readF32LE(fp, key.time) ||
                        !util::le_io::readF32LE(fp, key.local.translation.x) ||
                        !util::le_io::readF32LE(fp, key.local.translation.y) ||
                        !util::le_io::readF32LE(fp, key.local.translation.z) ||
                        !util::le_io::readF32LE(fp, key.local.rotation.x) ||
                        !util::le_io::readF32LE(fp, key.local.rotation.y) ||
                        !util::le_io::readF32LE(fp, key.local.rotation.z) ||
                        !util::le_io::readF32LE(fp, key.local.rotation.w) ||
                        !util::le_io::readF32LE(fp, key.local.scale.x) ||
                        !util::le_io::readF32LE(fp, key.local.scale.y) ||
                        !util::le_io::readF32LE(fp, key.local.scale.z) ||
                        !util::le_io::readBytes(fp, &easing, sizeof(easing)) ||
                        !read_zero_reserved3(fp) ||
                        !util::le_io::readF32LE(fp, key.bezierX1) ||
                        !util::le_io::readF32LE(fp, key.bezierY1) ||
                        !util::le_io::readF32LE(fp, key.bezierX2) ||
                        !util::le_io::readF32LE(fp, key.bezierY2))
                        return false;
                    key.easing = static_cast<mbm::skeletal::SKELETAL_EASING>(easing);
                    track.keys.push_back(std::move(key));
                }
                clip.tracks.push_back(std::move(track));
            }
            out.clips.push_back(std::move(clip));
        }
        return fp.pos == fp.size && mbm::skeletal::validateCanonicalAnimations(skeleton, out);
    }

    // Worker-thread-safe equivalent of MESH_MBM::loadV11's section loop - see the struct comment
    // above for the main-thread-only calls this deliberately omits (deferred to
    // MESH_MBM::finishLoadFromIntermediate instead). fileNamePath must already be a resolved path
    // (both callers - MESH_MBM::loadV11 and Impl::workerLoop - resolve it via util::getFullPath
    // before calling this) - opened directly via fopenApp so this never touches getFullPath/addPath,
    // which keeps it safe to call from a worker thread.
    bool parse_v11_intermediate(const char *fileNamePath, mbm::MESH_LOAD_INTERMEDIATE_V11 &out, std::string &errorOut)
    {
        FILE *fp = util::fopenApp(fileNamePath, "rb");
        if (!fp)
        {
            errorOut = "Failed to open file";
            return false;
        }

        util::FILE_HEADER_V11 fileHeader;
        if (!util::readFileHeaderV11(fp, fileHeader))
        {
            fclose(fp);
            errorOut = "failed to read v11 file header";
            return false;
        }

        out.typeMe = static_cast<util::TYPE_MESH>(fileHeader.typeMesh);

        struct StagedSection
        {
            util::SECTION_HEADER_V11 header;
            std::vector<uint8_t>     payload;
        };
        std::vector<StagedSection> sections;
        sections.reserve(fileHeader.sectionCount);
        for (uint32_t i = 0; i < fileHeader.sectionCount; ++i)
        {
            StagedSection staged;
            if (!util::readSectionV11(fp, staged.header, staged.payload))
            {
                fclose(fp);
                errorOut = "failed to read section";
                return false;
            }
            sections.push_back(std::move(staged));
        }
        fclose(fp);

        const mbm::VEC2 *frame0Uv      = nullptr;
        int               frame0UvCount = 0;
        bool              sawCanonicalSkeleton = false;
        bool              sawCanonicalWeights = false;
        bool              sawCanonicalAnimations = false;
        uint32_t          canonicalFrame0VertexCount = UINT32_MAX;

        // Canonical sections resolve by type, not file order. Pre-read the skeleton and frame-0
        // topology so weights can appear anywhere in the staged section list.
        for (const auto &staged : sections)
        {
            if (staged.header.type == util::SECTION_SKELETAL_SKELETON)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(staged.payload);
                if (sawCanonicalSkeleton || !parse_canonical_skeleton_section_v11(
                        tmp, staged.header.sectionVersion, out.canonicalSkeleton))
                { errorOut = "failed to parse SECTION_SKELETAL_SKELETON"; return false; }
                sawCanonicalSkeleton = true;
            }
            else if (staged.header.type == util::SECTION_FRAME_STATIC &&
                     canonicalFrame0VertexCount == UINT32_MAX)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(staged.payload);
                util::FRAME_HEADER_V11 header;
                if (!util::readFrameHeaderV11(tmp, header))
                { errorOut = "failed to inspect SECTION_FRAME_STATIC"; return false; }
                canonicalFrame0VertexCount = header.vertexCount;
            }
        }

        for (const auto &staged : sections)
        {
            util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(staged.payload);

            if (staged.header.type == util::SECTION_MATERIAL_TRANSFORM)
            {
                util::MATERIAL_TRANSFORM_V11 materialTransform;
                if (!util::readMaterialTransformV11(tmp, materialTransform))
                {
                    errorOut = "failed to parse SECTION_MATERIAL_TRANSFORM";
                    return false;
                }
                out.material        = materialTransform.material;
                out.positionOffset_deprecated  = mbm::VEC3(materialTransform.posX, materialTransform.posY, materialTransform.posZ);
                out.angleDefault_deprecated    = mbm::VEC3(materialTransform.angleX, materialTransform.angleY, materialTransform.angleZ);
                out.info_mode.mode_draw                 = materialTransform.mode_draw;
                out.info_mode.mode_cull_face            = materialTransform.mode_cull_face;
                out.info_mode.mode_front_face_direction = materialTransform.mode_front_face_direction;
            }
            else if (staged.header.type == util::SECTION_EXTRA_PATHS)
            {
                uint32_t count = 0;
                bool     ok    = util::le_io::readU32LE(tmp, count);
                for (uint32_t p = 0; ok && p < count; ++p)
                {
                    std::string path;
                    ok = util::readStringV11(tmp, path);
                    if (ok)
                        out.extraPaths.push_back(path);
                }
                if (!ok)
                {
                    errorOut = "failed to parse SECTION_EXTRA_PATHS";
                    return false;
                }
            }
            else if (staged.header.type == util::SECTION_DETAIL_PHYSICS)
            {
                const bool ok = read_detail_mesh_section(tmp, fileNamePath, out.infoPhysics,
                                                         [&out](util::MEM_CURSOR_V11 &f, const char *n, const int tb)
                                                         {
                                                             return read_triangle_detail_v11(f, n, tb, out.infoPhysics);
                                                         });
                if (!ok)
                {
                    errorOut = "failed to parse SECTION_DETAIL_PHYSICS";
                    return false;
                }
            }
            else if (staged.header.type == util::SECTION_ANIMATION)
            {
                util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead = parse_animation_section_v11(tmp);
                if (!infoHead)
                {
                    errorOut = "failed to parse SECTION_ANIMATION";
                    return false;
                }
                out.infoAnimation.lsHeaderAnim.push_back(infoHead);
            }
            else if (staged.header.type == util::SECTION_ARTICULATED_PARTS)
            {
                if (staged.header.sectionVersion != 1 ||
                    !parse_articulated_parts_section_v11(tmp, out.articulatedParts))
                {
                    errorOut = "failed to parse SECTION_ARTICULATED_PARTS";
                    return false;
                }
            }
            else if (staged.header.type == util::SECTION_ARTICULATED_ANIMATION)
            {
                if (staged.header.sectionVersion != 1 ||
                    !parse_articulated_animation_section_v11(tmp, out.articulatedClips))
                {
                    errorOut = "failed to parse SECTION_ARTICULATED_ANIMATION";
                    return false;
                }
            }
            else if (staged.header.type == util::SECTION_DETAIL_PARTICLE)
            {
                std::vector<util::STAGE_PARTICLE*> *lsStage = parse_particle_detail_section_v11(tmp);
                if (!lsStage)
                {
                    errorOut = "failed to parse SECTION_DETAIL_PARTICLE";
                    return false;
                }
                out.extraInfo = lsStage;
            }
            else if (staged.header.type == util::SECTION_DETAIL_FONT)
            {
                mbm::INFO_BOUND_FONT *font = parse_font_detail_section_v11(tmp);
                if (!font)
                {
                    errorOut = "failed to parse SECTION_DETAIL_FONT";
                    return false;
                }
                out.extraInfo = font;
            }
            else if (staged.header.type == util::SECTION_DETAIL_TILE)
            {
                util::BTILE_INFO *tileInfo = parse_tile_detail_section_v11(tmp);
                if (!tileInfo)
                {
                    errorOut = "failed to parse SECTION_DETAIL_TILE";
                    return false;
                }
                out.extraInfo = tileInfo;
            }
            else if (staged.header.type == util::SECTION_FRAME_STATIC)
            {
                mbm::IntermediateFrameV11 frame;
                const bool                 ok = parse_v11_frame_intermediate(tmp, fileNamePath, frame, frame0Uv, frame0UvCount);
                if (!ok)
                {
                    errorOut = "failed to parse SECTION_FRAME_STATIC";
                    return false;
                }
                out.frames.push_back(std::move(frame));
                if (out.frames.size() == 1)
                {
                    frame0Uv      = out.frames[0].uv.get();
                    frame0UvCount = static_cast<int>(out.frames[0].vertexCount);
                }
            }
            else if (staged.header.type == util::SECTION_SKELETAL_SKELETON)
            {
                // Parsed in the order-independent pre-pass above.
            }
            else if (staged.header.type == util::SECTION_SKELETAL_WEIGHTS)
            {
                if (sawCanonicalWeights || !sawCanonicalSkeleton ||
                    canonicalFrame0VertexCount == UINT32_MAX ||
                    !parse_canonical_weights_section_v11(tmp, staged.header.sectionVersion,
                        out.canonicalSkeleton, canonicalFrame0VertexCount, out.canonicalWeights))
                { errorOut = "failed to parse SECTION_SKELETAL_WEIGHTS"; return false; }
                sawCanonicalWeights = true;
            }
            else if (staged.header.type == util::SECTION_SKELETAL_ANIMATION)
            {
                if (sawCanonicalAnimations || !sawCanonicalSkeleton ||
                    !parse_canonical_animation_section_v11(tmp, staged.header.sectionVersion,
                        out.canonicalSkeleton, out.canonicalAnimations))
                { errorOut = "failed to parse SECTION_SKELETAL_ANIMATION"; return false; }
                sawCanonicalAnimations = true;
            }
            else
            {
                errorOut = "loadV11 does not support this section type";
                return false;
            }
        }
        return true;
    }

}

namespace mbm
{
    ARTICULATED_ANIMATION_PLAYER::ARTICULATED_ANIMATION_PLAYER()
        : impl(std::make_unique<Impl>())
    {
    }

    ARTICULATED_ANIMATION_PLAYER::~ARTICULATED_ANIMATION_PLAYER() = default;

    void ARTICULATED_ANIMATION_PLAYER::reset() noexcept
    {
        impl->activeClips.clear();
        impl->sequence = 0;
    }

    SKELETAL_ANIMATION_PLAYER::SKELETAL_ANIMATION_PLAYER()
        : impl(std::make_unique<Impl>())
    {
    }

    SKELETAL_ANIMATION_PLAYER::~SKELETAL_ANIMATION_PLAYER() = default;

    void SKELETAL_ANIMATION_PLAYER::reset() noexcept
    {
        impl->clipIndex = UINT32_MAX;
        impl->time = 0.0f;
        impl->absoluteLayerClipIndex = UINT32_MAX;
        impl->absoluteLayerTime = 0.0f;
        impl->absoluteLayerWeight = 0.0f;
        impl->absoluteLayerFadeStartWeight = 0.0f;
        impl->absoluteLayerFadeTargetWeight = 0.0f;
        impl->absoluteLayerFadeDuration = 0.0f;
        impl->absoluteLayerFadeElapsed = 0.0f;
        impl->absoluteLayerFadeActive = false;
        impl->absoluteLayerActive = false;
        impl->additiveLayer = false;
        impl->crossFadeActive = false;
        impl->layerPaused = false;
        impl->layerBoneMask.clear();
        impl->playbackSpeed = 1.0f;
        impl->active = false;
        impl->paused = false;
        impl->baseCompletionNotified = false;
        impl->layerCompletionNotified = false;
        impl->paletteRows.clear();
        impl->evaluatedGlobalTransforms.clear();
        impl->previousEvaluatedGlobalTransforms.clear();
        impl->rawEvaluatedGlobalTransforms.clear();
        impl->previousRawEvaluatedGlobalTransforms.clear();
        impl->evaluatedMotionDeltaValid = false;
        impl->authoringPose = false;
        impl->automaticRootMotionEnabled = false;
        impl->automaticRootMotionApplyRotation = false;
        impl->automaticRootMotionBoneName.clear();
        impl->automaticRootMotionBoneId = 0;
    }

    void SKELETAL_ANIMATION_PLAYER::setSkinningMethod(const SKELETAL_SHADER_METHOD method) noexcept
    {
        impl->requestedSkinningMethod = method;
        impl->resolvedSkinningMethod = method == SKELETAL_SHADER_METHOD::AUTO
            ? SKELETAL_SHADER_METHOD::NONE : method;
        impl->skinningResolutionReason = method == SKELETAL_SHADER_METHOD::AUTO
            ? "not-resolved" : method == SKELETAL_SHADER_METHOD::DQS_RIGID
            ? "explicit-dqs" : "explicit-lbs";
    }

    SKELETAL_SHADER_METHOD SKELETAL_ANIMATION_PLAYER::getSkinningMethod() const noexcept
    {
        return impl->requestedSkinningMethod;
    }

    SKELETAL_SHADER_METHOD SKELETAL_ANIMATION_PLAYER::getResolvedSkinningMethod() const noexcept
    {
        return impl->resolvedSkinningMethod;
    }

    const char *SKELETAL_ANIMATION_PLAYER::getSkinningResolutionReason() const noexcept
    {
        return impl->skinningResolutionReason;
    }

    struct MESH_MANAGER::Impl
    {
        std::unordered_map<std::string, MESH_MBM *> lsMeshes;
        std::vector<MESH_MBM *> lsFakeRelease;

        // Milestone 6: async loading. A small fixed worker pool does file I/O + v11 parsing
        // (parse_v11_intermediate, pure CPU, no GPU/global-state touch); pumpAsyncLoads() (main
        // thread) drains completedJobs and does the GPU-finish work. Lazily started on first
        // loadAsync() call so games that never use async loading never spin up threads.
        struct AsyncJob
        {
            std::string            fileName;         // original, caller-given path - carried through to
                                                       // AsyncResult::fileName/finishLoadFromIntermediate
            std::string            resolvedFileName;  // getFullPath(fileName) resolved on the main thread
                                                       // before queuing - the only path the worker opens
            MeshAsyncLoadCallback  onComplete;
        };

        struct AsyncResult
        {
            std::string                fileName;
            MeshAsyncLoadCallback      onComplete;
            bool                        cacheHit    = false; // already-loaded mesh, no parse/finish needed
            MESH_MBM                   *cachedMesh  = nullptr; // valid when cacheHit
            bool                        parseOk     = false;
            std::string                 error;
            MESH_LOAD_INTERMEDIATE_V11  intermediate;
        };

        std::vector<std::thread> workerThreads;
        std::atomic<bool>        stopWorkers{false};

        std::mutex                jobMutex;
        std::condition_variable   jobCv;
        std::queue<AsyncJob>      jobQueue;

        std::mutex                completionMutex;
        std::vector<AsyncResult>  completedJobs;

        void ensureWorkersStarted();
        void workerLoop();
        void stopAndJoinWorkers();
    };

    // Clamped to [2, 8]: 2 is the floor (matches the previous hardcoded value, also what any
    // platform that can't detect its core count - hardware_concurrency() returning 0 - or that only
    // reports 1 core falls back to); 8 is a cap so a many-core desktop doesn't spin up an excessive
    // number of threads for what is I/O-bound + comparatively light CPU parsing work, and so this
    // pool doesn't crowd out cores rendering/audio/physics need on the main thread.
    static unsigned int computeMeshWorkerCount()
    {
        const unsigned int hw = std::thread::hardware_concurrency();
        if (hw < 2u) return 2u;
        if (hw > 8u) return 8u;
        return hw;
    }

    void MESH_MANAGER::Impl::ensureWorkersStarted()
    {
        if (!workerThreads.empty())
            return;
        const unsigned int workerCount = computeMeshWorkerCount();
        for (unsigned int i = 0; i < workerCount; ++i)
            workerThreads.emplace_back([this]() { this->workerLoop(); });
    }

    void MESH_MANAGER::Impl::workerLoop()
    {
        for (;;)
        {
            AsyncJob job;
            {
                std::unique_lock<std::mutex> lock(jobMutex);
                jobCv.wait(lock, [this]() { return stopWorkers.load() || !jobQueue.empty(); });
                if (jobQueue.empty()) // predicate guarantees stopWorkers is true here
                    return;
                job = std::move(jobQueue.front());
                jobQueue.pop();
            }

            AsyncResult result;
            result.fileName   = job.fileName;
            result.onComplete = std::move(job.onComplete);
            result.parseOk    = parse_v11_intermediate(job.resolvedFileName.c_str(), result.intermediate, result.error);

            {
                std::lock_guard<std::mutex> lock(completionMutex);
                completedJobs.push_back(std::move(result));
            }
        }
    }

    void MESH_MANAGER::Impl::stopAndJoinWorkers()
    {
        if (workerThreads.empty())
            return;
        {
            std::lock_guard<std::mutex> lock(jobMutex);
            stopWorkers = true;
        }
        jobCv.notify_all();
        for (auto &worker : workerThreads)
        {
            if (worker.joinable())
                worker.join();
        }
        workerThreads.clear();
    }

    // (MESH_MBM has no equivalent method: MESH_MBM::loadV11 delegates entirely to
    // parse_v11_intermediate/finishLoadFromIntermediate, which use the free-function equivalent
    // read_triangle_detail_v11 above instead.)
    bool MESH_MBM_DEBUG::readDebugTriangleDetailCompat(util::MEM_CURSOR_V11 &fp, const char *fileNamePath, const int totalBounding)
    {
        for (int j = 0; j < totalBounding; j++)
        {
            auto triangle = new TRIANGLE();
            this->impl->infoPhysics.lsTriangle.push_back(triangle);
            if (!util::readTriangleV8(fp, *triangle))
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
        }
        return true;
    }


    BUFFER_MESH::BUFFER_MESH() noexcept : pBufferGL(nullptr), subset(nullptr), totalSubset(0)
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
        impl->positionOffset_deprecated = VEC3(0, 0, 0);
        impl->angleDefault_deprecated   = VEC3(0, 0, 0);
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
        if (impl->simplifyWorker.joinable())
            impl->simplifyWorker.join();
        this->release();
    }

    bool skeletal::copyCanonicalParityAsset(const MESH_MBM_DEBUG &mesh,
                                             CANONICAL_PARITY_ASSET &out) noexcept
    {
        out = {};
        if (!mesh.impl || mesh.impl->canonicalSkeleton.skeletonId == 0 ||
            mesh.impl->canonicalWeights.skeletonId == 0 ||
            mesh.impl->canonicalAnimations.skeletonId == 0)
            return false;
        out.skeleton = mesh.impl->canonicalSkeleton;
        out.weights = mesh.impl->canonicalWeights;
        out.animations = mesh.impl->canonicalAnimations;
        return true;
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

    // Peeks a mesh file's header without fully loading it (editor file-browser preview). A non-v11
    // file (bad magic) returns false, no legacy fallback. datailFontOut/lsStageParticle are never
    // populated here - this quick peek only walks SECTION_MATERIAL_TRANSFORM/ANIMATION/FRAME_STATIC,
    // it doesn't read font/particle detail sections - the (void) casts above are intentional, not
    // oversights.
    bool MESH_MBM_DEBUG::getInfo(const char *fileNamePath, util::HEADER_MESH &headerMeshMbmOut,util::INFO_DRAW_MODE & info_mode,
                              util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut,
                              std::vector<util::STAGE_PARTICLE> & lsStageParticle, int *versionOut,
                              bool *hasSkeletonOut, uint16_t *totalBonesOut)
    {
        (void)datailFontOut;
        (void)lsStageParticle;
        if (hasSkeletonOut) *hasSkeletonOut = false;
        if (totalBonesOut) *totalBonesOut = 0;
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

        bool sawFirstFrame = false;
        for (uint32_t i = 0; i < fileHeader.sectionCount; ++i)
        {
            util::SECTION_HEADER_V11 sectionHeader;
            std::vector<uint8_t> payload;
            if (!util::readSectionV11(fp, sectionHeader, payload))
                break;
            if (sectionHeader.type == util::SECTION_MATERIAL_TRANSFORM)
            {
                util::MEM_CURSOR_V11 tmpFp = stage_payload_as_cursor(payload);
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
            }
            else if (sectionHeader.type == util::SECTION_ANIMATION)
            {
                ++headerMeshMbmOut.totalAnimation;
            }
            else if (sectionHeader.type == util::SECTION_FRAME_STATIC)
            {
                ++headerMeshMbmOut.totalFrames;
                // hasNorText only reflects frame 0, mirroring loadV11's hasNormalFlag/hasTextureFlag derivation.
                if (!sawFirstFrame)
                {
                    sawFirstFrame = true;
                    util::MEM_CURSOR_V11 tmpFp = stage_payload_as_cursor(payload);
                    util::FRAME_HEADER_V11 v11FrameHeader;
                    if (util::readFrameHeaderV11(tmpFp, v11FrameHeader))
                    {
                        headerMeshMbmOut.hasNorText[0] = v11FrameHeader.hasNormal ? HAS_NOR_IN_FILE : HAS_NOR_NO;
                        headerMeshMbmOut.hasNorText[1] = !v11FrameHeader.hasUv ? HAS_TEX_NO
                                                        : (v11FrameHeader.uvSource == 0 ? HAS_TEX_EACH_FRAME : HAS_TEX_FIRST_FRAME);
                    }
                }
            }
            else if (sectionHeader.type == util::SECTION_SKELETAL_SKELETON)
            {
                if (hasSkeletonOut) *hasSkeletonOut = true;
                if (totalBonesOut)
                {
                    util::MEM_CURSOR_V11 tmpFp = stage_payload_as_cursor(payload);
                    uint64_t skeletonId = 0;
                    uint32_t boneCount = 0;
                    if (util::le_io::readU64LE(tmpFp, skeletonId) && util::le_io::readU32LE(tmpFp, boneCount))
                        *totalBonesOut = static_cast<uint16_t>(std::min<uint32_t>(boneCount, UINT16_MAX));
                }
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
        return impl->formatVersion;
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
        // Unrecognized extension - peek the v11 file header directly.
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
                    for (int i = subset->vertexStart; (i + 3) <= countSubset; i += 3)
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
                    int                 countIndexSubset = subset->indexStart + subset->indexCount;
                    countIndexSubset -= (countIndexSubset % 3);
                    for (int i = subset->indexStart; (i + 3) <= countIndexSubset; i += 3)
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

    bool MESH_MBM_DEBUG::simplify(const float targetTriangleRatio, MESH_SIMPLIFY_REPORT &report,
                                  char *errorOut, const int errorOutLen,
                                  const int targetSubsetIndex, const int targetFrameIndex,
                                  const bool preserveDetails)
    {
        report = {};
        auto fail = [errorOut, errorOutLen](const std::string &message)
        {
            if (errorOut && errorOutLen > 0)
                snprintf(errorOut, static_cast<size_t>(errorOutLen), "%s", message.c_str());
            return false;
        };
        if (!std::isfinite(targetTriangleRatio) || targetTriangleRatio <= 0.0f || targetTriangleRatio >= 1.0f)
            return fail("target triangle ratio must be finite, greater than zero, and smaller than one");
        if (impl->typeMe != util::TYPE_MESH_3D)
            return fail("simplification currently supports only 3D meshes");
        if (impl->info_mode.mode_draw != util::MODE_DRAW_TRIANGLES)
            return fail("simplification currently supports only triangle-list draw mode");
        const bool simplifyAllFrames = targetFrameIndex == -1;
        const int referenceFrameIndex = simplifyAllFrames ? 0 : targetFrameIndex;
        if (referenceFrameIndex < 0 || referenceFrameIndex >= static_cast<int>(impl->buffer.size()))
            return fail("target frame index is outside the mesh");
        const bool hasCanonicalData = impl->canonicalSkeleton.skeletonId != 0 ||
                                      impl->canonicalWeights.skeletonId != 0 ||
                                      impl->canonicalAnimations.skeletonId != 0;
        const bool hasCanonicalWeights = impl->canonicalWeights.skeletonId != 0;
        if (impl->buffer.size() > 1 && hasCanonicalData)
            return fail("selected-frame simplification does not support multi-frame skeletal assets");
        if (hasCanonicalData && (impl->canonicalSkeleton.skeletonId == 0 || !hasCanonicalWeights))
            return fail("canonical skeletal simplification requires both a skeleton and skin weights");
        if (hasCanonicalWeights &&
            (impl->canonicalWeights.skeletonId != impl->canonicalSkeleton.skeletonId ||
             impl->canonicalWeights.frameIndex != 0))
            return fail("canonical skin weights do not reference frame zero and the active skeleton");

        util::BUFFER_MESH_DEBUG *frame = impl->buffer[static_cast<size_t>(referenceFrameIndex)];
        if (!frame || !frame->position || frame->subset.empty())
            return fail("simplification requires a non-empty mesh");
        const bool sourceIndexed = frame->indexBuffer != nullptr;
        if (targetSubsetIndex < -1 ||
            targetSubsetIndex >= static_cast<int>(frame->subset.size()))
            return fail("target subset index is outside the selected frame");

        struct SUBSET_RANGE
        {
            int vertexStart = 0;
            int indexStart = 0;
            int vertexCount = 0;
            int indexCount = 0;
        };
        struct POSITION_KEY
        {
            uint32_t x = 0;
            uint32_t y = 0;
            uint32_t z = 0;

            bool operator==(const POSITION_KEY &other) const noexcept
            {
                return x == other.x && y == other.y && z == other.z;
            }
        };
        struct POSITION_KEY_HASH
        {
            size_t operator()(const POSITION_KEY &key) const noexcept
            {
                size_t value = static_cast<size_t>(key.x) * 0x9e3779b1u;
                value ^= static_cast<size_t>(key.y) + 0x9e3779b9u + (value << 6) + (value >> 2);
                value ^= static_cast<size_t>(key.z) + 0x9e3779b9u + (value << 6) + (value >> 2);
                return value;
            }
        };
        struct LOGICAL_SOURCE
        {
            std::unordered_map<uint32_t, uint32_t> globalBySubset;
            std::vector<uint32_t> globals;
            uint64_t topologyDomain = 0;
        };
        std::vector<SUBSET_RANGE> results(frame->subset.size());
        std::vector<VEC3> positions;
        std::vector<VEC3> normals;
        std::vector<VEC2> uvs;
        std::vector<uint16_t> indices;
        std::vector<skeletal::CANONICAL_VERTEX_WEIGHT> weights;
        skeletal::CANONICAL_WEIGHTS simplifiedWeights;
        std::vector<std::vector<VEC3>> deformationDeltas;

        const auto *sourcePositions = reinterpret_cast<const VEC3 *>(frame->position);
        const auto *sourceNormals = reinterpret_cast<const VEC3 *>(frame->normal);
        const auto *sourceUvs = reinterpret_cast<const VEC2 *>(frame->uv);
        report.sourceVertexCount = static_cast<uint32_t>(frame->headerFrame.sizeVertexBuffer);
        std::vector<uint64_t> subsetTopologyDomains(frame->subset.size(), 0);
        for (const util::ARTICULATED_PART_V11 &part : impl->articulatedParts)
        {
            if (part.frameIndex != static_cast<uint32_t>(referenceFrameIndex)) continue;
            if (part.subsetIndex >= subsetTopologyDomains.size())
                return fail("articulated Part references a subset outside the simplification frame");
            subsetTopologyDomains[part.subsetIndex] = part.partId;
        }
        if (simplifyAllFrames)
        {
            if (hasCanonicalData)
                return fail("shared multi-frame simplification does not support skeletal assets");
            for (size_t frameIndex = 1; frameIndex < impl->buffer.size(); ++frameIndex)
            {
                const util::BUFFER_MESH_DEBUG *candidate = impl->buffer[frameIndex];
                if (!candidate || !candidate->position ||
                    candidate->headerFrame.sizeVertexBuffer != frame->headerFrame.sizeVertexBuffer ||
                    candidate->headerFrame.sizeIndexBuffer != frame->headerFrame.sizeIndexBuffer ||
                    candidate->headerFrame.stride != frame->headerFrame.stride ||
                    candidate->subset.size() != frame->subset.size() ||
                    (candidate->indexBuffer != nullptr) != sourceIndexed ||
                    (candidate->normal != nullptr) != (frame->normal != nullptr) ||
                    (candidate->uv != nullptr) != (frame->uv != nullptr))
                    return fail("geometry frames do not have compatible vertex attributes and topology");
                if (sourceIndexed && memcmp(candidate->indexBuffer, frame->indexBuffer,
                           static_cast<size_t>(frame->headerFrame.sizeIndexBuffer) * sizeof(uint16_t)) != 0)
                    return fail("geometry frames do not share the same index topology");
                for (size_t subsetIndex = 0; subsetIndex < frame->subset.size(); ++subsetIndex)
                {
                    const util::SUBSET_DEBUG *referenceSubset = frame->subset[subsetIndex];
                    const util::SUBSET_DEBUG *candidateSubset = candidate->subset[subsetIndex];
                    if (!candidateSubset || candidateSubset->vertexStart != referenceSubset->vertexStart ||
                        candidateSubset->vertexCount != referenceSubset->vertexCount ||
                        candidateSubset->indexStart != referenceSubset->indexStart ||
                        candidateSubset->indexCount != referenceSubset->indexCount)
                        return fail("geometry frames do not share the same subset ranges");
                }
                const auto *candidatePositions = reinterpret_cast<const VEC3 *>(candidate->position);
                std::vector<VEC3> &sample = deformationDeltas.emplace_back();
                sample.reserve(report.sourceVertexCount);
                for (uint32_t vertex = 0; vertex < report.sourceVertexCount; ++vertex)
                    sample.push_back(candidatePositions[vertex] - sourcePositions[vertex]);
            }
            report.geometryFrameAware = true;
            report.geometryFrameCount = static_cast<uint32_t>(impl->buffer.size());
        }
        VEC3 sourceMinimum = sourcePositions[0];
        VEC3 sourceMaximum = sourcePositions[0];
        for (uint32_t vertex = 1; vertex < report.sourceVertexCount; ++vertex)
        {
            const VEC3 &position = sourcePositions[vertex];
            sourceMinimum.x = std::min(sourceMinimum.x, position.x);
            sourceMinimum.y = std::min(sourceMinimum.y, position.y);
            sourceMinimum.z = std::min(sourceMinimum.z, position.z);
            sourceMaximum.x = std::max(sourceMaximum.x, position.x);
            sourceMaximum.y = std::max(sourceMaximum.y, position.y);
            sourceMaximum.z = std::max(sourceMaximum.z, position.z);
        }
        if (hasCanonicalWeights)
        {
            if (impl->canonicalWeights.vertices.size() != report.sourceVertexCount ||
                !skeletal::validateCanonicalWeights(impl->canonicalSkeleton, impl->canonicalWeights,
                                                    report.sourceVertexCount))
                return fail("canonical skin weights are invalid for the source geometry");
            weights.reserve(report.sourceVertexCount);
            report.skinWeightAware = true;

            if (!impl->canonicalAnimations.clips.empty())
            {
                if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton,
                                                           impl->canonicalAnimations))
                    return fail("canonical animations are invalid for pose-sampled simplification");
                static constexpr uint32_t MAX_POSE_SAMPLES = 24;
                const uint32_t sampledClips = std::min<uint32_t>(MAX_POSE_SAMPLES,
                    static_cast<uint32_t>(impl->canonicalAnimations.clips.size()));
                const uint32_t samplesPerClip = MAX_POSE_SAMPLES / sampledClips;
                const uint32_t extraSamples = MAX_POSE_SAMPLES % sampledClips;
                std::vector<VEC3> bindPositions(sourcePositions,
                    sourcePositions + report.sourceVertexCount);
                for (uint32_t clipIndex = 0; clipIndex < sampledClips; ++clipIndex)
                {
                    const skeletal::SKELETAL_CLIP &clip = impl->canonicalAnimations.clips[clipIndex];
                    const uint32_t clipSamples = samplesPerClip + (clipIndex < extraSamples ? 1u : 0u);
                    for (uint32_t sampleIndex = 0; sampleIndex < clipSamples; ++sampleIndex)
                    {
                        const float time = clip.duration * static_cast<float>(sampleIndex + 1) /
                                           static_cast<float>(clipSamples + 1);
                        skeletal::SKELETAL_POSE pose;
                        std::vector<VEC3> skinnedPositions;
                        std::vector<VEC3> skinnedNormals;
                        if (!skeletal::sampleSkeletalClip(impl->canonicalSkeleton.compiled, clip, time, pose) ||
                            !skeletal::skinVerticesLbsReference(impl->canonicalSkeleton,
                                impl->canonicalWeights, pose, bindPositions, {},
                                skinnedPositions, skinnedNormals))
                            return fail("failed to evaluate a canonical animation pose for simplification");
                        std::vector<VEC3> &sample = deformationDeltas.emplace_back();
                        sample.reserve(report.sourceVertexCount);
                        for (uint32_t vertex = 0; vertex < report.sourceVertexCount; ++vertex)
                            sample.push_back(skinnedPositions[vertex] - bindPositions[vertex]);
                    }
                }
                report.poseSampledError = !deformationDeltas.empty();
                report.sampledPoseCount = static_cast<uint32_t>(deformationDeltas.size());
                report.sampledClipCount = sampledClips;
            }
        }
        auto positionKey = [](const VEC3 &position)
        {
            POSITION_KEY key;
            const float x = position.x == 0.0f ? 0.0f : position.x;
            const float y = position.y == 0.0f ? 0.0f : position.y;
            const float z = position.z == 0.0f ? 0.0f : position.z;
            memcpy(&key.x, &x, sizeof(uint32_t));
            memcpy(&key.y, &y, sizeof(uint32_t));
            memcpy(&key.z, &z, sizeof(uint32_t));
            return key;
        };

        mesh_simplifier::INPUT input;
        input.preserveDetails = preserveDetails;
        input.deformationDeltas.resize(deformationDeltas.size());
        input.indices.reserve(static_cast<size_t>(frame->headerFrame.sizeIndexBuffer));
        input.triangleGroups.reserve(static_cast<size_t>(frame->headerFrame.sizeIndexBuffer / 3));
        std::vector<LOGICAL_SOURCE> logicalSources;
        std::unordered_map<POSITION_KEY, std::vector<uint32_t>, POSITION_KEY_HASH> logicalByPosition;
        logicalByPosition.reserve(report.sourceVertexCount);
        for (uint32_t subsetIndex = 0; subsetIndex < frame->subset.size(); ++subsetIndex)
        {
            const util::SUBSET_DEBUG *subset = frame->subset[subsetIndex];
            if (!subset || subset->vertexStart < 0 || subset->vertexCount <= 0 ||
                subset->vertexStart + subset->vertexCount > frame->headerFrame.sizeVertexBuffer)
                return fail("subset vertex ranges are invalid for triangle simplification");
            if (sourceIndexed && (subset->indexStart < 0 || subset->indexCount < 3 ||
                subset->indexCount % 3 != 0 ||
                subset->indexStart + subset->indexCount > frame->headerFrame.sizeIndexBuffer))
                return fail("subset index ranges are invalid for triangle simplification");
            if (!sourceIndexed && subset->vertexCount % 3 != 0)
                return fail("non-indexed triangle subsets require a vertex count divisible by three");
            const int sourceElementCount = sourceIndexed ? subset->indexCount : subset->vertexCount;
            report.sourceTriangleCount += static_cast<uint32_t>(sourceElementCount / 3);
            if (targetSubsetIndex >= 0 && subsetIndex != static_cast<uint32_t>(targetSubsetIndex))
                continue;

            std::unordered_map<uint32_t, uint32_t> localByGlobal;
            localByGlobal.reserve(static_cast<size_t>(subset->vertexCount));
            std::unordered_map<std::string, uint32_t> localByAttributes;
            if (!sourceIndexed) localByAttributes.reserve(static_cast<size_t>(subset->vertexCount));
            for (int i = 0; i < sourceElementCount; ++i)
            {
                const uint32_t globalIndex = sourceIndexed
                    ? frame->indexBuffer[subset->indexStart + i]
                    : static_cast<uint32_t>(subset->vertexStart + i);
                if (globalIndex >= static_cast<uint32_t>(frame->headerFrame.sizeVertexBuffer))
                    return fail("subset index is outside the frame vertex buffer");
                uint32_t existingLogical = UINT32_MAX;
                std::string attributeKey;
                if (sourceIndexed)
                {
                    const auto found = localByGlobal.find(globalIndex);
                    if (found != localByGlobal.end()) existingLogical = found->second;
                }
                else
                {
                    attributeKey.append(reinterpret_cast<const char *>(&sourcePositions[globalIndex]),
                                        sizeof(VEC3));
                    if (sourceNormals)
                        attributeKey.append(reinterpret_cast<const char *>(&sourceNormals[globalIndex]),
                                            sizeof(VEC3));
                    if (sourceUvs)
                        attributeKey.append(reinterpret_cast<const char *>(&sourceUvs[globalIndex]),
                                            sizeof(VEC2));
                    const auto found = localByAttributes.find(attributeKey);
                    if (found != localByAttributes.end()) existingLogical = found->second;
                }
                if (existingLogical == UINT32_MAX)
                {
                    const POSITION_KEY key = positionKey(sourcePositions[globalIndex]);
                    std::vector<uint32_t> &candidates = logicalByPosition[key];
                    uint32_t logicalIndex = UINT32_MAX;
                    for (const uint32_t candidate : candidates)
                    {
                        if (logicalSources[candidate].topologyDomain == subsetTopologyDomains[subsetIndex] &&
                            logicalSources[candidate].globalBySubset.find(subsetIndex) ==
                            logicalSources[candidate].globalBySubset.end())
                        {
                            logicalIndex = candidate;
                            break;
                        }
                    }
                    if (logicalIndex == UINT32_MAX)
                    {
                        logicalIndex = static_cast<uint32_t>(input.positions.size());
                        input.positions.push_back(sourcePositions[globalIndex]);
                        logicalSources.emplace_back();
                        logicalSources.back().topologyDomain = subsetTopologyDomains[subsetIndex];
                        candidates.push_back(logicalIndex);
                        for (size_t sampleIndex = 0; sampleIndex < deformationDeltas.size(); ++sampleIndex)
                            input.deformationDeltas[sampleIndex].push_back(
                                deformationDeltas[sampleIndex][globalIndex]);
                    }
                    else
                    {
                        const float oldCount = static_cast<float>(logicalSources[logicalIndex].globals.size());
                        const float newCount = oldCount + 1.0f;
                        for (size_t sampleIndex = 0; sampleIndex < deformationDeltas.size(); ++sampleIndex)
                        {
                            VEC3 &logicalDelta = input.deformationDeltas[sampleIndex][logicalIndex];
                            const VEC3 &sourceDelta = deformationDeltas[sampleIndex][globalIndex];
                            logicalDelta.x = (logicalDelta.x * oldCount + sourceDelta.x) / newCount;
                            logicalDelta.y = (logicalDelta.y * oldCount + sourceDelta.y) / newCount;
                            logicalDelta.z = (logicalDelta.z * oldCount + sourceDelta.z) / newCount;
                        }
                    }
                    LOGICAL_SOURCE &logicalSource = logicalSources[logicalIndex];
                    logicalSource.globalBySubset.emplace(subsetIndex, globalIndex);
                    logicalSource.globals.push_back(globalIndex);
                    if (sourceIndexed) localByGlobal.emplace(globalIndex, logicalIndex);
                    else localByAttributes.emplace(std::move(attributeKey), logicalIndex);
                    existingLogical = logicalIndex;
                }
                input.indices.push_back(existingLogical);
            }
            input.triangleGroups.insert(input.triangleGroups.end(),
                static_cast<size_t>(sourceElementCount / 3), subsetIndex);
        }

        const uint32_t activeSourceTriangles = static_cast<uint32_t>(input.indices.size() / 3);
        if (activeSourceTriangles < 2)
            return fail("target simplification scope requires at least two triangles");
        const uint32_t minimumTriangles = targetSubsetIndex < 0
            ? static_cast<uint32_t>(frame->subset.size()) : 1u;
        const uint32_t targetTriangles = std::max<uint32_t>(minimumTriangles,
            static_cast<uint32_t>(std::floor(activeSourceTriangles * targetTriangleRatio)));
        mesh_simplifier::OUTPUT simplified;
        std::string simplifyError;
        if (!mesh_simplifier::simplify(input, targetTriangles, simplified, simplifyError,
            [this](const float progress) { impl->simplifyProgress = progress; }))
            return fail(std::string("frame simplification failed: ") + simplifyError);
        if (simplified.triangleGroups.size() != simplified.indices.size() / 3 ||
            simplified.sourceContributions.size() != simplified.positions.size())
            return fail("frame simplification returned inconsistent topology metadata");

        std::vector<std::vector<uint32_t>> subsetLogicalIndices(frame->subset.size());
        for (size_t triangle = 0; triangle < simplified.triangleGroups.size(); ++triangle)
        {
            const uint32_t group = simplified.triangleGroups[triangle];
            if (group >= subsetLogicalIndices.size())
                return fail("frame simplification returned an invalid subset group");
            std::vector<uint32_t> &groupIndices = subsetLogicalIndices[group];
            groupIndices.push_back(simplified.indices[triangle * 3]);
            groupIndices.push_back(simplified.indices[triangle * 3 + 1]);
            groupIndices.push_back(simplified.indices[triangle * 3 + 2]);
        }

        auto sourceGlobalsForSubset = [&logicalSources](const uint32_t logicalIndex,
                                                        const uint32_t subsetIndex,
                                                        std::vector<uint32_t> &fallback)
        {
            fallback.clear();
            const LOGICAL_SOURCE &source = logicalSources[logicalIndex];
            const auto found = source.globalBySubset.find(subsetIndex);
            if (found != source.globalBySubset.end()) fallback.push_back(found->second);
            else fallback = source.globals;
        };
        std::vector<uint32_t> attributeGlobals;
        for (uint32_t subsetIndex = 0; subsetIndex < subsetLogicalIndices.size(); ++subsetIndex)
        {
            const std::vector<uint32_t> &groupIndices = subsetLogicalIndices[subsetIndex];
            SUBSET_RANGE &range = results[subsetIndex];
            range.vertexStart = static_cast<int>(positions.size());
            range.indexStart = static_cast<int>(indices.size());
            if (targetSubsetIndex >= 0 && subsetIndex != static_cast<uint32_t>(targetSubsetIndex))
            {
                const util::SUBSET_DEBUG *sourceSubset = frame->subset[subsetIndex];
                std::unordered_map<uint32_t, uint32_t> copiedByGlobal;
                copiedByGlobal.reserve(static_cast<size_t>(sourceSubset->vertexCount));
                const int sourceElementCount = sourceIndexed
                    ? sourceSubset->indexCount : sourceSubset->vertexCount;
                for (int i = 0; i < sourceElementCount; ++i)
                {
                    const uint32_t globalIndex = sourceIndexed
                        ? frame->indexBuffer[sourceSubset->indexStart + i]
                        : static_cast<uint32_t>(sourceSubset->vertexStart + i);
                    if (globalIndex >= report.sourceVertexCount)
                        return fail("subset index is outside the frame vertex buffer");
                    auto copied = copiedByGlobal.find(globalIndex);
                    if (copied == copiedByGlobal.end())
                    {
                        if (positions.size() >= UINT16_MAX)
                            return fail("simplified frame still exceeds the uint16 vertex-index limit");
                        const uint32_t outputIndex = static_cast<uint32_t>(positions.size());
                        copied = copiedByGlobal.emplace(globalIndex, outputIndex).first;
                        positions.push_back(sourcePositions[globalIndex]);
                        if (sourceNormals) normals.push_back(sourceNormals[globalIndex]);
                        if (sourceUvs) uvs.push_back(sourceUvs[globalIndex]);
                        if (hasCanonicalWeights)
                            weights.push_back(impl->canonicalWeights.vertices[globalIndex]);
                    }
                    indices.push_back(static_cast<uint16_t>(copied->second));
                }
                range.vertexCount = static_cast<int>(positions.size()) - range.vertexStart;
                range.indexCount = static_cast<int>(indices.size()) - range.indexStart;
                continue;
            }
            if (groupIndices.empty())
                return fail("simplification would remove every triangle from a material subset");
            std::unordered_map<uint32_t, uint32_t> physicalByLogical;
            physicalByLogical.reserve(groupIndices.size());
            for (const uint32_t logicalOutput : groupIndices)
            {
                if (logicalOutput >= simplified.positions.size())
                    return fail("frame simplification returned an invalid vertex index");
                auto physical = physicalByLogical.find(logicalOutput);
                if (physical == physicalByLogical.end())
                {
                    if (positions.size() >= UINT16_MAX)
                        return fail("simplified frame still exceeds the uint16 vertex-index limit");
                    const uint32_t physicalIndex = static_cast<uint32_t>(positions.size());
                    physical = physicalByLogical.emplace(logicalOutput, physicalIndex).first;
                    positions.push_back(simplified.positions[logicalOutput]);

                    VEC3 blendedNormal(0.0f, 0.0f, 0.0f);
                    VEC2 blendedUv(0.0f, 0.0f);
                    double attributeTotal = 0.0;
                    std::unordered_map<uint32_t, double> mergedWeights;
                    for (const auto &contribution : simplified.sourceContributions[logicalOutput])
                    {
                        if (contribution.first >= logicalSources.size() || contribution.second <= 0.0f)
                            continue;
                        sourceGlobalsForSubset(contribution.first, subsetIndex, attributeGlobals);
                        if (attributeGlobals.empty()) continue;
                        const double perSource = static_cast<double>(contribution.second) /
                                                 static_cast<double>(attributeGlobals.size());
                        for (const uint32_t globalIndex : attributeGlobals)
                        {
                            if (sourceNormals)
                            {
                                blendedNormal.x += sourceNormals[globalIndex].x * static_cast<float>(perSource);
                                blendedNormal.y += sourceNormals[globalIndex].y * static_cast<float>(perSource);
                                blendedNormal.z += sourceNormals[globalIndex].z * static_cast<float>(perSource);
                            }
                            if (sourceUvs)
                            {
                                blendedUv.x += sourceUvs[globalIndex].x * static_cast<float>(perSource);
                                blendedUv.y += sourceUvs[globalIndex].y * static_cast<float>(perSource);
                            }
                            if (hasCanonicalWeights)
                            {
                                const skeletal::CANONICAL_VERTEX_WEIGHT &sourceWeight =
                                    impl->canonicalWeights.vertices[globalIndex];
                                for (uint32_t slot = 0; slot < 4; ++slot)
                                    if (sourceWeight.weight[slot] > 0.0f)
                                        mergedWeights[sourceWeight.paletteIndex[slot]] +=
                                            static_cast<double>(sourceWeight.weight[slot]) * perSource;
                            }
                        }
                        attributeTotal += contribution.second;
                    }
                    if (!(attributeTotal > 0.0) || !std::isfinite(attributeTotal))
                        return fail("collapsed vertex has no valid source attributes");
                    if (sourceNormals)
                    {
                        const float length = std::sqrt(blendedNormal.x * blendedNormal.x +
                                                       blendedNormal.y * blendedNormal.y +
                                                       blendedNormal.z * blendedNormal.z);
                        if (length > 1.0e-8f)
                        {
                            blendedNormal.x /= length;
                            blendedNormal.y /= length;
                            blendedNormal.z /= length;
                        }
                        normals.push_back(blendedNormal);
                    }
                    if (sourceUvs)
                    {
                        blendedUv.x /= static_cast<float>(attributeTotal);
                        blendedUv.y /= static_cast<float>(attributeTotal);
                        uvs.push_back(blendedUv);
                    }
                    if (hasCanonicalWeights)
                    {
                        std::vector<std::pair<uint32_t, double>> ranked(mergedWeights.begin(), mergedWeights.end());
                        std::sort(ranked.begin(), ranked.end(), [](const auto &left, const auto &right)
                        {
                            return left.second > right.second ||
                                   (left.second == right.second && left.first < right.first);
                        });
                        if (ranked.size() > 4) ranked.resize(4);
                        double total = 0.0;
                        for (const auto &entry : ranked) total += entry.second;
                        if (!(total > 0.0) || !std::isfinite(total))
                            return fail("collapsed vertex has no valid canonical skin influence");
                        skeletal::CANONICAL_VERTEX_WEIGHT outputWeight = {};
                        for (uint32_t slot = 0; slot < ranked.size(); ++slot)
                        {
                            outputWeight.paletteIndex[slot] = ranked[slot].first;
                            outputWeight.weight[slot] = static_cast<float>(ranked[slot].second / total);
                        }
                        weights.push_back(outputWeight);
                    }
                }
                indices.push_back(static_cast<uint16_t>(physical->second));
            }
            range.vertexCount = static_cast<int>(positions.size()) - range.vertexStart;
            range.indexCount = static_cast<int>(indices.size()) - range.indexStart;
        }
        report.resultTriangleCount = static_cast<uint32_t>(indices.size() / 3);

        const float boundsScale = std::max({sourceMaximum.x - sourceMinimum.x,
                                            sourceMaximum.y - sourceMinimum.y,
                                            sourceMaximum.z - sourceMinimum.z, 1.0f});
        const float boundsTolerance = boundsScale * 1.0e-5f;
        auto boundsViolation = [](const std::vector<VEC3> &candidatePositions,
                                  const VEC3 &minimum, const VEC3 &maximum,
                                  const float tolerance, const size_t frameIndex,
                                  const bool shared, std::string &error)
        {
            float worstViolation = 0.0f;
            float worstValue = 0.0f;
            float worstMinimum = 0.0f;
            float worstMaximum = 0.0f;
            float worstExcess = 0.0f;
            size_t worstVertex = 0;
            char worstAxis = 'X';
            for (size_t vertex = 0; vertex < candidatePositions.size(); ++vertex)
            {
                const VEC3 &position = candidatePositions[vertex];
                const float values[3] = {position.x, position.y, position.z};
                const float minima[3] = {minimum.x, minimum.y, minimum.z};
                const float maxima[3] = {maximum.x, maximum.y, maximum.z};
                for (size_t axis = 0; axis < 3; ++axis)
                {
                    if (!std::isfinite(values[axis]))
                    {
                        char message[192] = {};
                        snprintf(message, sizeof(message),
                            "simplify non-finite vertex: shared=%d frame=%zu vertex=%zu axis=%c",
                            shared ? 1 : 0, frameIndex + 1, vertex + 1, "XYZ"[axis]);
                        error.assign(message);
                        return true;
                    }
                    const float excess = values[axis] < minima[axis]
                        ? minima[axis] - values[axis]
                        : (values[axis] > maxima[axis] ? values[axis] - maxima[axis] : 0.0f);
                    const float violation = excess - tolerance;
                    if (violation <= worstViolation) continue;
                    worstViolation = violation;
                    worstValue = values[axis];
                    worstMinimum = minima[axis];
                    worstMaximum = maxima[axis];
                    worstExcess = excess;
                    worstVertex = vertex;
                    worstAxis = "XYZ"[axis];
                }
            }
            if (!(worstViolation > 0.0f)) return false;
            char message[384] = {};
            snprintf(message, sizeof(message),
                "simplify bounds violation: shared=%d frame=%zu vertex=%zu axis=%c value=%.9g min=%.9g max=%.9g excess=%.9g tolerance=%.9g",
                shared ? 1 : 0, frameIndex + 1, worstVertex + 1, worstAxis,
                worstValue, worstMinimum, worstMaximum, worstExcess, tolerance);
            error.assign(message);
            return true;
        };
        std::string boundsError;
        if (boundsViolation(positions, sourceMinimum, sourceMaximum, boundsTolerance,
                            static_cast<size_t>(referenceFrameIndex), false, boundsError))
            return fail(boundsError);

        if (hasCanonicalWeights)
        {
            simplifiedWeights = impl->canonicalWeights;
            simplifiedWeights.vertices = weights;
            if (!skeletal::validateCanonicalWeights(impl->canonicalSkeleton, simplifiedWeights,
                                                    static_cast<uint32_t>(positions.size())))
                return fail("simplified canonical skin weights failed validation");
        }

        struct SHARED_FRAME_RESULT
        {
            std::unique_ptr<float[]> positions;
            std::unique_ptr<float[]> normals;
            std::unique_ptr<float[]> uvs;
            std::unique_ptr<uint16_t[]> indices;
            std::vector<SUBSET_RANGE> ranges;
            size_t vertexCount = 0;
            size_t indexCount = 0;
        };
        std::vector<SHARED_FRAME_RESULT> sharedResults;
        if (simplifyAllFrames)
        {
            if (simplified.deformationDeltas.size() + 1 != impl->buffer.size())
                return fail("shared simplification returned incomplete frame deformation data");
            sharedResults.reserve(impl->buffer.size() - 1);
            for (size_t frameIndex = 1; frameIndex < impl->buffer.size(); ++frameIndex)
            {
                util::BUFFER_MESH_DEBUG *sharedFrame = impl->buffer[frameIndex];
                const auto *sharedPositions = reinterpret_cast<const VEC3 *>(sharedFrame->position);
                const auto *sharedNormals = reinterpret_cast<const VEC3 *>(sharedFrame->normal);
                const auto *sharedUvs = reinterpret_cast<const VEC2 *>(sharedFrame->uv);
                const std::vector<VEC3> &frameDeltas = simplified.deformationDeltas[frameIndex - 1];
                if (frameDeltas.size() != simplified.positions.size())
                    return fail("shared simplification returned an invalid frame deformation count");

                std::vector<VEC3> framePositions;
                std::vector<VEC3> frameNormals;
                std::vector<VEC2> frameUvs;
                std::vector<uint16_t> frameIndices;
                std::vector<SUBSET_RANGE> frameRanges(sharedFrame->subset.size());
                std::vector<uint32_t> frameAttributeGlobals;
                for (uint32_t subsetIndex = 0; subsetIndex < subsetLogicalIndices.size(); ++subsetIndex)
                {
                    const std::vector<uint32_t> &groupIndices = subsetLogicalIndices[subsetIndex];
                    SUBSET_RANGE &range = frameRanges[subsetIndex];
                    range.vertexStart = static_cast<int>(framePositions.size());
                    range.indexStart = static_cast<int>(frameIndices.size());
                    if (targetSubsetIndex >= 0 && subsetIndex != static_cast<uint32_t>(targetSubsetIndex))
                    {
                        const util::SUBSET_DEBUG *sourceSubset = sharedFrame->subset[subsetIndex];
                        std::unordered_map<uint32_t, uint32_t> copiedByGlobal;
                        copiedByGlobal.reserve(static_cast<size_t>(sourceSubset->vertexCount));
                        const int sourceElementCount = sourceIndexed
                            ? sourceSubset->indexCount : sourceSubset->vertexCount;
                        for (int i = 0; i < sourceElementCount; ++i)
                        {
                            const uint32_t globalIndex = sourceIndexed
                                ? sharedFrame->indexBuffer[sourceSubset->indexStart + i]
                                : static_cast<uint32_t>(sourceSubset->vertexStart + i);
                            auto copied = copiedByGlobal.find(globalIndex);
                            if (copied == copiedByGlobal.end())
                            {
                                if (framePositions.size() >= UINT16_MAX)
                                    return fail("shared simplified frame exceeds the uint16 vertex-index limit");
                                const uint32_t outputIndex = static_cast<uint32_t>(framePositions.size());
                                copied = copiedByGlobal.emplace(globalIndex, outputIndex).first;
                                framePositions.push_back(sharedPositions[globalIndex]);
                                if (sharedNormals) frameNormals.push_back(sharedNormals[globalIndex]);
                                if (sharedUvs) frameUvs.push_back(sharedUvs[globalIndex]);
                            }
                            frameIndices.push_back(static_cast<uint16_t>(copied->second));
                        }
                        range.vertexCount = static_cast<int>(framePositions.size()) - range.vertexStart;
                        range.indexCount = static_cast<int>(frameIndices.size()) - range.indexStart;
                        continue;
                    }

                    std::unordered_map<uint32_t, uint32_t> physicalByLogical;
                    physicalByLogical.reserve(groupIndices.size());
                    for (const uint32_t logicalOutput : groupIndices)
                    {
                        auto physical = physicalByLogical.find(logicalOutput);
                        if (physical == physicalByLogical.end())
                        {
                            if (logicalOutput >= frameDeltas.size() || framePositions.size() >= UINT16_MAX)
                                return fail("shared simplification returned an invalid vertex index");
                            const uint32_t physicalIndex = static_cast<uint32_t>(framePositions.size());
                            physical = physicalByLogical.emplace(logicalOutput, physicalIndex).first;
                            framePositions.push_back(simplified.positions[logicalOutput] + frameDeltas[logicalOutput]);

                            VEC3 blendedNormal(0.0f, 0.0f, 0.0f);
                            VEC2 blendedUv(0.0f, 0.0f);
                            double attributeTotal = 0.0;
                            for (const auto &contribution : simplified.sourceContributions[logicalOutput])
                            {
                                if (contribution.first >= logicalSources.size() || contribution.second <= 0.0f)
                                    continue;
                                sourceGlobalsForSubset(contribution.first, subsetIndex, frameAttributeGlobals);
                                if (frameAttributeGlobals.empty()) continue;
                                const double perSource = static_cast<double>(contribution.second) /
                                    static_cast<double>(frameAttributeGlobals.size());
                                for (const uint32_t globalIndex : frameAttributeGlobals)
                                {
                                    if (sharedNormals)
                                    {
                                        blendedNormal.x += sharedNormals[globalIndex].x * static_cast<float>(perSource);
                                        blendedNormal.y += sharedNormals[globalIndex].y * static_cast<float>(perSource);
                                        blendedNormal.z += sharedNormals[globalIndex].z * static_cast<float>(perSource);
                                    }
                                    if (sharedUvs)
                                    {
                                        blendedUv.x += sharedUvs[globalIndex].x * static_cast<float>(perSource);
                                        blendedUv.y += sharedUvs[globalIndex].y * static_cast<float>(perSource);
                                    }
                                }
                                attributeTotal += contribution.second;
                            }
                            if (!(attributeTotal > 0.0) || !std::isfinite(attributeTotal))
                                return fail("shared collapsed vertex has no valid source attributes");
                            if (sharedNormals)
                            {
                                const float length = std::sqrt(blendedNormal.x * blendedNormal.x +
                                    blendedNormal.y * blendedNormal.y + blendedNormal.z * blendedNormal.z);
                                if (length > 1.0e-8f) blendedNormal = blendedNormal * (1.0f / length);
                                frameNormals.push_back(blendedNormal);
                            }
                            if (sharedUvs)
                            {
                                blendedUv.x /= static_cast<float>(attributeTotal);
                                blendedUv.y /= static_cast<float>(attributeTotal);
                                frameUvs.push_back(blendedUv);
                            }
                        }
                        frameIndices.push_back(static_cast<uint16_t>(physical->second));
                    }
                    range.vertexCount = static_cast<int>(framePositions.size()) - range.vertexStart;
                    range.indexCount = static_cast<int>(frameIndices.size()) - range.indexStart;
                }

                VEC3 frameMinimum = sharedPositions[0];
                VEC3 frameMaximum = sharedPositions[0];
                for (uint32_t vertex = 1; vertex < report.sourceVertexCount; ++vertex)
                {
                    const VEC3 &position = sharedPositions[vertex];
                    frameMinimum.x = std::min(frameMinimum.x, position.x);
                    frameMinimum.y = std::min(frameMinimum.y, position.y);
                    frameMinimum.z = std::min(frameMinimum.z, position.z);
                    frameMaximum.x = std::max(frameMaximum.x, position.x);
                    frameMaximum.y = std::max(frameMaximum.y, position.y);
                    frameMaximum.z = std::max(frameMaximum.z, position.z);
                }
                const float frameScale = std::max({frameMaximum.x - frameMinimum.x,
                    frameMaximum.y - frameMinimum.y, frameMaximum.z - frameMinimum.z, 1.0f});
                const float frameTolerance = frameScale * 1.0e-5f;
                boundsError.clear();
                if (boundsViolation(framePositions, frameMinimum, frameMaximum, frameTolerance,
                                    frameIndex, true, boundsError))
                    return fail(boundsError);

                SHARED_FRAME_RESULT result;
                result.vertexCount = framePositions.size();
                result.indexCount = frameIndices.size();
                result.ranges = std::move(frameRanges);
                result.positions = std::make_unique<float[]>(framePositions.size() * 3);
                memcpy(result.positions.get(), framePositions.data(), framePositions.size() * sizeof(VEC3));
                if (sharedNormals)
                {
                    result.normals = std::make_unique<float[]>(frameNormals.size() * 3);
                    memcpy(result.normals.get(), frameNormals.data(), frameNormals.size() * sizeof(VEC3));
                }
                if (sharedUvs)
                {
                    result.uvs = std::make_unique<float[]>(frameUvs.size() * 2);
                    memcpy(result.uvs.get(), frameUvs.data(), frameUvs.size() * sizeof(VEC2));
                }
                result.indices = std::make_unique<uint16_t[]>(frameIndices.size());
                memcpy(result.indices.get(), frameIndices.data(), frameIndices.size() * sizeof(uint16_t));
                sharedResults.push_back(std::move(result));
            }
        }

        std::unique_ptr<float[]> newPositions(new float[positions.size() * 3]);
        std::unique_ptr<float[]> newNormals(sourceNormals ? new float[normals.size() * 3] : nullptr);
        std::unique_ptr<float[]> newUvs(sourceUvs ? new float[uvs.size() * 2] : nullptr);
        std::unique_ptr<uint16_t[]> newIndices(new uint16_t[indices.size()]);
        memcpy(newPositions.get(), positions.data(), positions.size() * sizeof(VEC3));
        if (newNormals) memcpy(newNormals.get(), normals.data(), normals.size() * sizeof(VEC3));
        if (newUvs) memcpy(newUvs.get(), uvs.data(), uvs.size() * sizeof(VEC2));
        memcpy(newIndices.get(), indices.data(), indices.size() * sizeof(uint16_t));

        delete[] frame->position;
        delete[] frame->normal;
        delete[] frame->uv;
        delete[] frame->indexBuffer;
        frame->position = newPositions.release();
        frame->normal = newNormals.release();
        frame->uv = newUvs.release();
        frame->indexBuffer = newIndices.release();
        frame->headerFrame.sizeVertexBuffer = static_cast<int>(positions.size());
        frame->headerFrame.sizeIndexBuffer = static_cast<int>(indices.size());
        report.resultVertexCount = static_cast<uint32_t>(positions.size());
        for (size_t i = 0; i < results.size(); ++i)
        {
            util::SUBSET_DEBUG *subset = frame->subset[i];
            subset->vertexStart = results[i].vertexStart;
            subset->indexStart = results[i].indexStart;
            subset->vertexCount = results[i].vertexCount;
            subset->indexCount = results[i].indexCount;
        }
        for (size_t resultIndex = 0; resultIndex < sharedResults.size(); ++resultIndex)
        {
            util::BUFFER_MESH_DEBUG *sharedFrame = impl->buffer[resultIndex + 1];
            SHARED_FRAME_RESULT &result = sharedResults[resultIndex];
            delete[] sharedFrame->position;
            delete[] sharedFrame->normal;
            delete[] sharedFrame->uv;
            delete[] sharedFrame->indexBuffer;
            sharedFrame->position = result.positions.release();
            sharedFrame->normal = result.normals.release();
            sharedFrame->uv = result.uvs.release();
            sharedFrame->indexBuffer = result.indices.release();
            sharedFrame->headerFrame.sizeVertexBuffer = static_cast<int>(result.vertexCount);
            sharedFrame->headerFrame.sizeIndexBuffer = static_cast<int>(result.indexCount);
            for (size_t subsetIndex = 0; subsetIndex < result.ranges.size(); ++subsetIndex)
            {
                util::SUBSET_DEBUG *subset = sharedFrame->subset[subsetIndex];
                subset->vertexStart = result.ranges[subsetIndex].vertexStart;
                subset->indexStart = result.ranges[subsetIndex].indexStart;
                subset->vertexCount = result.ranges[subsetIndex].vertexCount;
                subset->indexCount = result.ranges[subsetIndex].indexCount;
            }
        }
        report.maximumGeometricError = simplified.maximumError;
        if (simplifyAllFrames) report.maximumFrameError = simplified.maximumPoseError;
        else report.maximumPoseError = simplified.maximumPoseError;
        report.maximumRelativeError = simplified.maximumRelativeError;
        report.collapseCount = simplified.collapseCount;
        report.boundaryRejectedCollapseCount = simplified.boundaryRejectedCollapseCount;
        report.topologyRejectedCollapseCount = simplified.topologyRejectedCollapseCount;
        report.orientationRejectedCollapseCount = simplified.orientationRejectedCollapseCount;
        report.invalidRejectedCollapseCount = simplified.invalidRejectedCollapseCount;
        report.degenerateTriangleCount = simplified.degenerateTriangleCount;
        report.nonManifoldEdgeCount = simplified.nonManifoldEdgeCount;
        report.connectedComponentCount = simplified.connectedComponentCount;
        report.detailPenalizedCandidateCount = simplified.detailPenalizedCandidateCount;
        report.detailPenalizedCollapseCount = simplified.detailPenalizedCollapseCount;
        report.clearanceRejectedCollapseCount = simplified.clearanceRejectedCollapseCount;
        if (hasCanonicalWeights)
            impl->canonicalWeights = std::move(simplifiedWeights);
        return true;
    }

    bool MESH_MBM_DEBUG::startSimplify(const float targetTriangleRatio,
                                       const int targetSubsetIndex,
                                       const int targetFrameIndex,
                                       const bool preserveDetails)
    {
        if (impl->simplifyState.load(std::memory_order_acquire) == MESH_SIMPLIFY_STATE::RUNNING)
            return false;
        if (impl->simplifyWorker.joinable())
            impl->simplifyWorker.join();
        impl->simplifyReport = {};
        impl->simplifyError.clear();
        impl->simplifyProgress = 0.0f;
        impl->simplifyState.store(MESH_SIMPLIFY_STATE::RUNNING, std::memory_order_release);
        try
        {
            impl->simplifyWorker = std::thread([this, targetTriangleRatio, targetSubsetIndex,
                                                targetFrameIndex, preserveDetails]()
            {
                char errorOut[255] = "";
                const bool success = simplify(targetTriangleRatio, impl->simplifyReport,
                    errorOut, static_cast<int>(sizeof(errorOut)), targetSubsetIndex,
                    targetFrameIndex, preserveDetails);
                if (!success) impl->simplifyError = errorOut;
                impl->simplifyState.store(success ? MESH_SIMPLIFY_STATE::SUCCEEDED
                                                  : MESH_SIMPLIFY_STATE::FAILED,
                                          std::memory_order_release);
            });
        }
        catch (...)
        {
            impl->simplifyError = "failed to start simplification worker";
            impl->simplifyState.store(MESH_SIMPLIFY_STATE::FAILED, std::memory_order_release);
            return false;
        }
        return true;
    }

    MESH_SIMPLIFY_STATE MESH_MBM_DEBUG::getSimplifyState(float &progress) noexcept
    {
        progress = std::max(0.0f, std::min(1.0f, impl->simplifyProgress.load()));
        return impl->simplifyState.load(std::memory_order_acquire);
    }

    bool MESH_MBM_DEBUG::getSimplifyResult(MESH_SIMPLIFY_REPORT &report,
                                           char *errorOut, const int errorOutLen)
    {
        const MESH_SIMPLIFY_STATE state = impl->simplifyState.load(std::memory_order_acquire);
        if (state == MESH_SIMPLIFY_STATE::RUNNING || state == MESH_SIMPLIFY_STATE::IDLE)
            return false;
        if (impl->simplifyWorker.joinable())
            impl->simplifyWorker.join();
        report = impl->simplifyReport;
        if (errorOut && errorOutLen > 0)
            snprintf(errorOut, static_cast<size_t>(errorOutLen), "%s", impl->simplifyError.c_str());
        return state == MESH_SIMPLIFY_STATE::SUCCEEDED;
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
        // Canonical type-42 weights are stored in the same frame-global vertex order as the
        // geometry. Keep that contract intact when an editor compacts the weighted frame (for
        // example Mesh Debug's subset-filter preview). Refuse an already-inconsistent mutation
        // instead of turning a recoverable in-memory problem into an unsavable mesh.
        if (this->impl->canonicalWeights.skeletonId != 0 &&
            this->impl->canonicalWeights.frameIndex == indexFrame)
        {
            const size_t weightStart = static_cast<size_t>(vStart);
            const size_t weightCount = static_cast<size_t>(vCount);
            if (weightStart > this->impl->canonicalWeights.vertices.size() ||
                weightCount > this->impl->canonicalWeights.vertices.size() - weightStart)
                return;
            this->impl->canonicalWeights.vertices.erase(
                this->impl->canonicalWeights.vertices.begin() + static_cast<ptrdiff_t>(weightStart),
                this->impl->canonicalWeights.vertices.begin() +
                    static_cast<ptrdiff_t>(weightStart + weightCount));
        }
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

    bool MESH_MBM_DEBUG::moveSubsetUp(uint32_t indexFrame, uint32_t indexSubset)
    {
        if (indexFrame >= static_cast<uint32_t>(this->impl->buffer.size()) || indexSubset == 0)
            return false;
        util::BUFFER_MESH_DEBUG *buf = this->impl->buffer[indexFrame];
        if (!buf || indexSubset >= static_cast<uint32_t>(buf->subset.size()))
            return false;

        const uint32_t previousSubsetIndex = indexSubset - 1;
        std::swap(buf->subset[previousSubsetIndex], buf->subset[indexSubset]);

        // Articulated tracks and hierarchy target stable part IDs. Keep each Part attached to
        // the same geometry by remapping only the two subset occurrences whose order changed.
        for (auto &part : this->impl->articulatedParts)
        {
            if (part.frameIndex != indexFrame)
                continue;
            if (part.subsetIndex == indexSubset)
                part.subsetIndex = previousSubsetIndex;
            else if (part.subsetIndex == previousSubsetIndex)
                part.subsetIndex = indexSubset;
        }
        return true;
    }

    bool MESH_MBM_DEBUG::mergeSubsets(uint32_t indexFrame, const std::vector<uint32_t> &subsetIndices)
    {
        if (indexFrame >= static_cast<uint32_t>(this->impl->buffer.size()) || subsetIndices.size() < 2)
            return false;
        util::BUFFER_MESH_DEBUG *buf = this->impl->buffer[indexFrame];
        if (!buf || buf->subset.size() < 2)
            return false;

        std::vector<bool> selected(buf->subset.size(), false);
        uint32_t firstSelected = UINT32_MAX;
        size_t selectedCount = 0;
        for (const uint32_t subsetIndex : subsetIndices)
        {
            if (subsetIndex >= buf->subset.size())
                return false;
            if (!selected[subsetIndex])
            {
                selected[subsetIndex] = true;
                firstSelected = std::min(firstSelected, subsetIndex);
                ++selectedCount;
            }
        }
        if (selectedCount < 2)
            return false;

        const int stride = buf->headerFrame.stride;
        int totalVertices = 0;
        int totalIndices = 0;
        for (const util::SUBSET_DEBUG *subset : buf->subset)
        {
            if (!subset || subset->vertexStart < 0 || subset->vertexCount < 0 ||
                subset->indexStart < 0 || subset->indexCount < 0)
                return false;
            totalVertices = std::max(totalVertices, subset->vertexStart + subset->vertexCount);
            totalIndices = std::max(totalIndices, subset->indexStart + subset->indexCount);
        }
        if (stride <= 0 || totalVertices < 0 || totalIndices < 0 || !buf->position)
            return false;

        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> uvs;
        std::vector<uint16_t> indices;
        positions.reserve(static_cast<size_t>(totalVertices * stride));
        if (buf->normal) normals.reserve(static_cast<size_t>(totalVertices * 3));
        if (buf->uv) uvs.reserve(static_cast<size_t>(totalVertices * 2));
        if (buf->indexBuffer) indices.reserve(static_cast<size_t>(totalIndices));

        const bool reorderWeights = this->impl->canonicalWeights.skeletonId != 0 &&
                                    this->impl->canonicalWeights.frameIndex == indexFrame;
        std::vector<skeletal::CANONICAL_VERTEX_WEIGHT> reorderedWeights;
        if (reorderWeights)
        {
            if (this->impl->canonicalWeights.vertices.size() != static_cast<size_t>(totalVertices))
                return false;
            reorderedWeights.reserve(static_cast<size_t>(totalVertices));
        }

        std::vector<util::SUBSET_DEBUG *> newSubsets;
        std::vector<uint32_t> oldToNew(buf->subset.size(), UINT32_MAX);
        newSubsets.reserve(buf->subset.size() - selectedCount + 1);

        auto appendSubsetGeometry = [&](const uint32_t oldIndex, util::SUBSET_DEBUG *target) -> bool {
            const util::SUBSET_DEBUG *source = buf->subset[oldIndex];
            if (!source || source->vertexStart < 0 || source->vertexCount < 0 ||
                source->vertexStart + source->vertexCount > totalVertices ||
                source->indexStart < 0 || source->indexCount < 0 ||
                source->indexStart + source->indexCount > totalIndices)
                return false;
            const int newVertexStart = target->vertexStart + target->vertexCount;
            positions.insert(positions.end(), buf->position + source->vertexStart * stride,
                             buf->position + (source->vertexStart + source->vertexCount) * stride);
            if (buf->normal)
                normals.insert(normals.end(), buf->normal + source->vertexStart * 3,
                               buf->normal + (source->vertexStart + source->vertexCount) * 3);
            if (buf->uv)
                uvs.insert(uvs.end(), buf->uv + source->vertexStart * 2,
                           buf->uv + (source->vertexStart + source->vertexCount) * 2);
            if (reorderWeights)
                reorderedWeights.insert(reorderedWeights.end(),
                    this->impl->canonicalWeights.vertices.begin() + source->vertexStart,
                    this->impl->canonicalWeights.vertices.begin() + source->vertexStart + source->vertexCount);
            if (buf->indexBuffer)
            {
                for (int i = 0; i < source->indexCount; ++i)
                {
                    const int localIndex = static_cast<int>(buf->indexBuffer[source->indexStart + i]) - source->vertexStart;
                    if (localIndex < 0 || localIndex >= source->vertexCount)
                        return false;
                    indices.push_back(static_cast<uint16_t>(newVertexStart + localIndex));
                }
            }
            target->vertexCount += source->vertexCount;
            target->indexCount += source->indexCount;
            return true;
        };

        bool valid = true;
        for (uint32_t oldIndex = 0; oldIndex < buf->subset.size() && valid; ++oldIndex)
        {
            if (selected[oldIndex] && oldIndex != firstSelected)
                continue;
            auto *target = new util::SUBSET_DEBUG();
            *target = *buf->subset[oldIndex];
            target->vertexStart = static_cast<int>(positions.size() / static_cast<size_t>(stride));
            target->vertexCount = 0;
            target->indexStart = static_cast<int>(indices.size());
            target->indexCount = 0;
            const uint32_t newIndex = static_cast<uint32_t>(newSubsets.size());
            newSubsets.push_back(target);
            if (oldIndex == firstSelected)
            {
                for (uint32_t selectedIndex = 0; selectedIndex < selected.size() && valid; ++selectedIndex)
                {
                    if (selected[selectedIndex])
                    {
                        oldToNew[selectedIndex] = newIndex;
                        valid = appendSubsetGeometry(selectedIndex, target);
                    }
                }
            }
            else
            {
                oldToNew[oldIndex] = newIndex;
                valid = appendSubsetGeometry(oldIndex, target);
            }
        }
        if (!valid || positions.size() != static_cast<size_t>(totalVertices * stride) ||
            (buf->indexBuffer && indices.size() != static_cast<size_t>(totalIndices)))
        {
            for (util::SUBSET_DEBUG *subset : newSubsets) delete subset;
            return false;
        }

        auto newPositions = std::make_unique<float[]>(positions.size());
        std::copy(positions.begin(), positions.end(), newPositions.get());
        std::unique_ptr<float[]> newNormals;
        if (buf->normal)
        {
            newNormals = std::make_unique<float[]>(normals.size());
            std::copy(normals.begin(), normals.end(), newNormals.get());
        }
        std::unique_ptr<float[]> newUvs;
        if (buf->uv)
        {
            newUvs = std::make_unique<float[]>(uvs.size());
            std::copy(uvs.begin(), uvs.end(), newUvs.get());
        }
        std::unique_ptr<uint16_t[]> newIndices;
        if (buf->indexBuffer)
        {
            newIndices = std::make_unique<uint16_t[]>(indices.size());
            std::copy(indices.begin(), indices.end(), newIndices.get());
        }

        delete[] buf->position;
        delete[] buf->normal;
        delete[] buf->uv;
        delete[] buf->indexBuffer;
        for (util::SUBSET_DEBUG *subset : buf->subset) delete subset;
        buf->position = newPositions.release();
        buf->normal = newNormals.release();
        buf->uv = newUvs.release();
        buf->indexBuffer = newIndices.release();
        buf->subset = std::move(newSubsets);
        buf->headerFrame.totalSubset = static_cast<int>(buf->subset.size());
        buf->headerFrame.sizeVertexBuffer = totalVertices;
        buf->headerFrame.sizeIndexBuffer = totalIndices;
        if (reorderWeights)
            this->impl->canonicalWeights.vertices = std::move(reorderedWeights);

        for (auto &part : this->impl->articulatedParts)
        {
            if (part.frameIndex == indexFrame && part.subsetIndex < oldToNew.size())
                part.subsetIndex = oldToNew[part.subsetIndex];
        }
        return true;
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
        const bool copyCanonicalWeights = this->impl->canonicalWeights.skeletonId != 0 &&
            this->impl->canonicalWeights.frameIndex == targetFrame;
        std::vector<skeletal::CANONICAL_VERTEX_WEIGHT> sourceWeights;
        if (copyCanonicalWeights)
        {
            if (src.impl->canonicalWeights.skeletonId == 0 ||
                src.impl->canonicalWeights.frameIndex != srcFrame ||
                this->impl->canonicalWeights.skeletonId != src.impl->canonicalWeights.skeletonId ||
                this->impl->canonicalWeights.paletteBoneIds != src.impl->canonicalWeights.paletteBoneIds ||
                this->impl->canonicalWeights.vertices.size() != static_cast<size_t>(tgtOldV) ||
                src.impl->canonicalWeights.vertices.size() !=
                    static_cast<size_t>(srcBuf->headerFrame.sizeVertexBuffer) ||
                srcVStart < 0 || srcVCount < 0 ||
                static_cast<size_t>(srcVStart + srcVCount) > src.impl->canonicalWeights.vertices.size())
                return 0;
            sourceWeights.insert(sourceWeights.end(),
                src.impl->canonicalWeights.vertices.begin() + srcVStart,
                src.impl->canonicalWeights.vertices.begin() + srcVStart + srcVCount);
        }
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
        if (copyCanonicalWeights)
            this->impl->canonicalWeights.vertices.insert(
                this->impl->canonicalWeights.vertices.end(), sourceWeights.begin(), sourceWeights.end());
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
    
    bool MESH_MBM_DEBUG::saveV11(const char *fileOut, const bool recalculateNormal, const bool recalculateUV, const bool compress, char *errorOut,const int lenErrorOut)
    {
        if (this->impl->buffer.size() == 0)
            return false;

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

        const bool hasCanonicalSkeleton = impl->canonicalSkeleton.skeletonId != 0;
        const bool hasCanonicalWeights = impl->canonicalWeights.skeletonId != 0;
        const bool hasCanonicalAnimations = impl->canonicalAnimations.skeletonId != 0;
        if (hasCanonicalWeights || hasCanonicalAnimations)
        {
            if (!hasCanonicalSkeleton)
                return log_util::onFailed(file, __FILE__, __LINE__,
                                          "canonical weights or animations require a canonical skeleton [%s]", fileOut);
        }
        if (hasCanonicalSkeleton)
        {
            skeletal::COMPILED_SKELETON compiled;
            if (!skeletal::compileCanonicalSkeleton(impl->canonicalSkeleton.sourceBones, compiled))
                return log_util::onFailed(file, __FILE__, __LINE__, "invalid canonical skeleton [%s]", fileOut);

            skeletal::CANONICAL_SKELETON skeleton = impl->canonicalSkeleton;
            skeleton.compiled = std::move(compiled);
            if (hasCanonicalWeights)
            {
                if (impl->canonicalWeights.frameIndex >= impl->buffer.size())
                    return log_util::onFailed(file, __FILE__, __LINE__, "canonical weight frame is out of range [%s]", fileOut);
                const util::BUFFER_MESH_DEBUG *frame = impl->buffer[impl->canonicalWeights.frameIndex];
                uint32_t vertexCount = 0;
                for (const util::SUBSET_DEBUG *subset : frame->subset)
                    vertexCount += static_cast<uint32_t>(subset->vertexCount);
                if (!skeletal::validateCanonicalWeights(skeleton, impl->canonicalWeights, vertexCount))
                    return log_util::onFailed(file, __FILE__, __LINE__, "invalid canonical weights [%s]", fileOut);
            }
            if (hasCanonicalAnimations &&
                !skeletal::validateCanonicalAnimations(skeleton, impl->canonicalAnimations))
                return log_util::onFailed(file, __FILE__, __LINE__, "invalid canonical animations [%s]", fileOut);
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
        fileHeader.backBufferWidth  = impl->backBufferWidth;
        fileHeader.backBufferHeight = impl->backBufferHeight;
        fileHeader.sectionCount     = 1u /*material*/ + 1u /*physics*/ + (ls_paths.empty() ? 0u : 1u)
                                     + (hasCanonicalSkeleton ? 1u : 0u)
                                     + (hasCanonicalWeights ? 1u : 0u)
                                     + (hasCanonicalAnimations ? 1u : 0u)
                                     + (impl->articulatedParts.empty() ? 0u : 1u)
                                     + (impl->articulatedClips.empty() ? 0u : 1u)
                                     + static_cast<uint32_t>(impl->headerMesh.totalFrames)
                                     + this->getTotalAnimationHeaders()
                                     + ((impl->typeMe == util::TYPE_MESH_PARTICLE) ? 1u : 0u)
                                     + ((impl->typeMe == util::TYPE_MESH_FONT) ? 1u : 0u)
                                     + ((impl->typeMe == util::TYPE_MESH_TILE_MAP) ? 1u : 0u);
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

        // Canonical skeletal sections are emitted only from canonical data already loaded/imported.
        // Legacy editor-only joints and name palettes are deliberately not converted here.
        if (hasCanonicalSkeleton)
        {
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_SKELETAL_SKELETON;
            sectionHeader.sectionVersion = 3;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [this](FILE *fp)
            {
                const skeletal::CANONICAL_SKELETON &skeleton = this->impl->canonicalSkeleton;
                if (!util::le_io::writeU64LE(fp, skeleton.skeletonId) ||
                    !util::le_io::writeU32LE(fp, static_cast<uint32_t>(skeleton.sourceBones.size())))
                    return false;
                for (const skeletal::CANONICAL_BONE &bone : skeleton.sourceBones)
                {
                    const skeletal::LOCAL_TRANSFORM &local = bone.localBind;
                    if (!util::le_io::writeU64LE(fp, bone.boneId) ||
                        !util::le_io::writeU64LE(fp, bone.parentBoneId) ||
                        !util::writeStringV11(fp, bone.name) ||
                        !util::le_io::writeF32LE(fp, local.translation.x) ||
                        !util::le_io::writeF32LE(fp, local.translation.y) ||
                        !util::le_io::writeF32LE(fp, local.translation.z) ||
                        !util::le_io::writeF32LE(fp, local.rotation.x) ||
                        !util::le_io::writeF32LE(fp, local.rotation.y) ||
                        !util::le_io::writeF32LE(fp, local.rotation.z) ||
                        !util::le_io::writeF32LE(fp, local.rotation.w) ||
                        !util::le_io::writeF32LE(fp, local.scale.x) ||
                        !util::le_io::writeF32LE(fp, local.scale.y) ||
                        !util::le_io::writeF32LE(fp, local.scale.z) ||
                        !util::le_io::writeF32LE(fp, bone.radius) ||
                        !util::le_io::writeF32LE(fp, bone.length) ||
                        !util::le_io::writeF32LE(fp, bone.tailOffset.x) ||
                        !util::le_io::writeF32LE(fp, bone.tailOffset.y) ||
                        !util::le_io::writeF32LE(fp, bone.tailOffset.z))
                        return false;
                    const uint8_t explicitTail = bone.hasExplicitTail ? 1 : 0;
                    if (!util::le_io::writeBytes(fp, &explicitTail, sizeof(explicitTail))) return false;
                    const uint8_t connected = bone.connectedToParent ? 1 : 0;
                    if (!util::le_io::writeBytes(fp, &connected, sizeof(connected))) return false;
                }
                return true;
            });
            if (!ok)
                return log_util::onFailed(file, __FILE__, __LINE__, "failed to write SECTION_SKELETAL_SKELETON [%s]", fileOut);
        }

        if (hasCanonicalWeights)
        {
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_SKELETAL_WEIGHTS;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [this](FILE *fp)
            {
                const skeletal::CANONICAL_WEIGHTS &weights = this->impl->canonicalWeights;
                if (!util::le_io::writeU64LE(fp, weights.skeletonId) ||
                    !util::le_io::writeU32LE(fp, weights.frameIndex) ||
                    !util::le_io::writeU32LE(fp, static_cast<uint32_t>(weights.vertices.size())) ||
                    !util::le_io::writeU32LE(fp, static_cast<uint32_t>(weights.paletteBoneIds.size())))
                    return false;
                for (const uint64_t boneId : weights.paletteBoneIds)
                    if (!util::le_io::writeU64LE(fp, boneId))
                        return false;
                for (const skeletal::CANONICAL_VERTEX_WEIGHT &vertex : weights.vertices)
                {
                    for (uint8_t slot = 0; slot < 4; ++slot)
                        if (!util::le_io::writeU16LE(fp, vertex.paletteIndex[slot]))
                            return false;
                    for (uint8_t slot = 0; slot < 4; ++slot)
                        if (!util::le_io::writeF32LE(fp, vertex.weight[slot]))
                            return false;
                }
                return true;
            });
            if (!ok)
                return log_util::onFailed(file, __FILE__, __LINE__, "failed to write SECTION_SKELETAL_WEIGHTS [%s]", fileOut);
        }

        if (hasCanonicalAnimations)
        {
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_SKELETAL_ANIMATION;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [this](FILE *fp)
            {
                const skeletal::CANONICAL_ANIMATIONS &animations = this->impl->canonicalAnimations;
                const uint8_t reserved[3] = {0, 0, 0};
                if (!util::le_io::writeU64LE(fp, animations.skeletonId) ||
                    !util::le_io::writeU32LE(fp, static_cast<uint32_t>(animations.clips.size())))
                    return false;
                for (const skeletal::SKELETAL_CLIP &clip : animations.clips)
                {
                    const uint8_t loop = clip.loop ? 1 : 0;
                    if (!util::le_io::writeU64LE(fp, clip.clipId) || !util::writeStringV11(fp, clip.name) ||
                        !util::le_io::writeF32LE(fp, clip.duration) ||
                        !util::le_io::writeBytes(fp, &loop, 1) ||
                        !util::le_io::writeBytes(fp, reserved, sizeof(reserved)) ||
                        !util::le_io::writeU32LE(fp, static_cast<uint32_t>(clip.tracks.size())))
                        return false;
                    for (const skeletal::SKELETAL_TRACK &track : clip.tracks)
                    {
                        if (!util::le_io::writeU64LE(fp, track.boneId) ||
                            !util::le_io::writeBytes(fp, &track.channelMask, 1) ||
                            !util::le_io::writeBytes(fp, reserved, sizeof(reserved)) ||
                            !util::le_io::writeU32LE(fp, static_cast<uint32_t>(track.keys.size())))
                            return false;
                        for (const skeletal::SKELETAL_KEY &key : track.keys)
                        {
                            const uint8_t easing = static_cast<uint8_t>(key.easing);
                            const skeletal::LOCAL_TRANSFORM &local = key.local;
                            if (!util::le_io::writeF32LE(fp, key.time) ||
                                !util::le_io::writeF32LE(fp, local.translation.x) ||
                                !util::le_io::writeF32LE(fp, local.translation.y) ||
                                !util::le_io::writeF32LE(fp, local.translation.z) ||
                                !util::le_io::writeF32LE(fp, local.rotation.x) ||
                                !util::le_io::writeF32LE(fp, local.rotation.y) ||
                                !util::le_io::writeF32LE(fp, local.rotation.z) ||
                                !util::le_io::writeF32LE(fp, local.rotation.w) ||
                                !util::le_io::writeF32LE(fp, local.scale.x) ||
                                !util::le_io::writeF32LE(fp, local.scale.y) ||
                                !util::le_io::writeF32LE(fp, local.scale.z) ||
                                !util::le_io::writeBytes(fp, &easing, 1) ||
                                !util::le_io::writeBytes(fp, reserved, sizeof(reserved)) ||
                                !util::le_io::writeF32LE(fp, key.bezierX1) ||
                                !util::le_io::writeF32LE(fp, key.bezierY1) ||
                                !util::le_io::writeF32LE(fp, key.bezierX2) ||
                                !util::le_io::writeF32LE(fp, key.bezierY2))
                                return false;
                        }
                    }
                }
                return true;
            });
            if (!ok)
                return log_util::onFailed(file, __FILE__, __LINE__, "failed to write SECTION_SKELETAL_ANIMATION [%s]", fileOut);
        }

        // SECTION_ARTICULATED_PARTS - optional rigid-part identities and pivots -----------------------
        if (!impl->articulatedParts.empty())
        {
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_ARTICULATED_PARTS;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [this](FILE *fp)
            {
                util::ARTICULATED_PARTS_HEADER_V11 partsHeader;
                partsHeader.partCount = static_cast<uint32_t>(this->impl->articulatedParts.size());
                if (!util::writeArticulatedPartsHeaderV11(fp, partsHeader))
                    return false;
                for (const auto &part : this->impl->articulatedParts)
                    if (!util::writeArticulatedPartV11(fp, part))
                        return false;
                return true;
            });
            if (!ok)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to write SECTION_ARTICULATED_PARTS [%s]", fileOut);
        }

        // SECTION_ARTICULATED_ANIMATION - optional named clips and transform tracks -------------------
        if (!impl->articulatedClips.empty())
        {
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_ARTICULATED_ANIMATION;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [this](FILE *fp)
            {
                util::ARTICULATED_ANIMATION_HEADER_V11 animationHeader;
                animationHeader.clipCount = static_cast<uint32_t>(this->impl->articulatedClips.size());
                if (!util::writeArticulatedAnimationHeaderV11(fp, animationHeader))
                    return false;
                for (const auto &clip : this->impl->articulatedClips)
                {
                    if (!util::writeArticulatedClipV11(fp, clip.header) ||
                        !util::le_io::writeU32LE(fp, static_cast<uint32_t>(clip.tracks.size())))
                        return false;
                    for (const auto &track : clip.tracks)
                    {
                        util::ARTICULATED_TRACK_V11 trackHeader = track.header;
                        trackHeader.keyCount = static_cast<uint32_t>(track.keys.size());
                        if (!util::writeArticulatedTrackV11(fp, trackHeader))
                            return false;
                        for (const auto &key : track.keys)
                            if (!util::writeArticulatedKeyV11(fp, key))
                                return false;
                    }
                }
                return true;
            });
            if (!ok)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to write SECTION_ARTICULATED_ANIMATION [%s]", fileOut);
        }

        // SECTION_ANIMATION, one per animation, including its FX block ---------------------------------------
        for (uint32_t i = 0; i < this->getTotalAnimationHeaders(); ++i)
        {
            util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead = this->getAnimationHeader(i);
            util::HEADER_ANIMATION *headerAnim = infoHead->headerAnim;
            util::INFO_FX *fx = infoHead->effectShader;

            if (fx && ((fx->dataPS && fx->dataPS->fileNameTextureStage2) ||
                       (fx->dataVS && fx->dataVS->fileNameTextureStage2)))
            {
                return log_util::onFailed(file, __FILE__, __LINE__,
                                          "saveV11 does not support a shader step's secondary texture stage yet (animation [%s]) [%s]",
                                          headerAnim->nameAnimation, fileOut);
            }

            util::ANIMATION_HEADER_V11 v11Anim;
            v11Anim.name             = headerAnim->nameAnimation;
            v11Anim.initialFrame     = headerAnim->initialFrame;
            v11Anim.finalFrame       = headerAnim->finalFrame;
            v11Anim.timeBetweenFrame = headerAnim->timeBetweenFrame;
            v11Anim.typeAnimation    = headerAnim->typeAnimation;
            v11Anim.blendState       = headerAnim->blendState;
            v11Anim.hasFx            = (fx != nullptr) ? 1 : 0;

            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_ANIMATION;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [&](FILE *fp) -> bool
            {
                if (!util::writeAnimationHeaderV11(fp, v11Anim))
                    return false;
                if (!fx)
                    return true;

                util::FX_HEADER_V11 v11Fx;
                v11Fx.blendOperation = fx->blendOperation;
                const char *fxTexPath = fx->getTextureAnimationEffectFileName();
                v11Fx.hasFxTexture = (fxTexPath && fxTexPath[0] != '\0') ? 1 : 0;
                if (v11Fx.hasFxTexture)
                {
                    v11Fx.fxTexture.storage = util::TEXTURE_REF_STORAGE_PATH;
                    v11Fx.fxTexture.path    = fxTexPath;
                }
                v11Fx.hasPS = (fx->dataPS != nullptr) ? 1 : 0;
                v11Fx.hasVS = (fx->dataVS != nullptr) ? 1 : 0;
                if (v11Fx.hasPS)
                {
                    v11Fx.ps.name          = fx->dataPS->fileNameShader ? fx->dataPS->fileNameShader : "";
                    v11Fx.ps.timeAnimation = fx->dataPS->timeAnimation;
                    v11Fx.ps.typeAnimation = fx->dataPS->typeAnimation;
                    v11Fx.ps.varCount      = static_cast<uint16_t>(fx->dataPS->lenVars);
                }
                if (v11Fx.hasVS)
                {
                    v11Fx.vs.name          = fx->dataVS->fileNameShader ? fx->dataVS->fileNameShader : "";
                    v11Fx.vs.timeAnimation = fx->dataVS->timeAnimation;
                    v11Fx.vs.typeAnimation = fx->dataVS->typeAnimation;
                    v11Fx.vs.varCount      = static_cast<uint16_t>(fx->dataVS->lenVars);
                }
                if (!util::writeFxHeaderV11(fp, v11Fx))
                    return false;

                const auto writeVars = [fp](const util::INFO_SHADER_DATA *data) -> bool
                {
                    for (int v = 0; v < data->lenVars; ++v)
                    {
                        util::SHADER_VAR_V11 var;
                        var.typeVar = static_cast<uint8_t>(data->typeVars[v]);
                        for (int c = 0; c < 4; ++c)
                        {
                            var.min[c] = data->min[v * 4 + c];
                            var.max[c] = data->max[v * 4 + c];
                        }
                        if (!util::writeShaderVarV11(fp, var))
                            return false;
                    }
                    return true;
                };
                if (v11Fx.hasPS && !writeVars(fx->dataPS))
                    return false;
                if (v11Fx.hasVS && !writeVars(fx->dataVS))
                    return false;
                return true;
            });
            if (!ok)
                return log_util::onFailed(file, __FILE__, __LINE__, "failed to write SECTION_ANIMATION for animation [%s] [%s]",
                                          headerAnim->nameAnimation, fileOut);
        }

        // SECTION_DETAIL_PARTICLE, all stages bundled into one section ---------------------------------------
        if (impl->typeMe == util::TYPE_MESH_PARTICLE)
        {
            const auto *lsStage = static_cast<const std::vector<util::STAGE_PARTICLE*>*>(this->getDetailInfo());
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_DETAIL_PARTICLE;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [&](FILE *fp) -> bool
            {
                const uint16_t stageCount = lsStage ? static_cast<uint16_t>(lsStage->size()) : 0;
                if (!util::le_io::writeU16LE(fp, stageCount))
                    return false;
                if (lsStage)
                    for (const auto *stage : *lsStage)
                        if (!util::writeStageParticleV11(fp, *stage))
                            return false;
                return true;
            });
            if (!ok)
                return log_util::onFailed(file, __FILE__, __LINE__, "failed to write SECTION_DETAIL_PARTICLE [%s]", fileOut);
        }

        // SECTION_DETAIL_FONT, one section bundling the name/spacing header + all letter entries ------------
        if (impl->typeMe == util::TYPE_MESH_FONT)
        {
            const auto *font = static_cast<const INFO_BOUND_FONT*>(this->getDetailInfo());
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_DETAIL_FONT;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [&](FILE *fp) -> bool
            {
                util::FONT_DETAIL_HEADER_V11 v11Font;
                v11Font.name            = font ? font->fontName : "";
                v11Font.spaceXCharacter = font ? font->spaceXCharacter : 0;
                v11Font.spaceYCharacter = font ? font->spaceYCharacter : 0;
                v11Font.heightLetter    = font ? font->heightLetter : 0;
                v11Font.letterCount     = 0;
                if (font)
                    for (const auto &l : font->letter)
                        if (l.detail)
                            ++v11Font.letterCount;
                if (!util::writeFontDetailHeaderV11(fp, v11Font))
                    return false;
                if (!font)
                    return true;
                for (int asciiCode = 0; asciiCode < 255; ++asciiCode)
                {
                    const util::DETAIL_LETTER *detail = font->letter[asciiCode].detail;
                    if (!detail)
                        continue;
                    if (!util::writeDetailLetterV11(fp, *detail))
                        return false;
                }
                return true;
            });
            if (!ok)
                return log_util::onFailed(file, __FILE__, __LINE__, "failed to write SECTION_DETAIL_FONT [%s]", fileOut);
        }

        // SECTION_DETAIL_TILE, one section bundling map header + bricks + layers + objects + properties ----
        if (impl->typeMe == util::TYPE_MESH_TILE_MAP)
        {
            const auto *tileInfo = static_cast<const util::BTILE_INFO*>(this->getDetailInfo());
            util::SECTION_HEADER_V11 sectionHeader;
            sectionHeader.type = util::SECTION_DETAIL_TILE;
            sectionHeader.sectionVersion = 1;
            const bool ok = util::writeSectionV11Streamed(file, sectionHeader, [&](FILE *fp) -> bool
            {
                util::TILE_HEADER_MAP_V11 v11Header;
                if (!tileInfo)
                    return util::writeTileHeaderMapV11(fp, v11Header);

                v11Header.count_width_tile           = tileInfo->map.count_width_tile;
                v11Header.count_height_tile          = tileInfo->map.count_height_tile;
                v11Header.size_width_tile            = tileInfo->map.size_width_tile;
                v11Header.size_height_tile           = tileInfo->map.size_height_tile;
                v11Header.layerCount                 = tileInfo->map.layerCount;
                v11Header.countRawTiles              = tileInfo->map.countRawTiles;
                v11Header.objectCount                = static_cast<uint32_t>(tileInfo->lsObj.size());
                v11Header.propertyCount              = static_cast<uint32_t>(tileInfo->lsProperty.size());
                v11Header.typeMap                    = static_cast<uint32_t>(tileInfo->map.typeMap);
                v11Header.background                 = tileInfo->map.background;
                v11Header.backgroundTexture           = tileInfo->map.background_texture;
                v11Header.renderDirectionLeftToRight = static_cast<uint8_t>(tileInfo->map.renderDirection[0]);
                v11Header.renderDirectionTopToDown   = static_cast<uint8_t>(tileInfo->map.renderDirection[1]);
                if (!util::writeTileHeaderMapV11(fp, v11Header))
                    return false;

                for (uint32_t i = 0; i < v11Header.countRawTiles; ++i)
                    if (!util::writeBtileBrickInfoV11(fp, tileInfo->infoBrickEditor[i]))
                        return false;

                const uint32_t cellsPerLayer = v11Header.count_width_tile * v11Header.count_height_tile;
                for (uint32_t l = 0; l < v11Header.layerCount; ++l)
                {
                    util::TILE_LAYER_HEADER_V11 v11Layer;
                    v11Layer.offsetX = tileInfo->layers[l].offset[0];
                    v11Layer.offsetY = tileInfo->layers[l].offset[1];
                    v11Layer.offsetZ = tileInfo->layers[l].offset[2];
                    if (!util::writeTileLayerHeaderV11(fp, v11Layer))
                        return false;
                    for (uint32_t c = 0; c < cellsPerLayer; ++c)
                        if (!util::writeBtileIndexTileV11(fp, tileInfo->layers[l].lsIndexTiles[c]))
                            return false;
                }

                for (const auto *obj : tileInfo->lsObj)
                {
                    util::TILE_OBJ_HEADER_V11 v11Obj;
                    v11Obj.name       = obj->name;
                    v11Obj.type       = static_cast<uint16_t>(obj->type);
                    v11Obj.pointCount = static_cast<uint16_t>(obj->lsPoints.size());
                    if (!util::writeTileObjHeaderV11(fp, v11Obj))
                        return false;
                    for (const auto *point : obj->lsPoints)
                        if (!util::le_io::writeF32LE(fp, point->x) || !util::le_io::writeF32LE(fp, point->y))
                            return false;
                }

                for (const auto *prop : tileInfo->lsProperty)
                {
                    util::TILE_PROPERTY_V11 v11Prop;
                    v11Prop.owner = prop->owner;
                    v11Prop.name  = prop->name;
                    v11Prop.value = prop->value;
                    v11Prop.type  = static_cast<uint16_t>(prop->type);
                    if (!util::writeTilePropertyV11(fp, v11Prop))
                        return false;
                }
                return true;
            });
            if (!ok)
                return log_util::onFailed(file, __FILE__, __LINE__, "failed to write SECTION_DETAIL_TILE [%s]", fileOut);
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
            if (compress)
                sectionHeader.compression = util::SECTION_COMPRESSION_DEFLATE;
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

    bool MESH_MBM_DEBUG::readFrameStaticV11Payload(util::MEM_CURSOR_V11 &fp, const util::BUFFER_MESH_DEBUG *frame0, util::BUFFER_MESH_DEBUG *&out,
                                                   util::FRAME_HEADER_V11 &outFrameHeader)
    {
        out = nullptr;

        util::FRAME_HEADER_V11 &frameHeader = outFrameHeader;
        if (!util::readFrameHeaderV11(fp, frameHeader))
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "failed to read FRAME_HEADER_V11");
        if (frameHeader.indexWidth != 16)
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "loadV11 only supports 16-bit indices");

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

        impl->typeMe              = static_cast<util::TYPE_MESH>(fileHeader.typeMesh);
        impl->backBufferWidth     = fileHeader.backBufferWidth;
        impl->backBufferHeight    = fileHeader.backBufferHeight;
        impl->formatVersion       = fileHeader.formatVersion;

        int16_t hasNormalFlag  = HAS_NOR_NO;
        int16_t hasTextureFlag = HAS_TEX_NO;
        bool    sawFirstFrame  = false;
        bool    sawCanonicalSkeleton = false;
        bool    sawCanonicalWeights = false;
        bool    sawCanonicalAnimations = false;
        uint32_t canonicalFrame0VertexCount = UINT32_MAX;
        util::BUFFER_MESH_DEBUG *frame0 = nullptr;

        struct DebugStagedSection
        {
            util::SECTION_HEADER_V11 header;
            std::vector<uint8_t> payload;
        };
        std::vector<DebugStagedSection> sections(fileHeader.sectionCount);
        for (uint32_t i = 0; i < fileHeader.sectionCount; ++i)
            if (!util::readSectionV11(fp, sections[i].header, sections[i].payload))
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read section %u [%s]", i, fileNamePath);

        for (const DebugStagedSection &staged : sections)
        {
            if (staged.header.type == util::SECTION_SKELETAL_SKELETON)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(staged.payload);
                if (sawCanonicalSkeleton || !parse_canonical_skeleton_section_v11(
                        tmp, staged.header.sectionVersion, impl->canonicalSkeleton))
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_SKELETAL_SKELETON [%s]", fileNamePath);
                sawCanonicalSkeleton = true;
            }
            else if (staged.header.type == util::SECTION_FRAME_STATIC &&
                     canonicalFrame0VertexCount == UINT32_MAX)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(staged.payload);
                util::FRAME_HEADER_V11 header;
                if (!util::readFrameHeaderV11(tmp, header))
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to inspect SECTION_FRAME_STATIC [%s]", fileNamePath);
                canonicalFrame0VertexCount = header.vertexCount;
            }
        }

        for (const DebugStagedSection &staged : sections)
        {
            const util::SECTION_HEADER_V11 &sectionHeader = staged.header;
            const std::vector<uint8_t> &payload = staged.payload;

            if (sectionHeader.type == util::SECTION_MATERIAL_TRANSFORM)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                util::MATERIAL_TRANSFORM_V11 materialTransform;
                if (!util::readMaterialTransformV11(tmp, materialTransform))
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_MATERIAL_TRANSFORM [%s]", fileNamePath);

                impl->headerMesh.material = materialTransform.material;
                impl->headerMesh.angleX   = materialTransform.angleX;
                impl->headerMesh.angleY   = materialTransform.angleY;
                impl->headerMesh.angleZ   = materialTransform.angleZ;
                impl->headerMesh.posX     = materialTransform.posX;
                impl->headerMesh.posY     = materialTransform.posY;
                impl->headerMesh.posZ     = materialTransform.posZ;
                // Deprecated fields (see MESH_MBM::Impl's own comment) - mirrored here purely so a
                // load-then-save round trip preserves a legacy file's stored bytes unchanged; there
                // is no public getter/setter left, so nothing else ever writes these two.
                impl->angleDefault_deprecated = VEC3(materialTransform.angleX, materialTransform.angleY, materialTransform.angleZ);
                impl->positionOffset_deprecated = VEC3(materialTransform.posX, materialTransform.posY, materialTransform.posZ);
                impl->info_mode.mode_draw = materialTransform.mode_draw;
                impl->info_mode.mode_cull_face = materialTransform.mode_cull_face;
                impl->info_mode.mode_front_face_direction = materialTransform.mode_front_face_direction;
            }
            else if (sectionHeader.type == util::SECTION_EXTRA_PATHS)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
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
                if (!ok)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_EXTRA_PATHS [%s]", fileNamePath);
            }
            else if (sectionHeader.type == util::SECTION_DETAIL_PHYSICS)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                const bool ok = read_detail_mesh_section(tmp, fileNamePath, this->impl->infoPhysics,
                                                         [this](util::MEM_CURSOR_V11 &f, const char *n, const int tb)
                                                         {
                                                             return this->readDebugTriangleDetailCompat(f, n, tb);
                                                         });
                if (!ok)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_DETAIL_PHYSICS [%s]", fileNamePath);
            }
            else if (sectionHeader.type == util::SECTION_ANIMATION)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead = parse_animation_section_v11(tmp);
                if (!infoHead)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_ANIMATION [%s]", fileNamePath);
                this->appendAnimationHeader(infoHead);
            }
            else if (sectionHeader.type == util::SECTION_ARTICULATED_PARTS)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                if (sectionHeader.sectionVersion != 1 ||
                    !parse_articulated_parts_section_v11(tmp, impl->articulatedParts))
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_ARTICULATED_PARTS [%s]", fileNamePath);
            }
            else if (sectionHeader.type == util::SECTION_ARTICULATED_ANIMATION)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                if (sectionHeader.sectionVersion != 1 ||
                    !parse_articulated_animation_section_v11(tmp, impl->articulatedClips))
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_ARTICULATED_ANIMATION [%s]", fileNamePath);
            }
            else if (sectionHeader.type == util::SECTION_DETAIL_PARTICLE)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                std::vector<util::STAGE_PARTICLE*> *lsStage = parse_particle_detail_section_v11(tmp);
                if (!lsStage)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_DETAIL_PARTICLE [%s]", fileNamePath);
                this->replaceDetailInfo(lsStage);
            }
            else if (sectionHeader.type == util::SECTION_DETAIL_FONT)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                INFO_BOUND_FONT *font = parse_font_detail_section_v11(tmp);
                if (!font)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_DETAIL_FONT [%s]", fileNamePath);
                this->replaceDetailInfo(font);
            }
            else if (sectionHeader.type == util::SECTION_DETAIL_TILE)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                util::BTILE_INFO *tileInfo = parse_tile_detail_section_v11(tmp);
                if (!tileInfo)
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_DETAIL_TILE [%s]", fileNamePath);
                this->replaceDetailInfo(tileInfo);
            }
            else if (sectionHeader.type == util::SECTION_FRAME_STATIC)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                util::BUFFER_MESH_DEBUG *pBuffer = nullptr;
                util::FRAME_HEADER_V11 v11FrameHeader;
                const bool ok = this->readFrameStaticV11Payload(tmp, frame0, pBuffer, v11FrameHeader);
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
            else if (sectionHeader.type == util::SECTION_SKELETAL_SKELETON)
            {
                // Parsed in the order-independent pre-pass above.
            }
            else if (sectionHeader.type == util::SECTION_SKELETAL_WEIGHTS)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                if (sawCanonicalWeights || !sawCanonicalSkeleton ||
                    canonicalFrame0VertexCount == UINT32_MAX ||
                    !parse_canonical_weights_section_v11(tmp, sectionHeader.sectionVersion,
                        impl->canonicalSkeleton, canonicalFrame0VertexCount, impl->canonicalWeights))
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_SKELETAL_WEIGHTS [%s]", fileNamePath);
                sawCanonicalWeights = true;
            }
            else if (sectionHeader.type == util::SECTION_SKELETAL_ANIMATION)
            {
                util::MEM_CURSOR_V11 tmp = stage_payload_as_cursor(payload);
                if (sawCanonicalAnimations || !sawCanonicalSkeleton ||
                    !parse_canonical_animation_section_v11(tmp, sectionHeader.sectionVersion,
                        impl->canonicalSkeleton, impl->canonicalAnimations))
                    return log_util::onFailed(fp, __FILE__, __LINE__, "failed to parse SECTION_SKELETAL_ANIMATION [%s]", fileNamePath);
                sawCanonicalAnimations = true;
            }
            else
            {
                return log_util::onFailed(fp, __FILE__, __LINE__,
                                          "loadV11 does not support section type %u [%s]",
                                          sectionHeader.type, fileNamePath);
            }
        }

        impl->headerMesh.totalFrames    = static_cast<int>(this->impl->buffer.size());
        impl->headerMesh.totalAnimation = static_cast<int>(this->getTotalAnimationHeaders());
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
            // indexSubset selects the geometry used to calculate the center. The resulting
            // translation is always applied to the whole frame so the relative placement of
            // its subsets is preserved.
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
            for (uint32_t i = 0; i < s; ++i)
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[i];
                const auto n = static_cast<uint32_t>(pTmpSubset->vertexStart + pTmpSubset->vertexCount);
                for (auto j = static_cast<uint32_t>(pTmpSubset->vertexStart); j < n; ++j)
                {
                    VEC3 *pos = &pPosition[j];
                    pos->x -= offset.x;
                    pos->y -= offset.y;
                    pos->z -= offset.z;
                }
            }
        }
    }

    void MESH_MBM_DEBUG::centralizeFrameItself(const int indexFrame, const int indexSubset)
    {
        if (indexFrame < 0)
        {
            for (uint32_t i = 0; i < this->impl->buffer.size(); ++i)
                centralizeFrameItself(static_cast<int>(i), indexSubset);
            return;
        }
        if (indexFrame >= static_cast<int>(this->impl->buffer.size()))
            return;

        util::BUFFER_MESH_DEBUG *bufferCurrent =
            this->impl->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)];
        auto *const pPosition = reinterpret_cast<VEC3 *>(bufferCurrent->position);
        const auto totalSubsets = static_cast<uint32_t>(bufferCurrent->subset.size());
        if (indexSubset >= static_cast<int>(totalSubsets))
            return;

        const uint32_t firstSubset = indexSubset < 0 ? 0 : static_cast<uint32_t>(indexSubset);
        const uint32_t endSubset = indexSubset < 0 ? totalSubsets : firstSubset + 1;
        for (uint32_t i = firstSubset; i < endSubset; ++i)
        {
            util::SUBSET_DEBUG *subset = bufferCurrent->subset[i];
            if (subset->vertexCount <= 0)
                continue;

            VEC3 maxSize(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            VEC3 minSize(FLT_MAX, FLT_MAX, FLT_MAX);
            const auto vertexEnd = static_cast<uint32_t>(subset->vertexStart + subset->vertexCount);
            for (auto j = static_cast<uint32_t>(subset->vertexStart); j < vertexEnd; ++j)
            {
                const VEC3 &pos = pPosition[j];
                if (pos.x < minSize.x)
                    minSize.x = pos.x;
                if (pos.y < minSize.y)
                    minSize.y = pos.y;
                if (pos.z < minSize.z)
                    minSize.z = pos.z;
                if (pos.x > maxSize.x)
                    maxSize.x = pos.x;
                if (pos.y > maxSize.y)
                    maxSize.y = pos.y;
                if (pos.z > maxSize.z)
                    maxSize.z = pos.z;
            }

            VEC3 dist(maxSize - minSize);
            const float xDif = maxSize.x < 0.0f ? -maxSize.x : maxSize.x;
            const float yDif = maxSize.y < 0.0f ? -maxSize.y : maxSize.y;
            const float zDif = maxSize.z < 0.0f ? -maxSize.z : maxSize.z;
            const float xDiff = minSize.x < 0.0f ? -minSize.x : minSize.x;
            const float yDiff = minSize.y < 0.0f ? -minSize.y : minSize.y;
            const float zDiff = minSize.z < 0.0f ? -minSize.z : minSize.z;
            const float xMin = xDiff < xDif ? xDiff : xDif;
            const float xMax = xDiff > xDif ? xDiff : xDif;
            const float yMin = yDiff < yDif ? yDiff : yDif;
            const float yMax = yDiff > yDif ? yDiff : yDif;
            const float zMin = zDiff < zDif ? zDiff : zDif;
            const float zMax = zDiff > zDif ? zDiff : zDif;

            if ((xMin / xMax) < 0.001f)
            {
                dist.x = xMin;
                minSize.x = 0.0f;
            }
            if ((yMin / yMax) < 0.001f)
            {
                dist.y = yMin;
                minSize.y = 0.0f;
            }
            if ((zMin / zMax) < 0.001f)
            {
                dist.z = zMin;
                minSize.z = 0.0f;
            }

            const VEC3 offset(minSize.x + dist.x * 0.5f,
                              minSize.y + dist.y * 0.5f,
                              minSize.z + dist.z * 0.5f);
            for (auto j = static_cast<uint32_t>(subset->vertexStart); j < vertexEnd; ++j)
            {
                VEC3 &pos = pPosition[j];
                pos.x -= offset.x;
                pos.y -= offset.y;
                pos.z -= offset.z;
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
        }
        else if (indexFrame < static_cast<int>(this->impl->buffer.size()))
        {
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

    }

    bool MESH_MBM_DEBUG::scaleSkeletalAsset(const float scale, char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0)
                snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (!std::isfinite(scale) || scale <= 0.0f)
            return fail("skeletal asset scale must be finite and greater than zero");
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton to scale");

        skeletal::CANONICAL_SKELETON scaledSkeleton;
        skeletal::CANONICAL_ANIMATIONS scaledAnimations;
        if (!skeletal::buildUniformlyScaledCanonicalAsset(impl->canonicalSkeleton,
                                                           impl->canonicalAnimations,
                                                           scale,
                                                           scaledSkeleton,
                                                           scaledAnimations))
        {
            if (!scaledSkeleton.compiled.diagnostics.empty())
            {
                const skeletal::DIAGNOSTIC &diagnostic = scaledSkeleton.compiled.diagnostics.front();
                if (errorOut && errorOutLen > 0)
                    snprintf(errorOut, errorOutLen, "scaled canonical skeleton is invalid: %s at bone %u ('%s'), error %.9g",
                             skeletal::diagnosticCodeName(diagnostic.code), diagnostic.sourceIndex,
                             diagnostic.boneName.c_str(), diagnostic.observedError);
                return false;
            }
            return fail("scaled canonical animation would be invalid");
        }

        const auto validProduct = [scale](const float value)
        {
            return std::isfinite(value) && std::isfinite(value * scale);
        };
        for (const util::BUFFER_MESH_DEBUG *frame : impl->buffer)
        {
            const auto *positions = reinterpret_cast<const VEC3 *>(frame->position);
            for (const util::SUBSET_DEBUG *subset : frame->subset)
            {
                const uint32_t end = static_cast<uint32_t>(subset->vertexStart + subset->vertexCount);
                for (uint32_t vertex = static_cast<uint32_t>(subset->vertexStart); vertex < end; ++vertex)
                {
                    if (!validProduct(positions[vertex].x) || !validProduct(positions[vertex].y) ||
                        !validProduct(positions[vertex].z))
                        return fail("scaled mesh vertex would not be finite");
                }
            }
        }
        for (const CUBE *cube : impl->infoPhysics.lsCube)
        {
            if (!validProduct(cube->halfDim.x) || !validProduct(cube->halfDim.y) ||
                !validProduct(cube->halfDim.z) || !validProduct(cube->absCenter.x) ||
                !validProduct(cube->absCenter.y) || !validProduct(cube->absCenter.z))
                return fail("scaled cube bounds would not be finite");
        }
        for (const SPHERE *sphere : impl->infoPhysics.lsSphere)
        {
            if (!validProduct(sphere->ray) || !validProduct(sphere->absCenter[0]) ||
                !validProduct(sphere->absCenter[1]) || !validProduct(sphere->absCenter[2]))
                return fail("scaled sphere bounds would not be finite");
        }
        for (const CUBE_COMPLEX *cube : impl->infoPhysics.lsCubeComplex)
            for (const _VEC3_POINT &point : cube->p)
                if (!validProduct(point.x) || !validProduct(point.y) || !validProduct(point.z))
                    return fail("scaled complex-cube bounds would not be finite");
        for (const TRIANGLE *triangle : impl->infoPhysics.lsTriangle)
        {
            for (const VEC3 &point : triangle->point)
                if (!validProduct(point.x) || !validProduct(point.y) || !validProduct(point.z))
                    return fail("scaled triangle bounds would not be finite");
            if (!validProduct(triangle->position.x) || !validProduct(triangle->position.y))
                return fail("scaled triangle position would not be finite");
        }

        scaleFrame(-1, -1, scale, scale, scale);
        for (CUBE *cube : impl->infoPhysics.lsCube)
        {
            cube->halfDim.x *= scale; cube->halfDim.y *= scale; cube->halfDim.z *= scale;
            cube->absCenter.x *= scale; cube->absCenter.y *= scale; cube->absCenter.z *= scale;
        }
        for (SPHERE *sphere : impl->infoPhysics.lsSphere)
        {
            sphere->ray *= scale;
            sphere->absCenter[0] *= scale; sphere->absCenter[1] *= scale; sphere->absCenter[2] *= scale;
        }
        for (CUBE_COMPLEX *cube : impl->infoPhysics.lsCubeComplex)
            for (_VEC3_POINT &point : cube->p)
            {
                point.x *= scale; point.y *= scale; point.z *= scale;
            }
        for (TRIANGLE *triangle : impl->infoPhysics.lsTriangle)
        {
            for (VEC3 &point : triangle->point)
            {
                point.x *= scale; point.y *= scale; point.z *= scale;
            }
            triangle->position.x *= scale; triangle->position.y *= scale;
        }
        impl->canonicalSkeleton = std::move(scaledSkeleton);
        impl->canonicalAnimations = std::move(scaledAnimations);
        return true;
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
                    memcpy(&newIndex[s], &oldIndex[indexCountBefore + oldSizeIndex], sizeof(unsigned short) * static_cast<size_t>(indexCountAfter));
                }
                int diff = static_cast<int>(oldSizeIndex) - static_cast<int>(sizeArrayNewIndexPart);
                for (uint32_t i = (indexSubset + 1); i < sizeSubset; ++i)
                {
                    util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[std::vector<util::SUBSET_DEBUG *>::size_type(i)];
                    pTmpSubset->indexStart += diff;
                }

                for (uint32_t i = indexCountBefore; i < (indexCountBefore + sizeArrayNewIndexPart); ++i)
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

    const char *MESH_MBM_DEBUG::getAnimationEffectTexture(const uint32_t index) const noexcept
    {
        if (index >= this->impl->infoAnimation.lsHeaderAnim.size())
            return nullptr;
        const auto *infoHead = this->impl->infoAnimation.lsHeaderAnim[index];
        if (infoHead == nullptr || infoHead->effectShader == nullptr)
            return nullptr;
        return infoHead->effectShader->getTextureAnimationEffectFileName();
    }


    bool MESH_MBM_DEBUG::getSkeletonBindSummary(SKELETON_BIND_SUMMARY &out) const noexcept
    {
        if (impl->canonicalSkeleton.skeletonId == 0)
            return false;
        const skeletal::COMPILED_SKELETON &report = impl->canonicalSkeleton.compiled;
        out.boneCount = static_cast<uint32_t>(report.bones.size());
        out.diagnosticCount = static_cast<uint32_t>(report.diagnostics.size());
        out.animationClipCount = static_cast<uint32_t>(impl->canonicalAnimations.clips.size());
        out.maximumReconstructionError = report.maximumReconstructionError;
        out.maximumBindIdentityError = report.maximumBindIdentityError;
        out.valid = !report.hasFatalDiagnostics();
        out.canonical = true;
        return true;
    }

    bool MESH_MBM_DEBUG::getSkeletonBindBone(const uint32_t index, SKELETON_BIND_BONE_INFO &out,
                                              const bool includeDependencyImpact) const noexcept
    {
        const skeletal::COMPILED_SKELETON &report = impl->canonicalSkeleton.compiled;
        if (impl->canonicalSkeleton.skeletonId == 0 || index >= report.bones.size())
            return false;
        const skeletal::COMPILED_BONE &bone = report.bones[index];
        out.boneId = bone.boneId;
        out.parentBoneId = bone.parentBoneId;
        out.parentIndex = bone.parentIndex;
        out.sourceIndex = bone.sourceIndex;
        out.localTranslation = bone.localBind.translation;
        out.localRotationX = bone.localBind.rotation.x;
        out.localRotationY = bone.localBind.rotation.y;
        out.localRotationZ = bone.localBind.rotation.z;
        out.localRotationW = bone.localBind.rotation.w;
        out.localScale = bone.localBind.scale;
        out.localBindMatrix = bone.localBindMatrix;
        out.globalBindMatrix = bone.globalBindMatrix;
        out.inverseGlobalBindMatrix = bone.inverseGlobalBindMatrix;
        if (bone.sourceIndex < impl->canonicalSkeleton.sourceBones.size())
        {
            out.radius = impl->canonicalSkeleton.sourceBones[bone.sourceIndex].radius;
            out.length = impl->canonicalSkeleton.sourceBones[bone.sourceIndex].length;
            out.tailOffset = impl->canonicalSkeleton.sourceBones[bone.sourceIndex].tailOffset;
            out.hasExplicitTail = impl->canonicalSkeleton.sourceBones[bone.sourceIndex].hasExplicitTail;
            out.connectedToParent = impl->canonicalSkeleton.sourceBones[bone.sourceIndex].connectedToParent;
        }
        out.childCount = 0;
        for (const skeletal::CANONICAL_BONE &candidate : impl->canonicalSkeleton.sourceBones)
            if (candidate.parentBoneId == bone.boneId) ++out.childCount;
        out.weightPaletteReferenced = false;
        out.weightedVertexCount = 0;
        if (includeDependencyImpact)
        {
            for (uint32_t paletteIndex = 0; paletteIndex < impl->canonicalWeights.paletteBoneIds.size(); ++paletteIndex)
            {
                if (impl->canonicalWeights.paletteBoneIds[paletteIndex] != bone.boneId) continue;
                out.weightPaletteReferenced = true;
                for (const skeletal::CANONICAL_VERTEX_WEIGHT &vertex : impl->canonicalWeights.vertices)
                    for (uint32_t slot = 0; slot < 4; ++slot)
                        if (vertex.paletteIndex[slot] == paletteIndex && vertex.weight[slot] > 0.0f)
                        { ++out.weightedVertexCount; break; }
                break;
            }
        }
        out.animationTrackCount = 0;
        if (includeDependencyImpact)
            for (const skeletal::SKELETAL_CLIP &clip : impl->canonicalAnimations.clips)
                for (const skeletal::SKELETAL_TRACK &track : clip.tracks)
                    if (track.boneId == bone.boneId) ++out.animationTrackCount;
        out.hasNegativeScale = bone.hasNegativeScale;
        out.hasShear = bone.hasShear;
        return true;
    }

    const char *MESH_MBM_DEBUG::getSkeletonBindBoneName(const uint32_t index) const noexcept
    {
        const skeletal::COMPILED_SKELETON &report = impl->canonicalSkeleton.compiled;
        if (impl->canonicalSkeleton.skeletonId == 0 || index >= report.bones.size())
            return nullptr;
        return report.bones[index].name.c_str();
    }

    namespace
    {
        bool getSkeletalSharingCompatibilityImpl(
            const skeletal::CANONICAL_SKELETON &leftSkeleton,
            const skeletal::CANONICAL_SKELETON &rightSkeleton,
            SKELETAL_SHARING_COMPATIBILITY &out) noexcept
        {
            out = SKELETAL_SHARING_COMPATIBILITY();
            const skeletal::COMPILED_SKELETON &left = leftSkeleton.compiled;
            const skeletal::COMPILED_SKELETON &right = rightSkeleton.compiled;
            if (leftSkeleton.skeletonId == 0 || rightSkeleton.skeletonId == 0 ||
                left.hasFatalDiagnostics() || right.hasFatalDiagnostics() ||
                left.bones.empty() || right.bones.empty())
            {
                out.reason = "missing_skeleton";
                out.boneCount = static_cast<uint32_t>(left.bones.size());
                return false;
            }
            out.boneCount = static_cast<uint32_t>(left.bones.size());
            if (left.bones.size() != right.bones.size())
            {
                out.reason = "bone_count_mismatch";
                return false;
            }
            for (uint32_t index = 0; index < left.bones.size(); ++index)
            {
                const skeletal::COMPILED_BONE &leftBone = left.bones[index];
                const skeletal::COMPILED_BONE &rightBone = right.bones[index];
                out.boneIndex = index;
                out.boneName = leftBone.name.c_str();
                out.boneId = leftBone.boneId;
                out.otherBoneId = rightBone.boneId;
                if (leftBone.boneId != rightBone.boneId || leftBone.name != rightBone.name)
                {
                    out.reason = "bone_identity_mismatch";
                    return false;
                }
                out.parentIndex = leftBone.parentIndex;
                out.otherParentIndex = rightBone.parentIndex;
                out.parentBoneId = leftBone.parentBoneId;
                out.otherParentBoneId = rightBone.parentBoneId;
                if (leftBone.parentIndex != rightBone.parentIndex ||
                    leftBone.parentBoneId != rightBone.parentBoneId)
                {
                    out.reason = "hierarchy_mismatch";
                    return false;
                }
                out.observedError = skeletal::maximumMatrixDifference(leftBone.localBindMatrix,
                                                                      rightBone.localBindMatrix);
                out.tolerance = skeletal::matrixComparisonTolerance(leftBone.localBindMatrix,
                                                                    rightBone.localBindMatrix);
                if (out.observedError > out.tolerance)
                {
                    out.reason = "bind_transform_mismatch";
                    return false;
                }
                out.observedError = skeletal::maximumMatrixDifference(leftBone.globalBindMatrix,
                                                                      rightBone.globalBindMatrix);
                out.tolerance = skeletal::matrixComparisonTolerance(leftBone.globalBindMatrix,
                                                                    rightBone.globalBindMatrix);
                if (out.observedError > out.tolerance)
                {
                    out.reason = "bind_transform_mismatch";
                    return false;
                }
            }
            out.compatible = true;
            out.reason = "compatible";
            out.boneIndex = UINT32_MAX;
            out.boneName = nullptr;
            out.observedError = 0.0f;
            out.tolerance = 0.0f;
            return true;
        }
    }

    bool MESH_MBM_DEBUG::getSkeletalSharingCompatibility(
        const MESH_MBM_DEBUG &other, SKELETAL_SHARING_COMPATIBILITY &out) const noexcept
    {
        return getSkeletalSharingCompatibilityImpl(impl->canonicalSkeleton,
                                                   other.impl->canonicalSkeleton, out);
    }

    bool MESH_MBM::getSkeletalSharingCompatibility(
        const MESH_MBM &other, SKELETAL_SHARING_COMPATIBILITY &out) const noexcept
    {
        return getSkeletalSharingCompatibilityImpl(impl->canonicalSkeleton,
                                                   other.impl->canonicalSkeleton, out);
    }

    bool MESH_MBM_DEBUG::getSkeletonBindDiagnostic(const uint32_t index,
                                                    SKELETON_BIND_DIAGNOSTIC_INFO &out) const noexcept
    {
        const skeletal::COMPILED_SKELETON &report = impl->canonicalSkeleton.compiled;
        if (impl->canonicalSkeleton.skeletonId == 0 || index >= report.diagnostics.size())
            return false;
        const skeletal::DIAGNOSTIC &diagnostic = report.diagnostics[index];
        out.code = skeletal::diagnosticCodeName(diagnostic.code);
        out.sourceIndex = diagnostic.sourceIndex;
        out.observedError = diagnostic.observedError;
        out.fatal = diagnostic.fatal;
        return true;
    }

    uint32_t MESH_MBM_DEBUG::getTotalSkeletalClips() const noexcept
    {
        return static_cast<uint32_t>(impl->canonicalAnimations.clips.size());
    }

    bool MESH_MBM_DEBUG::getSkeletalClip(const uint32_t clipIndex, SKELETAL_CLIP_INFO &out) const noexcept
    {
        if (clipIndex >= impl->canonicalAnimations.clips.size()) return false;
        const skeletal::SKELETAL_CLIP &clip = impl->canonicalAnimations.clips[clipIndex];
        out.clipId = clip.clipId;
        out.duration = clip.duration;
        out.trackCount = static_cast<uint32_t>(clip.tracks.size());
        out.loop = clip.loop;
        return true;
    }

    const char *MESH_MBM_DEBUG::getSkeletalClipName(const uint32_t clipIndex) const noexcept
    {
        return clipIndex < impl->canonicalAnimations.clips.size() ?
            impl->canonicalAnimations.clips[clipIndex].name.c_str() : nullptr;
    }

    bool MESH_MBM_DEBUG::getSkeletalTrack(const uint32_t clipIndex, const uint32_t trackIndex,
                                           SKELETAL_TRACK_INFO &out) const noexcept
    {
        if (clipIndex >= impl->canonicalAnimations.clips.size()) return false;
        const skeletal::SKELETAL_CLIP &clip = impl->canonicalAnimations.clips[clipIndex];
        if (trackIndex >= clip.tracks.size()) return false;
        const skeletal::SKELETAL_TRACK &track = clip.tracks[trackIndex];
        const auto found = impl->canonicalSkeleton.compiled.indexById.find(track.boneId);
        if (found == impl->canonicalSkeleton.compiled.indexById.end()) return false;
        out.boneId = track.boneId;
        out.boneIndex = found->second;
        out.keyCount = static_cast<uint32_t>(track.keys.size());
        out.channelMask = track.channelMask;
        return true;
    }

    bool MESH_MBM_DEBUG::getSkeletalKey(const uint32_t clipIndex, const uint32_t trackIndex,
                                         const uint32_t keyIndex, SKELETAL_KEY_INFO &out) const noexcept
    {
        if (clipIndex >= impl->canonicalAnimations.clips.size()) return false;
        const skeletal::SKELETAL_CLIP &clip = impl->canonicalAnimations.clips[clipIndex];
        if (trackIndex >= clip.tracks.size() || keyIndex >= clip.tracks[trackIndex].keys.size()) return false;
        const skeletal::SKELETAL_KEY &key = clip.tracks[trackIndex].keys[keyIndex];
        out.time = key.time;
        out.localTranslation = key.local.translation;
        out.localRotationX = key.local.rotation.x;
        out.localRotationY = key.local.rotation.y;
        out.localRotationZ = key.local.rotation.z;
        out.localRotationW = key.local.rotation.w;
        out.localScale = key.local.scale;
        out.easing = static_cast<uint8_t>(key.easing);
        out.bezierX1 = key.bezierX1;
        out.bezierY1 = key.bezierY1;
        out.bezierX2 = key.bezierX2;
        out.bezierY2 = key.bezierY2;
        return true;
    }

    bool MESH_MBM_DEBUG::addSkeletalClip(const char *name, const float duration, const bool loop,
                                          uint32_t *newIndexOut, char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (impl->canonicalSkeleton.skeletonId == 0) return fail("mesh has no canonical skeleton");
        if (!name || !name[0]) return fail("canonical clip name must not be empty");
        if (!std::isfinite(duration) || duration < 0.0f)
            return fail("canonical clip duration must be finite and non-negative");
        skeletal::CANONICAL_ANIMATIONS candidate = impl->canonicalAnimations;
        if (candidate.skeletonId == 0) candidate.skeletonId = impl->canonicalSkeleton.skeletonId;
        for (const skeletal::SKELETAL_CLIP &clip : candidate.clips)
            if (clip.name == name) return fail("canonical clip name must be unique");
        uint64_t nextId = 1;
        while (std::any_of(candidate.clips.begin(), candidate.clips.end(),
                           [nextId](const skeletal::SKELETAL_CLIP &clip) { return clip.clipId == nextId; }))
        {
            if (nextId == std::numeric_limits<uint64_t>::max())
                return fail("canonical clip ID space is exhausted");
            ++nextId;
        }
        skeletal::SKELETAL_CLIP clip;
        clip.clipId = nextId;
        clip.name = name;
        clip.duration = duration;
        clip.loop = loop;
        candidate.clips.push_back(std::move(clip));
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton, candidate))
            return fail("new canonical clip would be invalid");
        impl->canonicalAnimations = std::move(candidate);
        if (newIndexOut) *newIndexOut = static_cast<uint32_t>(impl->canonicalAnimations.clips.size() - 1);
        return true;
    }

    bool MESH_MBM_DEBUG::updateSkeletalClip(const uint32_t clipIndex, const char *name,
                                             const float duration, const bool loop,
                                             char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (clipIndex >= impl->canonicalAnimations.clips.size())
            return fail("canonical clip index is out of range");
        if (!name || !name[0]) return fail("canonical clip name must not be empty");
        if (!std::isfinite(duration) || duration < 0.0f)
            return fail("canonical clip duration must be finite and non-negative");
        skeletal::CANONICAL_ANIMATIONS candidate = impl->canonicalAnimations;
        for (uint32_t index = 0; index < candidate.clips.size(); ++index)
            if (index != clipIndex && candidate.clips[index].name == name)
                return fail("canonical clip name must be unique");
        skeletal::SKELETAL_CLIP &clip = candidate.clips[clipIndex];
        clip.name = name;
        clip.duration = duration;
        clip.loop = loop;
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton, candidate))
            return fail("updated canonical clip would be invalid; duration may not exclude existing keys");
        impl->canonicalAnimations = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::removeSkeletalClip(const uint32_t clipIndex,
                                             char *errorOut, const int errorOutLen)
    {
        if (clipIndex >= impl->canonicalAnimations.clips.size())
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s",
                                                       "canonical clip index is out of range");
            return false;
        }
        skeletal::CANONICAL_ANIMATIONS candidate = impl->canonicalAnimations;
        candidate.clips.erase(candidate.clips.begin() + clipIndex);
        if (candidate.clips.empty()) candidate = {};
        else if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton, candidate))
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s",
                                                       "remaining canonical clips would be invalid");
            return false;
        }
        impl->canonicalAnimations = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::addSkeletalTrack(const uint32_t clipIndex, const uint32_t boneIndex,
                                           const uint8_t channelMask, uint32_t *newIndexOut,
                                           char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (clipIndex >= impl->canonicalAnimations.clips.size())
            return fail("canonical clip index is out of range");
        if (boneIndex >= impl->canonicalSkeleton.compiled.bones.size())
            return fail("canonical track bone index is out of range");
        if (channelMask == 0 || (channelMask & ~7u) != 0)
            return fail("canonical track channel mask must contain only T/R/S channels");
        skeletal::CANONICAL_ANIMATIONS candidate = impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip = candidate.clips[clipIndex];
        const skeletal::COMPILED_BONE &bone = impl->canonicalSkeleton.compiled.bones[boneIndex];
        for (const skeletal::SKELETAL_TRACK &track : clip.tracks)
            if (track.boneId == bone.boneId)
                return fail("canonical clip already has a track for this bone");
        skeletal::SKELETAL_TRACK track;
        track.boneId = bone.boneId;
        track.channelMask = channelMask;
        skeletal::SKELETAL_KEY bindKey;
        bindKey.time = 0.0f;
        bindKey.local = bone.localBind;
        track.keys.push_back(bindKey);
        clip.tracks.push_back(std::move(track));
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton, candidate))
            return fail("new canonical track would be invalid");
        impl->canonicalAnimations = std::move(candidate);
        if (newIndexOut)
            *newIndexOut = static_cast<uint32_t>(impl->canonicalAnimations.clips[clipIndex].tracks.size() - 1);
        return true;
    }

    bool MESH_MBM_DEBUG::updateSkeletalTrackChannels(const uint32_t clipIndex,
                                                      const uint32_t trackIndex,
                                                      const uint8_t channelMask,
                                                      char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (clipIndex >= impl->canonicalAnimations.clips.size() ||
            trackIndex >= impl->canonicalAnimations.clips[clipIndex].tracks.size())
            return fail("canonical track index is out of range");
        if (channelMask == 0 || (channelMask & ~7u) != 0)
            return fail("canonical track channel mask must contain only T/R/S channels");
        skeletal::CANONICAL_ANIMATIONS candidate = impl->canonicalAnimations;
        candidate.clips[clipIndex].tracks[trackIndex].channelMask = channelMask;
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton, candidate))
            return fail("updated canonical track would be invalid");
        impl->canonicalAnimations = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::removeSkeletalTrack(const uint32_t clipIndex, const uint32_t trackIndex,
                                              char *errorOut, const int errorOutLen)
    {
        if (clipIndex >= impl->canonicalAnimations.clips.size() ||
            trackIndex >= impl->canonicalAnimations.clips[clipIndex].tracks.size())
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s",
                                                       "canonical track index is out of range");
            return false;
        }
        skeletal::CANONICAL_ANIMATIONS candidate = impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip = candidate.clips[clipIndex];
        clip.tracks.erase(clip.tracks.begin() + trackIndex);
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton, candidate))
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s",
                                                       "remaining canonical tracks would be invalid");
            return false;
        }
        impl->canonicalAnimations = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::addSkeletalKey(const uint32_t clipIndex, const uint32_t trackIndex,
                                         const float time, uint32_t *newIndexOut,
                                         char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (clipIndex >= impl->canonicalAnimations.clips.size() ||
            trackIndex >= impl->canonicalAnimations.clips[clipIndex].tracks.size())
            return fail("canonical track index is out of range");
        const skeletal::SKELETAL_CLIP &sourceClip = impl->canonicalAnimations.clips[clipIndex];
        if (!std::isfinite(time) || time < 0.0f || time > sourceClip.duration)
            return fail("canonical key time must be finite and inside the clip duration");
        const skeletal::SKELETAL_TRACK &sourceTrack = sourceClip.tracks[trackIndex];
        for (const skeletal::SKELETAL_KEY &key : sourceTrack.keys)
            if (std::fabs(key.time - time) <= skeletal::KEY_TIME_TOLERANCE)
                return fail("canonical track already has a key at this time");
        skeletal::SKELETAL_POSE sampledPose;
        if (!skeletal::sampleSkeletalClip(impl->canonicalSkeleton.compiled, sourceClip, time, sampledPose))
            return fail("canonical clip could not be sampled for key insertion");
        const auto found = impl->canonicalSkeleton.compiled.indexById.find(sourceTrack.boneId);
        if (found == impl->canonicalSkeleton.compiled.indexById.end() ||
            found->second < 0 ||
            static_cast<size_t>(found->second) >= sampledPose.localTransforms.size())
            return fail("canonical track target could not be resolved");
        skeletal::CANONICAL_ANIMATIONS candidate = impl->canonicalAnimations;
        skeletal::SKELETAL_TRACK &track = candidate.clips[clipIndex].tracks[trackIndex];
        skeletal::SKELETAL_KEY inserted;
        inserted.time = time;
        inserted.local = sampledPose.localTransforms[static_cast<size_t>(found->second)];
        const auto position = std::lower_bound(track.keys.begin(), track.keys.end(), time,
            [](const skeletal::SKELETAL_KEY &key, const float value) { return key.time < value; });
        const uint32_t index = static_cast<uint32_t>(position - track.keys.begin());
        track.keys.insert(position, inserted);
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton, candidate))
            return fail("new canonical key would be invalid");
        impl->canonicalAnimations = std::move(candidate);
        if (newIndexOut) *newIndexOut = index;
        return true;
    }

    bool MESH_MBM_DEBUG::updateSkeletalKey(const uint32_t clipIndex, const uint32_t trackIndex,
                                            const uint32_t keyIndex, const float time,
                                            const VEC3 &translation, const float rotationX,
                                            const float rotationY, const float rotationZ,
                                            const float rotationW, const VEC3 &scale,
                                            const uint8_t easing, const float bezierX1,
                                            const float bezierY1, const float bezierX2,
                                            const float bezierY2, char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (clipIndex >= impl->canonicalAnimations.clips.size() ||
            trackIndex >= impl->canonicalAnimations.clips[clipIndex].tracks.size() ||
            keyIndex >= impl->canonicalAnimations.clips[clipIndex].tracks[trackIndex].keys.size())
            return fail("canonical key index is out of range");
        const float quaternionNorm = std::sqrt(rotationX * rotationX + rotationY * rotationY +
                                               rotationZ * rotationZ + rotationW * rotationW);
        if (!std::isfinite(quaternionNorm) || quaternionNorm <= skeletal::QUATERNION_ZERO_EPSILON)
            return fail("canonical key rotation quaternion must be nonzero and finite");
        skeletal::CANONICAL_ANIMATIONS candidate = impl->canonicalAnimations;
        skeletal::SKELETAL_TRACK &track = candidate.clips[clipIndex].tracks[trackIndex];
        skeletal::SKELETAL_KEY edited = track.keys[keyIndex];
        edited.time = time;
        edited.local.translation = translation;
        edited.local.rotation = {rotationX / quaternionNorm, rotationY / quaternionNorm,
                                 rotationZ / quaternionNorm, rotationW / quaternionNorm};
        edited.local.scale = scale;
        edited.easing = static_cast<skeletal::SKELETAL_EASING>(easing);
        edited.bezierX1 = bezierX1; edited.bezierY1 = bezierY1;
        edited.bezierX2 = bezierX2; edited.bezierY2 = bezierY2;
        track.keys.erase(track.keys.begin() + keyIndex);
        const auto position = std::lower_bound(track.keys.begin(), track.keys.end(), time,
            [](const skeletal::SKELETAL_KEY &key, const float value) { return key.time < value; });
        track.keys.insert(position, edited);
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton, candidate))
            return fail("updated canonical key would be invalid or collide with another key time");
        impl->canonicalAnimations = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::removeSkeletalKey(const uint32_t clipIndex, const uint32_t trackIndex,
                                            const uint32_t keyIndex, char *errorOut, const int errorOutLen)
    {
        if (clipIndex >= impl->canonicalAnimations.clips.size() ||
            trackIndex >= impl->canonicalAnimations.clips[clipIndex].tracks.size() ||
            keyIndex >= impl->canonicalAnimations.clips[clipIndex].tracks[trackIndex].keys.size())
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s",
                                                       "canonical key index is out of range");
            return false;
        }
        if (impl->canonicalAnimations.clips[clipIndex].tracks[trackIndex].keys.size() <= 1)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s",
                                                       "canonical track must retain at least one key");
            return false;
        }
        skeletal::CANONICAL_ANIMATIONS candidate = impl->canonicalAnimations;
        candidate.clips[clipIndex].tracks[trackIndex].keys.erase(
            candidate.clips[clipIndex].tracks[trackIndex].keys.begin() + keyIndex);
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton, candidate))
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s",
                                                       "remaining canonical keys would be invalid");
            return false;
        }
        impl->canonicalAnimations = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::moveSkeletalKeys(const uint32_t clipIndex,
                                           const uint32_t *trackIndices,
                                           const uint32_t *keyIndices,
                                           const uint32_t keyCount,
                                           const float timeDelta,
                                           char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if (clipIndex>=impl->canonicalAnimations.clips.size())
            return fail("canonical clip index is out of range");
        if (!trackIndices || !keyIndices || keyCount==0)
            return fail("canonical key move selection must not be empty");
        if (!std::isfinite(timeDelta)) return fail("canonical key move delta must be finite");

        const skeletal::SKELETAL_CLIP &sourceClip=impl->canonicalAnimations.clips[clipIndex];
        std::unordered_set<uint64_t> references;
        references.reserve(keyCount);
        for (uint32_t index=0;index<keyCount;++index)
        {
            const uint32_t trackIndex=trackIndices[index];
            const uint32_t keyIndex=keyIndices[index];
            if (trackIndex>=sourceClip.tracks.size() ||
                    keyIndex>=sourceClip.tracks[trackIndex].keys.size())
                return fail("canonical key move index is out of range");
            const uint64_t reference=(static_cast<uint64_t>(trackIndex)<<32u)|keyIndex;
            if (!references.insert(reference).second)
                return fail("canonical key move selection contains a duplicate reference");
            const float movedTime=sourceClip.tracks[trackIndex].keys[keyIndex].time+timeDelta;
            if (!std::isfinite(movedTime) || movedTime<0.0f || movedTime>sourceClip.duration)
                return fail("canonical moved key time must remain inside the clip duration");
        }

        skeletal::CANONICAL_ANIMATIONS candidate=impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip=candidate.clips[clipIndex];
        std::unordered_set<uint32_t> affectedTracks;
        for (uint32_t index=0;index<keyCount;++index)
        {
            clip.tracks[trackIndices[index]].keys[keyIndices[index]].time+=timeDelta;
            affectedTracks.insert(trackIndices[index]);
        }
        for (const uint32_t trackIndex:affectedTracks)
        {
            std::stable_sort(clip.tracks[trackIndex].keys.begin(),clip.tracks[trackIndex].keys.end(),
                [](const skeletal::SKELETAL_KEY &a,const skeletal::SKELETAL_KEY &b)
                { return a.time<b.time; });
        }
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton,candidate))
            return fail("moved canonical keys would be invalid or collide with another key time");
        impl->canonicalAnimations=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::duplicateSkeletalKeys(const uint32_t clipIndex,
                                                const uint32_t *trackIndices,
                                                const uint32_t *keyIndices,
                                                const uint32_t keyCount,
                                                const float timeDelta,
                                                char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if (clipIndex>=impl->canonicalAnimations.clips.size())
            return fail("canonical clip index is out of range");
        if (!trackIndices || !keyIndices || keyCount==0)
            return fail("canonical key duplicate selection must not be empty");
        if (!std::isfinite(timeDelta)) return fail("canonical key duplicate delta must be finite");
        const skeletal::SKELETAL_CLIP &sourceClip=impl->canonicalAnimations.clips[clipIndex];
        std::unordered_set<uint64_t> references;
        references.reserve(keyCount);
        for (uint32_t index=0;index<keyCount;++index)
        {
            const uint32_t trackIndex=trackIndices[index];
            const uint32_t keyIndex=keyIndices[index];
            if (trackIndex>=sourceClip.tracks.size() ||
                    keyIndex>=sourceClip.tracks[trackIndex].keys.size())
                return fail("canonical key duplicate index is out of range");
            const uint64_t reference=(static_cast<uint64_t>(trackIndex)<<32u)|keyIndex;
            if (!references.insert(reference).second)
                return fail("canonical key duplicate selection contains a duplicate reference");
            const float newTime=sourceClip.tracks[trackIndex].keys[keyIndex].time+timeDelta;
            if (!std::isfinite(newTime) || newTime<0.0f || newTime>sourceClip.duration)
                return fail("canonical duplicated key time must remain inside the clip duration");
        }
        skeletal::CANONICAL_ANIMATIONS candidate=impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip=candidate.clips[clipIndex];
        std::unordered_set<uint32_t> affectedTracks;
        for (uint32_t index=0;index<keyCount;++index)
        {
            skeletal::SKELETAL_KEY copy=sourceClip.tracks[trackIndices[index]].keys[keyIndices[index]];
            copy.time+=timeDelta;
            clip.tracks[trackIndices[index]].keys.push_back(copy);
            affectedTracks.insert(trackIndices[index]);
        }
        for (const uint32_t trackIndex:affectedTracks)
        {
            std::stable_sort(clip.tracks[trackIndex].keys.begin(),clip.tracks[trackIndex].keys.end(),
                [](const skeletal::SKELETAL_KEY &a,const skeletal::SKELETAL_KEY &b)
                { return a.time<b.time; });
        }
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton,candidate))
            return fail("duplicated canonical keys would be invalid or collide with another key time");
        impl->canonicalAnimations=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::pasteSkeletalKeys(const uint32_t clipIndex, const uint64_t *boneIds,
                                            const uint8_t *channelMasks,
                                            const SKELETAL_KEY_INFO *keys,
                                            const uint32_t keyCount,
                                            const float sourceMinimumTime,
                                            const float insertionTime,
                                            char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if (clipIndex>=impl->canonicalAnimations.clips.size())
            return fail("canonical paste clip index is out of range");
        if (!boneIds || !channelMasks || !keys || keyCount==0)
            return fail("canonical paste payload must not be empty");
        if (!std::isfinite(sourceMinimumTime) || !std::isfinite(insertionTime) ||
                insertionTime<0.0f)
            return fail("canonical paste times must be finite and nonnegative");

        skeletal::CANONICAL_ANIMATIONS candidate=impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip=candidate.clips[clipIndex];
        std::unordered_set<uint32_t> affectedTracks;
        for (uint32_t index=0;index<keyCount;++index)
        {
            const uint64_t boneId=boneIds[index];
            const uint8_t channelMask=channelMasks[index];
            if (impl->canonicalSkeleton.compiled.indexById.find(boneId)==
                    impl->canonicalSkeleton.compiled.indexById.end())
                return fail("canonical paste bone ID does not exist in the destination skeleton");
            if (channelMask==0 || (channelMask & ~7u)!=0)
                return fail("canonical paste channel mask must contain only T/R/S channels");
            const float destinationTime=insertionTime+(keys[index].time-sourceMinimumTime);
            if (!std::isfinite(destinationTime) || destinationTime<0.0f ||
                    destinationTime>clip.duration)
                return fail("canonical pasted key time must remain inside the destination clip");
            auto trackIt=std::find_if(clip.tracks.begin(),clip.tracks.end(),
                [boneId](const skeletal::SKELETAL_TRACK &track){ return track.boneId==boneId; });
            if (trackIt==clip.tracks.end())
            {
                skeletal::SKELETAL_TRACK track;
                track.boneId=boneId;
                track.channelMask=channelMask;
                clip.tracks.push_back(std::move(track));
                trackIt=clip.tracks.end()-1;
            }
            else if (trackIt->channelMask!=channelMask)
                return fail("canonical paste destination track has a different channel mask");

            const float quaternionNorm=std::sqrt(keys[index].localRotationX*
                    keys[index].localRotationX+keys[index].localRotationY*
                    keys[index].localRotationY+keys[index].localRotationZ*
                    keys[index].localRotationZ+keys[index].localRotationW*
                    keys[index].localRotationW);
            if (!std::isfinite(quaternionNorm) ||
                    quaternionNorm<=skeletal::QUATERNION_ZERO_EPSILON)
                return fail("canonical pasted key rotation quaternion must be nonzero and finite");
            skeletal::SKELETAL_KEY pasted;
            pasted.time=destinationTime;
            pasted.local.translation=keys[index].localTranslation;
            pasted.local.rotation={keys[index].localRotationX/quaternionNorm,
                keys[index].localRotationY/quaternionNorm,
                keys[index].localRotationZ/quaternionNorm,
                keys[index].localRotationW/quaternionNorm};
            pasted.local.scale=keys[index].localScale;
            pasted.easing=static_cast<skeletal::SKELETAL_EASING>(keys[index].easing);
            pasted.bezierX1=keys[index].bezierX1;
            pasted.bezierY1=keys[index].bezierY1;
            pasted.bezierX2=keys[index].bezierX2;
            pasted.bezierY2=keys[index].bezierY2;
            trackIt->keys.push_back(pasted);
            affectedTracks.insert(static_cast<uint32_t>(trackIt-clip.tracks.begin()));
        }
        for (const uint32_t trackIndex:affectedTracks)
        {
            std::stable_sort(clip.tracks[trackIndex].keys.begin(),clip.tracks[trackIndex].keys.end(),
                [](const skeletal::SKELETAL_KEY &a,const skeletal::SKELETAL_KEY &b)
                { return a.time<b.time; });
        }
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton,candidate))
            return fail("pasted canonical keys would be invalid or collide at the destination");
        impl->canonicalAnimations=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::insertSkeletalKeysRipple(const uint32_t clipIndex,
                                                   const uint32_t *trackIndices,
                                                   const uint32_t *keyIndices,
                                                   const uint32_t keyCount,
                                                   const float insertionTime,
                                                   char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if (clipIndex>=impl->canonicalAnimations.clips.size())
            return fail("canonical clip index is out of range");
        if (!trackIndices || !keyIndices || keyCount==0)
            return fail("canonical ripple selection must not be empty");
        const skeletal::SKELETAL_CLIP &sourceClip=impl->canonicalAnimations.clips[clipIndex];
        if (!std::isfinite(insertionTime) || insertionTime<0.0f || insertionTime>sourceClip.duration)
            return fail("canonical ripple insertion time must be inside the clip duration");
        std::unordered_set<uint64_t> references;
        references.reserve(keyCount);
        float minimumTime=std::numeric_limits<float>::max();
        float maximumTime=-std::numeric_limits<float>::max();
        for (uint32_t index=0;index<keyCount;++index)
        {
            const uint32_t trackIndex=trackIndices[index];
            const uint32_t keyIndex=keyIndices[index];
            if (trackIndex>=sourceClip.tracks.size() ||
                    keyIndex>=sourceClip.tracks[trackIndex].keys.size())
                return fail("canonical ripple key index is out of range");
            const uint64_t reference=(static_cast<uint64_t>(trackIndex)<<32u)|keyIndex;
            if (!references.insert(reference).second)
                return fail("canonical ripple selection contains a duplicate reference");
            const float time=sourceClip.tracks[trackIndex].keys[keyIndex].time;
            minimumTime=std::min(minimumTime,time);
            maximumTime=std::max(maximumTime,time);
        }
        const float span=maximumTime-minimumTime;
        if (!std::isfinite(span) || span<=skeletal::KEY_TIME_TOLERANCE)
            return fail("canonical ripple selection must span a positive duration");
        const float shift=span+skeletal::KEY_TIME_TOLERANCE*2.0f;

        skeletal::CANONICAL_ANIMATIONS candidate=impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip=candidate.clips[clipIndex];
        clip.duration+=shift;
        if (!std::isfinite(clip.duration))
            return fail("canonical ripple insertion would make the clip duration invalid");
        for (skeletal::SKELETAL_TRACK &track:clip.tracks)
        {
            for (skeletal::SKELETAL_KEY &key:track.keys)
            {
                if (key.time+skeletal::KEY_TIME_TOLERANCE>=insertionTime)
                {
                    key.time+=shift;
                    if (!std::isfinite(key.time))
                        return fail("canonical ripple shift would make a key time invalid");
                }
            }
        }
        for (uint32_t index=0;index<keyCount;++index)
        {
            skeletal::SKELETAL_KEY copy=sourceClip.tracks[trackIndices[index]].keys[keyIndices[index]];
            copy.time=insertionTime+(copy.time-minimumTime);
            clip.tracks[trackIndices[index]].keys.push_back(copy);
        }
        for (skeletal::SKELETAL_TRACK &track:clip.tracks)
        {
            std::stable_sort(track.keys.begin(),track.keys.end(),
                [](const skeletal::SKELETAL_KEY &a,const skeletal::SKELETAL_KEY &b)
                { return a.time<b.time; });
        }
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton,candidate))
            return fail("ripple-inserted canonical keys would collide or be invalid");
        impl->canonicalAnimations=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::insertSkeletalEmptyTime(const uint32_t clipIndex,
                                                  const float insertionTime,
                                                  const float duration,
                                                  char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if (clipIndex>=impl->canonicalAnimations.clips.size())
            return fail("canonical clip index is out of range");
        const skeletal::SKELETAL_CLIP &sourceClip=impl->canonicalAnimations.clips[clipIndex];
        if (!std::isfinite(insertionTime) || insertionTime<0.0f || insertionTime>sourceClip.duration)
            return fail("canonical empty-time insertion must be inside the clip duration");
        if (!std::isfinite(duration) || duration<=skeletal::KEY_TIME_TOLERANCE)
            return fail("canonical empty-time duration must be positive and finite");

        skeletal::CANONICAL_ANIMATIONS candidate=impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip=candidate.clips[clipIndex];
        clip.duration+=duration;
        if (!std::isfinite(clip.duration))
            return fail("canonical empty-time insertion would make the clip duration invalid");
        for (skeletal::SKELETAL_TRACK &track:clip.tracks)
        {
            for (skeletal::SKELETAL_KEY &key:track.keys)
            {
                if (key.time+skeletal::KEY_TIME_TOLERANCE>=insertionTime)
                {
                    key.time+=duration;
                    if (!std::isfinite(key.time))
                        return fail("canonical empty-time insertion would make a key time invalid");
                }
            }
        }
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton,candidate))
            return fail("empty-time-inserted canonical keys would be invalid");
        impl->canonicalAnimations=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::removeSkeletalTimeRange(const uint32_t clipIndex,
                                                  const float startTime,
                                                  const float duration,
                                                  uint32_t *removedKeyCountOut,
                                                  char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if (removedKeyCountOut) *removedKeyCountOut=0;
        if (clipIndex>=impl->canonicalAnimations.clips.size())
            return fail("canonical clip index is out of range");
        const skeletal::SKELETAL_CLIP &sourceClip=impl->canonicalAnimations.clips[clipIndex];
        if (!std::isfinite(startTime) || startTime<0.0f || startTime>=sourceClip.duration)
            return fail("canonical time removal must begin inside the clip duration");
        if (!std::isfinite(duration) || duration<=skeletal::KEY_TIME_TOLERANCE)
            return fail("canonical time-removal duration must be positive and finite");
        const float endTime=std::min(sourceClip.duration,startTime+duration);
        const float removedDuration=endTime-startTime;
        if (!std::isfinite(endTime) || removedDuration<=skeletal::KEY_TIME_TOLERANCE)
            return fail("canonical time-removal interval must have a positive duration");

        skeletal::CANONICAL_ANIMATIONS candidate=impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip=candidate.clips[clipIndex];
        uint32_t removedKeyCount=0;
        for (skeletal::SKELETAL_TRACK &track:clip.tracks)
        {
            const size_t oldSize=track.keys.size();
            track.keys.erase(std::remove_if(track.keys.begin(),track.keys.end(),
                [startTime,endTime](const skeletal::SKELETAL_KEY &key)
                {
                    return key.time+skeletal::KEY_TIME_TOLERANCE>=startTime &&
                           key.time<endTime-skeletal::KEY_TIME_TOLERANCE;
                }),track.keys.end());
            removedKeyCount+=static_cast<uint32_t>(oldSize-track.keys.size());
            if (track.keys.empty())
                return fail("canonical time removal would leave a track without keys");
            for (skeletal::SKELETAL_KEY &key:track.keys)
            {
                if (key.time+skeletal::KEY_TIME_TOLERANCE>=endTime)
                    key.time-=removedDuration;
            }
        }
        clip.duration-=removedDuration;
        if (!std::isfinite(clip.duration) || clip.duration<=skeletal::KEY_TIME_TOLERANCE)
            return fail("canonical time removal would leave an invalid clip duration");
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton,candidate))
            return fail("time-removed canonical keys would collide or be invalid");
        impl->canonicalAnimations=std::move(candidate);
        if (removedKeyCountOut) *removedKeyCountOut=removedKeyCount;
        return true;
    }

    bool MESH_MBM_DEBUG::commitSkeletalAuthoringKey(const uint32_t clipIndex,
                                                     const uint32_t boneIndex,
                                                     const float time,
                                                     const uint8_t channelMask,
                                                     const SKELETAL_KEY_INFO &local,
                                                     bool *createdKeyOut, char *errorOut,
                                                     const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if (clipIndex>=impl->canonicalAnimations.clips.size())
            return fail("canonical authoring clip index is out of range");
        if (boneIndex>=impl->canonicalSkeleton.compiled.bones.size())
            return fail("canonical authoring bone index is out of range");
        if (channelMask==0 || (channelMask & ~7u)!=0)
            return fail("canonical authoring key channels must contain only T/R/S");
        const skeletal::SKELETAL_CLIP &sourceClip=impl->canonicalAnimations.clips[clipIndex];
        if (!std::isfinite(time) || time<0.0f || time>sourceClip.duration)
            return fail("canonical authoring key time must be inside the clip duration");
        const float quaternionNorm=std::sqrt(local.localRotationX*local.localRotationX+
            local.localRotationY*local.localRotationY+local.localRotationZ*local.localRotationZ+
            local.localRotationW*local.localRotationW);
        if (!std::isfinite(quaternionNorm) || quaternionNorm<=skeletal::QUATERNION_ZERO_EPSILON)
            return fail("canonical authoring key rotation must be nonzero and finite");

        skeletal::LOCAL_TRANSFORM transform;
        transform.translation=local.localTranslation;
        transform.rotation={local.localRotationX/quaternionNorm,local.localRotationY/quaternionNorm,
                            local.localRotationZ/quaternionNorm,local.localRotationW/quaternionNorm};
        transform.scale=local.localScale;
        skeletal::CANONICAL_ANIMATIONS candidate=impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip=candidate.clips[clipIndex];
        const uint64_t boneId=impl->canonicalSkeleton.compiled.bones[boneIndex].boneId;
        auto trackIt=std::find_if(clip.tracks.begin(),clip.tracks.end(),
            [boneId](const skeletal::SKELETAL_TRACK &track){ return track.boneId==boneId; });
        if (trackIt==clip.tracks.end())
        {
            skeletal::SKELETAL_TRACK track;
            track.boneId=boneId;
            track.channelMask=channelMask;
            skeletal::SKELETAL_KEY bindKey;
            bindKey.time=0.0f;
            bindKey.local=impl->canonicalSkeleton.compiled.bones[boneIndex].localBind;
            track.keys.push_back(bindKey);
            clip.tracks.push_back(std::move(track));
            trackIt=clip.tracks.end()-1;
        }
        else trackIt->channelMask|=channelMask;

        auto keyIt=std::find_if(trackIt->keys.begin(),trackIt->keys.end(),[time](const skeletal::SKELETAL_KEY &key)
        { return std::fabs(key.time-time)<=skeletal::KEY_TIME_TOLERANCE; });
        const bool created=keyIt==trackIt->keys.end();
        if (created)
        {
            skeletal::SKELETAL_KEY key;
            key.time=time;
            key.local=transform;
            const auto position=std::lower_bound(trackIt->keys.begin(),trackIt->keys.end(),time,
                [](const skeletal::SKELETAL_KEY &key,const float value){ return key.time<value; });
            trackIt->keys.insert(position,key);
        }
        else keyIt->local=transform;
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton,candidate))
            return fail("committed canonical authoring key would be invalid");
        impl->canonicalAnimations=std::move(candidate);
        if (createdKeyOut) *createdKeyOut=created;
        return true;
    }

    bool MESH_MBM_DEBUG::commitSkeletalAuthoringPose(const uint32_t clipIndex,
                                                      const float time,
                                                      const uint64_t *boneIds,
                                                      const SKELETAL_KEY_INFO *locals,
                                                      const uint32_t boneCount,
                                                      char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if (clipIndex>=impl->canonicalAnimations.clips.size())
            return fail("canonical authoring pose clip index is out of range");
        if (!boneIds || !locals || boneCount==0 ||
                boneCount!=impl->canonicalSkeleton.compiled.bones.size())
            return fail("canonical authoring pose must contain every skeleton bone exactly once");
        const skeletal::SKELETAL_CLIP &sourceClip=impl->canonicalAnimations.clips[clipIndex];
        if (!std::isfinite(time) || time<0.0f || time>sourceClip.duration)
            return fail("canonical authoring pose time must be inside the clip duration");

        skeletal::CANONICAL_ANIMATIONS candidate=impl->canonicalAnimations;
        skeletal::SKELETAL_CLIP &clip=candidate.clips[clipIndex];
        std::unordered_set<uint64_t> seenBoneIds;
        seenBoneIds.reserve(boneCount);
        for (uint32_t itemIndex=0;itemIndex<boneCount;++itemIndex)
        {
            const uint64_t boneId=boneIds[itemIndex];
            if (!seenBoneIds.insert(boneId).second)
                return fail("canonical authoring pose contains a duplicate bone ID");
            const auto boneFound=impl->canonicalSkeleton.compiled.indexById.find(boneId);
            if (boneFound==impl->canonicalSkeleton.compiled.indexById.end())
                return fail("canonical authoring pose contains an unknown bone ID");
            const SKELETAL_KEY_INFO &local=locals[itemIndex];
            const float quaternionNorm=std::sqrt(local.localRotationX*local.localRotationX+
                local.localRotationY*local.localRotationY+local.localRotationZ*local.localRotationZ+
                local.localRotationW*local.localRotationW);
            if (!std::isfinite(quaternionNorm) ||
                    quaternionNorm<=skeletal::QUATERNION_ZERO_EPSILON)
                return fail("canonical authoring pose rotation must be nonzero and finite");
            skeletal::LOCAL_TRANSFORM transform;
            transform.translation=local.localTranslation;
            transform.rotation={local.localRotationX/quaternionNorm,
                local.localRotationY/quaternionNorm,local.localRotationZ/quaternionNorm,
                local.localRotationW/quaternionNorm};
            transform.scale=local.localScale;
            auto trackIt=std::find_if(clip.tracks.begin(),clip.tracks.end(),
                [boneId](const skeletal::SKELETAL_TRACK &track){ return track.boneId==boneId; });
            if (trackIt==clip.tracks.end())
            {
                skeletal::SKELETAL_TRACK track;
                track.boneId=boneId;
                track.channelMask=skeletal::SKELETAL_CHANNEL_TRANSLATION|
                    skeletal::SKELETAL_CHANNEL_ROTATION|skeletal::SKELETAL_CHANNEL_SCALE;
                skeletal::SKELETAL_KEY bindKey;
                bindKey.time=0.0f;
                bindKey.local=impl->canonicalSkeleton.compiled.bones[boneFound->second].localBind;
                track.keys.push_back(bindKey);
                clip.tracks.push_back(std::move(track));
                trackIt=clip.tracks.end()-1;
            }
            else trackIt->channelMask|=skeletal::SKELETAL_CHANNEL_TRANSLATION|
                skeletal::SKELETAL_CHANNEL_ROTATION|skeletal::SKELETAL_CHANNEL_SCALE;
            auto keyIt=std::find_if(trackIt->keys.begin(),trackIt->keys.end(),
                [time](const skeletal::SKELETAL_KEY &key)
                { return std::fabs(key.time-time)<=skeletal::KEY_TIME_TOLERANCE; });
            if (keyIt==trackIt->keys.end())
            {
                skeletal::SKELETAL_KEY key;
                key.time=time;
                key.local=transform;
                const auto position=std::lower_bound(trackIt->keys.begin(),trackIt->keys.end(),time,
                    [](const skeletal::SKELETAL_KEY &key,const float value)
                    { return key.time<value; });
                trackIt->keys.insert(position,key);
            }
            else keyIt->local=transform;
        }
        if (!skeletal::validateCanonicalAnimations(impl->canonicalSkeleton,candidate))
            return fail("committed canonical authoring pose would be invalid");
        impl->canonicalAnimations=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::evaluateSkeletalAuthoringPose(const uint32_t clipIndex, const float time,
                                                        const int32_t overrideBoneIndex,
                                                        const SKELETAL_KEY_INFO *overrideLocal,
                                                        const SKELETAL_SHADER_METHOD method,
                                                        char *errorOut, const int errorOutLen)
    {
        const auto fail = [this, errorOut, errorOutLen](const char *message)
        {
            impl->authoringPose = {};
            impl->authoringPaletteRows.clear();
            impl->authoringPoseValid = false;
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (clipIndex >= impl->canonicalAnimations.clips.size())
            return fail("canonical authoring clip index is out of range");
        if (method != SKELETAL_SHADER_METHOD::LBS && method != SKELETAL_SHADER_METHOD::DQS_RIGID)
            return fail("canonical authoring pose requires resolved LBS or DQS");
        skeletal::SKELETAL_POSE pose;
        if (!skeletal::sampleSkeletalClip(impl->canonicalSkeleton.compiled,
                impl->canonicalAnimations.clips[clipIndex], time, pose))
            return fail("canonical authoring pose could not be sampled");
        if (overrideLocal)
        {
            if (overrideBoneIndex < 0 || static_cast<size_t>(overrideBoneIndex) >= pose.localTransforms.size())
                return fail("canonical authoring override bone index is out of range");
            const float norm = std::sqrt(overrideLocal->localRotationX * overrideLocal->localRotationX +
                overrideLocal->localRotationY * overrideLocal->localRotationY +
                overrideLocal->localRotationZ * overrideLocal->localRotationZ +
                overrideLocal->localRotationW * overrideLocal->localRotationW);
            if (!std::isfinite(norm) || norm <= skeletal::QUATERNION_ZERO_EPSILON)
                return fail("canonical authoring override quaternion must be nonzero and finite");
            skeletal::LOCAL_TRANSFORM &local = pose.localTransforms[static_cast<size_t>(overrideBoneIndex)];
            local.translation = overrideLocal->localTranslation;
            local.rotation = {overrideLocal->localRotationX / norm, overrideLocal->localRotationY / norm,
                              overrideLocal->localRotationZ / norm, overrideLocal->localRotationW / norm};
            local.scale = overrideLocal->localScale;
            for (size_t boneIndex = 0; boneIndex < pose.localTransforms.size(); ++boneIndex)
            {
                const MATRIX localMatrix = skeletal::buildTrsMatrix(pose.localTransforms[boneIndex]);
                const int32_t parent = impl->canonicalSkeleton.compiled.bones[boneIndex].parentIndex;
                if (parent < 0) pose.globalTransforms[boneIndex] = localMatrix;
                else MatrixMultiply(&pose.globalTransforms[boneIndex], &localMatrix,
                                    &pose.globalTransforms[static_cast<size_t>(parent)]);
            }
        }
        std::vector<float> rows;
        if (method == SKELETAL_SHADER_METHOD::DQS_RIGID)
        {
            if (skeletal::buildDqsPalette(impl->canonicalSkeleton, pose, rows) !=
                    skeletal::DQS_PALETTE_STATUS::READY)
                return fail("canonical authoring pose is incompatible with rigid DQS");
        }
        else if (skeletal::buildLbsPalette(impl->canonicalSkeleton, pose, true, rows) !=
                     skeletal::LBS_PALETTE_STATUS::READY)
            return fail("canonical authoring pose is incompatible with compact LBS normals");
        impl->authoringPose = std::move(pose);
        impl->authoringPaletteRows = std::move(rows);
        impl->authoringPoseValid = true;
        return true;
    }

    uint32_t MESH_MBM_DEBUG::getSkeletalAuthoringPoseBoneCount() const noexcept
    {
        return impl->authoringPoseValid ? static_cast<uint32_t>(impl->authoringPose.localTransforms.size()) : 0;
    }

    bool MESH_MBM_DEBUG::getSkeletalAuthoringPoseBone(const uint32_t boneIndex,
                                                       SKELETAL_POSE_BONE_INFO &out) const noexcept
    {
        if (!impl->authoringPoseValid || boneIndex >= impl->authoringPose.localTransforms.size() ||
            boneIndex >= impl->authoringPose.globalTransforms.size()) return false;
        const skeletal::LOCAL_TRANSFORM &local = impl->authoringPose.localTransforms[boneIndex];
        out.boneId = impl->canonicalSkeleton.compiled.bones[boneIndex].boneId;
        out.localTranslation = local.translation;
        out.localRotationX = local.rotation.x; out.localRotationY = local.rotation.y;
        out.localRotationZ = local.rotation.z; out.localRotationW = local.rotation.w;
        out.localScale = local.scale;
        out.globalMatrix = impl->authoringPose.globalTransforms[boneIndex];
        return true;
    }

    uint32_t MESH_MBM_DEBUG::getSkeletalAuthoringPaletteSize() const noexcept
    {
        return impl->authoringPoseValid ? static_cast<uint32_t>(impl->authoringPaletteRows.size()) : 0;
    }

    bool MESH_MBM_DEBUG::copySkeletalAuthoringPalette(float *rows, const uint32_t rowCount) const noexcept
    {
        if (!impl->authoringPoseValid || !rows || rowCount != impl->authoringPaletteRows.size()) return false;
        std::copy(impl->authoringPaletteRows.begin(), impl->authoringPaletteRows.end(), rows);
        return true;
    }

    bool MESH_MBM_DEBUG::renameSkeletalBone(const uint32_t index, const char *name,
                                             char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (index >= impl->canonicalSkeleton.sourceBones.size())
            return fail("canonical bone index is out of range");
        if (!name || !name[0])
            return fail("canonical bone name must not be empty");
        for (uint32_t other = 0; other < impl->canonicalSkeleton.sourceBones.size(); ++other)
            if (other != index && impl->canonicalSkeleton.sourceBones[other].name == name)
                return fail("canonical bone name must be unique");

        skeletal::CANONICAL_SKELETON candidate = impl->canonicalSkeleton;
        candidate.sourceBones[index].name = name;
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("renamed canonical skeleton would be invalid");
        if (impl->canonicalWeights.skeletonId != 0)
        {
            if (impl->canonicalWeights.frameIndex >= impl->buffer.size())
                return fail("canonical weight frame is out of range");
            const util::BUFFER_MESH_DEBUG *frame = impl->buffer[impl->canonicalWeights.frameIndex];
            uint32_t vertexCount = 0;
            for (const util::SUBSET_DEBUG *subset : frame->subset)
                vertexCount += static_cast<uint32_t>(subset->vertexCount);
            if (!skeletal::validateCanonicalWeights(candidate, impl->canonicalWeights, vertexCount))
                return fail("canonical weights would be invalid after bone rename");
        }
        if (impl->canonicalAnimations.skeletonId != 0 &&
            !skeletal::validateCanonicalAnimations(candidate, impl->canonicalAnimations))
            return fail("canonical animations would be invalid after bone rename");
        impl->canonicalSkeleton = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::reparentSkeletalBone(const uint32_t index, const int32_t newParentIndex,
                                               const bool preserveGlobalBind,
                                               char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        const auto &source = impl->canonicalSkeleton.sourceBones;
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (index >= source.size())
            return fail("canonical bone index is out of range");
        if (newParentIndex < -1 || newParentIndex >= static_cast<int32_t>(source.size()))
            return fail("canonical parent index is out of range");
        if (newParentIndex == static_cast<int32_t>(index))
            return fail("canonical bone cannot be its own parent");

        const uint64_t boneId = source[index].boneId;
        const uint64_t newParentId = newParentIndex < 0 ? 0 : source[static_cast<uint32_t>(newParentIndex)].boneId;
        uint64_t cursor = newParentId;
        while (cursor != 0)
        {
            if (cursor == boneId)
                return fail("canonical reparent would create a hierarchy cycle");
            const auto found = impl->canonicalSkeleton.compiled.indexById.find(cursor);
            if (found == impl->canonicalSkeleton.compiled.indexById.end())
                return fail("canonical parent chain is invalid");
            cursor = source[static_cast<uint32_t>(found->second)].parentBoneId;
        }

        skeletal::CANONICAL_SKELETON candidate = impl->canonicalSkeleton;
        skeletal::CANONICAL_BONE &edited = candidate.sourceBones[index];
        edited.parentBoneId = newParentId;
        edited.connectedToParent = false;
        if (preserveGlobalBind)
        {
            MATRIX local = impl->canonicalSkeleton.compiled.bones[index].globalBindMatrix;
            if (newParentIndex >= 0)
            {
                MATRIX inverseParent;
                float determinant = 0.0f;
                MatrixInverse(&inverseParent, &determinant,
                    &impl->canonicalSkeleton.compiled.bones[static_cast<uint32_t>(newParentIndex)].globalBindMatrix);
                if (!std::isfinite(determinant) || std::fabs(determinant) <= skeletal::SINGULAR_TOLERANCE)
                    return fail("new canonical parent bind transform is not invertible");
                MatrixMultiply(&local, &impl->canonicalSkeleton.compiled.bones[index].globalBindMatrix,
                               &inverseParent);
            }
            bool hasNegativeScale = false, hasShear = false;
            if (!skeletal::decomposeTrsMatrix(local, edited.localBind, hasNegativeScale, hasShear) || hasShear)
                return fail("preserving global bind would require unsupported shear");
        }

        // Stable topological ordering: repeatedly append every bone whose parent is already placed.
        std::vector<skeletal::CANONICAL_BONE> ordered;
        ordered.reserve(candidate.sourceBones.size());
        std::unordered_set<uint64_t> placed;
        while (ordered.size() < candidate.sourceBones.size())
        {
            bool progress = false;
            for (const skeletal::CANONICAL_BONE &bone : candidate.sourceBones)
            {
                if (placed.find(bone.boneId) != placed.end()) continue;
                if (bone.parentBoneId == 0 || placed.find(bone.parentBoneId) != placed.end())
                {
                    ordered.push_back(bone);
                    placed.insert(bone.boneId);
                    progress = true;
                }
            }
            if (!progress) return fail("canonical reparent could not produce parent-first ordering");
        }
        candidate.sourceBones = std::move(ordered);
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("reparented canonical skeleton would be invalid");

        if (impl->canonicalWeights.skeletonId != 0)
        {
            if (impl->canonicalWeights.frameIndex >= impl->buffer.size())
                return fail("canonical weight frame is out of range");
            const util::BUFFER_MESH_DEBUG *frame = impl->buffer[impl->canonicalWeights.frameIndex];
            uint32_t vertexCount = 0;
            for (const util::SUBSET_DEBUG *subset : frame->subset)
                vertexCount += static_cast<uint32_t>(subset->vertexCount);
            if (!skeletal::validateCanonicalWeights(candidate, impl->canonicalWeights, vertexCount))
                return fail("canonical weights would be invalid after reparent");
        }
        if (impl->canonicalAnimations.skeletonId != 0 &&
            !skeletal::validateCanonicalAnimations(candidate, impl->canonicalAnimations))
            return fail("canonical animations would be invalid after reparent");
        impl->canonicalSkeleton = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::setSkeletalBoneBind(const uint32_t index, const VEC3 &translation,
                                              const float rotationX, const float rotationY,
                                              const float rotationZ, const float rotationW,
                                              const VEC3 &scale, const float radius, const float length,
                                              char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (index >= impl->canonicalSkeleton.sourceBones.size())
            return fail("canonical bone index is out of range");
        const float values[] = {translation.x, translation.y, translation.z,
                                rotationX, rotationY, rotationZ, rotationW,
                                scale.x, scale.y, scale.z, radius, length};
        for (const float value : values)
            if (!std::isfinite(value)) return fail("canonical bone bind values must be finite");
        if (radius < 0.0f || length < 0.0f)
            return fail("canonical bone radius and length must not be negative");
        const float quaternionNorm = std::sqrt(rotationX * rotationX + rotationY * rotationY +
                                               rotationZ * rotationZ + rotationW * rotationW);
        if (quaternionNorm <= skeletal::QUATERNION_ZERO_EPSILON)
            return fail("canonical bone bind quaternion must not be zero");

        skeletal::CANONICAL_SKELETON candidate = impl->canonicalSkeleton;
        skeletal::CANONICAL_BONE &edited = candidate.sourceBones[index];
        if (edited.connectedToParent)
        {
            const auto parent = candidate.compiled.indexById.find(edited.parentBoneId);
            if (parent != candidate.compiled.indexById.end())
            {
                const VEC3 &parentTail = candidate.sourceBones[static_cast<uint32_t>(parent->second)].tailOffset;
                if (std::fabs(translation.x-parentTail.x)>skeletal::MATRIX_TOLERANCE ||
                    std::fabs(translation.y-parentTail.y)>skeletal::MATRIX_TOLERANCE ||
                    std::fabs(translation.z-parentTail.z)>skeletal::MATRIX_TOLERANCE)
                    edited.connectedToParent = false;
            }
        }
        edited.localBind.translation = translation;
        edited.localBind.rotation.x = rotationX / quaternionNorm;
        edited.localBind.rotation.y = rotationY / quaternionNorm;
        edited.localBind.rotation.z = rotationZ / quaternionNorm;
        edited.localBind.rotation.w = rotationW / quaternionNorm;
        edited.localBind.scale = scale;
        edited.radius = radius;
        edited.length = length;
        const float oldLength = std::sqrt(edited.tailOffset.x * edited.tailOffset.x +
                                          edited.tailOffset.y * edited.tailOffset.y +
                                          edited.tailOffset.z * edited.tailOffset.z);
        if (oldLength > skeletal::SINGULAR_TOLERANCE)
        {
            edited.tailOffset.x *= length / oldLength;
            edited.tailOffset.y *= length / oldLength;
            edited.tailOffset.z *= length / oldLength;
        }
        else edited.tailOffset = VEC3(0.0f, length, 0.0f);
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("edited canonical bone bind would be invalid");
        if (impl->canonicalWeights.skeletonId != 0)
        {
            if (impl->canonicalWeights.frameIndex >= impl->buffer.size())
                return fail("canonical weight frame is out of range");
            const util::BUFFER_MESH_DEBUG *frame = impl->buffer[impl->canonicalWeights.frameIndex];
            uint32_t vertexCount = 0;
            for (const util::SUBSET_DEBUG *subset : frame->subset)
                vertexCount += static_cast<uint32_t>(subset->vertexCount);
            if (!skeletal::validateCanonicalWeights(candidate, impl->canonicalWeights, vertexCount))
                return fail("canonical weights would be invalid after bind edit");
        }
        if (impl->canonicalAnimations.skeletonId != 0 &&
            !skeletal::validateCanonicalAnimations(candidate, impl->canonicalAnimations))
            return fail("canonical animations would be invalid after bind edit");
        impl->canonicalSkeleton = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::addSkeletalBone(const int32_t parentIndex, const char *name,
                                          const VEC3 &translation, const float radius, const float length,
                                          const bool hasExplicitTail, const bool connectedToParent,
                                          uint32_t *newIndexOut, char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        const auto &source = impl->canonicalSkeleton.sourceBones;
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (parentIndex < -1 || parentIndex >= static_cast<int32_t>(source.size()))
            return fail("canonical parent index is out of range");
        if (!name || !name[0]) return fail("canonical bone name must not be empty");
        if (!std::isfinite(translation.x) || !std::isfinite(translation.y) ||
            !std::isfinite(translation.z) || !std::isfinite(radius) || !std::isfinite(length))
            return fail("canonical bone values must be finite");
        if (radius < 0.0f || length < 0.0f)
            return fail("canonical bone radius and length must not be negative");
        if (impl->canonicalSkeleton.compiled.indexByName.find(name) !=
            impl->canonicalSkeleton.compiled.indexByName.end())
            return fail("canonical bone name must be unique");

        uint64_t boneId = 1;
        while (impl->canonicalSkeleton.compiled.indexById.find(boneId) !=
               impl->canonicalSkeleton.compiled.indexById.end())
        {
            if (boneId == std::numeric_limits<uint64_t>::max())
                return fail("canonical bone ID space is exhausted");
            ++boneId;
        }
        skeletal::CANONICAL_SKELETON candidate = impl->canonicalSkeleton;
        skeletal::CANONICAL_BONE added;
        added.boneId = boneId;
        added.parentBoneId = parentIndex < 0 ? 0 : source[static_cast<uint32_t>(parentIndex)].boneId;
        added.name = name;
        added.localBind.translation = translation;
        added.radius = radius;
        added.length = length;
        added.tailOffset = VEC3(0.0f, hasExplicitTail ? length : 0.0f, 0.0f);
        if (hasExplicitTail && connectedToParent && parentIndex >= 0)
        {
            const VEC3 &parentTail = source[static_cast<uint32_t>(parentIndex)].tailOffset;
            const float parentTailLength = std::sqrt(parentTail.x * parentTail.x +
                                                     parentTail.y * parentTail.y +
                                                     parentTail.z * parentTail.z);
            if (parentTailLength > skeletal::SINGULAR_TOLERANCE)
                added.tailOffset = VEC3(parentTail.x * length / parentTailLength,
                                        parentTail.y * length / parentTailLength,
                                        parentTail.z * length / parentTailLength);
        }
        added.hasExplicitTail = hasExplicitTail;
        added.connectedToParent = parentIndex >= 0 && connectedToParent;
        candidate.sourceBones.push_back(std::move(added));
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("added canonical bone would be invalid");
        if (impl->canonicalWeights.skeletonId != 0)
        {
            if (impl->canonicalWeights.frameIndex >= impl->buffer.size())
                return fail("canonical weight frame is out of range");
            const util::BUFFER_MESH_DEBUG *frame = impl->buffer[impl->canonicalWeights.frameIndex];
            uint32_t vertexCount = 0;
            for (const util::SUBSET_DEBUG *subset : frame->subset)
                vertexCount += static_cast<uint32_t>(subset->vertexCount);
            if (!skeletal::validateCanonicalWeights(candidate, impl->canonicalWeights, vertexCount))
                return fail("canonical weights would be invalid after adding bone");
        }
        if (impl->canonicalAnimations.skeletonId != 0 &&
            !skeletal::validateCanonicalAnimations(candidate, impl->canonicalAnimations))
            return fail("canonical animations would be invalid after adding bone");
        impl->canonicalSkeleton = std::move(candidate);
        if (newIndexOut) *newIndexOut = static_cast<uint32_t>(impl->canonicalSkeleton.sourceBones.size() - 1);
        return true;
    }

    bool MESH_MBM_DEBUG::setSkeletalBoneTail(const uint32_t index, const VEC3 &tailOffset,
                                              const bool hasExplicitTail,
                                              const bool preserveOtherJoints,
                                              char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (index >= impl->canonicalSkeleton.sourceBones.size())
            return fail("canonical bone index is out of range");
        if (!std::isfinite(tailOffset.x) || !std::isfinite(tailOffset.y) ||
            !std::isfinite(tailOffset.z))
            return fail("canonical bone tail must be finite");
        const float length = std::sqrt(tailOffset.x * tailOffset.x + tailOffset.y * tailOffset.y +
                                       tailOffset.z * tailOffset.z);
        if (hasExplicitTail && length <= skeletal::SINGULAR_TOLERANCE)
            return fail("explicit canonical bone tail must differ from its head");

        skeletal::CANONICAL_SKELETON candidate = impl->canonicalSkeleton;
        skeletal::CANONICAL_BONE &edited = candidate.sourceBones[index];
        std::vector<MATRIX> oldGlobals;
        oldGlobals.reserve(candidate.compiled.bones.size());
        for (const skeletal::COMPILED_BONE &bone : candidate.compiled.bones)
            oldGlobals.push_back(bone.globalBindMatrix);
        std::vector<bool> movedConnectedChildren(candidate.sourceBones.size(), false);
        std::vector<std::pair<uint32_t, VEC3>> connectedChildTails;
        for (uint32_t childIndex = 0; childIndex < candidate.sourceBones.size(); ++childIndex)
        {
            const skeletal::CANONICAL_BONE &child = candidate.sourceBones[childIndex];
            if (child.parentBoneId != edited.boneId || !child.connectedToParent) continue;
            movedConnectedChildren[childIndex] = true;
            if (!child.hasExplicitTail) continue;
            VEC3 worldTail;
            vec3TransformCoord(&worldTail, &child.tailOffset,
                &impl->canonicalSkeleton.compiled.bones[childIndex].globalBindMatrix);
            connectedChildTails.emplace_back(childIndex, worldTail);
        }
        edited.tailOffset = hasExplicitTail ? tailOffset : VEC3();
        edited.length = hasExplicitTail ? length : 0.0f;
        edited.hasExplicitTail = hasExplicitTail;
        for (skeletal::CANONICAL_BONE &child : candidate.sourceBones)
            if (child.parentBoneId == edited.boneId && child.connectedToParent)
                child.localBind.translation = edited.tailOffset;
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("edited canonical tail would make the skeleton invalid");
        for (const auto &saved : connectedChildTails)
        {
            MATRIX inverse;
            float determinant = 0.0f;
            MatrixInverse(&inverse, &determinant,
                &candidate.compiled.bones[saved.first].globalBindMatrix);
            if (!std::isfinite(determinant) || std::fabs(determinant)<=skeletal::SINGULAR_TOLERANCE)
                return fail("connected child bind transform is not invertible");
            vec3TransformCoord(&candidate.sourceBones[saved.first].tailOffset,&saved.second,&inverse);
            const VEC3 &preservedOffset=candidate.sourceBones[saved.first].tailOffset;
            candidate.sourceBones[saved.first].length=std::sqrt(
                preservedOffset.x*preservedOffset.x+preservedOffset.y*preservedOffset.y+
                preservedOffset.z*preservedOffset.z);
            for (skeletal::CANONICAL_BONE &grandchild : candidate.sourceBones)
                if (grandchild.parentBoneId==candidate.sourceBones[saved.first].boneId &&
                    grandchild.connectedToParent)
                    grandchild.localBind.translation=candidate.sourceBones[saved.first].tailOffset;
        }
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("connected child tail preservation would make the skeleton invalid");
        if (preserveOtherJoints)
        {
            for (uint32_t boneIndex = 0; boneIndex < candidate.sourceBones.size(); ++boneIndex)
            {
                if (boneIndex == index || movedConnectedChildren[boneIndex]) continue;
                MATRIX local = oldGlobals[boneIndex];
                const int32_t parentIndex = candidate.compiled.bones[boneIndex].parentIndex;
                if (parentIndex >= 0)
                {
                    const uint32_t parent = static_cast<uint32_t>(parentIndex);
                    const MATRIX &parentGlobal =
                        (parent == index || movedConnectedChildren[parent])
                            ? candidate.compiled.bones[parent].globalBindMatrix
                            : oldGlobals[parent];
                    MATRIX inverseParent;
                    float determinant = 0.0f;
                    MatrixInverse(&inverseParent, &determinant, &parentGlobal);
                    if (!std::isfinite(determinant) ||
                        std::fabs(determinant) <= skeletal::SINGULAR_TOLERANCE)
                        return fail("preserved joint parent transform is not invertible");
                    MatrixMultiply(&local, &oldGlobals[boneIndex], &inverseParent);
                }
                bool negativeScale = false;
                bool shear = false;
                if (!skeletal::decomposeTrsMatrix(local, candidate.sourceBones[boneIndex].localBind,
                        negativeScale, shear) || shear)
                    return fail("preserving other joints would require an invalid local transform");
            }
            if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
                return fail("other-joint preservation would make the skeleton invalid");
        }
        // Tail geometry and connected-child bind translations do not change skeleton IDs, weight
        // palettes, vertex records, clip IDs, or track targets. Revalidating every vertex and clip
        // on every mouse-move event is both redundant and prohibitively expensive.
        impl->canonicalSkeleton = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::setSkeletalBoneHead(const uint32_t index, const VEC3 &translation,
                                              const bool preserveOtherJoints,
                                              char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut,errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if (impl->canonicalSkeleton.skeletonId==0) return fail("mesh has no canonical skeleton");
        if (index>=impl->canonicalSkeleton.sourceBones.size())
            return fail("canonical bone index is out of range");
        if (!std::isfinite(translation.x)||!std::isfinite(translation.y)||!std::isfinite(translation.z))
            return fail("canonical bone head must be finite");
        skeletal::CANONICAL_SKELETON candidate=impl->canonicalSkeleton;
        skeletal::CANONICAL_BONE &edited=candidate.sourceBones[index];
        std::vector<MATRIX> oldGlobals;
        oldGlobals.reserve(candidate.compiled.bones.size());
        for (const skeletal::COMPILED_BONE &bone:candidate.compiled.bones)
            oldGlobals.push_back(bone.globalBindMatrix);
        VEC3 oldWorldTail;
        if (edited.hasExplicitTail)
            vec3TransformCoord(&oldWorldTail,&edited.tailOffset,
                &impl->canonicalSkeleton.compiled.bones[index].globalBindMatrix);
        edited.localBind.translation=translation;
        edited.connectedToParent=false;
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
            return fail("edited canonical head would make the skeleton invalid");
        if (edited.hasExplicitTail)
        {
            MATRIX inverse;
            float determinant=0.0f;
            MatrixInverse(&inverse,&determinant,&candidate.compiled.bones[index].globalBindMatrix);
            if (!std::isfinite(determinant)||std::fabs(determinant)<=skeletal::SINGULAR_TOLERANCE)
                return fail("edited canonical head transform is not invertible");
            vec3TransformCoord(&edited.tailOffset,&oldWorldTail,&inverse);
            edited.length=std::sqrt(edited.tailOffset.x*edited.tailOffset.x+
                                    edited.tailOffset.y*edited.tailOffset.y+
                                    edited.tailOffset.z*edited.tailOffset.z);
            for (skeletal::CANONICAL_BONE &child:candidate.sourceBones)
                if (child.parentBoneId==edited.boneId&&child.connectedToParent)
                    child.localBind.translation=edited.tailOffset;
            if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
                return fail("head edit tail preservation would make the skeleton invalid");
        }
        if (preserveOtherJoints)
        {
            for (uint32_t boneIndex=0;boneIndex<candidate.sourceBones.size();++boneIndex)
            {
                if (boneIndex==index) continue;
                MATRIX local=oldGlobals[boneIndex];
                const int32_t parentIndex=candidate.compiled.bones[boneIndex].parentIndex;
                if (parentIndex>=0)
                {
                    const uint32_t parent=static_cast<uint32_t>(parentIndex);
                    const MATRIX &parentGlobal=parent==index
                        ? candidate.compiled.bones[parent].globalBindMatrix : oldGlobals[parent];
                    MATRIX inverseParent;
                    float determinant=0.0f;
                    MatrixInverse(&inverseParent,&determinant,&parentGlobal);
                    if (!std::isfinite(determinant)||
                        std::fabs(determinant)<=skeletal::SINGULAR_TOLERANCE)
                        return fail("preserved joint parent transform is not invertible");
                    MatrixMultiply(&local,&oldGlobals[boneIndex],&inverseParent);
                }
                bool negativeScale=false;
                bool shear=false;
                if (!skeletal::decomposeTrsMatrix(local,candidate.sourceBones[boneIndex].localBind,
                        negativeScale,shear)||shear)
                    return fail("preserving other joints would require an invalid local transform");
            }
            if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
                return fail("other-joint preservation would make the skeleton invalid");
        }
        impl->canonicalSkeleton=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::translateSkeletalBoneSegment(const uint32_t index,
                                                       const VEC3 &translation,
                                                       const bool preserveOtherJoints,
                                                       char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if(errorOut&&errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if(impl->canonicalSkeleton.skeletonId==0) return fail("mesh has no canonical skeleton");
        if(index>=impl->canonicalSkeleton.sourceBones.size())
            return fail("canonical bone index is out of range");
        if(!std::isfinite(translation.x)||!std::isfinite(translation.y)||
            !std::isfinite(translation.z)) return fail("canonical segment translation must be finite");
        if(!impl->canonicalSkeleton.sourceBones[index].hasExplicitTail)
            return fail("canonical segment translation requires an explicit tail");

        skeletal::CANONICAL_SKELETON candidate=impl->canonicalSkeleton;
        std::vector<MATRIX> oldGlobals;
        oldGlobals.reserve(candidate.compiled.bones.size());
        for(const skeletal::COMPILED_BONE &bone:candidate.compiled.bones)
            oldGlobals.push_back(bone.globalBindMatrix);
        std::vector<bool> movedConnectedChildren(candidate.sourceBones.size(),false);
        std::vector<std::pair<uint32_t,VEC3>> connectedChildTails;
        const uint64_t editedId=candidate.sourceBones[index].boneId;
        for(uint32_t childIndex=0;childIndex<candidate.sourceBones.size();++childIndex)
        {
            const skeletal::CANONICAL_BONE &child=candidate.sourceBones[childIndex];
            if(child.parentBoneId!=editedId||!child.connectedToParent) continue;
            movedConnectedChildren[childIndex]=true;
            if(child.hasExplicitTail)
            {
                VEC3 worldTail;
                vec3TransformCoord(&worldTail,&child.tailOffset,&oldGlobals[childIndex]);
                connectedChildTails.emplace_back(childIndex,worldTail);
            }
        }
        candidate.sourceBones[index].localBind.translation=translation;
        candidate.sourceBones[index].connectedToParent=false;
        if(!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
            return fail("segment translation would make the skeleton invalid");

        if(preserveOtherJoints)
        {
            for(const auto &saved:connectedChildTails)
            {
                MATRIX inverse;
                float determinant=0.0f;
                MatrixInverse(&inverse,&determinant,
                    &candidate.compiled.bones[saved.first].globalBindMatrix);
                if(!std::isfinite(determinant)||
                    std::fabs(determinant)<=skeletal::SINGULAR_TOLERANCE)
                    return fail("connected child bind transform is not invertible");
                skeletal::CANONICAL_BONE &child=candidate.sourceBones[saved.first];
                vec3TransformCoord(&child.tailOffset,&saved.second,&inverse);
                child.length=std::sqrt(child.tailOffset.x*child.tailOffset.x+
                    child.tailOffset.y*child.tailOffset.y+child.tailOffset.z*child.tailOffset.z);
                for(skeletal::CANONICAL_BONE &grandchild:candidate.sourceBones)
                    if(grandchild.parentBoneId==child.boneId&&grandchild.connectedToParent)
                        grandchild.localBind.translation=child.tailOffset;
            }
            if(!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
                return fail("connected child preservation would make the skeleton invalid");
            for(uint32_t boneIndex=0;boneIndex<candidate.sourceBones.size();++boneIndex)
            {
                if(boneIndex==index||movedConnectedChildren[boneIndex]) continue;
                MATRIX local=oldGlobals[boneIndex];
                const int32_t parentIndex=candidate.compiled.bones[boneIndex].parentIndex;
                if(parentIndex>=0)
                {
                    const uint32_t parent=static_cast<uint32_t>(parentIndex);
                    const MATRIX &parentGlobal=(parent==index||movedConnectedChildren[parent])
                        ? candidate.compiled.bones[parent].globalBindMatrix : oldGlobals[parent];
                    MATRIX inverseParent;
                    float determinant=0.0f;
                    MatrixInverse(&inverseParent,&determinant,&parentGlobal);
                    if(!std::isfinite(determinant)||
                        std::fabs(determinant)<=skeletal::SINGULAR_TOLERANCE)
                        return fail("preserved joint parent transform is not invertible");
                    MatrixMultiply(&local,&oldGlobals[boneIndex],&inverseParent);
                }
                bool negativeScale=false;
                bool shear=false;
                if(!skeletal::decomposeTrsMatrix(local,candidate.sourceBones[boneIndex].localBind,
                        negativeScale,shear)||shear)
                    return fail("preserving other joints would require an invalid local transform");
            }
            if(!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
                return fail("other-joint preservation would make the skeleton invalid");
        }
        impl->canonicalSkeleton=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::setSkeletalBoneConnectedToParent(const uint32_t index,
                                                           const bool connected,
                                                           const bool preserveOtherJoints,
                                                           char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if(errorOut&&errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if(impl->canonicalSkeleton.skeletonId==0) return fail("mesh has no canonical skeleton");
        if(index>=impl->canonicalSkeleton.sourceBones.size())
            return fail("canonical bone index is out of range");
        const int32_t parentIndex=impl->canonicalSkeleton.compiled.bones[index].parentIndex;
        if(connected&&parentIndex<0) return fail("canonical root cannot connect to a parent tail");
        if(!connected)
        {
            skeletal::CANONICAL_SKELETON candidate=impl->canonicalSkeleton;
            candidate.sourceBones[index].connectedToParent=false;
            if(!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
                return fail("disconnected canonical skeleton would be invalid");
            impl->canonicalSkeleton=std::move(candidate);
            return true;
        }
        const skeletal::CANONICAL_BONE &parent=
            impl->canonicalSkeleton.sourceBones[static_cast<uint32_t>(parentIndex)];
        if(!parent.hasExplicitTail) return fail("canonical parent has no explicit tail");

        skeletal::CANONICAL_SKELETON candidate=impl->canonicalSkeleton;
        std::vector<MATRIX> oldGlobals;
        oldGlobals.reserve(candidate.compiled.bones.size());
        for(const skeletal::COMPILED_BONE &bone:candidate.compiled.bones)
            oldGlobals.push_back(bone.globalBindMatrix);
        skeletal::CANONICAL_BONE &edited=candidate.sourceBones[index];
        VEC3 oldWorldTail;
        if(edited.hasExplicitTail)
            vec3TransformCoord(&oldWorldTail,&edited.tailOffset,&oldGlobals[index]);
        edited.localBind.translation=parent.tailOffset;
        edited.connectedToParent=true;
        if(!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
            return fail("connected canonical skeleton would be invalid");
        if(edited.hasExplicitTail)
        {
            MATRIX inverse;
            float determinant=0.0f;
            MatrixInverse(&inverse,&determinant,&candidate.compiled.bones[index].globalBindMatrix);
            if(!std::isfinite(determinant)||
                std::fabs(determinant)<=skeletal::SINGULAR_TOLERANCE)
                return fail("connected bone transform is not invertible");
            vec3TransformCoord(&edited.tailOffset,&oldWorldTail,&inverse);
            edited.length=std::sqrt(edited.tailOffset.x*edited.tailOffset.x+
                edited.tailOffset.y*edited.tailOffset.y+edited.tailOffset.z*edited.tailOffset.z);
            for(skeletal::CANONICAL_BONE &child:candidate.sourceBones)
                if(child.parentBoneId==edited.boneId&&child.connectedToParent)
                    child.localBind.translation=edited.tailOffset;
            if(!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
                return fail("connected tail preservation would make the skeleton invalid");
        }
        if(preserveOtherJoints)
        {
            for(uint32_t boneIndex=0;boneIndex<candidate.sourceBones.size();++boneIndex)
            {
                if(boneIndex==index) continue;
                MATRIX local=oldGlobals[boneIndex];
                const int32_t candidateParent=candidate.compiled.bones[boneIndex].parentIndex;
                if(candidateParent>=0)
                {
                    const uint32_t parentBone=static_cast<uint32_t>(candidateParent);
                    const MATRIX &parentGlobal=parentBone==index
                        ? candidate.compiled.bones[parentBone].globalBindMatrix : oldGlobals[parentBone];
                    MATRIX inverseParent;
                    float determinant=0.0f;
                    MatrixInverse(&inverseParent,&determinant,&parentGlobal);
                    if(!std::isfinite(determinant)||
                        std::fabs(determinant)<=skeletal::SINGULAR_TOLERANCE)
                        return fail("preserved joint parent transform is not invertible");
                    MatrixMultiply(&local,&oldGlobals[boneIndex],&inverseParent);
                }
                bool negativeScale=false;
                bool shear=false;
                if(!skeletal::decomposeTrsMatrix(local,candidate.sourceBones[boneIndex].localBind,
                        negativeScale,shear)||shear)
                    return fail("preserving other joints would require an invalid local transform");
            }
            if(!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
                return fail("other-joint preservation would make the skeleton invalid");
        }
        impl->canonicalSkeleton=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::setSkeletalBoneRadius(const uint32_t index, const float radius,
                                                const bool includeDescendants,
                                                char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if(errorOut&&errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        if(impl->canonicalSkeleton.skeletonId==0) return fail("mesh has no canonical skeleton");
        if(index>=impl->canonicalSkeleton.sourceBones.size())
            return fail("canonical bone index is out of range");
        if(!std::isfinite(radius)||radius<=skeletal::SINGULAR_TOLERANCE)
            return fail("canonical bone radius must be finite and positive");
        skeletal::CANONICAL_SKELETON candidate=impl->canonicalSkeleton;
        std::unordered_set<uint64_t> affected;
        affected.insert(candidate.sourceBones[index].boneId);
        if(includeDescendants)
        {
            bool changed=true;
            while(changed)
            {
                changed=false;
                for(const skeletal::CANONICAL_BONE &bone:candidate.sourceBones)
                    if(affected.find(bone.boneId)==affected.end()&&
                        affected.find(bone.parentBoneId)!=affected.end())
                    {
                        affected.insert(bone.boneId);
                        changed=true;
                    }
            }
        }
        for(skeletal::CANONICAL_BONE &bone:candidate.sourceBones)
            if(affected.find(bone.boneId)!=affected.end()) bone.radius=radius;
        if(!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
            return fail("radius edit would make the canonical skeleton invalid");
        impl->canonicalSkeleton=std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::addSkeletalBoneChain(const int32_t parentIndex, const char *namePrefix,
                                               const uint32_t count, const VEC3 &stepTranslation,
                                               const float radius, const float length,
                                               uint32_t *lastIndexOut,
                                               char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        const auto &source = impl->canonicalSkeleton.sourceBones;
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (parentIndex < -1 || parentIndex >= static_cast<int32_t>(source.size()))
            return fail("canonical chain parent index is out of range");
        if (!namePrefix || !namePrefix[0]) return fail("canonical chain name prefix must not be empty");
        if (count == 0 || count > 256) return fail("canonical chain count must be between 1 and 256");
        if (!std::isfinite(stepTranslation.x) || !std::isfinite(stepTranslation.y) ||
            !std::isfinite(stepTranslation.z) || !std::isfinite(radius) || !std::isfinite(length))
            return fail("canonical chain values must be finite");
        if (radius < 0.0f || length < 0.0f)
            return fail("canonical chain radius and length must not be negative");

        skeletal::CANONICAL_SKELETON candidate = impl->canonicalSkeleton;
        uint64_t chainParentId = parentIndex < 0 ? 0 : source[static_cast<uint32_t>(parentIndex)].boneId;
        uint64_t nextBoneId = 1;
        for (uint32_t item = 1; item <= count; ++item)
        {
            const std::string name = std::string(namePrefix) + std::to_string(item);
            if (candidate.compiled.indexByName.find(name) != candidate.compiled.indexByName.end() ||
                std::any_of(candidate.sourceBones.begin(), candidate.sourceBones.end(),
                    [&name](const skeletal::CANONICAL_BONE &bone) { return bone.name == name; }))
                return fail("canonical chain would create a duplicate bone name");
            while (candidate.compiled.indexById.find(nextBoneId) != candidate.compiled.indexById.end() ||
                   std::any_of(candidate.sourceBones.begin(), candidate.sourceBones.end(),
                    [nextBoneId](const skeletal::CANONICAL_BONE &bone) { return bone.boneId == nextBoneId; }))
            {
                if (nextBoneId == std::numeric_limits<uint64_t>::max())
                    return fail("canonical bone ID space is exhausted");
                ++nextBoneId;
            }
            skeletal::CANONICAL_BONE added;
            added.boneId = nextBoneId;
            added.parentBoneId = chainParentId;
            added.name = name;
            added.localBind.translation = stepTranslation;
            added.radius = radius;
            added.length = length;
            added.tailOffset = VEC3(0.0f, length, 0.0f);
            added.hasExplicitTail = true;
            added.connectedToParent = chainParentId != 0;
            chainParentId = added.boneId;
            candidate.sourceBones.push_back(std::move(added));
            if (item < count)
            {
                if (nextBoneId == std::numeric_limits<uint64_t>::max())
                    return fail("canonical bone ID space is exhausted");
                ++nextBoneId;
            }
        }
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("added canonical chain would be invalid");
        if (impl->canonicalWeights.skeletonId != 0)
        {
            if (impl->canonicalWeights.frameIndex >= impl->buffer.size())
                return fail("canonical weight frame is out of range");
            const util::BUFFER_MESH_DEBUG *frame = impl->buffer[impl->canonicalWeights.frameIndex];
            uint32_t vertexCount = 0;
            for (const util::SUBSET_DEBUG *subset : frame->subset)
                vertexCount += static_cast<uint32_t>(subset->vertexCount);
            if (!skeletal::validateCanonicalWeights(candidate, impl->canonicalWeights, vertexCount))
                return fail("canonical weights would be invalid after adding chain");
        }
        if (impl->canonicalAnimations.skeletonId != 0 &&
            !skeletal::validateCanonicalAnimations(candidate, impl->canonicalAnimations))
            return fail("canonical animations would be invalid after adding chain");
        impl->canonicalSkeleton = std::move(candidate);
        if (lastIndexOut) *lastIndexOut = static_cast<uint32_t>(impl->canonicalSkeleton.sourceBones.size() - 1);
        return true;
    }

    bool MESH_MBM_DEBUG::extendSkeletalBoneTail(const uint32_t index, const uint32_t count,
                                                 const float radius, const float length,
                                                 uint32_t *lastIndexOut,
                                                 char *errorOut, const int errorOutLen)
    {
        const auto fail=[errorOut,errorOutLen](const char *message)
        {
            if(errorOut&&errorOutLen>0) snprintf(errorOut,errorOutLen,"%s",message);
            return false;
        };
        const auto &source=impl->canonicalSkeleton.sourceBones;
        if(impl->canonicalSkeleton.skeletonId==0) return fail("mesh has no canonical skeleton");
        if(index>=source.size()) return fail("canonical extension bone index is out of range");
        if(count==0||count>256) return fail("canonical extension count must be between 1 and 256");
        if(!std::isfinite(radius)||!std::isfinite(length)||radius<0.0f||
            length<=skeletal::SINGULAR_TOLERANCE)
            return fail("canonical extension radius and length must be finite and positive");
        const skeletal::CANONICAL_BONE &selected=source[index];
        if(!selected.hasExplicitTail) return fail("canonical extension requires an explicit tail");
        const float selectedLength=std::sqrt(selected.tailOffset.x*selected.tailOffset.x+
            selected.tailOffset.y*selected.tailOffset.y+selected.tailOffset.z*selected.tailOffset.z);
        if(selectedLength<=skeletal::SINGULAR_TOLERANCE)
            return fail("canonical extension requires a nonzero tail direction");
        const VEC3 newTail(selected.tailOffset.x*length/selectedLength,
                           selected.tailOffset.y*length/selectedLength,
                           selected.tailOffset.z*length/selectedLength);

        skeletal::CANONICAL_SKELETON candidate=impl->canonicalSkeleton;
        uint64_t parentId=selected.boneId;
        VEC3 head=selected.tailOffset;
        uint64_t nextBoneId=1;
        uint32_t nextName=1;
        for(uint32_t item=0;item<count;++item)
        {
            while(candidate.compiled.indexById.find(nextBoneId)!=candidate.compiled.indexById.end()||
                std::any_of(candidate.sourceBones.begin(),candidate.sourceBones.end(),
                    [nextBoneId](const skeletal::CANONICAL_BONE &bone)
                    { return bone.boneId==nextBoneId; }))
            {
                if(nextBoneId==std::numeric_limits<uint64_t>::max())
                    return fail("canonical bone ID space is exhausted");
                ++nextBoneId;
            }
            std::string name;
            do name="Bone_"+std::to_string(nextName++);
            while(candidate.compiled.indexByName.find(name)!=candidate.compiled.indexByName.end()||
                std::any_of(candidate.sourceBones.begin(),candidate.sourceBones.end(),
                    [&name](const skeletal::CANONICAL_BONE &bone){ return bone.name==name; }));
            skeletal::CANONICAL_BONE added;
            added.boneId=nextBoneId;
            added.parentBoneId=parentId;
            added.name=std::move(name);
            added.localBind.translation=head;
            added.radius=radius;
            added.length=length;
            added.tailOffset=newTail;
            added.hasExplicitTail=true;
            added.connectedToParent=true;
            parentId=added.boneId;
            head=newTail;
            candidate.sourceBones.push_back(std::move(added));
            if(nextBoneId<std::numeric_limits<uint64_t>::max()) ++nextBoneId;
        }
        if(!skeletal::compileCanonicalSkeleton(candidate.sourceBones,candidate.compiled))
            return fail("extended canonical chain would be invalid");
        if(impl->canonicalWeights.skeletonId!=0)
        {
            if(impl->canonicalWeights.frameIndex>=impl->buffer.size())
                return fail("canonical weight frame is out of range");
            const util::BUFFER_MESH_DEBUG *frame=impl->buffer[impl->canonicalWeights.frameIndex];
            uint32_t vertexCount=0;
            for(const util::SUBSET_DEBUG *subset:frame->subset)
                vertexCount+=static_cast<uint32_t>(subset->vertexCount);
            if(!skeletal::validateCanonicalWeights(candidate,impl->canonicalWeights,vertexCount))
                return fail("canonical weights would be invalid after extending tail");
        }
        if(impl->canonicalAnimations.skeletonId!=0&&
            !skeletal::validateCanonicalAnimations(candidate,impl->canonicalAnimations))
            return fail("canonical animations would be invalid after extending tail");
        impl->canonicalSkeleton=std::move(candidate);
        if(lastIndexOut) *lastIndexOut=static_cast<uint32_t>(
            impl->canonicalSkeleton.sourceBones.size()-1);
        return true;
    }

    bool MESH_MBM_DEBUG::mirrorSkeletalBoneSubtree(const uint32_t index, const uint32_t axis,
                                                    const char *namePrefix, uint32_t *newRootIndexOut,
                                                    char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        const auto &source = impl->canonicalSkeleton.sourceBones;
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (index >= source.size()) return fail("canonical mirror root index is out of range");
        if (axis > 2) return fail("canonical mirror axis must be X, Y, or Z");
        if (!namePrefix || !namePrefix[0]) return fail("canonical mirror name prefix must not be empty");
        if (!impl->canonicalAnimations.clips.empty())
            return fail("canonical subtree mirror requires an asset without animation clips");

        std::vector<uint32_t> subtree;
        std::unordered_set<uint64_t> subtreeIds;
        subtreeIds.insert(source[index].boneId);
        for (uint32_t candidateIndex = index; candidateIndex < source.size(); ++candidateIndex)
        {
            const skeletal::CANONICAL_BONE &candidate = source[candidateIndex];
            if (subtreeIds.find(candidate.boneId) != subtreeIds.end() ||
                subtreeIds.find(candidate.parentBoneId) != subtreeIds.end())
            {
                subtree.push_back(candidateIndex);
                subtreeIds.insert(candidate.boneId);
            }
        }

        MATRIX reflection;
        MatrixIdentity(&reflection);
        reflection.m[axis][axis] = -1.0f;
        skeletal::CANONICAL_SKELETON candidate = impl->canonicalSkeleton;
        std::unordered_map<uint64_t, uint64_t> mirroredIds;
        std::unordered_map<uint64_t, MATRIX> mirroredGlobals;
        uint64_t nextBoneId = 1;
        const uint32_t newRootIndex = static_cast<uint32_t>(candidate.sourceBones.size());
        for (const uint32_t sourceIndex : subtree)
        {
            const skeletal::CANONICAL_BONE &original = source[sourceIndex];
            const std::string mirroredName = std::string(namePrefix) + original.name;
            if (std::any_of(candidate.sourceBones.begin(), candidate.sourceBones.end(),
                [&mirroredName](const skeletal::CANONICAL_BONE &bone) { return bone.name == mirroredName; }))
                return fail("canonical mirror would create a duplicate bone name");
            while (std::any_of(candidate.sourceBones.begin(), candidate.sourceBones.end(),
                [nextBoneId](const skeletal::CANONICAL_BONE &bone) { return bone.boneId == nextBoneId; }))
            {
                if (nextBoneId == std::numeric_limits<uint64_t>::max())
                    return fail("canonical bone ID space is exhausted");
                ++nextBoneId;
            }
            MATRIX temporary, mirroredGlobal;
            MatrixMultiply(&temporary, &reflection,
                           &impl->canonicalSkeleton.compiled.bones[sourceIndex].globalBindMatrix);
            MatrixMultiply(&mirroredGlobal, &temporary, &reflection);
            skeletal::CANONICAL_BONE mirrored = original;
            if (axis == 0) mirrored.tailOffset.x = -mirrored.tailOffset.x;
            else if (axis == 1) mirrored.tailOffset.y = -mirrored.tailOffset.y;
            else mirrored.tailOffset.z = -mirrored.tailOffset.z;
            mirrored.boneId = nextBoneId;
            mirrored.name = mirroredName;
            const auto mirroredParent = mirroredIds.find(original.parentBoneId);
            mirrored.parentBoneId = mirroredParent == mirroredIds.end()
                ? original.parentBoneId : mirroredParent->second;
            MATRIX local = mirroredGlobal;
            if (mirrored.parentBoneId != 0)
            {
                MATRIX parentGlobal, inverseParent;
                const auto generatedParent = mirroredGlobals.find(mirrored.parentBoneId);
                if (generatedParent != mirroredGlobals.end()) parentGlobal = generatedParent->second;
                else
                {
                    const auto existingParent = impl->canonicalSkeleton.compiled.indexById.find(mirrored.parentBoneId);
                    if (existingParent == impl->canonicalSkeleton.compiled.indexById.end())
                        return fail("canonical mirror parent is missing");
                    parentGlobal = impl->canonicalSkeleton.compiled.bones[existingParent->second].globalBindMatrix;
                }
                float determinant = 0.0f;
                MatrixInverse(&inverseParent, &determinant, &parentGlobal);
                if (!std::isfinite(determinant) || std::fabs(determinant) <= skeletal::SINGULAR_TOLERANCE)
                    return fail("canonical mirror parent bind transform is not invertible");
                MatrixMultiply(&local, &mirroredGlobal, &inverseParent);
            }
            bool negativeScale = false, shear = false;
            if (!skeletal::decomposeTrsMatrix(local, mirrored.localBind, negativeScale, shear) || shear)
                return fail("canonical mirrored bind would require unsupported shear");
            mirroredIds.emplace(original.boneId, mirrored.boneId);
            mirroredGlobals.emplace(mirrored.boneId, mirroredGlobal);
            candidate.sourceBones.push_back(std::move(mirrored));
            if (nextBoneId != std::numeric_limits<uint64_t>::max()) ++nextBoneId;
        }
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("mirrored canonical subtree would be invalid");
        if (impl->canonicalWeights.skeletonId != 0)
        {
            if (impl->canonicalWeights.frameIndex >= impl->buffer.size())
                return fail("canonical weight frame is out of range");
            const util::BUFFER_MESH_DEBUG *frame = impl->buffer[impl->canonicalWeights.frameIndex];
            uint32_t vertexCount = 0;
            for (const util::SUBSET_DEBUG *subset : frame->subset)
                vertexCount += static_cast<uint32_t>(subset->vertexCount);
            if (!skeletal::validateCanonicalWeights(candidate, impl->canonicalWeights, vertexCount))
                return fail("canonical weights would be invalid after subtree mirror");
        }
        impl->canonicalSkeleton = std::move(candidate);
        if (newRootIndexOut) *newRootIndexOut = newRootIndex;
        return true;
    }

    bool MESH_MBM_DEBUG::initializeSkeletalSkeleton(const char *rootName, const VEC3 &translation,
                                                     const float radius, const float length,
                                                     const bool hasExplicitTail,
                                                     char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (impl->buffer.empty()) return fail("a loaded mesh is required to initialize a skeleton");
        if (impl->canonicalSkeleton.skeletonId != 0 || impl->canonicalWeights.skeletonId != 0 ||
            impl->canonicalAnimations.skeletonId != 0)
            return fail("mesh already contains canonical skeletal data");
        if (!rootName || !rootName[0]) return fail("canonical root bone name must not be empty");
        if (!std::isfinite(translation.x) || !std::isfinite(translation.y) ||
            !std::isfinite(translation.z) || !std::isfinite(radius) || !std::isfinite(length))
            return fail("canonical root bone values must be finite");
        if (radius < 0.0f || length < 0.0f)
            return fail("canonical root bone radius and length must not be negative");
        skeletal::CANONICAL_SKELETON candidate;
        candidate.skeletonId = 1;
        skeletal::CANONICAL_BONE root;
        root.boneId = 1;
        root.name = rootName;
        root.localBind.translation = translation;
        root.radius = radius;
        root.length = length;
        root.tailOffset = VEC3(0.0f, hasExplicitTail ? length : 0.0f, 0.0f);
        root.hasExplicitTail = hasExplicitTail;
        candidate.sourceBones.push_back(std::move(root));
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("initial canonical skeleton would be invalid");
        impl->canonicalSkeleton = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::removeSkeletalBone(const uint32_t index,
                                             char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        const auto &source = impl->canonicalSkeleton.sourceBones;
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (index >= source.size()) return fail("canonical bone index is out of range");
        if (source.size() <= 1) return fail("canonical skeleton must retain at least one bone");
        const uint64_t boneId = source[index].boneId;
        for (const skeletal::CANONICAL_BONE &candidate : source)
            if (candidate.parentBoneId == boneId)
                return fail("canonical bone has children; choose an explicit descendant policy first");
        for (const uint64_t paletteBoneId : impl->canonicalWeights.paletteBoneIds)
            if (paletteBoneId == boneId)
                return fail("canonical bone is referenced by the weight palette; remapping is required");
        for (const skeletal::SKELETAL_CLIP &clip : impl->canonicalAnimations.clips)
            for (const skeletal::SKELETAL_TRACK &track : clip.tracks)
                if (track.boneId == boneId)
                    return fail("canonical bone is targeted by animation tracks; remapping is required");

        skeletal::CANONICAL_SKELETON candidate = impl->canonicalSkeleton;
        candidate.sourceBones.erase(candidate.sourceBones.begin() + index);
        if (!skeletal::compileCanonicalSkeleton(candidate.sourceBones, candidate.compiled))
            return fail("removed canonical skeleton would be invalid");
        if (impl->canonicalWeights.skeletonId != 0)
        {
            if (impl->canonicalWeights.frameIndex >= impl->buffer.size())
                return fail("canonical weight frame is out of range");
            const util::BUFFER_MESH_DEBUG *frame = impl->buffer[impl->canonicalWeights.frameIndex];
            uint32_t vertexCount = 0;
            for (const util::SUBSET_DEBUG *subset : frame->subset)
                vertexCount += static_cast<uint32_t>(subset->vertexCount);
            if (!skeletal::validateCanonicalWeights(candidate, impl->canonicalWeights, vertexCount))
                return fail("canonical weights would be invalid after removing bone");
        }
        if (impl->canonicalAnimations.skeletonId != 0 &&
            !skeletal::validateCanonicalAnimations(candidate, impl->canonicalAnimations))
            return fail("canonical animations would be invalid after removing bone");
        impl->canonicalSkeleton = std::move(candidate);
        return true;
    }

    bool MESH_MBM_DEBUG::removeSkeletalBoneRemapped(const uint32_t index,
                                                     const uint32_t replacementIndex,
                                                     const bool discardAnimationTracks,
                                                     const bool reparentChildrenPreserveGlobal,
                                                     char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        const auto &source = impl->canonicalSkeleton.sourceBones;
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (index >= source.size() || replacementIndex >= source.size())
            return fail("canonical bone or replacement index is out of range");
        if (index == replacementIndex) return fail("replacement bone must differ from removed bone");
        if (source.size() <= 1) return fail("canonical skeleton must retain at least one bone");
        const uint64_t removedId = source[index].boneId;
        const uint64_t replacementId = source[replacementIndex].boneId;
        bool hasChildren = false;
        for (const skeletal::CANONICAL_BONE &candidate : source)
            if (candidate.parentBoneId == removedId) hasChildren = true;
        if (hasChildren && !reparentChildrenPreserveGlobal)
            return fail("canonical bone has children; choose an explicit descendant policy first");

        skeletal::CANONICAL_SKELETON skeletonCandidate = impl->canonicalSkeleton;
        skeletal::CANONICAL_WEIGHTS weightsCandidate = impl->canonicalWeights;
        skeletal::CANONICAL_ANIMATIONS animationsCandidate = impl->canonicalAnimations;
        if (hasChildren)
        {
            std::vector<uint64_t> childIds;
            for (const skeletal::CANONICAL_BONE &candidate : source)
                if (candidate.parentBoneId == removedId) childIds.push_back(candidate.boneId);
            for (skeletal::SKELETAL_CLIP &clip : animationsCandidate.clips)
            {
                skeletal::SKELETAL_TRACK removedTrack;
                bool hasRemovedTrack = false;
                for (const skeletal::SKELETAL_TRACK &track : clip.tracks)
                    if (track.boneId == removedId) { removedTrack = track; hasRemovedTrack = true; break; }
                for (const uint64_t childId : childIds)
                {
                    skeletal::SKELETAL_TRACK childTrack;
                    bool hasChildTrack = false;
                    for (const skeletal::SKELETAL_TRACK &track : clip.tracks)
                        if (track.boneId == childId) { childTrack = track; hasChildTrack = true; break; }
                    if (!hasRemovedTrack && !hasChildTrack) continue;
                    std::set<float> sampleTimes = {0.0f, clip.duration};
                    if (hasRemovedTrack) for (const skeletal::SKELETAL_KEY &key : removedTrack.keys)
                        sampleTimes.insert(key.time);
                    if (hasChildTrack) for (const skeletal::SKELETAL_KEY &key : childTrack.keys)
                        sampleTimes.insert(key.time);
                    skeletal::SKELETAL_TRACK baked;
                    baked.boneId = childId;
                    baked.channelMask = skeletal::SKELETAL_CHANNEL_TRANSLATION |
                                        skeletal::SKELETAL_CHANNEL_ROTATION |
                                        skeletal::SKELETAL_CHANNEL_SCALE;
                    const uint32_t childIndex = static_cast<uint32_t>(
                        impl->canonicalSkeleton.compiled.indexById.at(childId));
                    for (const float time : sampleTimes)
                    {
                        skeletal::SKELETAL_POSE pose;
                        if (!skeletal::sampleSkeletalClip(impl->canonicalSkeleton.compiled, clip, time, pose))
                            return fail("could not sample canonical clip while converting child tracks");
                        MATRIX composed;
                        const MATRIX childLocal = skeletal::buildTrsMatrix(pose.localTransforms[childIndex]);
                        const MATRIX removedLocal = skeletal::buildTrsMatrix(pose.localTransforms[index]);
                        MatrixMultiply(&composed, &childLocal, &removedLocal);
                        skeletal::SKELETAL_KEY key;
                        key.time = time;
                        bool negativeScale = false, shear = false;
                        if (!skeletal::decomposeTrsMatrix(composed, key.local, negativeScale, shear) || shear)
                            return fail("converted child animation track would require unsupported shear");
                        baked.keys.push_back(std::move(key));
                    }
                    bool replaced = false;
                    for (skeletal::SKELETAL_TRACK &track : clip.tracks)
                        if (track.boneId == childId) { track = baked; replaced = true; break; }
                    if (!replaced) clip.tracks.push_back(std::move(baked));
                }
            }
        }
        int32_t removedPalette = -1, replacementPalette = -1;
        for (uint32_t palette = 0; palette < weightsCandidate.paletteBoneIds.size(); ++palette)
        {
            if (weightsCandidate.paletteBoneIds[palette] == removedId) removedPalette = static_cast<int32_t>(palette);
            if (weightsCandidate.paletteBoneIds[palette] == replacementId) replacementPalette = static_cast<int32_t>(palette);
        }
        if (removedPalette >= 0 && replacementPalette < 0)
        {
            weightsCandidate.paletteBoneIds[static_cast<uint32_t>(removedPalette)] = replacementId;
        }
        else if (removedPalette >= 0)
        {
            for (skeletal::CANONICAL_VERTEX_WEIGHT &vertex : weightsCandidate.vertices)
            {
                std::map<uint16_t, float> merged;
                for (uint32_t slot = 0; slot < 4; ++slot)
                {
                    uint16_t palette = vertex.paletteIndex[slot];
                    if (palette == UINT16_MAX || vertex.weight[slot] <= 0.0f) continue;
                    if (palette == static_cast<uint16_t>(removedPalette))
                        palette = static_cast<uint16_t>(replacementPalette);
                    merged[palette] += vertex.weight[slot];
                }
                uint32_t slot = 0;
                for (const auto &influence : merged)
                {
                    uint16_t palette = influence.first;
                    if (palette > static_cast<uint16_t>(removedPalette)) --palette;
                    vertex.paletteIndex[slot] = palette;
                    vertex.weight[slot] = influence.second;
                    ++slot;
                }
                while (slot < 4)
                {
                    vertex.paletteIndex[slot] = UINT16_MAX;
                    vertex.weight[slot] = 0.0f;
                    ++slot;
                }
            }
            weightsCandidate.paletteBoneIds.erase(weightsCandidate.paletteBoneIds.begin() + removedPalette);
        }

        uint32_t removedTracks = 0;
        for (skeletal::SKELETAL_CLIP &clip : animationsCandidate.clips)
        {
            const auto before = clip.tracks.size();
            clip.tracks.erase(std::remove_if(clip.tracks.begin(), clip.tracks.end(),
                [removedId](const skeletal::SKELETAL_TRACK &track) { return track.boneId == removedId; }),
                clip.tracks.end());
            removedTracks += static_cast<uint32_t>(before - clip.tracks.size());
        }
        if (removedTracks > 0 && !discardAnimationTracks)
            return fail("canonical bone has animation tracks; explicit discard confirmation is required");

        if (hasChildren)
        {
            const uint64_t newParentId = source[index].parentBoneId;
            const int32_t newParentIndex = impl->canonicalSkeleton.compiled.bones[index].parentIndex;
            for (uint32_t childIndex = 0; childIndex < skeletonCandidate.sourceBones.size(); ++childIndex)
            {
                skeletal::CANONICAL_BONE &child = skeletonCandidate.sourceBones[childIndex];
                if (child.parentBoneId != removedId) continue;
                MATRIX local = impl->canonicalSkeleton.compiled.bones[childIndex].globalBindMatrix;
                if (newParentIndex >= 0)
                {
                    MATRIX inverseParent;
                    float determinant = 0.0f;
                    MatrixInverse(&inverseParent, &determinant,
                        &impl->canonicalSkeleton.compiled.bones[static_cast<uint32_t>(newParentIndex)].globalBindMatrix);
                    if (!std::isfinite(determinant) || std::fabs(determinant) <= skeletal::SINGULAR_TOLERANCE)
                        return fail("new child parent bind transform is not invertible");
                    MatrixMultiply(&local, &impl->canonicalSkeleton.compiled.bones[childIndex].globalBindMatrix,
                                   &inverseParent);
                }
                bool negativeScale = false, shear = false;
                if (!skeletal::decomposeTrsMatrix(local, child.localBind, negativeScale, shear) || shear)
                    return fail("preserving child global bind would require unsupported shear");
                child.parentBoneId = newParentId;
            }
        }
        skeletonCandidate.sourceBones.erase(skeletonCandidate.sourceBones.begin() + index);
        if (hasChildren)
        {
            std::vector<skeletal::CANONICAL_BONE> ordered;
            std::unordered_set<uint64_t> placed;
            ordered.reserve(skeletonCandidate.sourceBones.size());
            while (ordered.size() < skeletonCandidate.sourceBones.size())
            {
                bool progress = false;
                for (const skeletal::CANONICAL_BONE &candidate : skeletonCandidate.sourceBones)
                {
                    if (placed.find(candidate.boneId) != placed.end()) continue;
                    if (candidate.parentBoneId == 0 || placed.find(candidate.parentBoneId) != placed.end())
                    {
                        ordered.push_back(candidate); placed.insert(candidate.boneId); progress = true;
                    }
                }
                if (!progress) return fail("child reparent could not produce parent-first ordering");
            }
            skeletonCandidate.sourceBones = std::move(ordered);
        }
        if (!skeletal::compileCanonicalSkeleton(skeletonCandidate.sourceBones, skeletonCandidate.compiled))
            return fail("remapped canonical skeleton would be invalid");
        if (weightsCandidate.skeletonId != 0)
        {
            if (weightsCandidate.frameIndex >= impl->buffer.size())
                return fail("canonical weight frame is out of range");
            const util::BUFFER_MESH_DEBUG *frame = impl->buffer[weightsCandidate.frameIndex];
            uint32_t vertexCount = 0;
            for (const util::SUBSET_DEBUG *subset : frame->subset)
                vertexCount += static_cast<uint32_t>(subset->vertexCount);
            if (!skeletal::validateCanonicalWeights(skeletonCandidate, weightsCandidate, vertexCount))
                return fail("remapped canonical weights would be invalid");
        }
        if (animationsCandidate.skeletonId != 0 &&
            !skeletal::validateCanonicalAnimations(skeletonCandidate, animationsCandidate))
            return fail("remapped canonical animations would be invalid");
        impl->canonicalSkeleton = std::move(skeletonCandidate);
        impl->canonicalWeights = std::move(weightsCandidate);
        impl->canonicalAnimations = std::move(animationsCandidate);
        return true;
    }



    bool MESH_MBM_DEBUG::setSkeletalVertexWeight(const uint32_t vertexIndex,
                                                  const char *boneName0, const float weight0,
                                                  const char *boneName1, const float weight1,
                                                  const char *boneName2, const float weight2,
                                                  const char *boneName3, const float weight3,
                                                  char *errorOut, const int errorOutLen)
    {
        const SKELETAL_VERTEX_WEIGHT_EDIT edit = {
            vertexIndex,
            {boneName0, boneName1, boneName2, boneName3},
            {weight0, weight1, weight2, weight3}
        };
        return setSkeletalVertexWeightsBatch(&edit, 1, errorOut, errorOutLen);
    }

    bool MESH_MBM_DEBUG::setSkeletalVertexWeightsBatch(const SKELETAL_VERTEX_WEIGHT_EDIT *edits,
                                                        const uint32_t editCount,
                                                        char *errorOut, const int errorOutLen)
    {
        auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (impl->canonicalSkeleton.skeletonId == 0 || impl->canonicalWeights.skeletonId == 0 ||
            impl->canonicalWeights.skeletonId != impl->canonicalSkeleton.skeletonId)
            return fail("mesh has no matching canonical skeleton and type-42 weights");
        if (!edits || editCount == 0)
            return fail("canonical vertex weight batch must contain at least one edit");

        skeletal::CANONICAL_WEIGHTS candidateWeights = impl->canonicalWeights;
        std::unordered_set<uint32_t> editedVertices;
        for (uint32_t editIndex = 0; editIndex < editCount; ++editIndex)
        {
            const SKELETAL_VERTEX_WEIGHT_EDIT &edit = edits[editIndex];
            if (edit.vertexIndex >= candidateWeights.vertices.size())
                return fail("canonical vertex weight index out of range");
            if (!editedVertices.insert(edit.vertexIndex).second)
                return fail("canonical vertex weight batch contains a duplicate vertex index");

            skeletal::CANONICAL_VERTEX_WEIGHT candidate;
            float weightSum = 0.0f;
            uint32_t influenceCount = 0;
            for (int slot = 0; slot < 4; ++slot)
            {
                const char *name = edit.boneNames[slot];
                const float value = edit.weights[slot];
                if (!name || !name[0])
                {
                    if (value != 0.0f) return fail("unused canonical influence must have zero weight");
                    continue;
                }
                if (!std::isfinite(value) || value <= 0.0f)
                    return fail("canonical influence weight must be finite and positive");
                const auto found = impl->canonicalSkeleton.compiled.indexByName.find(name);
                if (found == impl->canonicalSkeleton.compiled.indexByName.end())
                    return fail("canonical influence references an unknown bone name");
                const uint64_t boneId = impl->canonicalSkeleton.compiled.bones[found->second].boneId;
                for (int previous = 0; previous < slot; ++previous)
                    if (candidate.paletteIndex[previous] != UINT16_MAX &&
                        candidateWeights.paletteBoneIds[candidate.paletteIndex[previous]] == boneId)
                        return fail("canonical vertex contains a duplicate bone influence");
                auto paletteIt = std::find(candidateWeights.paletteBoneIds.begin(),
                                           candidateWeights.paletteBoneIds.end(), boneId);
                if (paletteIt == candidateWeights.paletteBoneIds.end())
                {
                    if (candidateWeights.paletteBoneIds.size() >= UINT16_MAX)
                        return fail("canonical weight palette is full");
                    candidateWeights.paletteBoneIds.push_back(boneId);
                    paletteIt = candidateWeights.paletteBoneIds.end() - 1;
                }
                candidate.paletteIndex[slot] = static_cast<uint16_t>(
                    paletteIt - candidateWeights.paletteBoneIds.begin());
                candidate.weight[slot] = value;
                weightSum += value;
                ++influenceCount;
            }
            if (influenceCount == 0 ||
                std::fabs(weightSum - 1.0f) > skeletal::MATRIX_TOLERANCE)
                return fail("canonical influence weights must sum to one");
            candidateWeights.vertices[edit.vertexIndex] = candidate;
        }

        if (!skeletal::validateCanonicalWeights(impl->canonicalSkeleton, candidateWeights,
                static_cast<uint32_t>(candidateWeights.vertices.size())))
            return fail("canonical vertex weight batch would produce invalid type-42 weights");
        impl->canonicalWeights = std::move(candidateWeights);
        return true;
    }

    bool MESH_MBM_DEBUG::getSkeletalVertexWeight(const uint32_t vertexIndex,
                                                  const char **boneName0, float *weight0,
                                                  const char **boneName1, float *weight1,
                                                  const char **boneName2, float *weight2,
                                                  const char **boneName3, float *weight3) const noexcept
    {
        if (vertexIndex>=impl->canonicalWeights.vertices.size()) return false;
        const skeletal::CANONICAL_VERTEX_WEIGHT &entry=impl->canonicalWeights.vertices[vertexIndex];
        const char **outNames[4]={boneName0,boneName1,boneName2,boneName3};
        float *outWeights[4]={weight0,weight1,weight2,weight3};
        for (int slot=0; slot<4; ++slot)
        {
            const bool used=entry.paletteIndex[slot]!=UINT16_MAX &&
                entry.paletteIndex[slot]<impl->canonicalWeights.paletteBoneIds.size();
            const char *name=nullptr;
            if (used)
            {
                const uint64_t id=impl->canonicalWeights.paletteBoneIds[entry.paletteIndex[slot]];
                const auto found=impl->canonicalSkeleton.compiled.indexById.find(id);
                if (found==impl->canonicalSkeleton.compiled.indexById.end()) return false;
                name=impl->canonicalSkeleton.compiled.bones[found->second].name.c_str();
            }
            if (outNames[slot]) *outNames[slot]=name;
            if (outWeights[slot]) *outWeights[slot]=used ? entry.weight[slot] : 0.0f;
        }
        return true;
    }

    bool MESH_MBM_DEBUG::initializeSkeletalVertexWeights(const uint32_t boneIndex,
                                                          uint32_t *vertexCountOut,
                                                          char *errorOut, const int errorOutLen)
    {
        const auto fail = [errorOut, errorOutLen](const char *message)
        {
            if (errorOut && errorOutLen > 0) snprintf(errorOut, errorOutLen, "%s", message);
            return false;
        };
        if (impl->canonicalSkeleton.skeletonId == 0)
            return fail("mesh has no canonical skeleton");
        if (boneIndex >= impl->canonicalSkeleton.sourceBones.size())
            return fail("canonical rigid-bind bone index is out of range");
        if (impl->canonicalWeights.skeletonId != 0)
            return fail("mesh already contains canonical skeletal weights");
        if (impl->buffer.empty() || !impl->buffer[0])
            return fail("mesh has no frame-zero geometry for skeletal weights");
        uint32_t vertexCount = 0;
        for (const util::SUBSET_DEBUG *subset : impl->buffer[0]->subset)
            vertexCount += static_cast<uint32_t>(subset->vertexCount);
        if (vertexCount == 0) return fail("mesh frame zero has no vertices for skeletal weights");
        skeletal::CANONICAL_WEIGHTS candidate;
        candidate.skeletonId = impl->canonicalSkeleton.skeletonId;
        candidate.frameIndex = 0;
        candidate.paletteBoneIds.push_back(impl->canonicalSkeleton.sourceBones[boneIndex].boneId);
        candidate.vertices.resize(vertexCount);
        for (skeletal::CANONICAL_VERTEX_WEIGHT &vertex : candidate.vertices)
        {
            vertex.paletteIndex[0] = 0;
            vertex.weight[0] = 1.0f;
        }
        if (!skeletal::validateCanonicalWeights(impl->canonicalSkeleton, candidate, vertexCount))
            return fail("initial canonical skeletal weights would be invalid");
        impl->canonicalWeights = std::move(candidate);
        if (vertexCountOut) *vertexCountOut = vertexCount;
        return true;
    }

    bool MESH_MBM_DEBUG::hasSkeletalVertexWeights() const noexcept
    {
        return impl->canonicalWeights.skeletonId!=0 && !impl->canonicalWeights.vertices.empty();
    }

    bool MESH_MBM_DEBUG::removeSkeletalVertexWeights(uint32_t *vertexCountOut,
                                                      char *errorOut, const int errorOutLen)
    {
        if (impl->canonicalWeights.skeletonId == 0)
        {
            if (errorOut && errorOutLen > 0)
                snprintf(errorOut, errorOutLen, "%s", "mesh has no canonical skeletal weights");
            return false;
        }
        if (vertexCountOut)
            *vertexCountOut = static_cast<uint32_t>(impl->canonicalWeights.vertices.size());
        impl->canonicalWeights = skeletal::CANONICAL_WEIGHTS();
        return true;
    }

    bool MESH_MBM_DEBUG::removeAllSkeletalData(uint32_t *boneCountOut,
                                                uint32_t *vertexCountOut,
                                                uint32_t *clipCountOut,
                                                char *errorOut, const int errorOutLen)
    {
        if (impl->canonicalSkeleton.skeletonId == 0)
        {
            if (errorOut && errorOutLen > 0)
                snprintf(errorOut, errorOutLen, "%s", "mesh has no canonical skeleton");
            return false;
        }
        if (boneCountOut)
            *boneCountOut = static_cast<uint32_t>(impl->canonicalSkeleton.sourceBones.size());
        if (vertexCountOut)
            *vertexCountOut = static_cast<uint32_t>(impl->canonicalWeights.vertices.size());
        if (clipCountOut)
            *clipCountOut = static_cast<uint32_t>(impl->canonicalAnimations.clips.size());
        impl->canonicalAnimations = skeletal::CANONICAL_ANIMATIONS();
        impl->canonicalWeights = skeletal::CANONICAL_WEIGHTS();
        impl->canonicalSkeleton = skeletal::CANONICAL_SKELETON();
        return true;
    }

    uint32_t MESH_MBM_DEBUG::getTotalSkeletalWeightBones() const noexcept
    {
        return static_cast<uint32_t>(impl->canonicalWeights.paletteBoneIds.size());
    }

    uint32_t MESH_MBM_DEBUG::getTotalArticulatedParts() const noexcept
    {
        return static_cast<uint32_t>(impl->articulatedParts.size());
    }

    const util::ARTICULATED_PART_V11 *MESH_MBM_DEBUG::getArticulatedPart(const uint32_t index) const noexcept
    {
        return index < impl->articulatedParts.size() ? &impl->articulatedParts[index] : nullptr;
    }

    uint32_t MESH_MBM_DEBUG::initializeArticulatedParts()
    {
        uint64_t nextPartId = 1;
        for (const auto &part : impl->articulatedParts)
            if (part.partId >= nextPartId)
                nextPartId = part.partId + 1;

        uint32_t added = 0;
        for (uint32_t frameIndex = 0; frameIndex < impl->buffer.size(); ++frameIndex)
        {
            util::BUFFER_MESH_DEBUG *frame = impl->buffer[frameIndex];
            if (!frame)
                continue;
            for (uint32_t subsetIndex = 0; subsetIndex < frame->subset.size(); ++subsetIndex)
            {
                bool exists = false;
                for (const auto &part : impl->articulatedParts)
                {
                    if (part.frameIndex == frameIndex && part.subsetIndex == subsetIndex)
                    {
                        exists = true;
                        break;
                    }
                }
                if (exists)
                    continue;

                const util::SUBSET_DEBUG *subset = frame->subset[subsetIndex];
                VEC3 pivot(0.0f, 0.0f, 0.0f);
                if (subset && subset->vertexCount > 0)
                {
                    const VEC3 *positions = this->getPositionArray(frameIndex);
                    if (positions)
                    {
                        VEC3 minValue(FLT_MAX, FLT_MAX, FLT_MAX);
                        VEC3 maxValue(-FLT_MAX, -FLT_MAX, -FLT_MAX);
                        const int first = subset->vertexStart;
                        const int last = first + subset->vertexCount;
                        for (int vertex = first; vertex < last; ++vertex)
                        {
                            minValue.x = std::min(minValue.x, positions[vertex].x);
                            minValue.y = std::min(minValue.y, positions[vertex].y);
                            minValue.z = std::min(minValue.z, positions[vertex].z);
                            maxValue.x = std::max(maxValue.x, positions[vertex].x);
                            maxValue.y = std::max(maxValue.y, positions[vertex].y);
                            maxValue.z = std::max(maxValue.z, positions[vertex].z);
                        }
                        pivot = (minValue + maxValue) * 0.5f;
                    }
                }
                char errorOut[255] = "";
                const std::string name = "Frame " + std::to_string(frameIndex + 1) +
                                         " Subset " + std::to_string(subsetIndex + 1);
                if (this->addArticulatedPart(nextPartId++, frameIndex, subsetIndex, name.c_str(),
                                             pivot.x, pivot.y, pivot.z, 0.0f, 0.0f, 0.0f, 1.0f,
                                             0, errorOut, static_cast<int>(sizeof(errorOut))) > 0)
                    ++added;
            }
        }
        return added;
    }

    uint32_t MESH_MBM_DEBUG::removeArticulatedParts() noexcept
    {
        if (impl->articulatedParts.empty())
            return 0;
        std::unordered_set<uint64_t> removedPartIds;
        removedPartIds.reserve(impl->articulatedParts.size());
        for (const auto &part : impl->articulatedParts)
            removedPartIds.insert(part.partId);
        const uint32_t removedCount = static_cast<uint32_t>(impl->articulatedParts.size());
        impl->articulatedParts.clear();
        for (auto &clip : impl->articulatedClips)
        {
            clip.tracks.erase(std::remove_if(clip.tracks.begin(), clip.tracks.end(),
                                             [&removedPartIds](const ARTICULATED_TRACK_DATA &track)
                                             {
                                                 return removedPartIds.find(track.header.partId) != removedPartIds.end();
                                             }),
                               clip.tracks.end());
        }
        return removedCount;
    }

    int MESH_MBM_DEBUG::addArticulatedPart(const uint64_t partId, const uint32_t frameIndex,
                                           const uint32_t subsetIndex, const char *name,
                                           const float pivotX, const float pivotY, const float pivotZ,
                                           const float pivotQX, const float pivotQY, const float pivotQZ, const float pivotQW,
                                           const uint64_t parentPartId, char *errorOut, const int errorOutLen)
    {
        if (partId == 0)
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "partId must be non-zero");
            return 0;
        }
        if (frameIndex >= impl->buffer.size() || !impl->buffer[frameIndex] ||
            subsetIndex >= impl->buffer[frameIndex]->subset.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "frame/subset occurrence is out of range");
            return 0;
        }
        for (const auto &part : impl->articulatedParts)
        {
            if (part.partId == partId)
            {
                if (errorOut) snprintf(errorOut, errorOutLen, "partId [%llu] already exists",
                                       static_cast<unsigned long long>(partId));
                return 0;
            }
            if (part.frameIndex == frameIndex && part.subsetIndex == subsetIndex)
            {
                if (errorOut) snprintf(errorOut, errorOutLen, "frame [%u] subset [%u] already has a part",
                                       frameIndex + 1, subsetIndex + 1);
                return 0;
            }
        }
        if (parentPartId != 0)
        {
            bool parentExists = false;
            for (const auto &part : impl->articulatedParts)
            {
                if (part.frameIndex == frameIndex && part.partId == parentPartId)
                {
                    parentExists = true;
                    break;
                }
            }
            if (!parentExists)
            {
                if (errorOut) snprintf(errorOut, errorOutLen, "parent part [%llu] does not exist in frame [%u]",
                                       static_cast<unsigned long long>(parentPartId), frameIndex + 1);
                return 0;
            }
            if (parentPartId == partId)
            {
                if (errorOut) snprintf(errorOut, errorOutLen, "a part cannot be its own parent");
                return 0;
            }
        }
        util::ARTICULATED_PART_V11 part;
        part.partId = partId;
        part.frameIndex = frameIndex;
        part.subsetIndex = subsetIndex;
        part.parentPartId = parentPartId;
        part.name = name ? name : "";
        part.pivotX = pivotX; part.pivotY = pivotY; part.pivotZ = pivotZ;
        part.pivotQX = pivotQX; part.pivotQY = pivotQY; part.pivotQZ = pivotQZ; part.pivotQW = pivotQW;
        impl->articulatedParts.push_back(std::move(part));
        return static_cast<int>(impl->articulatedParts.size());
    }

    bool MESH_MBM_DEBUG::updateArticulatedPart(const uint32_t index, const char *name,
                                               const float pivotX, const float pivotY, const float pivotZ,
                                               const float pivotQX, const float pivotQY, const float pivotQZ, const float pivotQW,
                                               const uint64_t parentPartId, char *errorOut, const int errorOutLen)
    {
        if (index >= impl->articulatedParts.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated part index out of range");
            return false;
        }
        auto &part = impl->articulatedParts[index];
        if (parentPartId != 0)
        {
            uint64_t candidateId = parentPartId;
            for (size_t depth = 0; depth <= impl->articulatedParts.size(); ++depth)
            {
                if (candidateId == part.partId)
                {
                    if (errorOut) snprintf(errorOut, errorOutLen, "parent relationship would create a cycle");
                    return false;
                }
                const util::ARTICULATED_PART_V11 *candidate = nullptr;
                for (const auto &other : impl->articulatedParts)
                {
                    if (other.frameIndex == part.frameIndex && other.partId == candidateId)
                    {
                        candidate = &other;
                        break;
                    }
                }
                if (!candidate)
                {
                    if (errorOut) snprintf(errorOut, errorOutLen, "parent part [%llu] does not exist in frame [%u]",
                                           static_cast<unsigned long long>(candidateId), part.frameIndex + 1);
                    return false;
                }
                candidateId = candidate->parentPartId;
                if (candidateId == 0)
                    break;
            }
        }
        part.name = name ? name : "";
        part.pivotX = pivotX; part.pivotY = pivotY; part.pivotZ = pivotZ;
        part.pivotQX = pivotQX; part.pivotQY = pivotQY; part.pivotQZ = pivotQZ; part.pivotQW = pivotQW;
        part.parentPartId = parentPartId;
        return true;
    }

    uint32_t MESH_MBM_DEBUG::getTotalArticulatedAnimations() const noexcept
    {
        return static_cast<uint32_t>(impl->articulatedClips.size());
    }

    const char *MESH_MBM_DEBUG::getArticulatedAnimationName(const uint32_t index) const noexcept
    {
        return index < impl->articulatedClips.size() ? impl->articulatedClips[index].header.name.c_str() : nullptr;
    }

    bool MESH_MBM_DEBUG::getArticulatedAnimation(const uint32_t index, const char **name, float *duration,
                                                 float *speed, int *priority, bool *loop,
                                                 uint8_t *blendMode) const noexcept
    {
        if (index >= impl->articulatedClips.size() || !name || !duration || !speed ||
            !priority || !loop || !blendMode)
            return false;
        const auto &clip = impl->articulatedClips[index].header;
        *name = clip.name.c_str();
        *duration = clip.duration;
        *speed = clip.speed;
        *priority = clip.defaultPriority;
        *loop = clip.loop != 0;
        *blendMode = clip.blendMode;
        return true;
    }

    bool MESH_MBM_DEBUG::updateArticulatedAnimation(const uint32_t index, const char *name, const float duration,
                                                    const float speed, const int priority, const bool loop,
                                                    const uint8_t blendMode,
                                                    char *errorOut, const int errorOutLen)
    {
        if (index >= impl->articulatedClips.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation index out of range");
            return false;
        }
        if (blendMode != util::ARTICULATED_BLEND_ABSOLUTE &&
            blendMode != util::ARTICULATED_BLEND_ADDITIVE)
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "invalid articulated blend mode [%u]", blendMode);
            return false;
        }
        const std::string clipName = name ? name : "";
        if (clipName.empty())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation name cannot be empty");
            return false;
        }
        for (size_t i = 0; i < impl->articulatedClips.size(); ++i)
        {
            if (i != index && impl->articulatedClips[i].header.name == clipName)
            {
                if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation [%s] already exists", clipName.c_str());
                return false;
            }
        }
        auto &header = impl->articulatedClips[index].header;
        header.name = clipName;
        float greatestKeyTime = 0.0f;
        for (const auto &track : impl->articulatedClips[index].tracks)
            for (const auto &key : track.keys)
                greatestKeyTime = std::max(greatestKeyTime, key.time);
        header.duration = std::max(std::max(0.0f, duration), greatestKeyTime);
        header.speed = speed;
        header.defaultPriority = priority;
        header.loop = loop ? 1 : 0;
        header.blendMode = blendMode;
        return true;
    }

    uint32_t MESH_MBM_DEBUG::getTotalArticulatedTracks(const uint32_t animationIndex) const noexcept
    {
        return animationIndex < impl->articulatedClips.size()
            ? static_cast<uint32_t>(impl->articulatedClips[animationIndex].tracks.size()) : 0;
    }

    bool MESH_MBM_DEBUG::getArticulatedTrack(const uint32_t animationIndex, const uint32_t trackIndex,
                                             uint64_t *partId, uint8_t *channelMask, uint32_t *keyCount) const noexcept
    {
        if (animationIndex >= impl->articulatedClips.size() ||
            trackIndex >= impl->articulatedClips[animationIndex].tracks.size() ||
            !partId || !channelMask || !keyCount)
            return false;
        const auto &track = impl->articulatedClips[animationIndex].tracks[trackIndex];
        *partId = track.header.partId;
        *channelMask = track.header.channelMask;
        *keyCount = static_cast<uint32_t>(track.keys.size());
        return true;
    }

    bool MESH_MBM_DEBUG::getArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                           const uint32_t keyIndex, float *time,
                                           float *positionX, float *positionY, float *positionZ,
                                           float *rotationX, float *rotationY, float *rotationZ, float *rotationW,
                                           float *scaleX, float *scaleY, float *scaleZ,
                                           uint8_t *easing,
                                           float *bezierX1, float *bezierY1,
                                           float *bezierX2, float *bezierY2,
                                           float *rotationEulerX, float *rotationEulerY,
                                           float *rotationEulerZ, bool *hasRotationEuler) const noexcept
    {
        if (animationIndex >= impl->articulatedClips.size() ||
            trackIndex >= impl->articulatedClips[animationIndex].tracks.size() ||
            keyIndex >= impl->articulatedClips[animationIndex].tracks[trackIndex].keys.size() ||
            !time || !positionX || !positionY || !positionZ || !rotationX || !rotationY ||
            !rotationZ || !rotationW || !scaleX || !scaleY || !scaleZ || !easing ||
            !bezierX1 || !bezierY1 || !bezierX2 || !bezierY2 ||
            !rotationEulerX || !rotationEulerY || !rotationEulerZ || !hasRotationEuler)
            return false;
        const auto &key = impl->articulatedClips[animationIndex].tracks[trackIndex].keys[keyIndex];
        *time = key.time;
        *positionX = key.positionX; *positionY = key.positionY; *positionZ = key.positionZ;
        *rotationX = key.rotationX; *rotationY = key.rotationY;
        *rotationZ = key.rotationZ; *rotationW = key.rotationW;
        *scaleX = key.scaleX; *scaleY = key.scaleY; *scaleZ = key.scaleZ;
        *easing = key.easing;
        *bezierX1 = key.bezierX1; *bezierY1 = key.bezierY1;
        *bezierX2 = key.bezierX2; *bezierY2 = key.bezierY2;
        *rotationEulerX = key.rotationEulerX;
        *rotationEulerY = key.rotationEulerY;
        *rotationEulerZ = key.rotationEulerZ;
        *hasRotationEuler = key.hasRotationEuler != 0;
        return true;
    }

    int MESH_MBM_DEBUG::addArticulatedAnimation(const char *name, const float duration, const float speed,
                                                const int priority, const bool loop, const uint8_t blendMode,
                                                char *errorOut, const int errorOutLen)
    {
        const std::string clipName = name ? name : "";
        if (clipName.empty())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation name cannot be empty");
            return 0;
        }
        if (blendMode != util::ARTICULATED_BLEND_ABSOLUTE &&
            blendMode != util::ARTICULATED_BLEND_ADDITIVE)
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "invalid articulated blend mode [%u]", blendMode);
            return 0;
        }
        for (const auto &clip : impl->articulatedClips)
        {
            if (clip.header.name == clipName)
            {
                if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation [%s] already exists", clipName.c_str());
                return 0;
            }
        }
        ARTICULATED_CLIP_DATA clip;
        clip.header.name = clipName;
        clip.header.duration = duration < 0.0f ? 0.0f : duration;
        clip.header.speed = speed;
        clip.header.defaultPriority = priority;
        clip.header.loop = loop ? 1 : 0;
        clip.header.blendMode = blendMode;
        impl->articulatedClips.push_back(std::move(clip));
        return static_cast<int>(impl->articulatedClips.size());
    }

    bool MESH_MBM_DEBUG::removeArticulatedAnimation(const uint32_t animationIndex,
                                                    char *errorOut, const int errorOutLen)
    {
        if (animationIndex >= impl->articulatedClips.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation index out of range");
            return false;
        }
        impl->articulatedClips.erase(impl->articulatedClips.begin() + animationIndex);
        return true;
    }

    int MESH_MBM_DEBUG::addArticulatedTrack(const uint32_t animationIndex, const uint64_t partId,
                                            const uint8_t channelMask, char *errorOut, const int errorOutLen)
    {
        if (animationIndex >= impl->articulatedClips.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation index out of range");
            return 0;
        }
        if (!channelMask || (channelMask & ~(util::ARTICULATED_CHANNEL_POSITION |
                                             util::ARTICULATED_CHANNEL_ROTATION |
                                             util::ARTICULATED_CHANNEL_SCALE)))
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "invalid articulated channel mask");
            return 0;
        }
        auto &tracks = impl->articulatedClips[animationIndex].tracks;
        for (const auto &track : tracks)
        {
            if (track.header.partId == partId)
            {
                if (errorOut) snprintf(errorOut, errorOutLen, "track for partId [%llu] already exists in clip",
                                       static_cast<unsigned long long>(partId));
                return 0;
            }
        }
        ARTICULATED_TRACK_DATA track;
        track.header.partId = partId;
        track.header.channelMask = channelMask;
        tracks.push_back(std::move(track));
        return static_cast<int>(tracks.size());
    }

    bool MESH_MBM_DEBUG::setArticulatedTrackChannels(const uint32_t animationIndex,
                                                     const uint32_t trackIndex,
                                                     const uint8_t channelMask,
                                                     char *errorOut, const int errorOutLen)
    {
        if (animationIndex >= impl->articulatedClips.size() ||
            trackIndex >= impl->articulatedClips[animationIndex].tracks.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation/track index out of range");
            return false;
        }
        if (!channelMask || (channelMask & ~(util::ARTICULATED_CHANNEL_POSITION |
                                             util::ARTICULATED_CHANNEL_ROTATION |
                                             util::ARTICULATED_CHANNEL_SCALE)))
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "invalid articulated channel mask");
            return false;
        }
        impl->articulatedClips[animationIndex].tracks[trackIndex].header.channelMask = channelMask;
        return true;
    }

    bool MESH_MBM_DEBUG::addArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                           const float time, const float positionX, const float positionY, const float positionZ,
                                           const float rotationX, const float rotationY, const float rotationZ, const float rotationW,
                                           const float scaleX, const float scaleY, const float scaleZ,
                                           char *errorOut, const int errorOutLen)
    {
        if (animationIndex >= impl->articulatedClips.size() ||
            trackIndex >= impl->articulatedClips[animationIndex].tracks.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation/track index out of range");
            return false;
        }
        auto &clip = impl->articulatedClips[animationIndex];
        auto &keys = clip.tracks[trackIndex].keys;
        util::ARTICULATED_KEY_V11 key;
        key.time = time < 0.0f ? 0.0f : time;
        key.positionX = positionX; key.positionY = positionY; key.positionZ = positionZ;
        key.rotationX = rotationX; key.rotationY = rotationY; key.rotationZ = rotationZ; key.rotationW = rotationW;
        key.scaleX = scaleX; key.scaleY = scaleY; key.scaleZ = scaleZ;
        constexpr float keyTimeEpsilon = 0.00001f;
        for (auto &existing : keys)
        {
            if (std::fabs(existing.time - key.time) <= keyTimeEpsilon)
            {
                existing = key;
                clip.header.duration = std::max(clip.header.duration, key.time);
                return true;
            }
        }
        keys.push_back(key);
        std::sort(keys.begin(), keys.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
        clip.header.duration = std::max(clip.header.duration, key.time);
        return true;
    }

    bool MESH_MBM_DEBUG::setArticulatedKeyEuler(const uint32_t animationIndex, const uint32_t trackIndex,
                                                const float time, const float rotationEulerX,
                                                const float rotationEulerY, const float rotationEulerZ,
                                                char *errorOut, const int errorOutLen)
    {
        if (animationIndex >= impl->articulatedClips.size() ||
            trackIndex >= impl->articulatedClips[animationIndex].tracks.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation/track index out of range");
            return false;
        }
        auto &keys = impl->articulatedClips[animationIndex].tracks[trackIndex].keys;
        constexpr float keyTimeEpsilon = 0.00001f;
        for (auto &key : keys)
        {
            if (std::fabs(key.time - time) <= keyTimeEpsilon)
            {
                key.rotationEulerX = rotationEulerX;
                key.rotationEulerY = rotationEulerY;
                key.rotationEulerZ = rotationEulerZ;
                key.hasRotationEuler = 1;
                return true;
            }
        }
        if (errorOut) snprintf(errorOut, errorOutLen, "articulated key time not found");
        return false;
    }

    bool MESH_MBM_DEBUG::setArticulatedKeyEasing(const uint32_t animationIndex, const uint32_t trackIndex,
                                                 const uint32_t keyIndex, const uint8_t easing,
                                                 char *errorOut, const int errorOutLen)
    {
        if (animationIndex >= impl->articulatedClips.size() ||
            trackIndex >= impl->articulatedClips[animationIndex].tracks.size() ||
            keyIndex >= impl->articulatedClips[animationIndex].tracks[trackIndex].keys.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated key index out of range");
            return false;
        }
        if (easing > util::ARTICULATED_EASING_BEZIER)
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "invalid articulated easing");
            return false;
        }
        impl->articulatedClips[animationIndex].tracks[trackIndex].keys[keyIndex].easing = easing;
        return true;
    }

    bool MESH_MBM_DEBUG::setArticulatedKeyBezier(const uint32_t animationIndex, const uint32_t trackIndex,
                                                 const uint32_t keyIndex,
                                                 const float x1, const float y1,
                                                 const float x2, const float y2,
                                                 char *errorOut, const int errorOutLen)
    {
        if (animationIndex >= impl->articulatedClips.size() ||
            trackIndex >= impl->articulatedClips[animationIndex].tracks.size() ||
            keyIndex >= impl->articulatedClips[animationIndex].tracks[trackIndex].keys.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated key index out of range");
            return false;
        }
        if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2) ||
            x1 < 0.0f || x1 > 1.0f || x2 < 0.0f || x2 > 1.0f)
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "invalid articulated Bezier control points");
            return false;
        }
        auto &key = impl->articulatedClips[animationIndex].tracks[trackIndex].keys[keyIndex];
        key.bezierX1 = x1; key.bezierY1 = y1;
        key.bezierX2 = x2; key.bezierY2 = y2;
        return true;
    }

    bool MESH_MBM_DEBUG::updateArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                              const uint32_t keyIndex, const float time,
                                              const float positionX, const float positionY, const float positionZ,
                                              const float rotationX, const float rotationY, const float rotationZ, const float rotationW,
                                              const float scaleX, const float scaleY, const float scaleZ,
                                              char *errorOut, const int errorOutLen)
    {
        if (animationIndex >= impl->articulatedClips.size() ||
            trackIndex >= impl->articulatedClips[animationIndex].tracks.size() ||
            keyIndex >= impl->articulatedClips[animationIndex].tracks[trackIndex].keys.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation/track/key index out of range");
            return false;
        }
        auto &clip = impl->articulatedClips[animationIndex];
        auto &keys = clip.tracks[trackIndex].keys;
        util::ARTICULATED_KEY_V11 key = keys[keyIndex];
        key.time = time < 0.0f ? 0.0f : time;
        key.positionX = positionX; key.positionY = positionY; key.positionZ = positionZ;
        key.rotationX = rotationX; key.rotationY = rotationY; key.rotationZ = rotationZ; key.rotationW = rotationW;
        key.scaleX = scaleX; key.scaleY = scaleY; key.scaleZ = scaleZ;
        constexpr float keyTimeEpsilon = 0.00001f;

        keys.erase(keys.begin() + static_cast<ptrdiff_t>(keyIndex));
        for (auto it = keys.begin(); it != keys.end(); ++it)
        {
            if (std::fabs(it->time - key.time) <= keyTimeEpsilon)
            {
                *it = key;
                std::sort(keys.begin(), keys.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
                clip.header.duration = std::max(clip.header.duration, key.time);
                return true;
            }
        }
        keys.push_back(key);
        std::sort(keys.begin(), keys.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
        clip.header.duration = std::max(clip.header.duration, key.time);
        return true;
    }

    bool MESH_MBM_DEBUG::removeArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                              const uint32_t keyIndex, char *errorOut, const int errorOutLen)
    {
        if (animationIndex >= impl->articulatedClips.size() ||
            trackIndex >= impl->articulatedClips[animationIndex].tracks.size() ||
            keyIndex >= impl->articulatedClips[animationIndex].tracks[trackIndex].keys.size())
        {
            if (errorOut) snprintf(errorOut, errorOutLen, "articulated animation/track/key index out of range");
            return false;
        }
        auto &keys = impl->articulatedClips[animationIndex].tracks[trackIndex].keys;
        keys.erase(keys.begin() + static_cast<ptrdiff_t>(keyIndex));
        return true;
    }

    bool MESH_MBM_DEBUG::setAnimationEffectTexture(const uint32_t index, const char *fileName) noexcept
    {
        if (index >= this->impl->infoAnimation.lsHeaderAnim.size())
            return false;
        auto *infoHead = this->impl->infoAnimation.lsHeaderAnim[index];
        if (infoHead == nullptr)
            return false;
        const bool hasFileName = fileName != nullptr && fileName[0] != 0;
        if (hasFileName)
        {
            if (infoHead->effectShader == nullptr)
                infoHead->effectShader = new util::INFO_FX();
            infoHead->effectShader->setTextureAnimationEffectFileName(fileName);
            return true;
        }
        if (infoHead->effectShader == nullptr)
            return true;
        infoHead->effectShader->setTextureAnimationEffectFileName(nullptr);
        if (infoHead->effectShader->dataPS == nullptr && infoHead->effectShader->dataVS == nullptr)
        {
            delete infoHead->effectShader;
            infoHead->effectShader = nullptr;
        }
        return true;
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
        impl->angleDefault_deprecated   = VEC3(0, 0, 0);
        impl->positionOffset_deprecated = VEC3(0, 0, 0);
        impl->sizeCoordTexFrame_0 = 0;
        impl->typeMe              = util::TYPE_MESH_UNKNOWN;
        impl->backBufferWidth     = 0;
        impl->backBufferHeight    = 0;
        impl->formatVersion       = MBM_V11_FORMAT_VERSION;
        memset(static_cast<void*>(&this->impl->headerMesh), 0, sizeof(this->impl->headerMesh));
        impl->zoomEditorSprite.x = 1.0f;
        impl->zoomEditorSprite.y = 1.0f;
        util::MATERIAL m;
        this->impl->headerMesh.material      = m;
        this->impl->headerMesh.hasNorText[0] = HAS_NOR_NO;
        this->impl->headerMesh.hasNorText[1] = HAS_TEX_EACH_FRAME;
        this->impl->infoPhysics.release();
        this->impl->infoAnimation.release();
        impl->articulatedParts.clear();
        impl->articulatedClips.clear();
        impl->canonicalSkeleton = {};
        impl->canonicalWeights = {};
        impl->canonicalAnimations = {};
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
    
    TEXTURE * MESH_MBM::getMaterialTexture(const uint32_t indexFrame, const uint32_t indexSubset, const TEXTURE_ROLE role) const noexcept
    {
        if (indexFrame < impl->totalFramesMesh && impl->buffer)
        {
            BUFFER_GL *renderBuffer = impl->buffer[indexFrame].pBufferGL;
            if (renderBuffer)
                return renderBuffer->getTextureByStage(static_cast<uint32_t>(getTextureRoleBackendSlot(role)), indexSubset);
        }
        return nullptr;
    }

    bool MESH_MBM::setMaterialTexture(const uint32_t indexFrame, const uint32_t indexSubset, const TEXTURE_ROLE role,
                           const char *fileNameTexture, const bool hasAlpha) const
    {
        if (indexFrame < impl->totalFramesMesh && impl->buffer)
        {
            if (indexSubset < impl->buffer[indexFrame].totalSubset)
            {
                BUFFER_GL *renderBuffer = impl->buffer[indexFrame].pBufferGL;
                if (renderBuffer)
                {
                    const uint32_t stage = static_cast<uint32_t>(getTextureRoleBackendSlot(role));
                    if (fileNameTexture == nullptr || fileNameTexture[0] == 0)
                    {
                        renderBuffer->setTextureByStage(nullptr, stage, indexSubset);
                        return true;
                    }
                    TEXTURE *newTex = TEXTURE_MANAGER::getInstance()->load(fileNameTexture, hasAlpha);
                    if (newTex)
                    {
                        renderBuffer->setTextureByStage(newTex, stage, indexSubset);
                        return true;
                    }
                }
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
        impl->articulatedParts.clear();
        impl->articulatedClips.clear();
        impl->canonicalSkeleton = {};
        impl->canonicalWeights = {};
        impl->canonicalAnimations = {};
        impl->gpuSkinningInput = {};
        impl->skeletalBindPositions.clear();
        impl->skeletalBindNormals.clear();
        impl->skeletalBindUvs.clear();
        impl->skeletalBindIndices.clear();
        impl->skeletalBindFrameIndex = UINT32_MAX;
        impl->skeletalBindHasNormals = false;
        impl->skeletalBindHasUvs = false;
        impl->skeletalBindHasIndices = false;
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
        impl->skeletalBindFrameIndex = UINT32_MAX;
        impl->sizeCoordTexFrame_0 = 0;
    }
    
    bool MESH_MBM::isLoaded() const
    {
        return this->impl->buffer != nullptr;
    }

    uint32_t MESH_MBM::getPreparedSkeletalPaletteSize(const SKELETAL_SHADER_METHOD method) const noexcept
    {
        return impl->gpuSkinningInput.supports(method) ? impl->gpuSkinningInput.requiredBoneCount : 0;
    }

    bool MESH_MBM::supportsGpuSkeletalPath(const SKELETAL_SHADER_METHOD method) const noexcept
    {
        return impl->gpuSkinningInput.supports(method);
    }

    void MESH_MBM::resolveSkeletalSkinningMethod(SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        if (player.impl->requestedSkinningMethod != SKELETAL_SHADER_METHOD::AUTO)
        {
            player.impl->resolvedSkinningMethod = player.impl->requestedSkinningMethod;
            player.impl->skinningResolutionReason = player.impl->requestedSkinningMethod ==
                SKELETAL_SHADER_METHOD::DQS_RIGID ? "explicit-dqs" : "explicit-lbs";
            return;
        }

        const skeletal::DQS_COMPATIBILITY_STATUS compatibility =
            skeletal::getDqsCompatibility(impl->canonicalSkeleton, impl->canonicalAnimations);
        player.impl->resolvedSkinningMethod = compatibility == skeletal::DQS_COMPATIBILITY_STATUS::RIGID
            ? SKELETAL_SHADER_METHOD::DQS_RIGID : SKELETAL_SHADER_METHOD::LBS;
        player.impl->skinningResolutionReason = skeletal::dqsCompatibilityStatusName(compatibility);
    }

    void MESH_MBM::getSkeletalSkinningReport(const SKELETAL_SHADER_METHOD method, const char **status,
                                              uint32_t *requiredBoneCount,
                                              uint32_t *effectiveBoneCapacity) const noexcept
    {
        if (status)
        {
            const skeletal::GPU_SKINNING_PREPARATION_STATUS selectedStatus =
                impl->gpuSkinningInput.ready() && !impl->gpuSkinningInput.supports(method)
                    ? skeletal::GPU_SKINNING_PREPARATION_STATUS::PALETTE_TOO_LARGE
                    : impl->gpuSkinningInput.status;
            *status = skeletal::gpuSkinningPreparationStatusName(selectedStatus);
        }
        if (requiredBoneCount)
            *requiredBoneCount = impl->gpuSkinningInput.requiredBoneCount;
        if (effectiveBoneCapacity)
            *effectiveBoneCapacity = method == SKELETAL_SHADER_METHOD::DQS_RIGID
                ? impl->gpuSkinningInput.dqsBoneCapacity : impl->gpuSkinningInput.lbsBoneCapacity;
    }

    uint32_t MESH_MBM::getTotalSkeletalAnimations() const noexcept
    {
        return static_cast<uint32_t>(impl->canonicalAnimations.clips.size());
    }

    const char *MESH_MBM::getSkeletalAnimationName(const uint32_t index) const noexcept
    {
        return index < impl->canonicalAnimations.clips.size()
            ? impl->canonicalAnimations.clips[index].name.c_str() : nullptr;
    }

    bool MESH_MBM::getSkeletalAnimationDuration(const uint32_t index, float *duration) const noexcept
    {
        if (!duration || index >= impl->canonicalAnimations.clips.size())
            return false;
        *duration = impl->canonicalAnimations.clips[index].duration;
        return true;
    }

    bool MESH_MBM::playSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player, const char *name) const
    {
        if (impl->canonicalSkeleton.skeletonId == 0)
        {
#if defined _DEBUG || defined DEBUG || defined _DEBUG_
            WARN_LOG("playSkeletalAnimation failed: mesh has no canonical skeleton");
#endif
            return false;
        }
        if (impl->canonicalAnimations.clips.empty())
        {
#if defined _DEBUG || defined DEBUG || defined _DEBUG_
            WARN_LOG("playSkeletalAnimation failed: mesh has no canonical animation clips");
#endif
            return false;
        }
        if (!name || !name[0])
        {
#if defined _DEBUG || defined DEBUG || defined _DEBUG_
            WARN_LOG("playSkeletalAnimation failed: clip name is empty");
#endif
            return false;
        }
        if (player.impl->resolvedSkinningMethod == SKELETAL_SHADER_METHOD::NONE)
        {
#if defined _DEBUG || defined DEBUG || defined _DEBUG_
            WARN_LOG("playSkeletalAnimation failed for clip [%s]: skinning method is unresolved", name);
#endif
            return false;
        }
        for (uint32_t i = 0; i < impl->canonicalAnimations.clips.size(); ++i)
        {
            if (impl->canonicalAnimations.clips[i].name == name)
            {
                const uint32_t previousClipIndex = player.impl->clipIndex;
                const float previousTime = player.impl->time;
                const bool previousActive = player.impl->active;
                const bool previousPaused = player.impl->paused;
                const bool previousCompletionNotified = player.impl->baseCompletionNotified;
                const bool previousAuthoringPose = player.impl->authoringPose;
                player.impl->clipIndex = i;
                player.impl->time = 0.0f;
                player.impl->active = true;
                player.impl->paused = false;
                player.impl->baseCompletionNotified = false;
                player.impl->authoringPose = false;
                if (updateSkeletalAnimation(player, 0.0f))
                    return true;
                player.impl->clipIndex = previousClipIndex;
                player.impl->time = previousTime;
                player.impl->active = previousActive;
                player.impl->paused = previousPaused;
                player.impl->baseCompletionNotified = previousCompletionNotified;
                player.impl->authoringPose = previousAuthoringPose;
#if defined _DEBUG || defined DEBUG || defined _DEBUG_
                const BUFFER_GL *buffer = impl->buffer && impl->totalFramesMesh > 0
                    ? impl->buffer[0].pBufferGL : nullptr;
                const bool hasNormals = buffer &&
                    (buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR ||
                     buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
                skeletal::SKELETAL_POSE diagnosticPose;
                const skeletal::SKELETAL_CLIP &diagnosticClip = impl->canonicalAnimations.clips[i];
                if (!skeletal::sampleSkeletalClip(impl->canonicalSkeleton.compiled,
                        diagnosticClip, 0.0f, diagnosticPose))
                {
                    WARN_LOG("playSkeletalAnimation failed for clip [%s]: initial clip sampling failed", name);
                }
                else
                {
                    std::vector<float> diagnosticRows;
                    if (player.impl->resolvedSkinningMethod == SKELETAL_SHADER_METHOD::DQS_RIGID)
                    {
                        const skeletal::DQS_PALETTE_STATUS status = skeletal::buildDqsPalette(
                            impl->canonicalSkeleton, diagnosticPose, diagnosticRows);
                        const char *reason = status == skeletal::DQS_PALETTE_STATUS::INVALID_POSE
                            ? "invalid initial DQS pose"
                            : status == skeletal::DQS_PALETTE_STATUS::UNSUPPORTED_NON_RIGID_TRANSFORM
                                ? "initial DQS pose contains a non-rigid transform"
                                : "initial composed pose/player state evaluation failed";
                        WARN_LOG("playSkeletalAnimation failed for clip [%s]: %s", name, reason);
                    }
                    else
                    {
                        const skeletal::LBS_PALETTE_STATUS status = skeletal::buildLbsPalette(
                            impl->canonicalSkeleton, diagnosticPose, hasNormals, diagnosticRows);
                        const char *reason = status == skeletal::LBS_PALETTE_STATUS::INVALID_POSE
                            ? "invalid initial LBS pose"
                            : status == skeletal::LBS_PALETTE_STATUS::UNSUPPORTED_NORMAL_TRANSFORM
                                ? "initial LBS pose has an unsupported normal transform"
                                : "initial composed pose/player state evaluation failed";
                        WARN_LOG("playSkeletalAnimation failed for clip [%s]: %s", name, reason);
                    }
                }
#endif
                return false;
            }
        }
#if defined _DEBUG || defined DEBUG || defined _DEBUG_
        WARN_LOG("playSkeletalAnimation failed: clip [%s] was not found", name);
#endif
        return false;
    }

    bool MESH_MBM::hasActiveSkeletalAnimation(const SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        return player.impl->active;
    }

    bool MESH_MBM::crossFadeSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player,
                                               const char *name, const float duration) const
    {
        if (!player.impl->active || player.impl->authoringPose || player.impl->crossFadeActive ||
            !name || !name[0] ||
            !std::isfinite(duration) || duration < 0.0f)
            return false;
        if (duration == 0.0f)
            return playSkeletalAnimation(player, name);
        uint32_t targetIndex = UINT32_MAX;
        for (uint32_t index = 0; index < impl->canonicalAnimations.clips.size(); ++index)
        {
            if (impl->canonicalAnimations.clips[index].name == name)
            {
                targetIndex = index;
                break;
            }
        }
        if (targetIndex == UINT32_MAX)
            return false;

        const uint32_t previousIndex = player.impl->absoluteLayerClipIndex;
        const float previousTime = player.impl->absoluteLayerTime;
        const float previousWeight = player.impl->absoluteLayerWeight;
        const float previousFadeStart = player.impl->absoluteLayerFadeStartWeight;
        const float previousFadeTarget = player.impl->absoluteLayerFadeTargetWeight;
        const float previousFadeDuration = player.impl->absoluteLayerFadeDuration;
        const float previousFadeElapsed = player.impl->absoluteLayerFadeElapsed;
        const bool previousFadeActive = player.impl->absoluteLayerFadeActive;
        const bool previousCrossFade = player.impl->crossFadeActive;
        const bool previousActive = player.impl->absoluteLayerActive;
        const bool previousAdditive = player.impl->additiveLayer;
        const bool previousLayerPaused = player.impl->layerPaused;
        const bool previousCompletionNotified = player.impl->layerCompletionNotified;
        const auto previousMask = player.impl->layerBoneMask;

        player.impl->absoluteLayerClipIndex = targetIndex;
        player.impl->absoluteLayerTime = 0.0f;
        player.impl->absoluteLayerWeight = 0.0f;
        player.impl->absoluteLayerFadeStartWeight = 0.0f;
        player.impl->absoluteLayerFadeTargetWeight = 1.0f;
        player.impl->absoluteLayerFadeDuration = duration;
        player.impl->absoluteLayerFadeElapsed = 0.0f;
        player.impl->absoluteLayerFadeActive = true;
        player.impl->absoluteLayerActive = true;
        player.impl->additiveLayer = false;
        player.impl->crossFadeActive = true;
        player.impl->layerPaused = false;
        player.impl->layerCompletionNotified = false;
        player.impl->layerBoneMask.clear();
        if (updateSkeletalAnimation(player, 0.0f))
            return true;

        player.impl->absoluteLayerClipIndex = previousIndex;
        player.impl->absoluteLayerTime = previousTime;
        player.impl->absoluteLayerWeight = previousWeight;
        player.impl->absoluteLayerFadeStartWeight = previousFadeStart;
        player.impl->absoluteLayerFadeTargetWeight = previousFadeTarget;
        player.impl->absoluteLayerFadeDuration = previousFadeDuration;
        player.impl->absoluteLayerFadeElapsed = previousFadeElapsed;
        player.impl->absoluteLayerFadeActive = previousFadeActive;
        player.impl->absoluteLayerActive = previousActive;
        player.impl->additiveLayer = previousAdditive;
        player.impl->crossFadeActive = previousCrossFade;
        player.impl->layerPaused = previousLayerPaused;
        player.impl->layerCompletionNotified = previousCompletionNotified;
        player.impl->layerBoneMask = previousMask;
        return false;
    }

    bool MESH_MBM::pauseSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        if (!player.impl->active)
            return false;
        player.impl->paused = true;
        player.impl->evaluatedMotionDeltaValid = false;
        return true;
    }

    bool MESH_MBM::resumeSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        if (!player.impl->active)
            return false;
        player.impl->paused = false;
        return true;
    }

    bool MESH_MBM::stopSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        if (!player.impl->active)
            return false;
        player.impl->clipIndex = UINT32_MAX;
        player.impl->time = 0.0f;
        player.impl->absoluteLayerClipIndex = UINT32_MAX;
        player.impl->absoluteLayerTime = 0.0f;
        player.impl->absoluteLayerWeight = 0.0f;
        player.impl->absoluteLayerFadeStartWeight = 0.0f;
        player.impl->absoluteLayerFadeTargetWeight = 0.0f;
        player.impl->absoluteLayerFadeDuration = 0.0f;
        player.impl->absoluteLayerFadeElapsed = 0.0f;
        player.impl->absoluteLayerFadeActive = false;
        player.impl->absoluteLayerActive = false;
        player.impl->additiveLayer = false;
        player.impl->crossFadeActive = false;
        player.impl->layerPaused = false;
        player.impl->layerBoneMask.clear();
        player.impl->active = false;
        player.impl->paused = false;
        player.impl->baseCompletionNotified = false;
        player.impl->layerCompletionNotified = false;
        player.impl->paletteRows.clear();
        player.impl->evaluatedGlobalTransforms.clear();
        player.impl->previousEvaluatedGlobalTransforms.clear();
        player.impl->rawEvaluatedGlobalTransforms.clear();
        player.impl->previousRawEvaluatedGlobalTransforms.clear();
        player.impl->evaluatedMotionDeltaValid = false;
        player.impl->authoringPose = false;
        return true;
    }

    bool MESH_MBM::seekSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player, const float time) const
    {
        if (!player.impl->active || !std::isfinite(time) ||
            player.impl->clipIndex >= impl->canonicalAnimations.clips.size())
            return false;
        const skeletal::SKELETAL_CLIP &clip = impl->canonicalAnimations.clips[player.impl->clipIndex];
        const float previousTime = player.impl->time;
        const bool previousCompletionNotified = player.impl->baseCompletionNotified;
        player.impl->time = std::max(0.0f, std::min(clip.duration, time));
        if (player.impl->time < clip.duration)
            player.impl->baseCompletionNotified = false;
        if (updateSkeletalAnimation(player, 0.0f))
            return true;
        player.impl->time = previousTime;
        player.impl->baseCompletionNotified = previousCompletionNotified;
        return false;
    }

    bool MESH_MBM::getSkeletalAnimationTime(const SKELETAL_ANIMATION_PLAYER &player,
                                             float *time) const noexcept
    {
        if (!time || !player.impl->active)
            return false;
        *time = player.impl->time;
        return true;
    }

    bool MESH_MBM::setSkeletalAnimationPlaybackSpeed(SKELETAL_ANIMATION_PLAYER &player,
                                                       const float speed) const noexcept
    {
        if (!std::isfinite(speed) || speed < 0.0f)
            return false;
        player.impl->playbackSpeed = speed;
        if (speed == 0.0f)
            player.impl->evaluatedMotionDeltaValid = false;
        return true;
    }

    float MESH_MBM::getSkeletalAnimationPlaybackSpeed(
        const SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        return player.impl->playbackSpeed;
    }

    bool MESH_MBM::playSkeletalAnimationAbsoluteLayer(SKELETAL_ANIMATION_PLAYER &player,
                                                        const char *name, const float weight) const
    {
        return playSkeletalAnimationLayer(player, name, weight, false);
    }

    bool MESH_MBM::playSkeletalAnimationAdditiveLayer(SKELETAL_ANIMATION_PLAYER &player,
                                                        const char *name, const float weight) const
    {
        return playSkeletalAnimationLayer(player, name, weight, true);
    }

    bool MESH_MBM::playSkeletalAnimationLayer(SKELETAL_ANIMATION_PLAYER &player,
                                                const char *name, const float weight,
                                                const bool additive) const
    {
        if (!player.impl->active || player.impl->authoringPose || !name || !name[0] ||
            !std::isfinite(weight) || weight < 0.0f || weight > 1.0f)
            return false;
        for (uint32_t index = 0; index < impl->canonicalAnimations.clips.size(); ++index)
        {
            if (impl->canonicalAnimations.clips[index].name == name)
            {
                const uint32_t previousIndex = player.impl->absoluteLayerClipIndex;
                const float previousTime = player.impl->absoluteLayerTime;
                const float previousWeight = player.impl->absoluteLayerWeight;
                const float previousFadeStart = player.impl->absoluteLayerFadeStartWeight;
                const float previousFadeTarget = player.impl->absoluteLayerFadeTargetWeight;
                const float previousFadeDuration = player.impl->absoluteLayerFadeDuration;
                const float previousFadeElapsed = player.impl->absoluteLayerFadeElapsed;
                const bool previousFadeActive = player.impl->absoluteLayerFadeActive;
                const bool previousActive = player.impl->absoluteLayerActive;
                const bool previousAdditive = player.impl->additiveLayer;
                const bool previousCrossFade = player.impl->crossFadeActive;
                const bool previousLayerPaused = player.impl->layerPaused;
                const bool previousCompletionNotified = player.impl->layerCompletionNotified;
                player.impl->absoluteLayerClipIndex = index;
                player.impl->absoluteLayerTime = 0.0f;
                player.impl->absoluteLayerWeight = weight;
                player.impl->absoluteLayerFadeStartWeight = weight;
                player.impl->absoluteLayerFadeTargetWeight = weight;
                player.impl->absoluteLayerFadeDuration = 0.0f;
                player.impl->absoluteLayerFadeElapsed = 0.0f;
                player.impl->absoluteLayerFadeActive = false;
                player.impl->absoluteLayerActive = true;
                player.impl->additiveLayer = additive;
                player.impl->crossFadeActive = false;
                player.impl->layerPaused = false;
                player.impl->layerCompletionNotified = false;
                if (updateSkeletalAnimation(player, 0.0f)) return true;
                player.impl->absoluteLayerClipIndex = previousIndex;
                player.impl->absoluteLayerTime = previousTime;
                player.impl->absoluteLayerWeight = previousWeight;
                player.impl->absoluteLayerFadeStartWeight = previousFadeStart;
                player.impl->absoluteLayerFadeTargetWeight = previousFadeTarget;
                player.impl->absoluteLayerFadeDuration = previousFadeDuration;
                player.impl->absoluteLayerFadeElapsed = previousFadeElapsed;
                player.impl->absoluteLayerFadeActive = previousFadeActive;
                player.impl->absoluteLayerActive = previousActive;
                player.impl->additiveLayer = previousAdditive;
                player.impl->crossFadeActive = previousCrossFade;
                player.impl->layerPaused = previousLayerPaused;
                player.impl->layerCompletionNotified = previousCompletionNotified;
                return false;
            }
        }
        return false;
    }

    bool MESH_MBM::pauseSkeletalAnimationLayer(SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        if (!player.impl->active || !player.impl->absoluteLayerActive)
            return false;
        player.impl->layerPaused = true;
        return true;
    }

    bool MESH_MBM::resumeSkeletalAnimationLayer(SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        if (!player.impl->active || !player.impl->absoluteLayerActive)
            return false;
        player.impl->layerPaused = false;
        return true;
    }

    bool MESH_MBM::isSkeletalAnimationLayerPaused(
        const SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        return player.impl->active && player.impl->absoluteLayerActive && player.impl->layerPaused;
    }

    bool MESH_MBM::stopSkeletalAnimationAbsoluteLayer(SKELETAL_ANIMATION_PLAYER &player) const
    {
        if (!player.impl->active || !player.impl->absoluteLayerActive)
            return false;
        const uint32_t previousIndex = player.impl->absoluteLayerClipIndex;
        const float previousTime = player.impl->absoluteLayerTime;
        const float previousWeight = player.impl->absoluteLayerWeight;
        const float previousFadeStart = player.impl->absoluteLayerFadeStartWeight;
        const float previousFadeTarget = player.impl->absoluteLayerFadeTargetWeight;
        const float previousFadeDuration = player.impl->absoluteLayerFadeDuration;
        const float previousFadeElapsed = player.impl->absoluteLayerFadeElapsed;
        const bool previousFadeActive = player.impl->absoluteLayerFadeActive;
        const bool previousAdditive = player.impl->additiveLayer;
        const bool previousCrossFade = player.impl->crossFadeActive;
        const bool previousLayerPaused = player.impl->layerPaused;
        const bool previousCompletionNotified = player.impl->layerCompletionNotified;
        player.impl->absoluteLayerClipIndex = UINT32_MAX;
        player.impl->absoluteLayerTime = 0.0f;
        player.impl->absoluteLayerWeight = 0.0f;
        player.impl->absoluteLayerFadeStartWeight = 0.0f;
        player.impl->absoluteLayerFadeTargetWeight = 0.0f;
        player.impl->absoluteLayerFadeDuration = 0.0f;
        player.impl->absoluteLayerFadeElapsed = 0.0f;
        player.impl->absoluteLayerFadeActive = false;
        player.impl->absoluteLayerActive = false;
        player.impl->additiveLayer = false;
        player.impl->crossFadeActive = false;
        player.impl->layerPaused = false;
        player.impl->layerCompletionNotified = false;
        if (updateSkeletalAnimation(player, 0.0f)) return true;
        player.impl->absoluteLayerClipIndex = previousIndex;
        player.impl->absoluteLayerTime = previousTime;
        player.impl->absoluteLayerWeight = previousWeight;
        player.impl->absoluteLayerFadeStartWeight = previousFadeStart;
        player.impl->absoluteLayerFadeTargetWeight = previousFadeTarget;
        player.impl->absoluteLayerFadeDuration = previousFadeDuration;
        player.impl->absoluteLayerFadeElapsed = previousFadeElapsed;
        player.impl->absoluteLayerFadeActive = previousFadeActive;
        player.impl->absoluteLayerActive = true;
        player.impl->additiveLayer = previousAdditive;
        player.impl->crossFadeActive = previousCrossFade;
        player.impl->layerPaused = previousLayerPaused;
        player.impl->layerCompletionNotified = previousCompletionNotified;
        return false;
    }

    bool MESH_MBM::seekSkeletalAnimationAbsoluteLayer(SKELETAL_ANIMATION_PLAYER &player,
                                                        const float time) const
    {
        if (!player.impl->active || !player.impl->absoluteLayerActive || !std::isfinite(time) ||
            player.impl->absoluteLayerClipIndex >= impl->canonicalAnimations.clips.size())
            return false;
        const skeletal::SKELETAL_CLIP &clip =
            impl->canonicalAnimations.clips[player.impl->absoluteLayerClipIndex];
        const float previousTime = player.impl->absoluteLayerTime;
        const bool previousCompletionNotified = player.impl->layerCompletionNotified;
        player.impl->absoluteLayerTime = std::max(0.0f, std::min(clip.duration, time));
        if (player.impl->absoluteLayerTime < clip.duration)
            player.impl->layerCompletionNotified = false;
        if (updateSkeletalAnimation(player, 0.0f)) return true;
        player.impl->absoluteLayerTime = previousTime;
        player.impl->layerCompletionNotified = previousCompletionNotified;
        return false;
    }

    bool MESH_MBM::setSkeletalAnimationAbsoluteLayerWeight(SKELETAL_ANIMATION_PLAYER &player,
                                                             const float weight) const
    {
        if (!player.impl->active || !player.impl->absoluteLayerActive ||
            !std::isfinite(weight) || weight < 0.0f || weight > 1.0f)
            return false;
        const float previousWeight = player.impl->absoluteLayerWeight;
        const float previousFadeStart = player.impl->absoluteLayerFadeStartWeight;
        const float previousFadeTarget = player.impl->absoluteLayerFadeTargetWeight;
        const float previousFadeDuration = player.impl->absoluteLayerFadeDuration;
        const float previousFadeElapsed = player.impl->absoluteLayerFadeElapsed;
        const bool previousFadeActive = player.impl->absoluteLayerFadeActive;
        const bool previousCrossFade = player.impl->crossFadeActive;
        player.impl->absoluteLayerWeight = weight;
        player.impl->absoluteLayerFadeStartWeight = weight;
        player.impl->absoluteLayerFadeTargetWeight = weight;
        player.impl->absoluteLayerFadeDuration = 0.0f;
        player.impl->absoluteLayerFadeElapsed = 0.0f;
        player.impl->absoluteLayerFadeActive = false;
        player.impl->crossFadeActive = false;
        if (updateSkeletalAnimation(player, 0.0f)) return true;
        player.impl->absoluteLayerWeight = previousWeight;
        player.impl->absoluteLayerFadeStartWeight = previousFadeStart;
        player.impl->absoluteLayerFadeTargetWeight = previousFadeTarget;
        player.impl->absoluteLayerFadeDuration = previousFadeDuration;
        player.impl->absoluteLayerFadeElapsed = previousFadeElapsed;
        player.impl->absoluteLayerFadeActive = previousFadeActive;
        player.impl->crossFadeActive = previousCrossFade;
        return false;
    }

    bool MESH_MBM::fadeSkeletalAnimationAbsoluteLayer(SKELETAL_ANIMATION_PLAYER &player,
                                                        const float targetWeight,
                                                        const float duration) const
    {
        if (!player.impl->active || !player.impl->absoluteLayerActive ||
            !std::isfinite(targetWeight) || targetWeight < 0.0f || targetWeight > 1.0f ||
            !std::isfinite(duration) || duration < 0.0f)
            return false;
        if (duration == 0.0f)
        {
            if (targetWeight == 0.0f)
                return stopSkeletalAnimationAbsoluteLayer(player);
            return setSkeletalAnimationAbsoluteLayerWeight(player, targetWeight);
        }
        player.impl->absoluteLayerFadeStartWeight = player.impl->absoluteLayerWeight;
        player.impl->absoluteLayerFadeTargetWeight = targetWeight;
        player.impl->absoluteLayerFadeDuration = duration;
        player.impl->absoluteLayerFadeElapsed = 0.0f;
        player.impl->absoluteLayerFadeActive = true;
        player.impl->crossFadeActive = false;
        return true;
    }

    bool MESH_MBM::getSkeletalAnimationAbsoluteLayerWeight(
        const SKELETAL_ANIMATION_PLAYER &player, float *weight) const noexcept
    {
        if (!weight || !player.impl->active || !player.impl->absoluteLayerActive)
            return false;
        *weight = player.impl->absoluteLayerWeight;
        return true;
    }

    bool MESH_MBM::getSkeletalAnimationAbsoluteLayerTime(
        const SKELETAL_ANIMATION_PLAYER &player, float *time) const noexcept
    {
        if (!time || !player.impl->active || !player.impl->absoluteLayerActive)
            return false;
        *time = player.impl->absoluteLayerTime;
        return true;
    }

    bool MESH_MBM::setSkeletalAnimationLayerBoneWeight(SKELETAL_ANIMATION_PLAYER &player,
                                                         const uint64_t boneId,
                                                         const float weight) const
    {
        return setSkeletalAnimationLayerBoneWeights(player, &boneId, &weight, 1);
    }

    bool MESH_MBM::setSkeletalAnimationLayerBoneWeights(SKELETAL_ANIMATION_PLAYER &player,
                                                          const uint64_t *boneIds,
                                                          const float *weights,
                                                          const uint32_t count) const
    {
        if (!player.impl->active || !player.impl->absoluteLayerActive ||
            player.impl->crossFadeActive || !boneIds || !weights || count == 0)
            return false;
        std::unordered_set<uint64_t> uniqueIds;
        auto candidate = player.impl->layerBoneMask;
        for (uint32_t index = 0; index < count; ++index)
        {
            const uint64_t boneId = boneIds[index];
            const float weight = weights[index];
            if (boneId == 0 || !uniqueIds.insert(boneId).second || !std::isfinite(weight) ||
                weight < 0.0f || weight > 1.0f ||
                impl->canonicalSkeleton.compiled.indexById.find(boneId) ==
                    impl->canonicalSkeleton.compiled.indexById.end())
                return false;
            if (weight == 1.0f)
                candidate.erase(boneId);
            else
                candidate[boneId] = weight;
        }
        const auto previous = player.impl->layerBoneMask;
        player.impl->layerBoneMask = std::move(candidate);
        if (updateSkeletalAnimation(player, 0.0f))
            return true;
        player.impl->layerBoneMask = previous;
        return false;
    }

    bool MESH_MBM::getSkeletalAnimationLayerBoneWeight(
        const SKELETAL_ANIMATION_PLAYER &player, const uint64_t boneId,
        float *weight) const noexcept
    {
        if (!weight || !player.impl->active || !player.impl->absoluteLayerActive ||
            player.impl->crossFadeActive || boneId == 0 ||
            impl->canonicalSkeleton.compiled.indexById.find(boneId) ==
                impl->canonicalSkeleton.compiled.indexById.end())
            return false;
        const auto found = player.impl->layerBoneMask.find(boneId);
        *weight = found == player.impl->layerBoneMask.end() ? 1.0f : found->second;
        return true;
    }

    bool MESH_MBM::clearSkeletalAnimationLayerMask(SKELETAL_ANIMATION_PLAYER &player) const
    {
        if (!player.impl->active || !player.impl->absoluteLayerActive ||
            player.impl->crossFadeActive)
            return false;
        const auto previous = player.impl->layerBoneMask;
        player.impl->layerBoneMask.clear();
        if (updateSkeletalAnimation(player, 0.0f))
            return true;
        player.impl->layerBoneMask = previous;
        return false;
    }

    uint32_t MESH_MBM::getSkeletalAnimationPoseBoneCount(
        const SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        return player.impl->active && !player.impl->authoringPose &&
            player.impl->evaluatedGlobalTransforms.size() ==
                impl->canonicalSkeleton.compiled.bones.size()
            ? static_cast<uint32_t>(player.impl->evaluatedGlobalTransforms.size()) : 0;
    }

    bool MESH_MBM::getSkeletalAnimationPoseBone(
        const SKELETAL_ANIMATION_PLAYER &player, const uint32_t boneIndex,
        SKELETAL_RUNTIME_POSE_BONE_INFO &out) const noexcept
    {
        if (boneIndex >= getSkeletalAnimationPoseBoneCount(player))
            return false;
        const skeletal::COMPILED_BONE &bone = impl->canonicalSkeleton.compiled.bones[boneIndex];
        out.boneId = bone.boneId;
        out.parentIndex = bone.parentIndex;
        out.globalMatrix = player.impl->evaluatedGlobalTransforms[boneIndex];
        return true;
    }

    bool MESH_MBM::getSkeletalBoneTransform(
        const SKELETAL_ANIMATION_PLAYER &player, const char *boneName,
        const MATRIX *modelMatrix, uint64_t *boneId, MATRIX *matrix, VEC3 *position,
        float rotation[4], VEC3 *angle, VEC3 *scale) const noexcept
    {
        if (!boneName || !boneId || !matrix || !position || !rotation || !angle || !scale ||
            getSkeletalAnimationPoseBoneCount(player) == 0)
            return false;
        const auto found = impl->canonicalSkeleton.compiled.indexByName.find(boneName);
        if (found == impl->canonicalSkeleton.compiled.indexByName.end())
            return false;
        const uint32_t boneIndex = found->second;
        MATRIX result = player.impl->evaluatedGlobalTransforms[boneIndex];
        if (modelMatrix)
            MatrixMultiply(&result, &result, modelMatrix);
        skeletal::LOCAL_TRANSFORM transform;
        bool hasNegativeScale = false;
        bool hasShear = false;
        if (!skeletal::decomposeTrsMatrix(result, transform, hasNegativeScale, hasShear) || hasShear)
            return false;
        *boneId = impl->canonicalSkeleton.compiled.bones[boneIndex].boneId;
        *matrix = result;
        *position = transform.translation;
        rotation[0] = transform.rotation.x;
        rotation[1] = transform.rotation.y;
        rotation[2] = transform.rotation.z;
        rotation[3] = transform.rotation.w;
        skeletal::LOCAL_TRANSFORM rotationOnly;
        rotationOnly.rotation = transform.rotation;
        MATRIX rotationMatrix = skeletal::buildTrsMatrix(rotationOnly);
        const float clamped = std::max(-1.0f, std::min(1.0f, -rotationMatrix._13));
        angle->y = std::asin(clamped);
        if (std::fabs(rotationMatrix._13) > 0.999999f)
        {
            angle->x = 0.0f;
            angle->z = std::atan2(-rotationMatrix._21, rotationMatrix._22);
        }
        else
        {
            angle->x = std::atan2(rotationMatrix._23, rotationMatrix._33);
            angle->z = std::atan2(rotationMatrix._12, rotationMatrix._11);
        }
        *scale = transform.scale;
        return true;
    }

    bool MESH_MBM::getSkeletalRootMotionDelta(
        const SKELETAL_ANIMATION_PLAYER &player, const char *boneName,
        const MATRIX *modelMatrix, uint64_t *boneId, VEC3 *translation) const noexcept
    {
        if (!boneName || !boneId || !translation || !player.impl->evaluatedMotionDeltaValid ||
            player.impl->previousRawEvaluatedGlobalTransforms.size() !=
                player.impl->rawEvaluatedGlobalTransforms.size())
            return false;
        const auto found = impl->canonicalSkeleton.compiled.indexByName.find(boneName);
        if (found == impl->canonicalSkeleton.compiled.indexByName.end())
            return false;
        const uint32_t boneIndex = found->second;
        if (boneIndex >= player.impl->rawEvaluatedGlobalTransforms.size())
            return false;
        MATRIX previous = player.impl->previousRawEvaluatedGlobalTransforms[boneIndex];
        MATRIX current = player.impl->rawEvaluatedGlobalTransforms[boneIndex];
        if (modelMatrix)
        {
            MatrixMultiply(&previous, &previous, modelMatrix);
            MatrixMultiply(&current, &current, modelMatrix);
        }
        *boneId = impl->canonicalSkeleton.compiled.bones[boneIndex].boneId;
        *translation = VEC3(current._41 - previous._41, current._42 - previous._42,
                            current._43 - previous._43);
        return std::isfinite(translation->x) && std::isfinite(translation->y) &&
            std::isfinite(translation->z);
    }

    bool MESH_MBM::hasSkeletalRenderPalette(const SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        return player.impl->active && !player.impl->paletteRows.empty();
    }

    namespace
    {
        VEC3 transformSkeletalMotionDeltaToOwnerSpace(const RENDERIZABLE &owner,
                                                      const VEC3 &delta) noexcept
        {
            VEC3 zero(0.0f, 0.0f, 0.0f);
            MATRIX modelMatrix;
            VEC3 position(0.0f, 0.0f, 0.0f);
            const VEC3 &angle = owner.getAngle();
            const VEC3 &scale = owner.getScale();
            MatrixTranslationRotationScale(&modelMatrix, &position, &angle, &scale);
            VEC3 origin;
            VEC3 transformed;
            vec3TransformCoord(&origin, &zero, &modelMatrix);
            vec3TransformCoord(&transformed, &delta, &modelMatrix);
            return VEC3(transformed.x - origin.x, transformed.y - origin.y,
                        transformed.z - origin.z);
        }

        bool composeSkeletalMotionRotationIntoOwner(RENDERIZABLE &owner,
                                                    const skeletal::QUATERNION &delta) noexcept
        {
            skeletal::LOCAL_TRANSFORM deltaTransform;
            deltaTransform.rotation = delta;
            const MATRIX deltaMatrix = skeletal::buildTrsMatrix(deltaTransform);
            MATRIX ownerMatrix;
            const VEC3 origin(0.0f, 0.0f, 0.0f);
            const VEC3 scale(1.0f, 1.0f, 1.0f);
            const VEC3 &ownerAngle = owner.getAngle();
            MatrixTranslationRotationScale(&ownerMatrix, &origin, &ownerAngle, &scale);
            MATRIX composed;
            MatrixMultiply(&composed, &deltaMatrix, &ownerMatrix);
            skeletal::LOCAL_TRANSFORM decomposed;
            bool hasNegativeScale = false;
            bool hasShear = false;
            if (!skeletal::decomposeTrsMatrix(composed, decomposed, hasNegativeScale,
                    hasShear) || hasShear)
                return false;
            skeletal::LOCAL_TRANSFORM rotationOnly;
            rotationOnly.rotation = decomposed.rotation;
            const MATRIX rotationMatrix = skeletal::buildTrsMatrix(rotationOnly);
            VEC3 angle;
            const float clamped = std::max(-1.0f, std::min(1.0f, -rotationMatrix._13));
            angle.y = std::asin(clamped);
            if (std::fabs(rotationMatrix._13) > 0.999999f)
            {
                angle.x = 0.0f;
                angle.z = std::atan2(-rotationMatrix._21, rotationMatrix._22);
            }
            else
            {
                angle.x = std::atan2(rotationMatrix._23, rotationMatrix._33);
                angle.z = std::atan2(rotationMatrix._12, rotationMatrix._11);
            }
            if (!std::isfinite(angle.x) || !std::isfinite(angle.y) || !std::isfinite(angle.z))
                return false;
            owner.setAngle(angle);
            return true;
        }
    }

    bool MESH_MBM::updateSkeletalAnimation(SKELETAL_ANIMATION_PLAYER &player, const float delta,
                                            RENDERIZABLE *owner,
                                            OnEndAnimation onEndAnimation) const
    {
        if (player.impl->authoringPose)
            return player.impl->active && !player.impl->paletteRows.empty();
        if (!player.impl->active || !std::isfinite(delta) || delta < 0.0f ||
            player.impl->clipIndex >= impl->canonicalAnimations.clips.size())
            return false;
        const float scaledDelta = delta * player.impl->playbackSpeed;
        if (!std::isfinite(scaledDelta))
            return false;
        const skeletal::SKELETAL_CLIP &clip = impl->canonicalAnimations.clips[player.impl->clipIndex];
        const float previousBaseTime = player.impl->time;
        const float previousLayerTime = player.impl->absoluteLayerTime;
        float evaluatedBaseTime = player.impl->time;
        float evaluatedLayerTime = player.impl->absoluteLayerTime;
        float evaluatedLayerWeight = player.impl->absoluteLayerWeight;
        float evaluatedFadeElapsed = player.impl->absoluteLayerFadeElapsed;
        bool evaluatedFadeActive = player.impl->absoluteLayerFadeActive;
        bool removeCompletedLayer = false;
        bool promoteCrossFade = false;
        bool notifyBaseCompletion = false;
        bool notifyLayerCompletion = false;
        if (!player.impl->paused && scaledDelta > 0.0f)
        {
            if (!skeletal::advanceSkeletalClipTime(clip, scaledDelta, evaluatedBaseTime))
                return false;
            notifyBaseCompletion = skeletal::shouldNotifySkeletalClipCompletion(
                clip, scaledDelta, evaluatedBaseTime,
                player.impl->baseCompletionNotified);
        }
        const skeletal::SKELETAL_CLIP *absoluteLayer = nullptr;
        if (player.impl->absoluteLayerActive)
        {
            if (player.impl->absoluteLayerClipIndex >= impl->canonicalAnimations.clips.size())
                return false;
            absoluteLayer = &impl->canonicalAnimations.clips[player.impl->absoluteLayerClipIndex];
            if (!player.impl->paused && !player.impl->layerPaused && scaledDelta > 0.0f)
            {
                if (!skeletal::advanceSkeletalClipTime(*absoluteLayer, scaledDelta,
                        evaluatedLayerTime))
                    return false;
                notifyLayerCompletion = skeletal::shouldNotifySkeletalClipCompletion(
                    *absoluteLayer, scaledDelta, evaluatedLayerTime,
                    player.impl->layerCompletionNotified);
                if (evaluatedFadeActive)
                {
                    bool fadeComplete = false;
                    if (!skeletal::advanceSkeletalAbsoluteFade(
                            player.impl->absoluteLayerFadeStartWeight,
                            player.impl->absoluteLayerFadeTargetWeight,
                            player.impl->absoluteLayerFadeDuration, scaledDelta,
                            evaluatedFadeElapsed, evaluatedLayerWeight, fadeComplete))
                        return false;
                    if (fadeComplete)
                    {
                        evaluatedFadeActive = false;
                        removeCompletedLayer = evaluatedLayerWeight == 0.0f;
                        promoteCrossFade = player.impl->crossFadeActive &&
                            evaluatedLayerWeight == 1.0f;
                    }
                }
            }
        }
        const BUFFER_GL *buffer = impl->buffer && impl->totalFramesMesh > 0
            ? impl->buffer[0].pBufferGL : nullptr;
        const bool hasNormals = buffer &&
            (buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR ||
             buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
        skeletal::SKELETAL_POSE pose;
        if (absoluteLayer)
        {
            std::vector<float> boneMask;
            if (!player.impl->layerBoneMask.empty())
            {
                boneMask.assign(impl->canonicalSkeleton.compiled.bones.size(), 1.0f);
                for (const auto &entry : player.impl->layerBoneMask)
                {
                    const auto found = impl->canonicalSkeleton.compiled.indexById.find(entry.first);
                    if (found == impl->canonicalSkeleton.compiled.indexById.end())
                        return false;
                    boneMask[static_cast<size_t>(found->second)] = entry.second;
                }
            }
            const bool sampled = player.impl->additiveLayer
                ? skeletal::sampleSkeletalClipsAdditiveMasked(impl->canonicalSkeleton.compiled,
                    clip, evaluatedBaseTime, *absoluteLayer, evaluatedLayerTime,
                    evaluatedLayerWeight, boneMask, pose)
                : skeletal::sampleSkeletalClipsAbsoluteMasked(impl->canonicalSkeleton.compiled,
                    clip, evaluatedBaseTime, *absoluteLayer, evaluatedLayerTime,
                    evaluatedLayerWeight, boneMask, pose);
            if (!sampled)
                return false;
        }
        else if (!skeletal::sampleSkeletalClip(impl->canonicalSkeleton.compiled, clip,
                     evaluatedBaseTime, pose))
            return false;
        const skeletal::SKELETAL_POSE rawPose = pose;
        const auto rootMotionBone = player.impl->automaticRootMotionEnabled
            ? impl->canonicalSkeleton.compiled.indexByName.find(
                  player.impl->automaticRootMotionBoneName)
            : impl->canonicalSkeleton.compiled.indexByName.end();
        if (rootMotionBone != impl->canonicalSkeleton.compiled.indexByName.end())
        {
            const size_t rootMotionIndex = static_cast<size_t>(rootMotionBone->second);
            if (rootMotionIndex > UINT32_MAX ||
                !skeletal::neutralizeSkeletalPoseLocalTransform(
                    impl->canonicalSkeleton.compiled, static_cast<uint32_t>(rootMotionIndex),
                    true, player.impl->automaticRootMotionApplyRotation, pose))
                return false;
        }
        std::vector<float> paletteRows;
        if (player.impl->resolvedSkinningMethod == SKELETAL_SHADER_METHOD::DQS_RIGID)
        {
            if (skeletal::buildDqsPalette(impl->canonicalSkeleton, pose, paletteRows) !=
                    skeletal::DQS_PALETTE_STATUS::READY)
                return false;
        }
        else if (skeletal::buildLbsPalette(impl->canonicalSkeleton, pose, hasNormals,
                     paletteRows) != skeletal::LBS_PALETTE_STATUS::READY)
            return false;
        player.impl->time = evaluatedBaseTime;
        if (notifyBaseCompletion)
            player.impl->baseCompletionNotified = true;
        if (promoteCrossFade)
        {
            player.impl->clipIndex = player.impl->absoluteLayerClipIndex;
            player.impl->time = evaluatedLayerTime;
            player.impl->baseCompletionNotified =
                player.impl->layerCompletionNotified || notifyLayerCompletion;
            player.impl->absoluteLayerClipIndex = UINT32_MAX;
            player.impl->absoluteLayerTime = 0.0f;
            player.impl->absoluteLayerWeight = 0.0f;
            player.impl->absoluteLayerFadeStartWeight = 0.0f;
            player.impl->absoluteLayerFadeTargetWeight = 0.0f;
            player.impl->absoluteLayerFadeDuration = 0.0f;
            player.impl->absoluteLayerFadeElapsed = 0.0f;
            player.impl->absoluteLayerFadeActive = false;
            player.impl->absoluteLayerActive = false;
            player.impl->additiveLayer = false;
            player.impl->crossFadeActive = false;
            player.impl->layerPaused = false;
            player.impl->layerCompletionNotified = false;
            player.impl->layerBoneMask.clear();
        }
        else if (removeCompletedLayer)
        {
            player.impl->absoluteLayerClipIndex = UINT32_MAX;
            player.impl->absoluteLayerTime = 0.0f;
            player.impl->absoluteLayerWeight = 0.0f;
            player.impl->absoluteLayerFadeStartWeight = 0.0f;
            player.impl->absoluteLayerFadeTargetWeight = 0.0f;
            player.impl->absoluteLayerFadeDuration = 0.0f;
            player.impl->absoluteLayerFadeElapsed = 0.0f;
            player.impl->absoluteLayerFadeActive = false;
            player.impl->absoluteLayerActive = false;
            player.impl->additiveLayer = false;
            player.impl->crossFadeActive = false;
            player.impl->layerPaused = false;
            player.impl->layerCompletionNotified = false;
        }
        else
        {
            player.impl->absoluteLayerTime = evaluatedLayerTime;
            player.impl->absoluteLayerWeight = evaluatedLayerWeight;
            player.impl->absoluteLayerFadeElapsed = evaluatedFadeElapsed;
            player.impl->absoluteLayerFadeActive = evaluatedFadeActive;
            if (notifyLayerCompletion)
                player.impl->layerCompletionNotified = true;
        }
        player.impl->paletteRows = std::move(paletteRows);
        const bool wrappedLoop = (clip.loop && evaluatedBaseTime < previousBaseTime) ||
            (absoluteLayer && absoluteLayer->loop && evaluatedLayerTime < previousLayerTime);
        player.impl->previousEvaluatedGlobalTransforms =
            std::move(player.impl->evaluatedGlobalTransforms);
        player.impl->evaluatedGlobalTransforms = std::move(pose.globalTransforms);
        player.impl->previousRawEvaluatedGlobalTransforms =
            std::move(player.impl->rawEvaluatedGlobalTransforms);
        player.impl->rawEvaluatedGlobalTransforms = std::move(rawPose.globalTransforms);
        player.impl->evaluatedMotionDeltaValid = !player.impl->paused && scaledDelta > 0.0f &&
            !wrappedLoop && player.impl->previousRawEvaluatedGlobalTransforms.size() ==
                player.impl->rawEvaluatedGlobalTransforms.size();
        if (player.impl->automaticRootMotionEnabled)
        {
            const auto found = impl->canonicalSkeleton.compiled.indexByName.find(
                player.impl->automaticRootMotionBoneName);
            if (found == impl->canonicalSkeleton.compiled.indexByName.end())
                player.impl->evaluatedMotionDeltaValid = false;
            else if (player.impl->evaluatedMotionDeltaValid && owner)
            {
                const uint32_t boneIndex = static_cast<uint32_t>(found->second);
                const MATRIX &previous = player.impl->previousRawEvaluatedGlobalTransforms[boneIndex];
                const MATRIX &current = player.impl->rawEvaluatedGlobalTransforms[boneIndex];
                const VEC3 modelDelta(current._41 - previous._41, current._42 - previous._42,
                                      current._43 - previous._43);
                const VEC3 worldDelta = transformSkeletalMotionDeltaToOwnerSpace(*owner, modelDelta);
                if (std::isfinite(worldDelta.x) && std::isfinite(worldDelta.y) &&
                    std::isfinite(worldDelta.z))
                {
                    VEC3 position = owner->getPosition();
                    position += worldDelta;
                    owner->setPosition(position);
                }
                if (player.impl->automaticRootMotionApplyRotation)
                {
                    skeletal::QUATERNION rotationDelta;
                    if (skeletal::computeSkeletalRootMotionRotationDelta(previous, current,
                            rotationDelta))
                        composeSkeletalMotionRotationIntoOwner(*owner, rotationDelta);
                }
            }
        }
        if (owner && onEndAnimation)
        {
            if (notifyBaseCompletion)
                onEndAnimation(clip.name.c_str(), owner);
            if (notifyLayerCompletion && absoluteLayer)
            {
                const std::string layerName = absoluteLayer->name;
                onEndAnimation(layerName.c_str(), owner);
            }
        }
        return true;
    }

    bool MESH_MBM::enableAutomaticSkeletalRootMotion(SKELETAL_ANIMATION_PLAYER &player,
                                                      const char *boneName,
                                                      const bool applyRotation) const noexcept
    {
        if (!boneName || !boneName[0])
            return false;
        const auto found = impl->canonicalSkeleton.compiled.indexByName.find(boneName);
        if (found == impl->canonicalSkeleton.compiled.indexByName.end())
            return false;
        player.impl->automaticRootMotionEnabled = true;
        player.impl->automaticRootMotionApplyRotation = applyRotation;
        player.impl->automaticRootMotionBoneName = boneName;
        player.impl->automaticRootMotionBoneId =
            impl->canonicalSkeleton.compiled.bones[static_cast<size_t>(found->second)].boneId;
        player.impl->evaluatedMotionDeltaValid = false;
        return true;
    }

    bool MESH_MBM::disableAutomaticSkeletalRootMotion(SKELETAL_ANIMATION_PLAYER &player) const noexcept
    {
        player.impl->automaticRootMotionEnabled = false;
        player.impl->automaticRootMotionApplyRotation = false;
        player.impl->automaticRootMotionBoneName.clear();
        player.impl->automaticRootMotionBoneId = 0;
        player.impl->evaluatedMotionDeltaValid = false;
        return true;
    }

    bool MESH_MBM::getAutomaticSkeletalRootMotionBone(const SKELETAL_ANIMATION_PLAYER &player,
                                                       const char **boneName,
                                                       uint64_t *boneId,
                                                       bool *applyRotation) const noexcept
    {
        if (!boneName || !boneId || !player.impl->automaticRootMotionEnabled)
            return false;
        const auto found = impl->canonicalSkeleton.compiled.indexByName.find(
            player.impl->automaticRootMotionBoneName);
        if (found == impl->canonicalSkeleton.compiled.indexByName.end())
            return false;
        *boneName = player.impl->automaticRootMotionBoneName.c_str();
        *boneId = impl->canonicalSkeleton.compiled.bones[static_cast<size_t>(found->second)].boneId;
        if (applyRotation)
            *applyRotation = player.impl->automaticRootMotionApplyRotation;
        return true;
    }

    bool MESH_MBM::setSkeletalAuthoringPalette(SKELETAL_ANIMATION_PLAYER &player,
                                                const SKELETAL_SHADER_METHOD method,
                                                const float *rows, const uint32_t rowCount,
                                                const uint64_t *orderedBoneIds,
                                                const uint32_t boneIdCount,
                                                const float time, char *errorOut,
                                                const int errorOutLen) const noexcept
    {
        const auto fail=[errorOut,errorOutLen](const char *format,auto... values)
        {
            if (errorOut && errorOutLen>0)
            {
                if constexpr (sizeof...(values)==0) snprintf(errorOut,errorOutLen,"%s",format);
                else snprintf(errorOut,errorOutLen,format,values...);
            }
            return false;
        };
        if (!rows) return fail("authoring palette rows are missing");
        if (!orderedBoneIds) return fail("authoring ordered bone identities are missing");
        if (!std::isfinite(time) || time<0.0f) return fail("authoring pose time is invalid");
        if (method!=SKELETAL_SHADER_METHOD::LBS && method!=SKELETAL_SHADER_METHOD::DQS_RIGID)
            return fail("authoring skinning method is invalid");
        if (method!=player.impl->resolvedSkinningMethod)
            return fail("authoring method does not match preview resolved method");
        if (!impl->gpuSkinningInput.supports(method))
        {
            const skeletal::GPU_SKINNING_PREPARATION_STATUS selectedStatus=
                impl->gpuSkinningInput.ready() ? skeletal::GPU_SKINNING_PREPARATION_STATUS::PALETTE_TOO_LARGE :
                impl->gpuSkinningInput.status;
            return fail("preview skeletal input is not ready: %s (%s)",
                skeletal::gpuSkinningPreparationStatusName(selectedStatus),impl->gpuSkinningInput.diagnostic);
        }
        const uint32_t stride = method == SKELETAL_SHADER_METHOD::DQS_RIGID ? 8u : 12u;
        const uint32_t expected = static_cast<uint32_t>(impl->canonicalSkeleton.compiled.bones.size()) * stride;
        if (rowCount!=expected)
            return fail("authoring palette row count mismatch: got %u, expected %u",rowCount,expected);
        if (boneIdCount!=impl->canonicalSkeleton.compiled.bones.size())
            return fail("authoring bone count mismatch: got %u, expected %u",boneIdCount,
                static_cast<uint32_t>(impl->canonicalSkeleton.compiled.bones.size()));
        if (!std::all_of(rows,rows+rowCount,[](const float value){ return std::isfinite(value); }))
            return fail("authoring palette contains a non-finite value");
        for (uint32_t index=0; index<boneIdCount; ++index)
            if (orderedBoneIds[index]!=impl->canonicalSkeleton.compiled.bones[index].boneId)
                return fail("authoring bone identity mismatch at index %u: got %016llx, expected %016llx",index+1,
                    static_cast<unsigned long long>(orderedBoneIds[index]),
                    static_cast<unsigned long long>(impl->canonicalSkeleton.compiled.bones[index].boneId));
        player.impl->paletteRows.assign(rows, rows + rowCount);
        player.impl->clipIndex = UINT32_MAX;
        player.impl->absoluteLayerClipIndex = UINT32_MAX;
        player.impl->absoluteLayerTime = 0.0f;
        player.impl->absoluteLayerWeight = 0.0f;
        player.impl->absoluteLayerFadeStartWeight = 0.0f;
        player.impl->absoluteLayerFadeTargetWeight = 0.0f;
        player.impl->absoluteLayerFadeDuration = 0.0f;
        player.impl->absoluteLayerFadeElapsed = 0.0f;
        player.impl->absoluteLayerFadeActive = false;
        player.impl->absoluteLayerActive = false;
        player.impl->additiveLayer = false;
        player.impl->crossFadeActive = false;
        player.impl->layerPaused = false;
        player.impl->layerBoneMask.clear();
        player.impl->evaluatedGlobalTransforms.clear();
        player.impl->previousEvaluatedGlobalTransforms.clear();
        player.impl->rawEvaluatedGlobalTransforms.clear();
        player.impl->previousRawEvaluatedGlobalTransforms.clear();
        player.impl->evaluatedMotionDeltaValid = false;
        player.impl->time = time;
        player.impl->active = true;
        player.impl->paused = true;
        player.impl->baseCompletionNotified = false;
        player.impl->layerCompletionNotified = false;
        player.impl->authoringPose = true;
        return true;
    }

    bool MESH_MBM::renderSkeletal(const SKELETAL_ANIMATION_PLAYER &player,
                                  const uint32_t indexFrame, const SHADER *shader,
                                  const RENDERIZABLE *owner)
    {
        if (!player.impl->active || player.impl->paletteRows.empty() ||
            indexFrame >= impl->totalFramesMesh || !impl->buffer ||
            !impl->gpuSkinningInput.supports(player.impl->resolvedSkinningMethod))
            return false;
        DEVICE *device = DEVICE::getInstance();
        device->setRenderMaterial(impl->material);
        const bool rendered = shader->render(impl->buffer[indexFrame].pBufferGL, owner, -1,
                                             player.impl->paletteRows.data(),
                                             static_cast<uint32_t>(player.impl->paletteRows.size()));
        device->clearRenderMaterial();
        return rendered;
    }

    bool MESH_MBM::hasArticulatedAnimationData() const noexcept
    {
        return !impl->articulatedClips.empty();
    }

    uint32_t MESH_MBM::getTotalArticulatedAnimations() const noexcept
    {
        return static_cast<uint32_t>(impl->articulatedClips.size());
    }

    const char *MESH_MBM::getArticulatedAnimationName(const uint32_t index) const noexcept
    {
        return index < impl->articulatedClips.size() ? impl->articulatedClips[index].header.name.c_str() : nullptr;
    }

    bool MESH_MBM::hasActiveArticulatedAnimations(const ARTICULATED_ANIMATION_PLAYER &player) const noexcept
    {
        return !player.impl->activeClips.empty();
    }

    bool MESH_MBM::playArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                            const char *name, const int priority,
                                            const float blendDuration, const float weight) const
    {
        if (!name || !name[0])
            return false;
        uint32_t clipIndex = 0;
        bool found = false;
        for (uint32_t i = 0; i < impl->articulatedClips.size(); ++i)
        {
            if (impl->articulatedClips[i].header.name == name)
            {
                clipIndex = i;
                found = true;
                break;
            }
        }
        if (!found)
            return false;

        for (auto &active : player.impl->activeClips)
        {
            if (active.clipIndex == clipIndex)
            {
                active.time = 0.0f;
                active.priority = priority;
                active.sequence = ++player.impl->sequence;
                active.blendDuration = std::max(0.0f, blendDuration);
                active.blendElapsed = 0.0f;
                active.weight = std::max(0.0f, std::min(1.0f, weight));
                active.paused = false;
                active.ended = false;
                return true;
            }
        }
        ACTIVE_ARTICULATED_CLIP active;
        active.clipIndex = clipIndex;
        active.priority = priority;
        active.sequence = ++player.impl->sequence;
        active.blendDuration = std::max(0.0f, blendDuration);
        active.weight = std::max(0.0f, std::min(1.0f, weight));
        player.impl->activeClips.push_back(active);
        return true;
    }

    bool MESH_MBM::pauseArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                             const char *name) const noexcept
    {
        if (!name)
            return false;
        for (auto &active : player.impl->activeClips)
        {
            if (active.clipIndex < impl->articulatedClips.size() &&
                impl->articulatedClips[active.clipIndex].header.name == name)
            {
                active.paused = true;
                return true;
            }
        }
        return false;
    }

    bool MESH_MBM::resumeArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                              const char *name) const noexcept
    {
        if (!name)
            return false;
        for (auto &active : player.impl->activeClips)
        {
            if (active.clipIndex < impl->articulatedClips.size() &&
                impl->articulatedClips[active.clipIndex].header.name == name)
            {
                active.paused = false;
                return true;
            }
        }
        return false;
    }

    bool MESH_MBM::disableArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                               const char *name) const noexcept
    {
        if (!name)
            return false;
        for (auto it = player.impl->activeClips.begin(); it != player.impl->activeClips.end(); ++it)
        {
            if (it->clipIndex < impl->articulatedClips.size() &&
                impl->articulatedClips[it->clipIndex].header.name == name)
            {
                player.impl->activeClips.erase(it);
                return true;
            }
        }
        return false;
    }

    bool MESH_MBM::seekArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                            const char *name, const float time) const noexcept
    {
        if (!name)
            return false;
        for (auto &active : player.impl->activeClips)
        {
            if (active.clipIndex < impl->articulatedClips.size() &&
                impl->articulatedClips[active.clipIndex].header.name == name)
            {
                const float duration = impl->articulatedClips[active.clipIndex].header.duration;
                active.time = std::max(0.0f, duration > 0.0f ? std::min(time, duration) : 0.0f);
                active.ended = false;
                return true;
            }
        }
        return false;
    }

    bool MESH_MBM::getArticulatedAnimationTime(const ARTICULATED_ANIMATION_PLAYER &player,
                                               const char *name, float *time) const noexcept
    {
        if (!name || !time)
            return false;
        for (const auto &active : player.impl->activeClips)
        {
            if (active.clipIndex < impl->articulatedClips.size() &&
                impl->articulatedClips[active.clipIndex].header.name == name)
            {
                *time = active.time;
                return true;
            }
        }
        return false;
    }

    void MESH_MBM::updateArticulatedAnimations(ARTICULATED_ANIMATION_PLAYER &player,
                                               const float delta, RENDERIZABLE *owner,
                                               OnEndAnimation onEndAnimation) const
    {
        if (delta <= 0.0f)
            return;
        std::vector<std::string> endedClipNames;
        for (auto &active : player.impl->activeClips)
        {
            if (active.paused || active.clipIndex >= impl->articulatedClips.size())
                continue;
            if (active.blendElapsed < active.blendDuration)
                active.blendElapsed = std::min(active.blendDuration, active.blendElapsed + delta);
            if (active.ended)
                continue;
            const ARTICULATED_CLIP_DATA &clip = impl->articulatedClips[active.clipIndex];
            const float duration = clip.header.duration;
            if (duration <= 0.0f)
            {
                active.time = 0.0f;
                active.ended = !clip.header.loop;
                if (active.ended && onEndAnimation)
                    endedClipNames.push_back(clip.header.name);
                continue;
            }
            const float speed = std::max(0.0f, clip.header.speed);
            active.time += delta * speed;
            if (active.time >= duration)
            {
                if (clip.header.loop)
                    active.time = std::fmod(active.time, duration);
                else
                {
                    active.time = duration;
                    active.ended = true;
                    if (onEndAnimation)
                        endedClipNames.push_back(clip.header.name);
                }
            }
        }
        for (const std::string &clipName : endedClipNames)
            onEndAnimation(clipName.c_str(), owner);
    }

    bool MESH_MBM::getArticulatedTransform(const ARTICULATED_ANIMATION_PLAYER &player,
                                           const uint32_t frameIndex, const uint32_t subsetIndex,
                                           VEC3 *translation, float rotationQuaternion[4], VEC3 *scale,
                                           VEC3 *pivot, float pivotQuaternion[4]) const noexcept
    {
        if (!translation || !rotationQuaternion || !scale || !pivot || !pivotQuaternion)
            return false;
        const util::ARTICULATED_PART_V11 *part = nullptr;
        for (const auto &candidate : impl->articulatedParts)
        {
            if (candidate.frameIndex == frameIndex && candidate.subsetIndex == subsetIndex)
            {
                part = &candidate;
                break;
            }
        }
        if (!part)
            return false;

        *translation = VEC3(0.0f, 0.0f, 0.0f);
        *scale = VEC3(1.0f, 1.0f, 1.0f);
        rotationQuaternion[0] = rotationQuaternion[1] = rotationQuaternion[2] = 0.0f;
        rotationQuaternion[3] = 1.0f;
        *pivot = VEC3(part->pivotX, part->pivotY, part->pivotZ);
        pivotQuaternion[0] = part->pivotQX; pivotQuaternion[1] = part->pivotQY;
        pivotQuaternion[2] = part->pivotQZ; pivotQuaternion[3] = part->pivotQW;

        const auto applyEasing = [](float value, const util::ARTICULATED_KEY_V11 &key)
        {
            value = std::max(0.0f, std::min(1.0f, value));
            switch (key.easing)
            {
                case util::ARTICULATED_EASING_IN:
                    return value * value;
                case util::ARTICULATED_EASING_OUT:
                    return 1.0f - (1.0f - value) * (1.0f - value);
                case util::ARTICULATED_EASING_IN_OUT:
                    return value < 0.5f ? 2.0f * value * value
                                       : 1.0f - 2.0f * (1.0f - value) * (1.0f - value);
                case util::ARTICULATED_EASING_SMOOTHSTEP:
                    return value * value * (3.0f - 2.0f * value);
                case util::ARTICULATED_EASING_BEZIER:
                {
                    const auto cubic = [](const float t, const float p1, const float p2)
                    {
                        const float oneMinusT = 1.0f - t;
                        return 3.0f * oneMinusT * oneMinusT * t * p1 +
                               3.0f * oneMinusT * t * t * p2 +
                               t * t * t;
                    };
                    const auto cubicDerivative = [](const float t, const float p1, const float p2)
                    {
                        const float oneMinusT = 1.0f - t;
                        return 3.0f * oneMinusT * oneMinusT * p1 +
                               6.0f * oneMinusT * t * (p2 - p1) +
                               3.0f * t * t * (1.0f - p2);
                    };
                    float parameter = value;
                    for (int i = 0; i < 6; ++i)
                    {
                        const float difference = cubic(parameter, key.bezierX1, key.bezierX2) - value;
                        const float derivative = cubicDerivative(parameter, key.bezierX1, key.bezierX2);
                        if (std::fabs(difference) < 0.00001f || std::fabs(derivative) < 0.00001f)
                            break;
                        parameter = std::max(0.0f, std::min(1.0f, parameter - difference / derivative));
                    }
                    float low = 0.0f;
                    float high = 1.0f;
                    for (int i = 0; i < 10; ++i)
                    {
                        const float currentX = cubic(parameter, key.bezierX1, key.bezierX2);
                        if (std::fabs(currentX - value) < 0.00001f)
                            break;
                        if (currentX < value)
                            low = parameter;
                        else
                            high = parameter;
                        parameter = (low + high) * 0.5f;
                    }
                    return cubic(parameter, key.bezierY1, key.bezierY2);
                }
                default:
                    return value;
            }
        };
        const auto quaternionFromEulerDegrees = [](const float eulerX, const float eulerY,
                                                   const float eulerZ, float out[4])
        {
            constexpr float degreesToRadians = 0.017453292519943295769f;
            const float halfYaw = eulerY * degreesToRadians * 0.5f;
            const float halfPitch = -eulerX * degreesToRadians * 0.5f;
            const float halfRoll = eulerZ * degreesToRadians * 0.5f;
            const float sy = std::sin(halfYaw), cy = std::cos(halfYaw);
            const float sx = std::sin(halfPitch), cx = std::cos(halfPitch);
            const float sz = std::sin(halfRoll), cz = std::cos(halfRoll);
            const float yaw[4] = {0.0f, sy, 0.0f, cy};
            const float pitch[4] = {sx, 0.0f, 0.0f, cx};
            const float roll[4] = {0.0f, 0.0f, sz, cz};
            const auto multiply = [](const float a[4], const float b[4], float result[4])
            {
                result[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
                result[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
                result[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
                result[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
            };
            float yawPitch[4];
            multiply(yaw, pitch, yawPitch);
            multiply(yawPitch, roll, out);
        };

        struct SAMPLED_TRACK
        {
            VEC3 translation = VEC3(0.0f, 0.0f, 0.0f);
            VEC3 scale = VEC3(1.0f, 1.0f, 1.0f);
            VEC3 rotationEuler = VEC3(0.0f, 0.0f, 0.0f);
            float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            bool hasRotationEuler = false;
        };
        const auto sampleTrack = [&applyEasing, &quaternionFromEulerDegrees](
                                     const ARTICULATED_TRACK_DATA &track, const float time,
                                     SAMPLED_TRACK &sample)
        {
            const auto &keys = track.keys;
            const util::ARTICULATED_KEY_V11 *a = &keys.front();
            const util::ARTICULATED_KEY_V11 *b = &keys.front();
            float factor = 0.0f;
            if (time >= keys.back().time)
                a = b = &keys.back();
            else if (time > keys.front().time)
            {
                for (size_t i = 1; i < keys.size(); ++i)
                {
                    if (time <= keys[i].time)
                    {
                        a = &keys[i - 1];
                        b = &keys[i];
                        const float span = b->time - a->time;
                        factor = span > 0.0f ? (time - a->time) / span : 0.0f;
                        break;
                    }
                }
            }
            factor = applyEasing(factor, *a);
            const auto lerp = [factor](const float x, const float y) { return x + (y - x) * factor; };
            sample.translation.x = lerp(a->positionX, b->positionX);
            sample.translation.y = lerp(a->positionY, b->positionY);
            sample.translation.z = lerp(a->positionZ, b->positionZ);
            sample.scale.x = lerp(a->scaleX, b->scaleX);
            sample.scale.y = lerp(a->scaleY, b->scaleY);
            sample.scale.z = lerp(a->scaleZ, b->scaleZ);
            if (a->hasRotationEuler && b->hasRotationEuler)
            {
                sample.rotationEuler.x = lerp(a->rotationEulerX, b->rotationEulerX);
                sample.rotationEuler.y = lerp(a->rotationEulerY, b->rotationEulerY);
                sample.rotationEuler.z = lerp(a->rotationEulerZ, b->rotationEulerZ);
                sample.hasRotationEuler = true;
                quaternionFromEulerDegrees(
                    sample.rotationEuler.x,
                    sample.rotationEuler.y,
                    sample.rotationEuler.z,
                    sample.rotation);
            }
            else
            {
                float endQuaternion[4] = {b->rotationX, b->rotationY, b->rotationZ, b->rotationW};
                const float dot = a->rotationX * endQuaternion[0] + a->rotationY * endQuaternion[1] +
                                  a->rotationZ * endQuaternion[2] + a->rotationW * endQuaternion[3];
                if (dot < 0.0f)
                {
                    endQuaternion[0] = -endQuaternion[0]; endQuaternion[1] = -endQuaternion[1];
                    endQuaternion[2] = -endQuaternion[2]; endQuaternion[3] = -endQuaternion[3];
                }
                sample.rotation[0] = lerp(a->rotationX, endQuaternion[0]);
                sample.rotation[1] = lerp(a->rotationY, endQuaternion[1]);
                sample.rotation[2] = lerp(a->rotationZ, endQuaternion[2]);
                sample.rotation[3] = lerp(a->rotationW, endQuaternion[3]);
            }
            const float length = std::sqrt(sample.rotation[0] * sample.rotation[0] +
                                           sample.rotation[1] * sample.rotation[1] +
                                           sample.rotation[2] * sample.rotation[2] +
                                           sample.rotation[3] * sample.rotation[3]);
            if (length > 0.000001f)
            {
                sample.rotation[0] /= length; sample.rotation[1] /= length;
                sample.rotation[2] /= length; sample.rotation[3] /= length;
            }
        };

        struct CHANNEL_CANDIDATE
        {
            const ACTIVE_ARTICULATED_CLIP *active = nullptr;
            const ARTICULATED_TRACK_DATA *track = nullptr;
        };
        using CHANNEL_CANDIDATES = std::vector<CHANNEL_CANDIDATE>;
        CHANNEL_CANDIDATES positionCandidates;
        CHANNEL_CANDIDATES rotationCandidates;
        CHANNEL_CANDIDATES scaleCandidates;
        CHANNEL_CANDIDATES additivePositionCandidates;
        CHANNEL_CANDIDATES additiveRotationCandidates;
        CHANNEL_CANDIDATES additiveScaleCandidates;
        const auto addCandidate = [](CHANNEL_CANDIDATES &candidates,
                                     const ACTIVE_ARTICULATED_CLIP &active,
                                     const ARTICULATED_TRACK_DATA &track)
        {
            for (auto &candidate : candidates)
            {
                if (candidate.active == &active)
                {
                    candidate.track = &track;
                    return;
                }
            }
            candidates.push_back({&active, &track});
        };
        for (const auto &active : player.impl->activeClips)
        {
            if (active.clipIndex >= impl->articulatedClips.size())
                continue;
            const ARTICULATED_CLIP_DATA &clip = impl->articulatedClips[active.clipIndex];
            for (const auto &track : clip.tracks)
            {
                if (track.header.partId != part->partId || track.keys.empty())
                    continue;
                CHANNEL_CANDIDATES *positionTarget = clip.header.blendMode == util::ARTICULATED_BLEND_ADDITIVE
                    ? &additivePositionCandidates : &positionCandidates;
                CHANNEL_CANDIDATES *rotationTarget = clip.header.blendMode == util::ARTICULATED_BLEND_ADDITIVE
                    ? &additiveRotationCandidates : &rotationCandidates;
                CHANNEL_CANDIDATES *scaleTarget = clip.header.blendMode == util::ARTICULATED_BLEND_ADDITIVE
                    ? &additiveScaleCandidates : &scaleCandidates;
                const uint8_t mask = track.header.channelMask;
                if (mask & util::ARTICULATED_CHANNEL_POSITION)
                    addCandidate(*positionTarget, active, track);
                if (mask & util::ARTICULATED_CHANNEL_ROTATION)
                    addCandidate(*rotationTarget, active, track);
                if (mask & util::ARTICULATED_CHANNEL_SCALE)
                    addCandidate(*scaleTarget, active, track);
            }
        }
        const auto sortCandidates = [](CHANNEL_CANDIDATES &candidates)
        {
            std::sort(candidates.begin(), candidates.end(),
                [](const CHANNEL_CANDIDATE &left, const CHANNEL_CANDIDATE &right)
                {
                    return left.active->priority < right.active->priority ||
                           (left.active->priority == right.active->priority &&
                            left.active->sequence < right.active->sequence);
                });
        };
        sortCandidates(positionCandidates);
        sortCandidates(rotationCandidates);
        sortCandidates(scaleCandidates);
        sortCandidates(additivePositionCandidates);
        sortCandidates(additiveRotationCandidates);
        sortCandidates(additiveScaleCandidates);

        const auto blendFactor = [](const ACTIVE_ARTICULATED_CLIP &active)
        {
            if (active.blendDuration <= 0.0f)
                return 1.0f;
            return std::max(0.0f, std::min(1.0f, active.blendElapsed / active.blendDuration));
        };
        const auto blendVector = [](const VEC3 &source, const VEC3 &target, const float factor)
        {
            return VEC3(source.x + (target.x - source.x) * factor,
                        source.y + (target.y - source.y) * factor,
                        source.z + (target.z - source.z) * factor);
        };
        const auto blendQuaternion = [](const float source[4], const float target[4],
                                        const float factor, float out[4])
        {
            float end[4] = {target[0], target[1], target[2], target[3]};
            float dot = source[0] * end[0] + source[1] * end[1] +
                        source[2] * end[2] + source[3] * end[3];
            if (dot < 0.0f)
            {
                dot = -dot;
                end[0] = -end[0]; end[1] = -end[1]; end[2] = -end[2]; end[3] = -end[3];
            }
            dot = std::max(-1.0f, std::min(1.0f, dot));
            if (dot > 0.9995f)
            {
                out[0] = source[0] + (end[0] - source[0]) * factor;
                out[1] = source[1] + (end[1] - source[1]) * factor;
                out[2] = source[2] + (end[2] - source[2]) * factor;
                out[3] = source[3] + (end[3] - source[3]) * factor;
            }
            else
            {
                const float angle = std::acos(dot);
                const float denominator = std::sin(angle);
                const float sourceWeight = std::sin((1.0f - factor) * angle) / denominator;
                const float targetWeight = std::sin(factor * angle) / denominator;
                out[0] = source[0] * sourceWeight + end[0] * targetWeight;
                out[1] = source[1] * sourceWeight + end[1] * targetWeight;
                out[2] = source[2] * sourceWeight + end[2] * targetWeight;
                out[3] = source[3] * sourceWeight + end[3] * targetWeight;
            }
            const float length = std::sqrt(out[0] * out[0] + out[1] * out[1] +
                                           out[2] * out[2] + out[3] * out[3]);
            if (length > 0.000001f)
            {
                out[0] /= length; out[1] /= length; out[2] /= length; out[3] /= length;
            }
        };
        const auto multiplyQuaternion = [](const float left[4], const float right[4], float out[4])
        {
            out[0] = left[3] * right[0] + left[0] * right[3] +
                     left[1] * right[2] - left[2] * right[1];
            out[1] = left[3] * right[1] - left[0] * right[2] +
                     left[1] * right[3] + left[2] * right[0];
            out[2] = left[3] * right[2] + left[0] * right[1] -
                     left[1] * right[0] + left[2] * right[3];
            out[3] = left[3] * right[3] - left[0] * right[0] -
                     left[1] * right[1] - left[2] * right[2];
        };

        const auto applyVectorChannel = [&sampleTrack, &blendFactor, &blendVector](
                                            const CHANNEL_CANDIDATES &candidates,
                                            const bool useScale, VEC3 &value)
        {
            for (const auto &candidate : candidates)
            {
                SAMPLED_TRACK targetSample;
                sampleTrack(*candidate.track, candidate.active->time, targetSample);
                const VEC3 &target = useScale ? targetSample.scale : targetSample.translation;
                value = blendVector(value, target, blendFactor(*candidate.active));
            }
        };
        applyVectorChannel(positionCandidates, false, *translation);
        applyVectorChannel(scaleCandidates, true, *scale);

        for (const auto &candidate : rotationCandidates)
        {
            SAMPLED_TRACK targetSample;
            sampleTrack(*candidate.track, candidate.active->time, targetSample);
            float source[4] = {rotationQuaternion[0], rotationQuaternion[1],
                               rotationQuaternion[2], rotationQuaternion[3]};
            blendQuaternion(source, targetSample.rotation,
                            blendFactor(*candidate.active), rotationQuaternion);
        }

        for (const auto &candidate : additivePositionCandidates)
        {
            SAMPLED_TRACK sample;
            sampleTrack(*candidate.track, candidate.active->time, sample);
            const float effectiveWeight = candidate.active->weight * blendFactor(*candidate.active);
            translation->x += sample.translation.x * effectiveWeight;
            translation->y += sample.translation.y * effectiveWeight;
            translation->z += sample.translation.z * effectiveWeight;
        }
        for (const auto &candidate : additiveScaleCandidates)
        {
            SAMPLED_TRACK sample;
            sampleTrack(*candidate.track, candidate.active->time, sample);
            const float effectiveWeight = candidate.active->weight * blendFactor(*candidate.active);
            scale->x *= 1.0f + (sample.scale.x - 1.0f) * effectiveWeight;
            scale->y *= 1.0f + (sample.scale.y - 1.0f) * effectiveWeight;
            scale->z *= 1.0f + (sample.scale.z - 1.0f) * effectiveWeight;
        }
        for (const auto &candidate : additiveRotationCandidates)
        {
            SAMPLED_TRACK sample;
            sampleTrack(*candidate.track, candidate.active->time, sample);
            const float identity[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            float weightedDelta[4];
            const float effectiveWeight = candidate.active->weight * blendFactor(*candidate.active);
            if (sample.hasRotationEuler)
            {
                quaternionFromEulerDegrees(sample.rotationEuler.x * effectiveWeight,
                                           sample.rotationEuler.y * effectiveWeight,
                                           sample.rotationEuler.z * effectiveWeight,
                                           weightedDelta);
            }
            else
            {
                blendQuaternion(identity, sample.rotation, effectiveWeight, weightedDelta);
            }
            float composed[4];
            multiplyQuaternion(rotationQuaternion, weightedDelta, composed);
            rotationQuaternion[0] = composed[0]; rotationQuaternion[1] = composed[1];
            rotationQuaternion[2] = composed[2]; rotationQuaternion[3] = composed[3];
        }
        if (!additiveRotationCandidates.empty())
        {
            const float length = std::sqrt(rotationQuaternion[0] * rotationQuaternion[0] +
                                           rotationQuaternion[1] * rotationQuaternion[1] +
                                           rotationQuaternion[2] * rotationQuaternion[2] +
                                           rotationQuaternion[3] * rotationQuaternion[3]);
            if (length > 0.000001f)
            {
                rotationQuaternion[0] /= length; rotationQuaternion[1] /= length;
                rotationQuaternion[2] /= length; rotationQuaternion[3] /= length;
            }
        }

        return !positionCandidates.empty() || !rotationCandidates.empty() || !scaleCandidates.empty() ||
               !additivePositionCandidates.empty() || !additiveRotationCandidates.empty() ||
               !additiveScaleCandidates.empty();
    }

    bool MESH_MBM::buildArticulatedTransformMatrix(const ARTICULATED_ANIMATION_PLAYER &player,
                                                   const uint32_t frameIndex, const uint32_t subsetIndex,
                                                   MATRIX *out) const noexcept
    {
        if (!out)
            return false;
        MatrixIdentity(out);
        const util::ARTICULATED_PART_V11 *part = nullptr;
        for (const auto &candidate : impl->articulatedParts)
        {
            if (candidate.frameIndex == frameIndex && candidate.subsetIndex == subsetIndex)
            {
                part = &candidate;
                break;
            }
        }
        if (!part)
            return false;

        const auto normalizeQuaternion = [](float q[4])
        {
            const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
            if (length > 0.000001f)
            {
                q[0] /= length; q[1] /= length; q[2] /= length; q[3] /= length;
            }
            else
            {
                q[0] = q[1] = q[2] = 0.0f; q[3] = 1.0f;
            }
        };
        const auto multiplyQuaternion = [](const float a[4], const float b[4], float outQuaternion[4])
        {
            outQuaternion[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
            outQuaternion[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
            outQuaternion[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
            outQuaternion[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
        };
        const auto quaternionMatrix = [](MATRIX *matrix, const float q[4])
        {
            const float xx = q[0] * q[0], yy = q[1] * q[1], zz = q[2] * q[2];
            const float xy = q[0] * q[1], xz = q[0] * q[2], yz = q[1] * q[2];
            const float xw = q[0] * q[3], yw = q[1] * q[3], zw = q[2] * q[3];
            MatrixIdentity(matrix);
            matrix->_11 = 1.0f - 2.0f * (yy + zz);
            matrix->_12 = 2.0f * (xy + zw);
            matrix->_13 = 2.0f * (xz - yw);
            matrix->_21 = 2.0f * (xy - zw);
            matrix->_22 = 1.0f - 2.0f * (xx + zz);
            matrix->_23 = 2.0f * (yz + xw);
            matrix->_31 = 2.0f * (xz + yw);
            matrix->_32 = 2.0f * (yz - xw);
            matrix->_33 = 1.0f - 2.0f * (xx + yy);
        };
        const auto buildLocal = [&](const util::ARTICULATED_PART_V11 *localPart, MATRIX *matrix)
        {
            VEC3 translation(0.0f, 0.0f, 0.0f), scale(1.0f, 1.0f, 1.0f), pivot;
            float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            float pivotRotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            getArticulatedTransform(player, localPart->frameIndex, localPart->subsetIndex,
                                    &translation, rotation, &scale, &pivot, pivotRotation);
            normalizeQuaternion(rotation);
            normalizeQuaternion(pivotRotation);
            const float inversePivot[4] = {-pivotRotation[0], -pivotRotation[1], -pivotRotation[2], pivotRotation[3]};
            float orientedRotation[4], combinedRotation[4];
            multiplyQuaternion(pivotRotation, rotation, orientedRotation);
            multiplyQuaternion(orientedRotation, inversePivot, combinedRotation);
            normalizeQuaternion(combinedRotation);
            MATRIX step;
            MatrixTranslation(matrix, -pivot.x, -pivot.y, -pivot.z);
            MatrixScaling(&step, scale.x, scale.y, scale.z);
            MatrixMultiply(matrix, matrix, &step);
            quaternionMatrix(&step, combinedRotation);
            MatrixMultiply(matrix, matrix, &step);
            MatrixTranslation(&step, pivot.x + translation.x, pivot.y + translation.y, pivot.z + translation.z);
            MatrixMultiply(matrix, matrix, &step);
        };
        std::unordered_set<uint64_t> recursion;
        const auto buildRecursive = [&](auto &&self, const util::ARTICULATED_PART_V11 *current,
                                        MATRIX *matrix) -> bool
        {
            if (!current || !recursion.insert(current->partId).second)
                return false;
            MATRIX local;
            buildLocal(current, &local);
            if (current->parentPartId != 0)
            {
                const util::ARTICULATED_PART_V11 *parent = nullptr;
                for (const auto &candidate : impl->articulatedParts)
                {
                    if (candidate.frameIndex == current->frameIndex &&
                        candidate.partId == current->parentPartId)
                    {
                        parent = &candidate;
                        break;
                    }
                }
                if (parent)
                {
                    MATRIX parentMatrix;
                    if (self(self, parent, &parentMatrix))
                        // This engine uses row-vector transforms (translation lives in _41/_42/_43):
                        // apply the child's local transform first, then its parent hierarchy.
                        MatrixMultiply(matrix, &local, &parentMatrix);
                    else
                        *matrix = local;
                }
                else
                    *matrix = local;
            }
            else
                *matrix = local;
            recursion.erase(current->partId);
            return true;
        };
        return buildRecursive(buildRecursive, part, out);
    }

    bool MESH_MBM::renderArticulatedStatic(const ARTICULATED_ANIMATION_PLAYER &player,
                                           const uint32_t indexFrame, const SHADER *pShader,
                                           const MATRIX &viewMatrix, const MATRIX &perspectiveMatrix,
                                           const RENDERIZABLE *renderizableOwner)
    {
        if (!pShader || !hasActiveArticulatedAnimations(player) || !impl->buffer ||
            indexFrame >= impl->totalFramesMesh)
            return false;

        const BUFFER_MESH &frameBuffer = impl->buffer[indexFrame];
        if (!frameBuffer.pBufferGL || frameBuffer.totalSubset == 0)
            return false;

        const MATRIX baseModelView = SHADER::modelView;

        DEVICE *device = DEVICE::getInstance();
        device->setRenderMaterial(this->impl->material);
        bool rendered = true;
        for (uint32_t subsetIndex = 0; subsetIndex < frameBuffer.totalSubset; ++subsetIndex)
        {
            MATRIX partTransform;
            buildArticulatedTransformMatrix(player, indexFrame, subsetIndex, &partTransform);

            MatrixMultiply(&SHADER::modelView, &partTransform, &baseModelView);
            SHADER::updateMvpAndLightMatrices(viewMatrix, perspectiveMatrix);
            if (!pShader->render(frameBuffer.pBufferGL, renderizableOwner, static_cast<int32_t>(subsetIndex)))
                rendered = false;
        }
        device->clearRenderMaterial();
        SHADER::modelView = baseModelView;
        SHADER::updateMvpAndLightMatrices(viewMatrix, perspectiveMatrix);
        return rendered;
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

    bool MESH_MBM::canUseCpuSkeletalPath(const SKELETAL_SHADER_METHOD method,
                                         const SKELETAL_ANIMATION_PLAYER *player,
                                         const char **reason) const noexcept
    {
        if (method != SKELETAL_SHADER_METHOD::LBS && method != SKELETAL_SHADER_METHOD::DQS_RIGID)
        {
            if (reason) *reason = "cpu-method-unresolved";
            return false;
        }
        if (impl->canonicalSkeleton.skeletonId == 0 || impl->canonicalWeights.skeletonId == 0)
        {
            if (reason) *reason = "no-skeletal-data";
            return false;
        }
        if (impl->canonicalWeights.skeletonId != impl->canonicalSkeleton.skeletonId)
        {
            if (reason) *reason = "skeleton-weight-id-mismatch";
            return false;
        }
        if (impl->skeletalBindFrameIndex == UINT32_MAX || impl->skeletalBindPositions.empty())
        {
            if (reason) *reason = "missing-bind-geometry";
            return false;
        }
        if (impl->skeletalBindPositions.size() != impl->canonicalWeights.vertices.size())
        {
            if (reason) *reason = "weight-vertex-count-mismatch";
            return false;
        }
        if (method == SKELETAL_SHADER_METHOD::DQS_RIGID)
        {
            // DQS needs stricter checks than LBS because it only accepts rigid transforms.
            // An active player's evaluated pose and 8-float-per-bone palette have already passed
            // that validation, so avoid rescanning every clip in the per-frame render path.
            // Without an active pose, scan the skeleton and clips to report readiness truthfully.
            const uint32_t boneCount =
                static_cast<uint32_t>(impl->canonicalSkeleton.compiled.bones.size());
            if (player && player->impl->active)
            {
                if (player->impl->evaluatedGlobalTransforms.size() != boneCount)
                {
                    if (reason) *reason = "cpu-dqs-missing-evaluated-pose";
                    return false;
                }
                if (player->impl->paletteRows.size() != static_cast<size_t>(boneCount) * 8u)
                {
                    if (reason) *reason = "cpu-dqs-missing-evaluated-palette";
                    return false;
                }
            }
            else if (skeletal::getDqsCompatibility(impl->canonicalSkeleton, impl->canonicalAnimations) !=
                         skeletal::DQS_COMPATIBILITY_STATUS::RIGID)
            {
                if (reason) *reason = "cpu-dqs-non-rigid-skeleton-or-clips";
                return false;
            }
        }
        if (reason) *reason = "ready";
        return true;
    }

    bool MESH_MBM::renderCpuSkeletal(const SKELETAL_ANIMATION_PLAYER &player,
                                     const uint32_t indexFrame, BUFFER_MESH &dynamicBuffer,
                                     std::vector<VEC3> &positions, std::vector<VEC3> &normals,
                                     std::vector<VEC2> &uvs, bool &initialized,
                                     const SHADER *pShader,
                                     const RENDERIZABLE *renderizableOwner) const
    {
        const char *reason = nullptr;
        const SKELETAL_SHADER_METHOD method = player.impl->resolvedSkinningMethod;
        if (!canUseCpuSkeletalPath(method, &player, &reason) || !pShader ||
            indexFrame != impl->skeletalBindFrameIndex ||
            player.impl->paletteRows.empty())
            return false;
        const uint32_t vertexCount = static_cast<uint32_t>(impl->skeletalBindPositions.size());
        const uint32_t floatsPerBone = method == SKELETAL_SHADER_METHOD::DQS_RIGID ? 8u : 12u;
        if (player.impl->paletteRows.size() != impl->canonicalSkeleton.compiled.bones.size() * floatsPerBone)
            return false;
        if (!initialized)
        {
            dynamicBuffer.release();
            const BUFFER_MESH &source = impl->buffer[indexFrame];
            dynamicBuffer.totalSubset = source.totalSubset;
            dynamicBuffer.subset = new util::SUBSET[source.totalSubset];
            for (uint32_t subsetIndex = 0; subsetIndex < source.totalSubset; ++subsetIndex)
            {
                dynamicBuffer.subset[subsetIndex] = source.subset[subsetIndex];
                dynamicBuffer.subset[subsetIndex].materialTextureSlotHeaders =
                    source.subset[subsetIndex].materialTextureSlotHeaders;
                dynamicBuffer.subset[subsetIndex].materialTextures =
                    source.subset[subsetIndex].materialTextures;
            }
            dynamicBuffer.pBufferGL = new BUFFER_GL();
            bool ok = false;
            if (impl->skeletalBindHasIndices)
            {
                std::vector<int> indexStart(source.totalSubset);
                std::vector<int> indexCount(source.totalSubset);
                for (uint32_t subsetIndex = 0; subsetIndex < source.totalSubset; ++subsetIndex)
                {
                    indexStart[subsetIndex] = source.subset[subsetIndex].indexStart;
                    indexCount[subsetIndex] = source.subset[subsetIndex].indexCount;
                }
                ok = dynamicBuffer.pBufferGL->loadBufferDynamic(impl->skeletalBindIndices.data(),
                    source.totalSubset, indexStart.data(), indexCount.data(),
                    impl->skeletalBindHasNormals, impl->skeletalBindHasUvs, &impl->info_mode);
            }
            else
            {
                std::vector<int> vertexStart(source.totalSubset);
                std::vector<int> vertexCountSubset(source.totalSubset);
                for (uint32_t subsetIndex = 0; subsetIndex < source.totalSubset; ++subsetIndex)
                {
                    vertexStart[subsetIndex] = source.subset[subsetIndex].vertexStart;
                    vertexCountSubset[subsetIndex] = source.subset[subsetIndex].vertexCount;
                }
                ok = dynamicBuffer.pBufferGL->loadBuffer(impl->skeletalBindPositions.data(),
                    impl->skeletalBindHasNormals ? impl->skeletalBindNormals.data() : nullptr,
                    impl->skeletalBindHasUvs ? impl->skeletalBindUvs.data() : nullptr,
                    vertexCount, source.totalSubset, vertexStart.data(), vertexCountSubset.data(),
                    &impl->info_mode, true);
            }
            if (!ok)
            {
                dynamicBuffer.release();
                return false;
            }
            for (uint32_t subsetIndex = 0; subsetIndex < source.totalSubset; ++subsetIndex)
            {
                dynamicBuffer.pBufferGL->setTextureByStage(source.pBufferGL->getTextureByStage(0, subsetIndex), 0, subsetIndex);
                for (uint32_t stage = 1; stage <= 5; ++stage)
                    dynamicBuffer.pBufferGL->setTextureByStage(source.pBufferGL->getTextureByStage(stage, subsetIndex), stage, subsetIndex);
            }
            positions.resize(vertexCount);
            normals.resize(impl->skeletalBindHasNormals ? vertexCount : 0);
            uvs = impl->skeletalBindUvs;
            initialized = true;
        }

        if (method == SKELETAL_SHADER_METHOD::DQS_RIGID)
        {
            skeletal::SKELETAL_POSE pose;
            if (player.impl->evaluatedGlobalTransforms.empty())
            {
                pose.globalTransforms.reserve(impl->canonicalSkeleton.compiled.bones.size());
                for (const skeletal::COMPILED_BONE &bone : impl->canonicalSkeleton.compiled.bones)
                    pose.globalTransforms.push_back(bone.globalBindMatrix);
            }
            else
                pose.globalTransforms = player.impl->evaluatedGlobalTransforms;
            if (!skeletal::skinVerticesDqsRigidReference(impl->canonicalSkeleton, impl->canonicalWeights,
                    pose, impl->skeletalBindPositions,
                    impl->skeletalBindHasNormals ? impl->skeletalBindNormals : std::vector<VEC3>(),
                    positions, normals))
                return false;
        }
        else
        {
            const float *palette = player.impl->paletteRows.data();
            for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
            {
                const VEC3 &bindPosition = impl->skeletalBindPositions[vertexIndex];
                VEC3 outPosition(0.0f, 0.0f, 0.0f);
                VEC3 outNormal(0.0f, 0.0f, 0.0f);
                const bool hasNormal = impl->skeletalBindHasNormals && vertexIndex < impl->skeletalBindNormals.size();
                const VEC3 bindNormal = hasNormal ? impl->skeletalBindNormals[vertexIndex] : VEC3();
                const skeletal::CANONICAL_VERTEX_WEIGHT &weight = impl->canonicalWeights.vertices[vertexIndex];
                for (uint32_t slot = 0; slot < 4; ++slot)
                {
                    if (weight.paletteIndex[slot] == UINT16_MAX || weight.weight[slot] == 0.0f)
                        continue;
                    if (weight.paletteIndex[slot] >= impl->canonicalWeights.paletteBoneIds.size())
                        return false;
                    const uint64_t boneId = impl->canonicalWeights.paletteBoneIds[weight.paletteIndex[slot]];
                    const auto found = impl->canonicalSkeleton.compiled.indexById.find(boneId);
                    if (found == impl->canonicalSkeleton.compiled.indexById.end())
                        return false;
                    const float *rows = &palette[static_cast<size_t>(found->second) * 12u];
                    const float w = weight.weight[slot];
                    outPosition.x += (bindPosition.x * rows[0] + bindPosition.y * rows[1] +
                                      bindPosition.z * rows[2] + rows[3]) * w;
                    outPosition.y += (bindPosition.x * rows[4] + bindPosition.y * rows[5] +
                                      bindPosition.z * rows[6] + rows[7]) * w;
                    outPosition.z += (bindPosition.x * rows[8] + bindPosition.y * rows[9] +
                                      bindPosition.z * rows[10] + rows[11]) * w;
                    if (hasNormal)
                    {
                        outNormal.x += (bindNormal.x * rows[0] + bindNormal.y * rows[1] +
                                        bindNormal.z * rows[2]) * w;
                        outNormal.y += (bindNormal.x * rows[4] + bindNormal.y * rows[5] +
                                        bindNormal.z * rows[6]) * w;
                        outNormal.z += (bindNormal.x * rows[8] + bindNormal.y * rows[9] +
                                        bindNormal.z * rows[10]) * w;
                    }
                }
                positions[vertexIndex] = outPosition;
                if (hasNormal)
                {
                    const float lenSq = outNormal.x * outNormal.x + outNormal.y * outNormal.y +
                        outNormal.z * outNormal.z;
                    if (lenSq > 0.00000001f)
                    {
                        const float invLen = 1.0f / std::sqrt(lenSq);
                        outNormal.x *= invLen; outNormal.y *= invLen; outNormal.z *= invLen;
                    }
                    normals[vertexIndex] = outNormal;
                }
            }
        }
        std::vector<int> vertexStart(dynamicBuffer.totalSubset);
        std::vector<int> vertexCountSubset(dynamicBuffer.totalSubset);
        for (uint32_t subsetIndex = 0; subsetIndex < dynamicBuffer.totalSubset; ++subsetIndex)
        {
            vertexStart[subsetIndex] = dynamicBuffer.subset[subsetIndex].vertexStart;
            vertexCountSubset[subsetIndex] = dynamicBuffer.subset[subsetIndex].vertexCount;
        }
        if (!dynamicBuffer.pBufferGL->updateDynamic(positions.data(),
                normals.empty() ? nullptr : normals.data(),
                uvs.empty() ? nullptr : uvs.data(),
                vertexStart.data(), vertexCountSubset.data()))
            return false;
        DEVICE *device = DEVICE::getInstance();
        device->setRenderMaterial(this->impl->material);
        const bool ret = pShader->render(dynamicBuffer.pBufferGL, renderizableOwner);
        device->clearRenderMaterial();
        return ret;
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

    bool MESH_MBM::load(const char *fileNamePath, RENDERIZABLE * /*renderizable*/)
    {
        // `renderizable` used to receive positionOffset_deprecated/angleDefault_deprecated here;
        // no longer applied (see MESH_MBM::Impl's own comment on those fields). Parameter kept for
        // ABI/call-site compatibility.
        return this->loadV11(fileNamePath);
    }

    // Main-thread-only GPU-finish half of loading a v11 mesh (async loading). Consumes
    // the pure-CPU MESH_LOAD_INTERMEDIATE_V11 that parse_v11_intermediate produced (on the calling
    // thread for loadV11, on a worker thread for loadAsync) and does everything that touches the GPU
    // or other shared/global state: TEXTURE_MANAGER::load, BUFFER_GL, EnablePixelPerfectTexture,
    // util::addPath. `in` is consumed (its owning vectors/arrays are moved out of), not just read.
    bool MESH_MBM::finishLoadFromIntermediate(MESH_LOAD_INTERMEDIATE_V11 &in, const char *fileNamePath)
    {
        this->release();
        impl->fileName       = fileNamePath;
        impl->typeMe         = in.typeMe;
        impl->material       = in.material;
        impl->positionOffset_deprecated = in.positionOffset_deprecated;
        impl->angleDefault_deprecated   = in.angleDefault_deprecated;
        impl->info_mode      = in.info_mode;
        impl->infoPhysics.lsCube        = std::move(in.infoPhysics.lsCube);
        impl->infoPhysics.lsCubeComplex = std::move(in.infoPhysics.lsCubeComplex);
        impl->infoPhysics.lsSphere      = std::move(in.infoPhysics.lsSphere);
        impl->infoPhysics.lsTriangle    = std::move(in.infoPhysics.lsTriangle);
        impl->infoAnimation.lsHeaderAnim = std::move(in.infoAnimation.lsHeaderAnim);
        impl->articulatedParts = std::move(in.articulatedParts);
        impl->articulatedClips = std::move(in.articulatedClips);
        impl->canonicalSkeleton = std::move(in.canonicalSkeleton);
        impl->canonicalWeights = std::move(in.canonicalWeights);
        impl->canonicalAnimations = std::move(in.canonicalAnimations);
        if (impl->canonicalSkeleton.skeletonId != 0 || impl->canonicalWeights.skeletonId != 0)
        {
            const skeletal::SKINNING_CAPABILITY capability =
                skeletal::getMeasuredSkinningCapability();
            const skeletal::GPU_SKINNING_PREPARATION_STATUS status = skeletal::prepareGpuSkinningInput(
                impl->canonicalSkeleton, impl->canonicalWeights, capability, impl->gpuSkinningInput);
            const uint64_t lbsPaletteBytes =
                static_cast<uint64_t>(impl->gpuSkinningInput.requiredBoneCount) * 3u * 4u * sizeof(float);
            const uint64_t dqsPaletteBytes =
                static_cast<uint64_t>(impl->gpuSkinningInput.requiredBoneCount) * 2u * 4u * sizeof(float);
            INFO_LOG("GPU skeletal input: status=%s vertices=%u lbs-bones=%u/%u "
                     "lbs-palette-bytes=%llu dqs-bones=%u/%u dqs-palette-bytes=%llu [%s]",
                     skeletal::gpuSkinningPreparationStatusName(status),
                     static_cast<uint32_t>(impl->gpuSkinningInput.vertices.size()),
                     impl->gpuSkinningInput.requiredBoneCount, impl->gpuSkinningInput.lbsBoneCapacity,
                     static_cast<unsigned long long>(lbsPaletteBytes),
                     impl->gpuSkinningInput.requiredBoneCount, impl->gpuSkinningInput.dqsBoneCapacity,
                     static_cast<unsigned long long>(dqsPaletteBytes), fileNamePath);
        }
        impl->extraInfo = in.extraInfo;
        in.extraInfo    = nullptr;

        if (impl->typeMe == util::TYPE_MESH_TILE_MAP)
            mbm::TEXTURE::EnablePixelPerfectTexture(true);
        else
            mbm::TEXTURE::EnablePixelPerfectTexture(false);

#ifndef ANDROID
        for (const auto &path : in.extraPaths)
            util::addPath(path.c_str());
#endif

        const auto totalFrames = static_cast<uint32_t>(in.frames.size());
        impl->buffer           = new BUFFER_MESH[totalFrames];
        impl->totalFramesMesh  = totalFrames;

        TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
        for (uint32_t currentFrame = 0; currentFrame < totalFrames; ++currentFrame)
        {
            IntermediateFrameV11 &frame       = in.frames[currentFrame];
            const auto            totalSubset = static_cast<uint32_t>(frame.subsets.size());
            impl->buffer[currentFrame].subset      = new util::SUBSET[totalSubset];
            impl->buffer[currentFrame].totalSubset = totalSubset;

            std::vector<TEXTURE *> lsIdTexture;
            for (uint32_t s = 0; s < totalSubset; ++s)
            {
                IntermediateSubsetV11 &subsetIn = frame.subsets[s];
                util::SUBSET          &subset   = impl->buffer[currentFrame].subset[s];
                subset.vertexStart = subsetIn.vertexStart;
                subset.vertexCount = subsetIn.vertexCount;
                subset.indexStart  = subsetIn.indexStart;
                subset.indexCount  = subsetIn.indexCount;
                // Do not load texture named "default", they are meant to be null/empty and will be replaced by the renderizable's material texture at render time.
                if(strcasecmp(subsetIn.primaryTexturePath.c_str(), "default") != 0)
                {
                    subset.texture     = textureManager->load(subsetIn.primaryTexturePath.c_str(), subsetIn.hasAlphaColor);
                }
                lsIdTexture.push_back(subset.texture);

                for (const auto &extraSlot : subsetIn.extraSlots)
                {
                    util::MATERIAL_TEXTURE_SLOT_HEADER slotHeader;
                    slotHeader.type = extraSlot.legacyType;
                    strncpy(slotHeader.nameTexture, extraSlot.path.c_str(), sizeof(slotHeader.nameTexture) - 1);
                    slotHeader.nameTexture[sizeof(slotHeader.nameTexture) - 1] = 0;
                    slotHeader.payloadSizeInBytes                              = 0;
                    subset.materialTextureSlotHeaders.push_back(slotHeader);
                    subset.materialTextures.push_back(textureManager->load(extraSlot.path.c_str(), true));
                }
            }

            impl->buffer[currentFrame].pBufferGL = new BUFFER_GL();
            const bool hasIndex                  = frame.indexCount > 0;
            bool       loadOk                    = false;
            if (impl->canonicalWeights.skeletonId != 0 &&
                currentFrame == impl->canonicalWeights.frameIndex)
            {
                impl->skeletalBindFrameIndex = currentFrame;
                impl->skeletalBindHasNormals = frame.hasNormal;
                impl->skeletalBindHasUvs = frame.uv != nullptr;
                impl->skeletalBindHasIndices = hasIndex;
                impl->skeletalBindPositions.assign(frame.position.get(),
                    frame.position.get() + frame.vertexCount);
                impl->skeletalBindNormals.clear();
                if (frame.hasNormal && frame.normal)
                    impl->skeletalBindNormals.assign(frame.normal.get(),
                        frame.normal.get() + frame.vertexCount);
                impl->skeletalBindUvs.clear();
                if (frame.uv)
                    impl->skeletalBindUvs.assign(frame.uv.get(), frame.uv.get() + frame.vertexCount);
                impl->skeletalBindIndices.clear();
                if (hasIndex)
                    impl->skeletalBindIndices.assign(frame.index.get(),
                        frame.index.get() + frame.indexCount);
            }
            if (hasIndex)
            {
                auto indexStartSubset = new int[totalSubset];
                auto indexCountSubset = new int[totalSubset];
                for (uint32_t s = 0; s < totalSubset; ++s)
                {
                    indexStartSubset[s] = impl->buffer[currentFrame].subset[s].indexStart;
                    indexCountSubset[s] = impl->buffer[currentFrame].subset[s].indexCount;
                }
                loadOk = impl->buffer[currentFrame].pBufferGL->loadBuffer(frame.position.get(), frame.normal.get(), frame.uv.get(),
                                                                          frame.vertexCount, frame.index.get(), totalSubset,
                                                                          indexStartSubset, indexCountSubset, &impl->info_mode);
                delete[] indexStartSubset;
                delete[] indexCountSubset;
            }
            else
            {
                auto vertexStartSubset = new int[totalSubset];
                auto vertexCountSubset = new int[totalSubset];
                for (uint32_t s = 0; s < totalSubset; ++s)
                {
                    vertexStartSubset[s] = impl->buffer[currentFrame].subset[s].vertexStart;
                    vertexCountSubset[s] = impl->buffer[currentFrame].subset[s].vertexCount;
                }
                constexpr bool isDynamic = false;
                loadOk = impl->buffer[currentFrame].pBufferGL->loadBuffer(frame.position.get(), frame.normal.get(), frame.uv.get(),
                                                                          frame.vertexCount, totalSubset, vertexStartSubset,
                                                                          vertexCountSubset, &impl->info_mode, isDynamic);
                delete[] vertexStartSubset;
                delete[] vertexCountSubset;
            }
            if (!loadOk)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "error on load buffer for frame %u [%s]", currentFrame, fileNamePath);

            if (currentFrame == 0 && impl->gpuSkinningInput.ready() &&
                !skeletal::uploadSkinVertexStream(impl->buffer[currentFrame].pBufferGL,
                                                  impl->gpuSkinningInput))
                return log_util::onFailed(nullptr, __FILE__, __LINE__,
                                          "failed to upload GPU skinning vertex stream [%s]", fileNamePath);

            const std::vector<TEXTURE *>::size_type totalIdTexture =
                (impl->buffer[currentFrame].pBufferGL->totalSubset > lsIdTexture.size())
                    ? lsIdTexture.size()
                    : impl->buffer[currentFrame].pBufferGL->totalSubset;
            for (std::vector<TEXTURE *>::size_type s = 0; s < totalIdTexture; ++s)
                impl->buffer[currentFrame].pBufferGL->setTextureByStage(lsIdTexture[s], 0, static_cast<uint32_t>(s));
        }

        impl->hasNormTex[0] = 0;
        impl->hasNormTex[1] = 0;
        return true;
    }

    bool MESH_MBM::loadV11(const char *fileNamePath)
    {
        MESH_LOAD_INTERMEDIATE_V11 intermediate;
        std::string                error;
        // Resolve here (main thread) - same call util::openFile used to make internally, just one
        // frame earlier - so parse_v11_intermediate can stay a plain "open this exact path" call
        // shared with the worker-thread async path. fileNamePath itself (the caller's original,
        // possibly-relative string) still flows into finishLoadFromIntermediate below unchanged, so
        // MESH::getFileName()/onLoadMeshLua's same-file check keep seeing what the caller passed in.
        const std::string resolvedPath = util::getFullPath(fileNamePath, nullptr);
        if (!parse_v11_intermediate(resolvedPath.c_str(), intermediate, error))
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "%s [%s]", error.c_str(), fileNamePath);
        return this->finishLoadFromIntermediate(intermediate, fileNamePath);
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

    // onComplete always fires from pumpAsyncLoads() on the main thread, never inline here - even on
    // a cache hit, so callers never have to special-case "sometimes synchronous" behavior. This was
    // tried as an inline-dispatch optimization (fire onComplete directly on a cache hit) and reverted
    // after it crashed real generated code: editor/scene_editor3d.lua's _loadMeshAsyncQueued chains
    // same-file loadAsync requests by calling processNext() again from inside its own callback,
    // written under (and relying on) the "callback never fires before this call returns" contract.
    // Once inline cache-hit dispatch made every request after the first for one file (e.g. 400 placed
    // instances of one prop) resolve synchronously, that chain recursed once per remaining request
    // with no yield back to the event loop, blowing the C/Lua call stack (confirmed reproduction:
    // SIGABRT via a corrupted Lua debug stack, "Could not get stack from LUA"). Real parsing happens
    // on a worker thread (lazily started); the GPU-finish work
    // (TEXTURE_MANAGER::load/BUFFER_GL/addPath/EnablePixelPerfectTexture) happens in pumpAsyncLoads,
    // on the main thread, since those touch the GPU context and other shared state that isn't safe
    // off the main thread.
    void MESH_MANAGER::loadAsync(const char *fileName, MeshAsyncLoadCallback onComplete)
    {
        const std::string fileNameBase = util::getBaseName(fileName);
        auto               cached      = this->impl->lsMeshes[fileNameBase];
        if (cached)
        {
            Impl::AsyncResult result;
            result.fileName   = fileNameBase;
            result.onComplete = std::move(onComplete);
            result.cacheHit   = true;
            result.cachedMesh = cached;
            std::lock_guard<std::mutex> lock(this->impl->completionMutex);
            this->impl->completedJobs.push_back(std::move(result));
            return;
        }

        this->impl->ensureWorkersStarted();
        Impl::AsyncJob job;
        job.fileName         = fileName;
        job.resolvedFileName = util::getFullPath(fileName, nullptr); // main thread only - safe here
        job.onComplete       = std::move(onComplete);
        {
            std::lock_guard<std::mutex> lock(this->impl->jobMutex);
            this->impl->jobQueue.push(std::move(job));
        }
        this->impl->jobCv.notify_one();
    }

    void MESH_MANAGER::pumpAsyncLoads()
    {
        std::vector<Impl::AsyncResult> finished;
        {
            std::lock_guard<std::mutex> lock(this->impl->completionMutex);
            if (this->impl->completedJobs.empty())
                return;
            finished.swap(this->impl->completedJobs);
        }

        for (auto &result : finished)
        {
            if (result.cacheHit)
            {
                if (result.onComplete)
                    result.onComplete(result.cachedMesh, true);
                continue;
            }

            if (!result.parseOk)
            {
                log_util::onFailed(nullptr, __FILE__, __LINE__, "%s [%s]", result.error.c_str(), result.fileName.c_str());
                if (result.onComplete)
                    result.onComplete(nullptr, false);
                continue;
            }

            auto mesh = new MESH_MBM();
            if (mesh->finishLoadFromIntermediate(result.intermediate, result.fileName.c_str()))
            {
                const std::string fileNameBase     = util::getBaseName(result.fileName.c_str());
                this->impl->lsMeshes[fileNameBase] = mesh;
                if (result.onComplete)
                    result.onComplete(mesh, true);
            }
            else
            {
                delete mesh;
                if (result.onComplete)
                    result.onComplete(nullptr, false);
            }
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
            mesh->impl->positionOffset_deprecated                    = VEC3(0, 0, 0);
            mesh->impl->angleDefault_deprecated                      = VEC3(0, 0, 0);
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

        mesh->impl->positionOffset_deprecated = VEC3(0, 0, 0);
        mesh->impl->angleDefault_deprecated   = VEC3(0, 0, 0);
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

        mesh->impl->positionOffset_deprecated = VEC3(0, 0, 0);
        mesh->impl->angleDefault_deprecated   = VEC3(0, 0, 0);
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
        mesh->impl->positionOffset_deprecated = VEC3(0, 0, 0);
        mesh->impl->angleDefault_deprecated   = VEC3(0, 0, 0);
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
        // Stop and join the async worker pool first - in-flight jobs are allowed to finish, but no
        // new ones start. Their results (if any landed in completedJobs after this) are simply
        // never pumped, and any MESH_MBM* they reference was never registered in lsMeshes, so the
        // cache-release loop below sees a consistent, fully-synchronous state.
        this->impl->stopAndJoinWorkers();

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
                break;
            default:
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Mesh invalid type");
                break;
        }
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
            impl->backBufferHeight = pMemoryInfoFont->heightLetter;
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
        impl->positionOffset_deprecated = VEC3(impl->headerMesh.posX, impl->headerMesh.posY, impl->headerMesh.posZ);
        impl->angleDefault_deprecated = VEC3(impl->headerMesh.angleX, impl->headerMesh.angleY, impl->headerMesh.angleZ);
        this->impl->sizeCoordTexFrame_0 = 0;
        if (this->impl->coordTexFrame_0)
            delete[] this->impl->coordTexFrame_0;
        this->impl->coordTexFrame_0 = nullptr;
        return true;
    }
}

mbm::MESH_MANAGER *    mbm::MESH_MANAGER::instanceMeshManager        = nullptr;
