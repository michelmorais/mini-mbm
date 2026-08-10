#ifndef MESH_MANAGER_IMPL_H
#define MESH_MANAGER_IMPL_H

// Private implementation storage for MESH_MBM / MESH_MBM_DEBUG.
// include/core_mbm/mesh-manager.h only forward-declares `struct Impl` on both classes - this header
// is the one place their actual fields are defined, mirroring how MESH_MANAGER::Impl already works
// (struct MESH_MANAGER::Impl, defined directly in mesh-manager.cpp). Included by every .cpp file in
// core_mbm that constructs or directly touches these classes' fields; everything else uses the public
// getter/setter methods declared in mesh-manager.h and never needs to see this header.

#include <mesh-manager.h>
#include <header-mesh.h>
#include <physics.h>
#include <skeletal-animation-foundation.h>
#include <string>
#include <vector>

namespace mbm
{
    struct ARTICULATED_TRACK_DATA
    {
        util::ARTICULATED_TRACK_V11 header;
        std::vector<util::ARTICULATED_KEY_V11> keys;
    };

    struct ARTICULATED_CLIP_DATA
    {
        util::ARTICULATED_CLIP_V11 header;
        std::vector<ARTICULATED_TRACK_DATA> tracks;
    };

    struct ACTIVE_ARTICULATED_CLIP
    {
        uint32_t clipIndex = 0;
        float time = 0.0f;
        int priority = 0;
        uint64_t sequence = 0;
        float blendDuration = 0.0f;
        float blendElapsed = 0.0f;
        float weight = 1.0f;
        bool paused = false;
        bool ended = false;
    };

    struct ARTICULATED_ANIMATION_PLAYER::Impl
    {
        std::vector<ACTIVE_ARTICULATED_CLIP> activeClips;
        uint64_t sequence = 0;
    };

    struct MESH_MBM::Impl
    {
        // Deprecated: no longer applied to a loaded renderizable's position/angle at load time
        // (previously "Default position"/"Default angle" -- confusing, unused in practice, editing
        // it back to 0 visibly misaligned meshes that silently depended on a nonzero value). Field
        // kept only so existing .msh files' SECTION_MATERIAL_TRANSFORM payload still round-trips
        // byte-for-byte; there is no public getter/setter left, deliberately, so nothing new can
        // start relying on it again.
        VEC3 positionOffset_deprecated;
        VEC3 angleDefault_deprecated;
        util::MATERIAL material;
        INFO_PHYSICS infoPhysics;
        util::INFO_ANIMATION infoAnimation;
        util::INFO_DRAW_MODE info_mode;
        BUFFER_MESH *buffer;
        std::string fileName;
        VEC2 zoomEditorSprite;
        util::TYPE_MESH typeMe;
        int16_t hasNormTex[2];
        uint8_t depthUberImage;
        int sizeCoordTexFrame_0;
        VEC2 *coordTexFrame_0;
        uint32_t totalFramesMesh;
        void *extraInfo;
        std::vector<util::ARTICULATED_PART_V11> articulatedParts;
        std::vector<ARTICULATED_CLIP_DATA> articulatedClips;
        skeletal::CANONICAL_SKELETON canonicalSkeleton;
    };

    struct MESH_MBM_DEBUG::Impl
    {
        int32_t  backBufferWidth  = 0;
        int32_t  backBufferHeight = 0;
        uint16_t formatVersion    = MBM_V11_FORMAT_VERSION; // default for not-yet-saved/in-memory meshes
        util::HEADER_MESH headerMesh;
        INFO_PHYSICS infoPhysics;
        util::INFO_ANIMATION infoAnimation;
        util::INFO_DRAW_MODE info_mode;
        VEC2 zoomEditorSprite;
        util::TYPE_MESH typeMe;
        int sizeCoordTexFrame_0;
        VEC2 *coordTexFrame_0;
        // Deprecated: see MESH_MBM::Impl's positionOffset_deprecated/angleDefault_deprecated above
        // for the full rationale. Kept only for SECTION_MATERIAL_TRANSFORM round-trip fidelity;
        // no public getter/setter exists anymore.
        VEC3 positionOffset_deprecated;
        VEC3 angleDefault_deprecated;
        std::vector<util::BUFFER_MESH_DEBUG *> buffer;
        std::string fileName;
        std::vector<int> lsBlendOperation;
        void *extraInfo;
        // SECTION_FRAME_SKINNED (docs/mesh-v11-format.md Sec. 6e) - editor/diagnostic round-trip
        // only, never consulted by rendering. See MESH_MBM_DEBUG::addBone/getBone/getTotalBone.
        std::vector<util::SKELETON_BONE_V11> skeleton;
        skeletal::COMPILED_SKELETON compiledSkeletonBindReport;
        bool hasCompiledSkeletonBindReport = false;
        skeletal::CANONICAL_SKELETON canonicalSkeleton;
        // SECTION_VERTEX_SKIN_WEIGHTS (docs/mesh-v11-format.md Sec. 6f) - editor/diagnostic + FBX
        // re-export round-trip only, never consulted by rendering. weightPalette holds the unique
        // bone names referenced by any entry in vertexWeights; vertexWeights[i].paletteIndex values
        // index into weightPalette, not into `skeleton` above. Empty (both) means "no stored weight
        // data" - see MESH_MBM_DEBUG::hasVertexWeights/setVertexWeight/getVertexWeight.
        std::vector<std::string> weightPalette;
        std::vector<util::VERTEX_BONE_WEIGHT_V11> vertexWeights;
        std::vector<util::ARTICULATED_PART_V11> articulatedParts;
        std::vector<ARTICULATED_CLIP_DATA> articulatedClips;
    };
}

#endif
