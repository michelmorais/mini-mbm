#ifndef MESH_MANAGER_IMPL_H
#define MESH_MANAGER_IMPL_H

// Private implementation storage for MESH_MBM / MESH_MBM_DEBUG (docs/mesh-v11-plan.md, milestone 2).
// include/core_mbm/mesh-manager.h only forward-declares `struct Impl` on both classes - this header
// is the one place their actual fields are defined, mirroring how MESH_MANAGER::Impl already works
// (struct MESH_MANAGER::Impl, defined directly in mesh-manager.cpp). Included by every .cpp file in
// core_mbm that constructs or directly touches these classes' fields; everything else uses the public
// getter/setter methods declared in mesh-manager.h and never needs to see this header.

#include <mesh-manager.h>
#include <header-mesh.h>
#include <physics.h>
#include <string>
#include <vector>

namespace mbm
{
    struct MESH_MBM::Impl
    {
        VEC3 positionOffset;
        VEC3 angleDefault;
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
    };

    struct MESH_MBM_DEBUG::Impl
    {
        util::HEADER headerMain;
        util::HEADER_MESH headerMesh;
        INFO_PHYSICS infoPhysics;
        util::INFO_ANIMATION infoAnimation;
        util::INFO_DRAW_MODE info_mode;
        VEC2 zoomEditorSprite;
        util::TYPE_MESH typeMe;
        int sizeCoordTexFrame_0;
        VEC2 *coordTexFrame_0;
        VEC3 positionOffset;
        VEC3 angleDefault;
        std::vector<util::BUFFER_MESH_DEBUG *> buffer;
        std::string fileName;
        std::vector<int> lsBlendOperation;
        void *extraInfo;
    };
}

#endif
