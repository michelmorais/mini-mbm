#include "mesh-deprecated.h"
#include "mesh-v8-io-legacy.h"
#include "mesh-manager.h"
#include "header-mesh.h"
#include "header-mesh-legacy-disk.h"
#include "util-interface.h"
#include <miniz-wrap/miniz-wrap.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Phase B2 + milestone 10 (docs/mesh-v11-plan.md): converts a legacy v8-v10 mesh file into v11 by
// parsing the old bytes directly into a mbm::MESH_MBM_DEBUG via its PUBLIC API (this library has no
// access to core_mbm's private impl-> members), then calling MESH_MBM_DEBUG::saveV11. Scoped to
// exactly what saveV11 can persist today: 3D/SPRITE/USER/TEXTURE/SHAPE meshes with path-referenced
// textures and animations (frame ranges plus an optional named-shader-effect FX block) - FONT/
// PARTICLE/TILE_MAP, embedded/solid-color textures, a shader step's secondary texture stage, and
// pre-v8 files are all out of scope and rejected with a clear message rather than silently
// mishandled. See the mesh-v11-migration-status memory / docs/mesh-v11-plan.md for the full rationale.
namespace
{
    using mbm::VEC2;
    using mbm::VEC3;

    bool failf(char *errorOut, int lenErrorOut, const char *fmt, ...)
    {
        if (errorOut && lenErrorOut > 0)
        {
            va_list args;
            va_start(args, fmt);
            vsnprintf(errorOut, static_cast<size_t>(lenErrorOut), fmt, args);
            va_end(args);
        }
        return false;
    }

    util::TYPE_MESH typeMeshFromTypeApp(const char *typeApp) noexcept
    {
        if (strcmp(typeApp, MBM_TYPE_APP_MESH_3D) == 0)  return util::TYPE_MESH_3D;
        if (strcmp(typeApp, MBM_TYPE_APP_USER) == 0)     return util::TYPE_MESH_USER;
        if (strcmp(typeApp, MBM_TYPE_APP_SPRITE) == 0)   return util::TYPE_MESH_SPRITE;
        if (strcmp(typeApp, MBM_TYPE_APP_FONT) == 0)     return util::TYPE_MESH_FONT;
        if (strcmp(typeApp, MBM_TYPE_APP_TEXTURE) == 0)  return util::TYPE_MESH_TEXTURE;
        if (strcmp(typeApp, MBM_TYPE_APP_SHAPE) == 0)    return util::TYPE_MESH_SHAPE;
        if (strcmp(typeApp, MBM_TYPE_APP_PARTICLE) == 0) return util::TYPE_MESH_PARTICLE;
        if (strcmp(typeApp, MBM_TYPE_APP_TILE) == 0)     return util::TYPE_MESH_TILE_MAP;
        return util::TYPE_MESH_UNKNOWN;
    }

    // Mirrors loadDebugImpl's v3+ physics-detail loop (mesh-manager.cpp, deleted in milestone 5) -
    // a DETAIL_MESH 'P' header whose totalBounding is a running total consumed by nested,
    // type-grouped sub-headers (one CUBE/SPHERE/CUBE_COMPLEX/TRIANGLE group at a time). Shapes are
    // pushed straight into INFO_PHYSICS's public owning vectors - the same access pattern used
    // everywhere else this struct is touched from outside core_mbm.
    bool readPhysicsDetail(FILE *fp, const char *legacyPath, mbm::INFO_PHYSICS &infoPhysics, char *errorOut,
                            int lenErrorOut)
    {
        util::DETAIL_MESH outer;
        if (!util::readDetailMeshV8(fp, outer))
            return failf(errorOut, lenErrorOut, "failed to read physics detail header [%s]", legacyPath);
        if (outer.type != 'P')
            return failf(errorOut, lenErrorOut, "expected physics detail marker 'P' [%s]", legacyPath);

        for (int32_t i = 0; i < outer.totalBounding; )
        {
            util::DETAIL_MESH detail;
            if (!util::readDetailMeshV8(fp, detail))
                return failf(errorOut, lenErrorOut, "failed to read DETAIL_MESH [%s]", legacyPath);
            switch (detail.type)
            {
                case 1:
                    for (int32_t j = 0; j < detail.totalBounding; ++j)
                    {
                        auto *cube = new mbm::CUBE();
                        infoPhysics.lsCube.push_back(cube);
                        if (!util::readCubeV8(fp, *cube))
                            return failf(errorOut, lenErrorOut, "failed to read CUBE [%s]", legacyPath);
                    }
                    break;
                case 2:
                    for (int32_t j = 0; j < detail.totalBounding; ++j)
                    {
                        auto *sphere = new mbm::SPHERE();
                        infoPhysics.lsSphere.push_back(sphere);
                        if (!util::readSphereV8(fp, *sphere))
                            return failf(errorOut, lenErrorOut, "failed to read SPHERE [%s]", legacyPath);
                    }
                    break;
                case 3:
                    for (int32_t j = 0; j < detail.totalBounding; ++j)
                    {
                        auto *complex = new mbm::CUBE_COMPLEX();
                        infoPhysics.lsCubeComplex.push_back(complex);
                        if (!util::readCubeComplexV8(fp, *complex))
                            return failf(errorOut, lenErrorOut, "failed to read CUBE_COMPLEX [%s]", legacyPath);
                    }
                    break;
                case 4:
                    for (int32_t j = 0; j < detail.totalBounding; ++j)
                    {
                        auto *triangle = new mbm::TRIANGLE();
                        infoPhysics.lsTriangle.push_back(triangle);
                        // In-scope versions are always v8-v10, i.e. always >= MODE_DRAW_VERSION_MBM_HEADER (5),
                        // so the position-bearing reader always applies (readTriangleLegacyNoPosV8 is only
                        // for pre-5 files, out of scope).
                        if (!util::readTriangleV8(fp, *triangle))
                            return failf(errorOut, lenErrorOut, "failed to read TRIANGLE [%s]", legacyPath);
                    }
                    break;
                default:
                    return failf(errorOut, lenErrorOut,
                                "unsupported physics detail type [%d] [%s] - only static 3D/SPRITE/USER meshes "
                                "are supported",
                                detail.type, legacyPath);
            }
            i += detail.totalBounding;
        }
        return true;
    }

    struct ImportSlot
    {
        uint16_t    type;
        std::string texture;
    };

    struct ImportSubset
    {
        int32_t                  vertexStart = 0;
        int32_t                  vertexCount = 0;
        int32_t                  indexStart  = 0;
        int32_t                  indexCount  = 0;
        std::string              texture;
        std::vector<ImportSlot>  slots;
    };

    // Reads one subset's HEADER_DESC_SUBSET (v8 or v9, dispatched on file version) plus its v9+
    // material texture slots. Rejects embedded (#u) and solid-color-synthesized (#M/#RRGGBBAA)
    // texture references outright - saveV11 only ever writes TEXTURE_REF_V11::PATH_REFERENCE
    // (milestone 3 scope), so there's nowhere for those to go yet.
    bool readSubsetHeader(FILE *fp, const char *legacyPath, const int version, ImportSubset &out, char *errorOut,
                          int lenErrorOut)
    {
        util::HEADER_DESC_SUBSET desc;
        const bool ok = (version >= MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER) ? util::readHeaderDescSubsetV9(fp, desc)
                                                                              : util::readHeaderDescSubsetV8(fp, desc);
        if (!ok)
            return failf(errorOut, lenErrorOut, "failed to read subset header [%s]", legacyPath);

        out.vertexStart = desc.vertexStart;
        out.vertexCount = desc.vertexCount;
        out.indexStart  = desc.indexStart;
        out.indexCount  = desc.indexCount;

        if (desc.nameTexture[0] != 0 && strcmp(desc.nameTexture, "default") != 0)
        {
            if (desc.nameTexture[0] == '#')
            {
                return failf(errorOut, lenErrorOut,
                            "embedded or solid-color subset textures are not supported yet [%s]", legacyPath);
            }
            out.texture = desc.nameTexture;
        }

        if (version >= MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER)
        {
            for (uint16_t s = 0; s < desc.materialTextureSlotCount; ++s)
            {
                util::MATERIAL_TEXTURE_SLOT_HEADER slotHeader;
                if (!util::readMaterialTextureSlotHeaderV9(fp, slotHeader))
                    return failf(errorOut, lenErrorOut, "failed to read material texture slot [%s]", legacyPath);
                if (slotHeader.payloadSizeInBytes > 0 &&
                    std::fseek(fp, static_cast<long>(slotHeader.payloadSizeInBytes), SEEK_CUR) != 0)
                {
                    return failf(errorOut, lenErrorOut, "failed to skip material texture slot payload [%s]",
                                legacyPath);
                }
                // Known types carry their texture path inline in the fixed-size header itself; unknown
                // (future) slot types are simply skipped via payloadSizeInBytes above, per the format's
                // own forward-compatibility design - not imported, not an error.
                if (slotHeader.type == util::MATERIAL_TEXTURE_SLOT_NORMAL ||
                    slotHeader.type == util::MATERIAL_TEXTURE_SLOT_SPECULAR ||
                    slotHeader.type == util::MATERIAL_TEXTURE_SLOT_EMISSIVE ||
                    slotHeader.type == util::MATERIAL_TEXTURE_SLOT_MASK)
                {
                    out.slots.push_back({slotHeader.type, std::string(slotHeader.nameTexture)});
                }
            }
        }
        return true;
    }

    struct ImportFrame
    {
        int32_t                    stride      = 3;
        uint32_t                   vertexCount = 0;
        bool                        hasIndex    = false;
        std::vector<VEC3>          position;
        std::vector<VEC3>          normal; // empty when HAS_NOR_NO
        std::vector<VEC2>          uv;      // empty when HAS_TEX_NO
        std::vector<uint16_t>      index;
        std::vector<ImportSubset>  subsets;
    };

    // Mirrors load_from_separated_buffers_common's read side (mesh-manager.cpp, deleted in
    // milestone 5): frame header + every subset header, then the frame-wide position/normal/uv/index
    // arrays (shared across all of the frame's subsets, sliced per-subset by the caller).
    // `frame0Uv` is an in/out cache for HAS_TEX_FIRST_FRAME: empty in means "this is frame 0, read and
    // populate it"; non-empty in means "later frame, reuse it, nothing to read from the file."
    bool readFrame(FILE *fp, const char *legacyPath, const int version, const int16_t hasNorText[2],
                  std::vector<VEC2> &frame0Uv, ImportFrame &outFrame, char *errorOut, int lenErrorOut)
    {
        util::HEADER_FRAME headerFrame;
        if (!util::readHeaderFrameV8(fp, headerFrame))
            return failf(errorOut, lenErrorOut, "failed to read frame header [%s]", legacyPath);

        outFrame.stride      = headerFrame.stride;
        outFrame.vertexCount = static_cast<uint32_t>(headerFrame.sizeVertexBuffer);
        outFrame.hasIndex    = strncmp(headerFrame.typeBuffer, "IB", 2) == 0;

        outFrame.subsets.resize(static_cast<size_t>(headerFrame.totalSubset));
        for (auto &subset : outFrame.subsets)
        {
            if (!readSubsetHeader(fp, legacyPath, version, subset, errorOut, lenErrorOut))
                return false;
        }

        // On disk, the index buffer comes before the vertex/normal/uv buffers (mirrors the deleted
        // mesh-manager.cpp's read_frame_geometry/load_from_separated_buffers_common: index buffer is
        // read first when typeBuffer == "IB", vertex data always follows).
        if (outFrame.hasIndex)
        {
            outFrame.index.resize(static_cast<size_t>(headerFrame.sizeIndexBuffer));
            if (!util::readU16ArrayV8(fp, outFrame.index.data(), static_cast<uint32_t>(headerFrame.sizeIndexBuffer)))
                return failf(errorOut, lenErrorOut, "failed to read frame index [%s]", legacyPath);
        }

        outFrame.position.resize(outFrame.vertexCount);
        if (headerFrame.stride == 2)
        {
            for (uint32_t i = 0; i < outFrame.vertexCount; ++i)
            {
                VEC2 p;
                if (!util::readVec2V8(fp, p))
                    return failf(errorOut, lenErrorOut, "failed to read frame position [%s]", legacyPath);
                outFrame.position[i] = VEC3(p.x, p.y, 0.0f);
            }
        }
        else
        {
            if (!util::readVec3ArrayV8(fp, outFrame.position.data(), outFrame.vertexCount))
                return failf(errorOut, lenErrorOut, "failed to read frame position [%s]", legacyPath);
        }

        if (hasNorText[0] == HAS_NOR_IN_FILE)
        {
            outFrame.normal.resize(outFrame.vertexCount);
            if (!util::readVec3ArrayV8(fp, outFrame.normal.data(), outFrame.vertexCount))
                return failf(errorOut, lenErrorOut, "failed to read frame normal [%s]", legacyPath);
        }

        if (hasNorText[1] == HAS_TEX_FIRST_FRAME && !frame0Uv.empty())
        {
            const size_t safeCopy = std::min(frame0Uv.size(), static_cast<size_t>(outFrame.vertexCount));
            outFrame.uv.resize(outFrame.vertexCount);
            std::memcpy(outFrame.uv.data(), frame0Uv.data(), safeCopy * sizeof(VEC2));
        }
        else if (hasNorText[1] == HAS_TEX_EACH_FRAME || hasNorText[1] == HAS_TEX_FIRST_FRAME)
        {
            outFrame.uv.resize(outFrame.vertexCount);
            if (!util::readVec2ArrayV8(fp, outFrame.uv.data(), outFrame.vertexCount))
                return failf(errorOut, lenErrorOut, "failed to read frame uv [%s]", legacyPath);
            if (hasNorText[1] == HAS_TEX_FIRST_FRAME)
                frame0Uv = outFrame.uv;
        }
        return true;
    }
}

namespace mesh_deprecated
{
    bool convertLegacyMeshToV11(const char *legacyPath, const char *v11OutPath, char *errorOut, int lenErrorOut)
    {
        FILE *probe = util::openFile(legacyPath, "rb");
        if (!probe)
            return failf(errorOut, lenErrorOut, "failed to open file [%s]", legacyPath);
        fclose(probe);

        mbm::MINIZ miniz;
        char       minizError[256] = {0};
        if (!miniz.decompressFile(legacyPath, util::getDecompressModelFileName(), minizError, sizeof(minizError) - 1))
            return failf(errorOut, lenErrorOut, "failed to decompress legacy file [%s]\n%s", legacyPath, minizError);

        const char *tmpPath = util::getDecompressModelFileName();
        FILE       *fp      = util::openFile(tmpPath, "rb");
        if (!fp)
            return failf(errorOut, lenErrorOut, "failed to open decompressed temp file for [%s]", legacyPath);

        auto cleanup = [&]()
        {
            if (fp)
            {
                fclose(fp);
                fp = nullptr;
            }
            std::remove(tmpPath);
        };

        util::HEADER header;
        if (!util::readHeaderV8(fp, header))
        {
            cleanup();
            return failf(errorOut, lenErrorOut, "failed to read legacy file header [%s]", legacyPath);
        }
        if (header.version < STRONG_TYPES_VERSION_MBM_HEADER)
        {
            cleanup();
            return failf(errorOut, lenErrorOut, "pre-v8 legacy format is not supported yet [%s] (version %d)",
                        legacyPath, header.version);
        }

        const util::TYPE_MESH typeMe = typeMeshFromTypeApp(header.typeApp);
        if (typeMe == util::TYPE_MESH_FONT || typeMe == util::TYPE_MESH_PARTICLE || typeMe == util::TYPE_MESH_TILE_MAP)
        {
            cleanup();
            return failf(errorOut, lenErrorOut, "FONT/PARTICLE/TILE_MAP meshes are not supported yet [%s]",
                        legacyPath);
        }
        if (typeMe == util::TYPE_MESH_UNKNOWN)
        {
            cleanup();
            return failf(errorOut, lenErrorOut, "unrecognized mesh type app [%s] [%s]", header.typeApp, legacyPath);
        }

        // Extra headers (v6+, always present in-scope) - read and discard. saveV11 auto-derives its own
        // SECTION_EXTRA_PATHS from whatever texture paths actually get set below (getKnowPathsToExtraHeader),
        // so these legacy path hints aren't needed.
        for (int32_t i = 0; i < header.extraHeader; ++i)
        {
            util::EXTRA_HEADER extra;
            if (!util::readExtraHeaderV8(fp, extra))
            {
                cleanup();
                return failf(errorOut, lenErrorOut, "failed to read EXTRA_HEADER [%s]", legacyPath);
            }
            if (extra.type != 1)
            {
                cleanup();
                return failf(errorOut, lenErrorOut, "unsupported EXTRA_HEADER type [%d] [%s]", extra.type, legacyPath);
            }
            if (extra.sizeExtraHeader > 0 && std::fseek(fp, extra.sizeExtraHeader, SEEK_CUR) != 0)
            {
                cleanup();
                return failf(errorOut, lenErrorOut, "failed to skip EXTRA_HEADER path [%s]", legacyPath);
            }
        }

        util::INFO_DRAW_MODE infoMode;
        if (!util::readInfoDrawModeV8(fp, infoMode))
        {
            cleanup();
            return failf(errorOut, lenErrorOut, "failed to read draw mode [%s]", legacyPath);
        }

        mbm::MESH_MBM_DEBUG meshDebug;
        if (!readPhysicsDetail(fp, legacyPath, meshDebug.getPhysicsInfo(), errorOut, lenErrorOut))
        {
            cleanup();
            return false;
        }

        util::HEADER_MESH headerMesh;
        if (!util::readHeaderMeshV8(fp, headerMesh))
        {
            cleanup();
            return failf(errorOut, lenErrorOut, "failed to read mesh header [%s]", legacyPath);
        }

        // Animations: read each legacy HEADER_ANIMATION and, if present, its FX block (a real shader
        // effect referencing a built-in/custom shader by name, e.g. "transparent.ps" - never an
        // embedded shader source). Read order matters and was reverse-engineered from the pre-deletion
        // writer: for version >= TEXTURE_ANIMATION_EFFECT_VERSION_MBM_HEADER, the animation-level FX
        // texture-effect header comes BEFORE the PS/VS step pair, not after; and a step's variable
        // count is sizeArrayVarInBytes / 4, not the raw byte count.
        for (int32_t i = 0; i < headerMesh.totalAnimation; ++i)
        {
            util::HEADER_ANIMATION anim;
            if (!util::readHeaderAnimationV8(fp, anim))
            {
                cleanup();
                return failf(errorOut, lenErrorOut, "failed to read animation header [%s]", legacyPath);
            }

            auto *infoHead   = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            auto *headerAnim = new util::HEADER_ANIMATION(anim);
            infoHead->headerAnim = headerAnim;

            if (anim.hasShaderEffect != 0)
            {
                util::INFO_FX *fx = new util::INFO_FX();
                infoHead->effectShader = fx;

                if (header.version >= TEXTURE_ANIMATION_EFFECT_VERSION_MBM_HEADER)
                {
                    int16_t lenTextureAnimationEffect = 0;
                    if (!util::readHeaderInfoShaderEffectV10(fp, lenTextureAnimationEffect))
                    {
                        delete infoHead;
                        cleanup();
                        return failf(errorOut, lenErrorOut, "failed to read animation FX header [%s]", legacyPath);
                    }
                    if (lenTextureAnimationEffect > 0)
                    {
                        std::vector<char> fxTexPath(static_cast<size_t>(lenTextureAnimationEffect));
                        if (!fread(fxTexPath.data(), static_cast<size_t>(lenTextureAnimationEffect), 1, fp))
                        {
                            delete infoHead;
                            cleanup();
                            return failf(errorOut, lenErrorOut, "failed to read animation FX texture name [%s]", legacyPath);
                        }
                        fx->setTextureAnimationEffectFileName(fxTexPath.data());
                    }
                }

                const auto readStep = [&](util::INFO_SHADER_DATA *&outData, const char *label) -> bool
                {
                    util::HEADER_INFO_SHADER_STEP step;
                    if (!util::readHeaderInfoShaderStepV8(fp, step))
                        return failf(errorOut, lenErrorOut, "failed to read %s shader step [%s]", label, legacyPath);
                    fx->blendOperation = (step.blendOperation != 0) ? step.blendOperation : 1;
                    if (step.lenNameShader == 0)
                        return true;
                    if (step.lenTextureStage2 != 0)
                        return failf(errorOut, lenErrorOut,
                                    "a shader step's secondary texture stage is not supported yet [%s]", legacyPath);

                    auto *data = new util::INFO_SHADER_DATA(step.sizeArrayVarInBytes, step.lenNameShader, 0);
                    data->typeAnimation = step.typeAnimation;
                    data->timeAnimation = step.timeAnimation;
                    if (!fread(data->fileNameShader, static_cast<size_t>(step.lenNameShader), 1, fp))
                    {
                        delete data;
                        return failf(errorOut, lenErrorOut, "failed to read %s shader name [%s]", label, legacyPath);
                    }
                    if (data->lenVars > 0)
                    {
                        if (!fread(data->typeVars, static_cast<size_t>(data->lenVars), 1, fp) ||
                            !util::readFloatArrayV8(fp, data->min, static_cast<uint32_t>(data->lenVars) * 4) ||
                            !util::readFloatArrayV8(fp, data->max, static_cast<uint32_t>(data->lenVars) * 4))
                        {
                            delete data;
                            return failf(errorOut, lenErrorOut, "failed to read %s shader variables [%s]", label, legacyPath);
                        }
                    }
                    outData = data;
                    return true;
                };

                if (!readStep(fx->dataPS, "pixel"))
                {
                    delete infoHead;
                    cleanup();
                    return false;
                }
                if (!readStep(fx->dataVS, "vertex"))
                {
                    delete infoHead;
                    cleanup();
                    return false;
                }
            }

            meshDebug.appendAnimationHeader(infoHead);
        }

        meshDebug.setMeshType(typeMe);
        meshDebug.getMaterial() = headerMesh.material;
        meshDebug.setAngleDefault(VEC3(headerMesh.angleX, headerMesh.angleY, headerMesh.angleZ));
        meshDebug.setPositionOffset(VEC3(headerMesh.posX, headerMesh.posY, headerMesh.posZ));
        meshDebug.setModeDraw(infoMode.mode_draw);
        meshDebug.setModeCullFace(infoMode.mode_cull_face);
        meshDebug.setModeFrontFaceDirection(infoMode.mode_front_face_direction);

        std::vector<VEC2> frame0Uv;
        for (int32_t f = 0; f < headerMesh.totalFrames; ++f)
        {
            ImportFrame frame;
            if (!readFrame(fp, legacyPath, header.version, headerMesh.hasNorText, frame0Uv, frame, errorOut,
                           lenErrorOut))
            {
                cleanup();
                return false;
            }

            const uint32_t frameIdx = meshDebug.addBuffer(frame.stride) - 1;
            for (size_t s = 0; s < frame.subsets.size(); ++s)
            {
                const ImportSubset &legacySubset = frame.subsets[s];
                const uint32_t       subsetIdx    = meshDebug.addSubset(frameIdx) - 1;
                if (!meshDebug.addVertex(frameIdx, subsetIdx, static_cast<uint32_t>(legacySubset.vertexCount)))
                {
                    cleanup();
                    return failf(errorOut, lenErrorOut, "failed to add vertices for frame %d subset %zu [%s]", f, s,
                                legacyPath);
                }
                // Must re-fetch after every addVertex - it reallocates the frame's shared
                // position/normal/uv arrays, invalidating any pointers fetched before this call.
                util::SUBSET_DEBUG *newSubset  = meshDebug.getSubset(frameIdx, subsetIdx);
                VEC3                *newPosition = meshDebug.getPositionArray(frameIdx);
                VEC3                *newNormal   = meshDebug.getNormalArray(frameIdx);
                VEC2                *newUv       = meshDebug.getUvArray(frameIdx);

                const auto srcStart = static_cast<size_t>(legacySubset.vertexStart);
                const auto count    = static_cast<size_t>(legacySubset.vertexCount);
                const auto dstStart = static_cast<size_t>(newSubset->vertexStart);
                std::memcpy(&newPosition[dstStart], &frame.position[srcStart], sizeof(VEC3) * count);
                if (!frame.normal.empty())
                    std::memcpy(&newNormal[dstStart], &frame.normal[srcStart], sizeof(VEC3) * count);
                if (!frame.uv.empty())
                    std::memcpy(&newUv[dstStart], &frame.uv[srcStart], sizeof(VEC2) * count);

                newSubset->texture = legacySubset.texture;
                for (const auto &slot : legacySubset.slots)
                {
                    util::MATERIAL_TEXTURE_SLOT_DEBUG newSlot;
                    newSlot.type    = slot.type;
                    newSlot.texture = slot.texture;
                    newSubset->materialTextureSlots.push_back(newSlot);
                }

                if (legacySubset.indexCount > 0)
                {
                    // addIndex re-adds the subset's *new* vertexStart internally, so the indices passed
                    // in must be subset-local (0-based) - rebase the legacy frame-global indices by
                    // subtracting the legacy vertexStart they were originally offset by.
                    std::vector<uint16_t> localIndex(static_cast<size_t>(legacySubset.indexCount));
                    for (int32_t k = 0; k < legacySubset.indexCount; ++k)
                    {
                        const uint16_t globalIndex = frame.index[static_cast<size_t>(legacySubset.indexStart + k)];
                        localIndex[static_cast<size_t>(k)] =
                            static_cast<uint16_t>(globalIndex - legacySubset.vertexStart);
                    }
                    char addIndexErr[256] = {0};
                    if (!meshDebug.addIndex(frameIdx, subsetIdx, localIndex.data(),
                                            static_cast<uint32_t>(localIndex.size()), addIndexErr,
                                            sizeof(addIndexErr) - 1))
                    {
                        cleanup();
                        return failf(errorOut, lenErrorOut, "failed to add indices for frame %d subset %zu [%s]\n%s",
                                    f, s, legacyPath, addIndexErr);
                    }
                }
            }
        }

        // addVertex unconditionally forces hasNorText[0] to HAS_NOR_IN_FILE on every call (it always
        // allocates a normal array), so the mesh-wide flags must be set after the frame/subset loop,
        // not before - otherwise they'd just get clobbered back.
        meshDebug.setHasNormal(headerMesh.hasNorText[0]);
        meshDebug.setHasTexture(headerMesh.hasNorText[1]);
        if (headerMesh.hasNorText[0] == HAS_NOR_CALCULATE)
            meshDebug.calculateNormals();

        cleanup();

        return meshDebug.saveV11(v11OutPath, false, false, false, errorOut, lenErrorOut);
    }
}
